
/* 
 * International Color Consortium Format Library (icclib)
 * For ICC profile version 3.4
 *
 * Author:  Graeme W. Gill
 * Date:    2002/04/22
 * Version: 2.15
 *
 * Copyright 1997 - 2013 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License.txt file in this directory for licensing details.
 */

/*
 * TTBD:
 *
 *      Add a "warning mode" to file reading, in which file format
 *      errors are ignored where possible, rather than generating
 *      a fatal error (see ICM_STRICT #define).
 *
 *      NameColor Dump doesn't handle device space correctly - 
 *	    should use appropriate interpretation in case device is Lab etc.
 *
 *      Should recognise & honour unicode 0xFFFE endian marker.
 *      Should generate it on writing too ?
 *
 *      Add support for copying tags from one icc to another.
 *
 *		Should fix all write_number failure errors to indicate failed value.
 *		(Partially implemented - need to check all write_number functions)
 *
 *		Make write fail error messages be specific on which element failed.
 *
 *		Should add named color space lookup function support.
 *
 *      Would be nice to add generic ability to add new tag type handling,
 *      so that the base library doesn't need to be modified (ie. VideoCardGamma) ?
 *
 *		Need to add DeviceSettings and OutputResponse tags to bring up to
 *		ICC.1:1998-09 [started but not complete]
 *
 */

#undef ICM_STRICT	/* Not fully implimented - switch off strict checking of file format */

/*  Make the default grid points of the Lab clut be symetrical about */
/* a/b 0.0, and also make L = 100.0 fall on a grid point. */
#define SYMETRICAL_DEFAULT_LAB_RANGE

#define _ICC_C_				/* Turn on implimentation code */

#undef DEBUG_SETLUT			/* [Und] Show each value being set in setting lut contents */
#undef DEBUG_SETLUT_CLIP	/* [Und] Show clipped values when setting LUT */
#undef DEBUG_LULUT			/* [Und] Show each value being looked up from lut contents */
#undef DEBUG_LLULUT			/* [Und] Debug individual lookup steps (not fully implemented) */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#ifdef __sun
#include <unistd.h>
#endif
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif
#include "icc.h"

#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif

/* ========================================================== */
/* Default system interface object implementations */

#ifndef SEPARATE_STD
#define COMBINED_STD

#include "iccstd.c"

#undef COMBINED_STD
#endif /* SEPARATE_STD */

/* Forced byte alignment for tag table and tags */
#define ALIGN_SIZE 4

/* =========================================================== */

#ifdef DEBUG_SETLUT
#undef DBGSL
#define DBGSL(xxx) printf xxx ;
#else
#undef DBGSL
#define DBGSL(xxx) 
#endif

#if defined(DEBUG_SETLUT) || defined(DEBUG_SETLUT_CLIP)
#undef DBGSLC
#define DBGSLC(xxx) printf xxx ;
#else
#undef DBGSLC
#define DBGSLC(xxx) 
#endif

#ifdef DEBUG_LULUT
#undef DBGLL
#define DBGLL(xxx) printf xxx ;
#else
#undef DBGLL
#define DBGLL(xxx) 
#endif

#ifdef DEBUG_LLULUT
#undef DBLLL
#define DBLLL(xxx) printf xxx ;
#else
#undef DBLLL
#define DBLLL(xxx) 
#endif

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

/* =========================================================== */
/* Overflow protected unsigned int arithmatic functions. */
/* These functions saturate rather than wrapping around. */
/* (Divide doesn't need protection) */
/* They return UINT_MAX if there was an overflow */

/* a + b */
static unsigned int sat_add(unsigned int a, unsigned int b) {
	if (b > (UINT_MAX - a))
		return UINT_MAX;
	return a + b;
}

/* a - b */
static unsigned int sat_sub(unsigned int a, unsigned int b) {
	if (a < b)
		return UINT_MAX;
	return a - b;
}

/* a * b */
static unsigned int sat_mul(unsigned int a, unsigned int b) {
	unsigned int c;

	if (a == 0 || b == 0)
		return 0;

	if (a > (UINT_MAX/b))
		return UINT_MAX;
	else
		return a * b;
}

/* A + B + C */
#define sat_addadd(A, B, C) sat_add(A, sat_add(B, C))

/* A + B * C */
#define sat_addmul(A, B, C) sat_add(A, sat_mul(B, C))

/* A + B + C * D */
#define sat_addaddmul(A, B, C, D) sat_add(A, sat_add(B, sat_mul(C, D)))

/* A * B * C */
#define sat_mul3(A, B, C) sat_mul(A, sat_mul(B, C))

/* a ^ b */
static unsigned int sat_pow(unsigned int a, unsigned int b) {
	unsigned int c = 1;
	for (; b > 0; b--) {
		c = sat_mul(c, a);
		if (c == UINT_MAX)
			break;
	}
	return c;
}

/* Alignment */
static unsigned int sat_align(unsigned int align_size, unsigned int a) {
	align_size--;

	if (align_size > (UINT_MAX - a))
		return UINT_MAX;

	return (a + align_size) & ~align_size;
}

/* These test functions detect whether an overflow would occur */

/* Return nz if add would overflow */
static int ovr_add(unsigned int a, unsigned int b) {

	if (b > (UINT_MAX - a))
		return 1;
	return 0;
}

/* Return nz if sub would overflow */
static int ovr_sub(unsigned int a, unsigned int b) {
	if (a < b)
		return 1;
	return 0;
}

/* Return nz if mult would overflow */
static int ovr_mul(unsigned int a, unsigned int b) {
	if (a > (UINT_MAX/b))
		return 1;
	return 0;
}


/* size_t versions of saturating arithmatic */

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)(-1))
#endif

/* a + b */
static size_t ssat_add(size_t a, size_t b) {
	if (b > (SIZE_MAX - a))
		return SIZE_MAX;
	return a + b;
}

/* a - b */
static size_t ssat_sub(size_t a, size_t b) {
	if (a < b)
		return SIZE_MAX;
	return a - b;
}

/* a * b */
static size_t ssat_mul(size_t a, size_t b) {
	size_t c;

	if (a == 0 || b == 0)
		return 0;

	if (a > (SIZE_MAX/b))
		return SIZE_MAX;
	else
		return a * b;
}

/* ------------------------------------------------- */
/* Memory image icmFile compatible class */
/* Buffer is assumed to have been allocated by the given allocator, */
/* and will be expanded on write. */

/* Get the size of the file */
static size_t icmFileMem_get_size(icmFile *pp) {
	icmFileMem *p = (icmFileMem *)pp;

	return p->end - p->start;
}

/* Set current position to offset. Return 0 on success, nz on failure. */
static int icmFileMem_seek(
icmFile *pp,
unsigned int offset
) {
	icmFileMem *p = (icmFileMem *)pp;
	unsigned char *np;

	np = p->start + offset;
	if (np < p->start || np >= p->end)
		return 1;
	p->cur = np;
	return 0;
}

/* Read count items of size length. Return number of items successfully read. */
static size_t icmFileMem_read(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileMem *p = (icmFileMem *)pp;
	size_t len;

	len = ssat_mul(size, count);
	if (len > (p->end - p->cur)) { /* Too much */
		if (size > 0)
			count = (p->end - p->cur)/size;
		else
			count = 0;
	}
	len = size * count;
	if (len > 0)
		memmove(buffer, p->cur, len);
	p->cur += len;
	return count;
}

/* Expand the memory buffer file to hold up to pointer ep */
/* Don't expand if realloc fails */
static void icmFileMem_filemem_resize(icmFileMem *p, unsigned char *ep) {
	size_t na, co, ce;
	unsigned char *nstart;
	
	/* No need to realloc */
	if (ep <= p->aend) {
		return;
	}

	co = p->cur - p->start;		/* Current offset */
	ce = p->end - p->start;     /* Current end */
	na = ep - p->start;			/* new allocated size */

	/* Round new allocation up */
	if (na <= 1024)
		na += 1024;
	else
		na += 4096;

	if ((nstart = p->al->realloc(p->al, p->start, na)) != NULL) {
		p->start = nstart;
		p->cur = nstart + co;
		p->end = nstart + ce;
		p->aend = nstart + na;
	}
}

/* write count items of size length. Return number of items successfully written. */
static size_t icmFileMem_write(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileMem *p = (icmFileMem *)pp;
	size_t len;

	len = ssat_mul(size, count);
	if (len > (size_t)(p->aend - p->cur))  /* Try and expand buffer */
		icmFileMem_filemem_resize(p, p->start + len);

	if (len > (size_t)(p->aend - p->cur)) {
		if (size > 0)
			count = (p->aend - p->cur)/size;
		else
			count = 0;
	}
	len = size * count;
	if (len > 0)
		memmove(p->cur, buffer, len);
	p->cur += len;
	if (p->end < p->cur)
		p->end = p->cur;
	return count;
}

/* do a printf */
static int icmFileMem_printf(
icmFile *pp,
const char *format,
...
) {
	int rv;
	va_list args;
	icmFileMem *p = (icmFileMem *)pp;
	int alen, len;

	va_start(args, format);

	rv = 1;
	alen = 100;					/* Initial allocation for printf */
	icmFileMem_filemem_resize(p, p->cur + alen);

	/* We have to use the available printf functions to resize the buffer if needed. */
	for (;rv != 0;) {
		/* vsnprintf() either returns -1 if it doesn't fit, or */
		/* returns the size-1 needed in order to fit. */
		len = vsnprintf((char *)p->cur, (p->aend - p->cur), format, args);

		if (len > -1 && ((p->cur + len +1) <= p->aend))	/* Fitted in current allocation */
			break;

		if (len > -1)				/* vsnprintf returned needed size-1 */
			alen = len+2;			/* (In case vsnprintf returned 1 less than it needs) */
		else
			alen *= 2;				/* We just have to guess */

		/* Attempt to resize */
		icmFileMem_filemem_resize(p, p->cur + alen);

		/* If resize failed */
		if ((p->aend - p->cur) < alen) {
			rv = 0;
			break;			
		}
	}
	if (rv != 0) {
		/* Figure out where end of printf is */
		len = strlen((char *)p->cur);	/* Length excluding nul */
		p->cur += len;
		if (p->cur > p->end)
			p->end = p->cur;
		rv = len;
	}
	va_end(args);
	return rv;
}

/* flush all write data out to secondary storage. Return nz on failure. */
static int icmFileMem_flush(
icmFile *pp
) {
	return 0;
}

/* Return the memory buffer. Error if not icmFileMem */
static int icmFileMem_get_buf(
icmFile *pp,
unsigned char **buf,
size_t *len
) {
	icmFileMem *p = (icmFileMem *)pp;
	if (buf != NULL)
		*buf = p->start;
	if (len != NULL)
		*len = p->end - p->start;
	return 0;
}

/* we're done with the file object, return nz on failure */
static int icmFileMem_delete(
icmFile *pp
) {
	icmFileMem *p = (icmFileMem *)pp;
	icmAlloc *al = p->al;
	int del_al   = p->del_al;

	if (p->del_buf)		/* Free the memory buffer */
		al->free(al, p->start);
	al->free(al, p);	/* Free object */
	if (del_al)			/* We are responsible for deleting allocator */
		al->del(al);
	return 0;
}

/* Create a memory image file access class with allocator */
/* Buffer is used as is. */
icmFile *new_icmFileMem_a(
void *base,			/* Pointer to base of memory buffer */
size_t length,		/* Number of bytes in buffer */
icmAlloc *al		/* heap allocator */
) {
	icmFileMem *p;

	if ((p = (icmFileMem *) al->calloc(al, 1, sizeof(icmFileMem))) == NULL) {
		return NULL;
	}
	p->al       = al;				/* Heap allocator */
	p->get_size = icmFileMem_get_size;
	p->seek     = icmFileMem_seek;
	p->read     = icmFileMem_read;
	p->write    = icmFileMem_write;
	p->gprintf  = icmFileMem_printf;
	p->flush    = icmFileMem_flush;
	p->get_buf  = icmFileMem_get_buf;
	p->del      = icmFileMem_delete;

	p->start = (unsigned char *)base;
	p->cur = p->start;
	p->aend = p->end = p->start + length;

	return (icmFile *)p;
}

/* Create a memory image file access class with given allocator */
/* and delete base when icmFile is deleted. */
icmFile *new_icmFileMem_ad(void *base, size_t length, icmAlloc *al) {
	icmFile *fp;

	if ((fp = new_icmFileMem_a(base, length, al)) != NULL) {	
		((icmFileMem *)fp)->del_buf = 1;
	}

	return fp;
}

/* ========================================================== */
/* Conversion support functions */
/* Convert between ICC storage types and native C types */
/* Write routine return non-zero if numbers can't be represented */

/* Unsigned */
static unsigned int read_UInt8Number(char *p) {
	unsigned int rv;
	rv = (unsigned int)((ORD8 *)p)[0];
	return rv;
}

static int write_UInt8Number(unsigned int d, char *p) {
	if (d > 255)
		return 1;
	((ORD8 *)p)[0] = (ORD8)d;
	return 0;
}

static unsigned int read_UInt16Number(char *p) {
	unsigned int rv;
	rv = 256 * (unsigned int)((ORD8 *)p)[0]
	   +       (unsigned int)((ORD8 *)p)[1];
	return rv;
}

static int write_UInt16Number(unsigned int d, char *p) {
	if (d > 65535)
		return 1;
	((ORD8 *)p)[0] = (ORD8)(d >> 8);
	((ORD8 *)p)[1] = (ORD8)(d);
	return 0;
}

static unsigned int read_UInt32Number(char *p) {
	unsigned int rv;
	rv = 16777216 * (unsigned int)((ORD8 *)p)[0]
	   +    65536 * (unsigned int)((ORD8 *)p)[1]
	   +      256 * (unsigned int)((ORD8 *)p)[2]
	   +            (unsigned int)((ORD8 *)p)[3];
	return rv;
}

static int write_UInt32Number(unsigned int d, char *p) {
	((ORD8 *)p)[0] = (ORD8)(d >> 24);
	((ORD8 *)p)[1] = (ORD8)(d >> 16);
	((ORD8 *)p)[2] = (ORD8)(d >> 8);
	((ORD8 *)p)[3] = (ORD8)(d);
	return 0;
}

static void read_UInt64Number(icmUint64 *d, char *p) {
	d->h = 16777216 * (unsigned int)((ORD8 *)p)[0]
	     +    65536 * (unsigned int)((ORD8 *)p)[1]
	     +      256 * (unsigned int)((ORD8 *)p)[2]
	     +            (unsigned int)((ORD8 *)p)[3];
	d->l = 16777216 * (unsigned int)((ORD8 *)p)[4]
	     +    65536 * (unsigned int)((ORD8 *)p)[5]
	     +      256 * (unsigned int)((ORD8 *)p)[6]
	     +            (unsigned int)((ORD8 *)p)[7];
}

static int write_UInt64Number(icmUint64 *d, char *p) {
	((ORD8 *)p)[0] = (ORD8)(d->h >> 24);
	((ORD8 *)p)[1] = (ORD8)(d->h >> 16);
	((ORD8 *)p)[2] = (ORD8)(d->h >> 8);
	((ORD8 *)p)[3] = (ORD8)(d->h);
	((ORD8 *)p)[4] = (ORD8)(d->l >> 24);
	((ORD8 *)p)[5] = (ORD8)(d->l >> 16);
	((ORD8 *)p)[6] = (ORD8)(d->l >> 8);
	((ORD8 *)p)[7] = (ORD8)(d->l);
	return 0;
}

static double read_U8Fixed8Number(char *p) {
	ORD32 o32;
	o32 = 256 * (ORD32)((ORD8 *)p)[0]		/* Read big endian 16 bit unsigned */
        +       (ORD32)((ORD8 *)p)[1];
	return (double)o32/256.0;
}

static int write_U8Fixed8Number(double d, char *p) {
	ORD32 o32;
	d = d * 256.0 + 0.5;
	if (d >= 65536.0)
		return 1;
	if (d < 0.0)
		return 1;
	o32 = (ORD32)d;
	((ORD8 *)p)[0] = (ORD8)((o32) >> 8);
	((ORD8 *)p)[1] = (ORD8)((o32));
	return 0;
}

static double read_U16Fixed16Number(char *p) {
	ORD32 o32;
	o32 = 16777216 * (ORD32)((ORD8 *)p)[0]		/* Read big endian 32 bit unsigned */
        +    65536 * (ORD32)((ORD8 *)p)[1]
	    +      256 * (ORD32)((ORD8 *)p)[2]
	    +            (ORD32)((ORD8 *)p)[3];
	return (double)o32/65536.0;
}

static int write_U16Fixed16Number(double d, char *p) {
	ORD32 o32;
	d = d * 65536.0 + 0.5;
	if (d >= 4294967296.0)
		return 1;
	if (d < 0.0)
		return 1;
	o32 = (ORD32)d;
	((ORD8 *)p)[0] = (ORD8)((o32) >> 24);
	((ORD8 *)p)[1] = (ORD8)((o32) >> 16);
	((ORD8 *)p)[2] = (ORD8)((o32) >> 8);
	((ORD8 *)p)[3] = (ORD8)((o32));
	return 0;
}


/* Signed numbers */
static int read_SInt8Number(char *p) {
	int rv;
	rv = (int)((INR8 *)p)[0];
	return rv;
}

static int write_SInt8Number(int d, char *p) {
	if (d > 127)
		return 1;
	else if (d < -128)
		return 1;
	((INR8 *)p)[0] = (INR8)d;
	return 0;
}

static int read_SInt16Number(char *p) {
	int rv;
	rv = 256 * (int)((INR8 *)p)[0]
	   +       (int)((ORD8 *)p)[1];
	return rv;
}

static int write_SInt16Number(int d, char *p) {
	if (d > 32767)
		return 1;
	else if (d < -32768)
		return 1;
	((INR8 *)p)[0] = (INR8)(d >> 8);
	((ORD8 *)p)[1] = (ORD8)(d);
	return 0;
}

static int read_SInt32Number(char *p) {
	int rv;
	rv = 16777216 * (int)((INR8 *)p)[0]
	   +    65536 * (int)((ORD8 *)p)[1]
	   +      256 * (int)((ORD8 *)p)[2]
	   +            (int)((ORD8 *)p)[3];
	return rv;
}

static int write_SInt32Number(int d, char *p) {
	((INR8 *)p)[0] = (INR8)(d >> 24);
	((ORD8 *)p)[1] = (ORD8)(d >> 16);
	((ORD8 *)p)[2] = (ORD8)(d >> 8);
	((ORD8 *)p)[3] = (ORD8)(d);
	return 0;
}

static void read_SInt64Number(icmInt64 *d, char *p) {
	d->h = 16777216 * (int)((INR8 *)p)[0]
	     +    65536 * (int)((ORD8 *)p)[1]
	     +      256 * (int)((ORD8 *)p)[2]
	     +            (int)((ORD8 *)p)[3];
	d->l = 16777216 * (unsigned int)((ORD8 *)p)[4]
	     +    65536 * (unsigned int)((ORD8 *)p)[5]
	     +      256 * (unsigned int)((ORD8 *)p)[6]
	     +            (unsigned int)((ORD8 *)p)[7];
}

static int write_SInt64Number(icmInt64 *d, char *p) {
	((INR8 *)p)[0] = (INR8)(d->h >> 24);
	((ORD8 *)p)[1] = (ORD8)(d->h >> 16);
	((ORD8 *)p)[2] = (ORD8)(d->h >> 8);
	((ORD8 *)p)[3] = (ORD8)(d->h);
	((ORD8 *)p)[4] = (ORD8)(d->l >> 24);
	((ORD8 *)p)[5] = (ORD8)(d->l >> 16);
	((ORD8 *)p)[6] = (ORD8)(d->l >> 8);
	((ORD8 *)p)[7] = (ORD8)(d->l);
	return 0;
}

static double read_S15Fixed16Number(char *p) {
	INR32 i32;
	i32 = 16777216 * (INR32)((INR8 *)p)[0]		/* Read big endian 32 bit signed */
        +    65536 * (INR32)((ORD8 *)p)[1]
	    +      256 * (INR32)((ORD8 *)p)[2]
	    +            (INR32)((ORD8 *)p)[3];
	return (double)i32/65536.0;
}

static int write_S15Fixed16Number(double d, char *p) {
	INR32 i32;
	d = floor(d * 65536.0 + 0.5);		/* Beware! (int)(d + 0.5) doesn't work! */
	if (d >= 2147483648.0)
		return 1;
	if (d < -2147483648.0)
		return 1;
	i32 = (INR32)d;
	((INR8 *)p)[0] = (INR8)((i32) >> 24);		/* Write big endian 32 bit signed */
	((ORD8 *)p)[1] = (ORD8)((i32) >> 16);
	((ORD8 *)p)[2] = (ORD8)((i32) >> 8);
	((ORD8 *)p)[3] = (ORD8)((i32));
	return 0;
}

/* Round a number to the same quantization as a S15Fixed16 */
static double round_S15Fixed16Number(double d) {
	d = floor(d * 65536.0 + 0.5);		/* Beware! (int)(d + 0.5) doesn't work for -ve nummbets ! */
	d = d/65536.0;
	return d;
}

/* Macro version */
#define RND_S15FIXED16(xxx) ((xxx) > 0.0 ? (int)((xxx) * 65536.0 + 0.5)/65536.0			\
                                         : (int)((xxx) * 65536.0 - 0.5)/65536.0)

/* Device coordinate as 8 bit value range 0.0 - 1.0 */
static double read_DCS8Number(char *p) {
	unsigned int rv;
	rv =   (unsigned int)((ORD8 *)p)[0];
	return (double)rv/255.0;
}

static int write_DCS8Number(double d, char *p) {
	ORD32 o32;
	d = d * 255.0 + 0.5;
	if (d >= 256.0)
		return 1;
	if (d < 0.0)
		return 1;
	o32 = (ORD32)d;
	((ORD8 *)p)[0] = (ORD8)(o32);
	return 0;
}

/* Device coordinate as 16 bit value range 0.0 - 1.0 */
static double read_DCS16Number(char *p) {
	unsigned int rv;
	rv = 256 * (unsigned int)((ORD8 *)p)[0]
	   +       (unsigned int)((ORD8 *)p)[1];
	return (double)rv/65535.0;
}

static int write_DCS16Number(double d, char *p) {
	ORD32 o32;
	d = d * 65535.0 + 0.5;
	if (d >= 65536.0)
		return 1;
	if (d < 0.0)
		return 1;
	o32 = (ORD32)d;
	((ORD8 *)p)[0] = (ORD8)(o32 >> 8);
	((ORD8 *)p)[1] = (ORD8)(o32);
	return 0;
}

static void Lut_Lut2XYZ(double *out, double *in);
static void Lut_XYZ2Lut(double *out, double *in);
static void Lut_Lut2Lab_8(double *out, double *in);
static void Lut_Lab2Lut_8(double *out, double *in);
static void Lut_Lut2LabV2_16(double *out, double *in);
static void Lut_Lab2LutV2_16(double *out, double *in);
static void Lut_Lut2LabV4_16(double *out, double *in);
static void Lut_Lab2LutV4_16(double *out, double *in);

static void Lut_Lut2Y(double *out, double *in);
static void Lut_Y2Lut(double *out, double *in);
static void Lut_Lut2L_8(double *out, double *in);
static void Lut_L2Lut_8(double *out, double *in);
static void Lut_Lut2LV2_16(double *out, double *in);
static void Lut_L2LutV2_16(double *out, double *in);
static void Lut_Lut2LV4_16(double *out, double *in);
static void Lut_L2LutV4_16(double *out, double *in);

/* read a PCS number. PCS can be profile PCS, profile version Lab, */
/* or a specific type of Lab, depending on the value of csig: */
/*   icmSigPCSData, icSigXYZData, icmSigLab8Data, icSigLabData, */
/*   icmSigLabV2Data or icmSigLabV4Data */
/* Do nothing if not one of the above. */
static void read_PCSNumber(icc *icp, icColorSpaceSignature csig, double pcs[3], char *p) {

	if (csig == icmSigPCSData)
		csig = icp->header->pcs;
	if (csig == icSigLabData) {
		if (icp->ver >= icmVersion4_1)
			csig = icmSigLabV4Data;
		else
			csig = icmSigLabV2Data;
	}

	if (csig == icmSigLab8Data) {
		pcs[0] = read_DCS8Number(p);
		pcs[1] = read_DCS8Number(p+1);
		pcs[2] = read_DCS8Number(p+2);
	} else {
		pcs[0] = read_DCS16Number(p);
		pcs[1] = read_DCS16Number(p+2);
		pcs[2] = read_DCS16Number(p+4);
	}
	switch (csig) {
		case icSigXYZData:
			Lut_Lut2XYZ(pcs, pcs);
			break;
		case icmSigLab8Data:
			Lut_Lut2Lab_8(pcs, pcs);
			break;
		case icmSigLabV2Data:
			Lut_Lut2LabV2_16(pcs, pcs);
			break;
		case icmSigLabV4Data:
			Lut_Lut2LabV4_16(pcs, pcs);
			break;
		default:
			break;
	}
}

/* write a PCS number. PCS can be profile PCS, profile version Lab, */
/* or a specific type of Lab, depending on the value of csig: */
/*   icmSigPCSData, icSigXYZData, icmSigLab8Data, icSigLabData, */
/*   icmSigLabV2Data or icmSigLabV4Data */
/* Return 1 if error */
static int write_PCSNumber(icc *icp, icColorSpaceSignature csig, double pcs[3], char *p) {
	double v[3];
	int j;

	if (csig == icmSigPCSData)
		csig = icp->header->pcs;
	if (csig == icSigLabData) {
		if (icp->ver >= icmVersion4_1)
			csig = icmSigLabV4Data;
		else
			csig = icmSigLabV2Data;
	}

	switch (csig) {
		case icSigXYZData:
			Lut_XYZ2Lut(v, pcs);
			break;
		case icmSigLab8Data:
			Lut_Lab2Lut_8(v, pcs);
			break;
		case icmSigLabV2Data:
			Lut_Lab2LutV2_16(v, pcs);
			break;
		case icmSigLabV4Data:
			Lut_Lab2LutV4_16(v, pcs);
			break;
		default:
			return 1;
	}
	if (csig == icmSigLab8Data) {
		for (j = 0; j < 3; j++) {
			if (write_DCS8Number(v[j], p+j))
				return 1;
		}
	} else {
		for (j = 0; j < 3; j++) {
			if (write_DCS16Number(v[j], p+(2 * j)))
				return 1;
		}
	}

	return 0;
}

/* Read a given primitive type. Return non-zero on error */
/* (Not currently used internaly ?) */
/* Public: */
int read_Primitive(icc *icp, icmPrimType ptype, void *prim, char *p) {

	switch(ptype) {
		case icmUInt8Number:
			*((unsigned int *)prim) = read_UInt8Number(p);
			return 0;
		case icmUInt16Number:
			*((unsigned int *)prim) = read_UInt16Number(p);
			return 0;
		case icmUInt32Number:
			*((unsigned int *)prim) = read_UInt32Number(p);
			return 0;
		case icmUInt64Number:
			read_UInt64Number((icmUint64 *)prim, p);
			return 0;
		case icmU8Fixed8Number:
			*((double *)prim) = read_U8Fixed8Number(p);
			return 0;
		case icmU16Fixed16Number:
			*((double *)prim) = read_U16Fixed16Number(p);
			return 0;
		case icmSInt8Number:
			*((int *)prim) = read_SInt8Number(p);
			return 0;
		case icmSInt16Number:
			*((int *)prim) = read_SInt16Number(p);
			return 0;
		case icmSInt32Number:
			*((int *)prim) = read_SInt32Number(p);
			return 0;
		case icmSInt64Number:
			read_SInt64Number((icmInt64 *)prim, p);
			return 0;
		case icmS15Fixed16Number:
			*((double *)prim) = read_S15Fixed16Number(p);
			return 0;
		case icmDCS8Number:
			*((double *)prim) = read_DCS8Number(p);
			return 0;
		case icmDCS16Number:
			*((double *)prim) = read_DCS16Number(p);
			return 0;
		case icmPCSNumber:
			read_PCSNumber(icp, icmSigPCSData, ((double *)prim), p);
			return 0;
		case icmPCSXYZNumber:
			read_PCSNumber(icp, icSigXYZData, ((double *)prim), p);
			return 0;
		case icmPCSLab8Number:
			read_PCSNumber(icp, icmSigLab8Data, ((double *)prim), p);
			return 0;
		case icmPCSLabNumber:
			read_PCSNumber(icp, icSigLabData, ((double *)prim), p);
			return 0;
		case icmPCSLabV2Number:
			read_PCSNumber(icp, icmSigLabV2Data, ((double *)prim), p);
			return 0;
		case icmPCSLabV4Number:
			read_PCSNumber(icp, icmSigLabV4Data, ((double *)prim), p);
			return 0;
	}

	return 2;
}

/* Write a given primitive type. Return non-zero on error */
/* (Not currently used internaly ?) */
/* Public: */
int write_Primitive(icc *icp, icmPrimType ptype, char *p, void *prim) {

	switch(ptype) {
		case icmUInt8Number:
			return write_UInt8Number(*((unsigned int *)prim), p);
		case icmUInt16Number:
			return write_UInt16Number(*((unsigned int *)prim), p);
		case icmUInt32Number:
			return write_UInt32Number(*((unsigned int *)prim), p);
		case icmUInt64Number:
			return write_UInt64Number((icmUint64 *)prim, p);
		case icmU8Fixed8Number:
			return write_U8Fixed8Number(*((double *)prim), p);
		case icmU16Fixed16Number:
			return write_U16Fixed16Number(*((double *)prim), p);
		case icmSInt8Number:
			return write_SInt8Number(*((int *)prim), p);
		case icmSInt16Number:
			return write_SInt16Number(*((int *)prim), p);
		case icmSInt32Number:
			return write_SInt32Number(*((int *)prim), p);
		case icmSInt64Number:
			return write_SInt64Number((icmInt64 *)prim, p);
		case icmS15Fixed16Number:
			return write_S15Fixed16Number(*((double *)prim), p);
		case icmDCS8Number:
			return write_DCS8Number(*((double *)prim), p);
		case icmDCS16Number:
			return write_DCS16Number(*((double *)prim), p);
		case icmPCSNumber:
			return write_PCSNumber(icp, icmSigPCSData, ((double *)prim), p);
		case icmPCSXYZNumber:
			return write_PCSNumber(icp, icSigXYZData, ((double *)prim), p);
		case icmPCSLab8Number:
			return write_PCSNumber(icp, icmSigLab8Data, ((double *)prim), p);
		case icmPCSLabNumber:
			return write_PCSNumber(icp, icSigLabData, ((double *)prim), p);
		case icmPCSLabV2Number:
			return write_PCSNumber(icp, icmSigLabV2Data, ((double *)prim), p);
		case icmPCSLabV4Number:
			return write_PCSNumber(icp, icmSigLabV4Data, ((double *)prim), p);
	}

	return 2;
}

/* ---------------------------------------------------------- */
/* Auiliary function - return a string that represents a tag */
/* Note - returned buffers are static, can only be used 5 */
/* times before buffers get reused. */
char *tag2str(
	int tag
) {
	int i;
	static int si = 0;			/* String buffer index */
	static char buf[5][20];		/* String buffers */
	char *bp;
	unsigned char c[4];

	bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	c[0] = 0xff & (tag >> 24);
	c[1] = 0xff & (tag >> 16);
	c[2] = 0xff & (tag >> 8);
	c[3] = 0xff & (tag >> 0);
	for (i = 0; i < 4; i++) {	/* Can we represent it as a string ? */
		if (!isprint(c[i]))
			break;
	}
	if (i < 4) {	/* Not printable - use hex */
		sprintf(bp,"0x%x",tag);
	} else {		/* Printable */
		sprintf(bp,"'%c%c%c%c'",c[0],c[1],c[2],c[3]);
	}
	return bp;
}

/* Auiliary function - return a tag created from a string */
/* Note there is also the icmMakeTag() macro */
unsigned int str2tag(
	const char *str
) {
	unsigned int tag;
	tag = (((unsigned int)str[0]) << 24)
	    + (((unsigned int)str[1]) << 16)
	    + (((unsigned int)str[2]) << 8)
	    + (((unsigned int)str[3]));
	return tag;
}

/* helper - return 1 if the string doesn't have a */
/* null terminator within len, return 0 has null at exactly len, */
/* and 2 if it has null before len. */
/* Note: will return 1 if len == 0 */
static int check_null_string(char *cp, int len) {
	for (; len > 0; len--) {
		if (cp[0] == '\000')
			break;
		cp++;
	}
	if (len == 0)
		return 1;
	if (len > 1)
		return 2;
	return 0;
}

/* helper - return 1 if the string doesn't have a */
/* null terminator within len, return 0 has null at exactly len, */
/* and 2 if it has null before len. */
/* Note: will return 1 if len == 0 */
/* Unicode version */
static int check_null_string16(char *cp, int len) {
	for (; len > 0; len--) {	/* Length is in characters */
		if (cp[0] == 0 && cp[1] == 0)
			break;
		cp += 2;
	}
	if (len == 0) 
		return 1;
	if (len > 1)
		return 2;
	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Color Space to number of component conversion */
/* Return 0 on error */
static unsigned int number_ColorSpaceSignature(icColorSpaceSignature sig) {
	switch(sig) {
		case icSigXYZData:
			return 3;
		case icSigLabData:
			return 3;
		case icSigLuvData:
			return 3;
		case icSigYCbCrData:
			return 3;
		case icSigYxyData:
			return 3;
		case icSigRgbData:
			return 3;
		case icSigGrayData:
			return 1;
		case icSigHsvData:
			return 3;
		case icSigHlsData:
			return 3;
		case icSigCmykData:
			return 4;
		case icSigCmyData:
			return 3;
		case icSig2colorData:
			return 2;
		case icSig3colorData:
			return 3;
		case icSig4colorData:
			return 4;
		case icSig5colorData:
		case icSigMch5Data:
			return 5;
		case icSig6colorData:
		case icSigMch6Data:
			return 6;
		case icSig7colorData:
		case icSigMch7Data:
			return 7;
		case icSig8colorData:
		case icSigMch8Data:
			return 8;
		case icSig9colorData:
			return 9;
		case icSig10colorData:
			return 10;
		case icSig11colorData:
			return 11;
		case icSig12colorData:
			return 12;
		case icSig13colorData:
			return 13;
		case icSig14colorData:
			return 14;
		case icSig15colorData:
			return 15;

		/* Non-standard and Pseudo spaces */
		case icmSigYData:
			return 1;
		case icmSigLData:
			return 1;
		case icmSigL8Data:
			return 1;
		case icmSigLV2Data:
			return 1;
		case icmSigLV4Data:
			return 1;
		case icmSigPCSData:
			return 3;
		case icmSigLab8Data:
			return 3;
		case icmSigLabV2Data:
			return 3;
		case icmSigLabV4Data:
			return 3;

		default:
			break;
	}
	return 0;
}

/* Public version of above */

/* Return the number of channels for the given color space. Return 0 if unknown. */
ICCLIB_API unsigned int icmCSSig2nchan(icColorSpaceSignature sig) {
	return number_ColorSpaceSignature(sig);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return the individual channel names and number of channels give a colorspace signature. */
/* Return 0 if it is not a colorspace that itself defines particular channels, */ 
/* 1 if it is a colorant based colorspace, and 2 if it is not a colorant based space */
static int chnames_ColorSpaceSignature(
icColorSpaceSignature sig,
char *cvals[]				/* Pointers to return for each channel */
) {
	switch (sig) {
    	case icSigXYZData:
			cvals[0] = "CIE X";
			cvals[1] = "CIE Y";
			cvals[2] = "CIE Z";
			return 2;

    	case icSigLabData:
			cvals[0] = "CIE L*";
			cvals[1] = "CIE a*";
			cvals[2] = "CIE b*";
			return 2;

    	case icSigLuvData:
			cvals[0] = "CIE L*";
			cvals[1] = "CIE u*";
			cvals[2] = "CIE v*";
			return 2;

		/* Usually ITU-R BT.601 (was CCIR 601) */
    	case icSigYCbCrData:
			cvals[0] = "ITU Y";
			cvals[1] = "ITU Cb";
			cvals[2] = "ITU Cr";
			return 2;

    	case icSigYxyData:
			cvals[0] = "CIE Y";
			cvals[1] = "CIE x";
			cvals[2] = "CIE y";
			return 2;

		/* Alvy Ray Smith ? */
    	case icSigHsvData:
			cvals[0] = "RGB Hue";
			cvals[1] = "RGB Saturation";
			cvals[2] = "RGB Value";
			return 2;

		/* GSPC ? */
    	case icSigHlsData:
			cvals[0] = "RGB Hue";
			cvals[1] = "RGB Lightness";
			cvals[2] = "RGB Saturation";
			return 2;

		case icSigCmyData:
			cvals[0] = "Cyan";
			cvals[1] = "Magenta";
			cvals[2] = "Yellow";
			return 1;

		case icSigRgbData:
			cvals[0] = "Red";
			cvals[1] = "Green";
			cvals[2] = "Blue";
			return 1;

		case icSigCmykData:
			cvals[0] = "Cyan";
			cvals[1] = "Magenta";
			cvals[2] = "Yellow";
			cvals[3] = "Black";
			return 1;

		
		/* Non-standard and Pseudo spaces */
    	case icmSigYData:
			cvals[0] = "CIE Y";
			return 2;

    	case icmSigLData:
			cvals[0] = "CIE L*";
			return 2;

		default:
			break;

	}
	return 0;
}

/* Public version of above */

/* Return the individual channel names and number of channels give a colorspace signature. */
/* Return 0 if it is not a colorspace that itself defines particular channels, */ 
/* 1 if it is a colorant based colorspace, and 2 if it is not a colorant based space */
ICCLIB_API unsigned int icmCSSig2chanNames(icColorSpaceSignature sig, char *cvals[]) {

	return chnames_ColorSpaceSignature(sig, cvals);
}

/* ------------------------------------------------------- */
/* Flag dump functions */
/* Note - returned buffers are static, can only be used 5 */
/* times before buffers get reused. */

/* Screening Encodings */
static char *string_ScreenEncodings(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icPrtrDefaultScreensTrue) {
		sprintf(cp,"Default Screen");
	} else {
		sprintf(cp,"No Default Screen");
	}
	cp = cp + strlen(cp);
	if (flags & icLinesPerInch) {
		sprintf(cp,", Lines Per Inch");
	} else {
		sprintf(cp,", Lines Per cm");
	}
	cp = cp + strlen(cp);

	return bp;
}

/* Device attributes */
static char *string_DeviceAttributes(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icTransparency) {
		sprintf(cp,"Transparency");
	} else {
		sprintf(cp,"Reflective");
	}
	cp = cp + strlen(cp);
	if (flags & icMatte) {
		sprintf(cp,", Matte");
	} else {
		sprintf(cp,", Glossy");
	}
	cp = cp + strlen(cp);
	if (flags & icNegative) {
		sprintf(cp,", Negative");
	} else {
		sprintf(cp,", Positive");
	}
	cp = cp + strlen(cp);
	if (flags & icBlackAndWhite) {
		sprintf(cp,", BlackAndWhite");
	} else {
		sprintf(cp,", Color");
	}
	cp = cp + strlen(cp);

	return bp;
}

/* Profile header flags */
static char *string_ProfileHeaderFlags(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icEmbeddedProfileTrue) {
		sprintf(cp,"Embedded Profile");
	} else {
		sprintf(cp,"Not Embedded Profile");
	}
	cp = cp + strlen(cp);
	if (flags & icUseWithEmbeddedDataOnly) {
		sprintf(cp,", Use with embedded data only");
	} else {
		sprintf(cp,", Use anywhere");
	}
	cp = cp + strlen(cp);

	return bp;
}


static char *string_AsciiOrBinaryData(unsigned int flags) {
	static int si = 0;			/* String buffer index */
	static char buf[5][80];		/* String buffers */
	char *bp, *cp;

	cp = bp = buf[si++];
	si %= 5;				/* Rotate through buffers */

	if (flags & icBinaryData) {
		sprintf(cp,"Binary");
	} else {
		sprintf(cp,"Ascii");
	}
	cp = cp + strlen(cp);

	return bp;
}

/* ------------------------------------------------------------ */
/* Enumeration dump functions */
/* Note - returned buffers are static, can only be used once */
/* before buffers get reused if type is unknown. */

/* public tags and sizes */
static const char *string_TagSignature(icTagSignature sig) {
	static char buf[80];
	switch(sig) {
		case icSigAToB0Tag:
			return "AToB0 Multidimentional Transform";
		case icSigAToB1Tag:
			return "AToB1 Multidimentional Transform";
		case icSigAToB2Tag:
			return "AToB2 Multidimentional Transform";
		case icSigBlueColorantTag:
			return "Blue Colorant";
		case icSigBlueTRCTag:
			return "Blue Tone Reproduction Curve";
		case icSigBToA0Tag:
			return "BToA0 Multidimentional Transform";
		case icSigBToA1Tag:
			return "BToA1 Multidimentional Transform";
		case icSigBToA2Tag:
			return "BToA2 Multidimentional Transform";
		case icSigCalibrationDateTimeTag:
			return "Calibration Date & Time";
		case icSigChromaticAdaptationTag:
			return "Chromatic Adaptation";
		case icSigCharTargetTag:
			return "Characterization Target";
		case icSigCopyrightTag:
			return "Copyright";
		case icSigCrdInfoTag:
			return "CRD Info";
		case icSigDeviceMfgDescTag:
			return "Device Manufacturer Description";
		case icSigDeviceModelDescTag:
			return "Device Model Description";
		case icSigGamutTag:
			return "Gamut";
		case icSigGrayTRCTag:
			return "Gray Tone Reproduction Curve";
		case icSigGreenColorantTag:
			return "Green Colorant";
		case icSigGreenTRCTag:
			return "Green Tone Reproduction Curve";
		case icSigLuminanceTag:
			return "Luminance";
		case icSigMeasurementTag:
			return "Measurement";
		case icSigMediaBlackPointTag:
			return "Media Black Point";
		case icSigMediaWhitePointTag:
			return "Media White Point";
		case icSigNamedColorTag:
			return "Named Color";
		case icSigNamedColor2Tag:
			return "Named Color 2";
		case icSigPreview0Tag:
			return "Preview0";
		case icSigPreview1Tag:
			return "Preview1";
		case icSigPreview2Tag:
			return "Preview2";
		case icSigProfileDescriptionTag:
			return "Profile Description";
		case icSigProfileSequenceDescTag:
			return "Profile Sequence";
		case icSigPs2CRD0Tag:
			return "PS Level 2 CRD perceptual";
		case icSigPs2CRD1Tag:
			return "PS Level 2 CRD colorimetric";
		case icSigPs2CRD2Tag:
			return "PS Level 2 CRD saturation";
		case icSigPs2CRD3Tag:
			return "PS Level 2 CRD absolute";
		case icSigPs2CSATag:
			return "PS Level 2 color space array";
		case icSigPs2RenderingIntentTag:
			return "PS Level 2 Rendering Intent";
		case icSigRedColorantTag:
			return "Red Colorant";
		case icSigRedTRCTag:
			return "Red Tone Reproduction Curve";
		case icSigScreeningDescTag:
			return "Screening Description";
		case icSigScreeningTag:
			return "Screening Attributes";
		case icSigTechnologyTag:
			return "Device Technology";
		case icSigUcrBgTag:
			return "Under Color Removal & Black Generation";
		case icSigVideoCardGammaTag:
			return "Video Card Gamma Curve";
		case icSigViewingCondDescTag:
			return "Viewing Condition Description";
		case icSigViewingConditionsTag:
			return "Viewing Condition Paramaters";

		/* ArgyllCMS private tag: */
		case icmSigAbsToRelTransSpace:
			return "Absolute to Media Relative Transformation Space matrix";

		default:
			sprintf(buf,"Unrecognized - %s",tag2str(sig));
			return buf;
	}
}

/* technology signature descriptions */
static const char *string_TechnologySignature(icTechnologySignature sig) {
	static char buf[80];
	switch(sig) {
		case icSigDigitalCamera:
			return "Digital Camera";
		case icSigFilmScanner:
			return "Film Scanner";
		case icSigReflectiveScanner:
			return "Reflective Scanner";
		case icSigInkJetPrinter:
			return "InkJet Printer";
		case icSigThermalWaxPrinter:
			return "Thermal WaxPrinter";
		case icSigElectrophotographicPrinter:
			return "Electrophotographic Printer";
		case icSigElectrostaticPrinter:
			return "Electrostatic Printer";
		case icSigDyeSublimationPrinter:
			return "DyeSublimation Printer";
		case icSigPhotographicPaperPrinter:
			return "Photographic Paper Printer";
		case icSigFilmWriter:
			return "Film Writer";
		case icSigVideoMonitor:
			return "Video Monitor";
		case icSigVideoCamera:
			return "Video Camera";
		case icSigProjectionTelevision:
			return "Projection Television";
		case icSigCRTDisplay:
			return "Cathode Ray Tube Display";
		case icSigPMDisplay:
			return "Passive Matrix Display";
		case icSigAMDisplay:
			return "Active Matrix Display";
		case icSigPhotoCD:
			return "Photo CD";
		case icSigPhotoImageSetter:
			return "Photo ImageSetter";
		case icSigGravure:
			return "Gravure";
		case icSigOffsetLithography:
			return "Offset Lithography";
		case icSigSilkscreen:
			return "Silkscreen";
		case icSigFlexography:
			return "Flexography";
		default:
			sprintf(buf,"Unrecognized - %s",tag2str(sig));
			return buf;
	}
}

/* type signatures */
static const char *string_TypeSignature(icTagTypeSignature sig) {
	static char buf[80];
	switch(sig) {
		case icSigCurveType:
			return "Curve";
		case icSigDataType:
			return "Data";
		case icSigDateTimeType:
			return "DateTime";
		case icSigLut16Type:
			return "Lut16";
		case icSigLut8Type:
			return "Lut8";
		case icSigMeasurementType:
			return "Measurement";
		case icSigNamedColorType:
			return "Named Color";
		case icSigProfileSequenceDescType:
			return "Profile Sequence Desc";
		case icSigS15Fixed16ArrayType:
			return "S15Fixed16 Array";
		case icSigScreeningType:
			return "Screening";
		case icSigSignatureType:
			return "Signature";
		case icSigTextType:
			return "Text";
		case icSigTextDescriptionType:
			return "Text Description";
		case icSigU16Fixed16ArrayType:
			return "U16Fixed16 Array";
		case icSigUcrBgType:
			return "Under Color Removal & Black Generation";
		case icSigUInt16ArrayType:
			return "UInt16 Array";
		case icSigUInt32ArrayType:
			return "UInt32 Array";
		case icSigUInt64ArrayType:
			return "UInt64 Array";
		case icSigUInt8ArrayType:
			return "UInt8 Array";
		case icSigVideoCardGammaType:
			return "Video Card Gamma";
		case icSigViewingConditionsType:
			return "Viewing Conditions";
		case icSigXYZType:
			return "XYZ (Array?)";
		case icSigNamedColor2Type:
			return "Named Color 2";
		case icSigCrdInfoType:
			return "CRD Info";
		default:
			sprintf(buf,"Unrecognized - %s",tag2str(sig));
			return buf;
	}
}

/* Color Space Signatures */
static const char *string_ColorSpaceSignature(icColorSpaceSignature sig) {
	static char buf[80];
	switch(sig) {
		case icSigXYZData:
			return "XYZ";
		case icSigLabData:
			return "Lab";
		case icSigLuvData:
			return "Luv";
		case icSigYCbCrData:
			return "YCbCr";
		case icSigYxyData:
			return "Yxy";
		case icSigRgbData:
			return "RGB";
		case icSigGrayData:
			return "Gray";
		case icSigHsvData:
			return "HSV";
		case icSigHlsData:
			return "HLS";
		case icSigCmykData:
			return "CMYK";
		case icSigCmyData:
			return "CMY";
		case icSig2colorData:
			return "2 Color";
		case icSig3colorData:
			return "3 Color";
		case icSig4colorData:
			return "4 Color";
		case icSig5colorData:
		case icSigMch5Data:
			return "5 Color";
		case icSig6colorData:
		case icSigMch6Data:
			return "6 Color";
		case icSig7colorData:
		case icSigMch7Data:
			return "7 Color";
		case icSig8colorData:
		case icSigMch8Data:
			return "8 Color";
		case icSig9colorData:
			return "9 Color";
		case icSig10colorData:
			return "10 Color";
		case icSig11colorData:
			return "11 Color";
		case icSig12colorData:
			return "12 Color";
		case icSig13colorData:
			return "13 Color";
		case icSig14colorData:
			return "14 Color";
		case icSig15colorData:
			return "15 Color";

		/* Non-standard and Pseudo spaces */
		case icmSigYData:
			return "Y";
		case icmSigLData:
			return "L";
		case icmSigL8Data:
			return "L";
		case icmSigLV2Data:
			return "L";
		case icmSigLV4Data:
			return "L";
		case icmSigPCSData:
			return "PCS";
		case icmSigLab8Data:
			return "Lab";
		case icmSigLabV2Data:
			return "Lab";
		case icmSigLabV4Data:
			return "Lab";

		default:
			sprintf(buf,"Unrecognized - %s",tag2str(sig));
			return buf;
	}
}

#ifdef NEVER
/* Public version of above */
char *ColorSpaceSignature2str(icColorSpaceSignature sig) {
	return string_ColorSpaceSignature(sig);
}
#endif


/* profileClass enumerations */
static const char *string_ProfileClassSignature(icProfileClassSignature sig) {
	static char buf[80];
	switch(sig) {
		case icSigInputClass:
			return "Input";
		case icSigDisplayClass:
			return "Display";
		case icSigOutputClass:
			return "Output";
		case icSigLinkClass:
			return "Link";
		case icSigAbstractClass:
			return "Abstract";
		case icSigColorSpaceClass:
			return "Color Space";
		case icSigNamedColorClass:
			return "Named Color";
		default:
			sprintf(buf,"Unrecognized - %s",tag2str(sig));
			return buf;
	}
}

/* Platform Signatures */
static const char *string_PlatformSignature(icPlatformSignature sig) {
	static char buf[80];
	switch(sig) {
		case icSigMacintosh:
			return "Macintosh";
		case icSigMicrosoft:
			return "Microsoft";
		case icSigSolaris:
			return "Solaris";
		case icSigSGI:
			return "SGI";
		case icSigTaligent:
			return "Taligent";
		case icmSig_nix:
			return "*nix";
		default:
			sprintf(buf,"Unrecognized - %s",tag2str(sig));
			return buf;
	}
}

/* Measurement Geometry, used in the measurmentType tag */
static const char *string_MeasurementGeometry(icMeasurementGeometry sig) {
	static char buf[30];
	switch(sig) {
		case icGeometryUnknown:
			return "Unknown";
		case icGeometry045or450:
			return "0/45 or 45/0";
		case icGeometry0dord0:
			return "0/d or d/0";
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}

/* Rendering Intents, used in the profile header */
static const char *string_RenderingIntent(icRenderingIntent sig) {
	static char buf[30];
	switch(sig) {
		case icPerceptual:
			return "Perceptual";
		case icRelativeColorimetric:
    		return "Relative Colorimetric";
		case icSaturation:
    		return "Saturation";
		case icAbsoluteColorimetric:
    		return "Absolute Colorimetric";
		case icmAbsolutePerceptual:				/* icclib specials */
			return "Absolute Perceptual";
		case icmAbsoluteSaturation:				/* icclib specials */
			return "Absolute Saturation";
		case icmDefaultIntent:					/* icclib specials */
    		return "Default Intent";
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}

/* Transform Lookup function */
static const char *string_LookupFunc(icmLookupFunc sig) {
	static char buf[30];
	switch(sig) {
		case icmFwd:
			return "Forward";
		case icmBwd:
    		return "Backward";
		case icmGamut:
    		return "Gamut";
		case icmPreview:
    		return "Preview";
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}


/* Different Spot Shapes currently defined, used for screeningType */
static const char *string_SpotShape(icSpotShape sig) {
	static char buf[30];
	switch(sig) {
		case icSpotShapeUnknown:
			return "Unknown";
		case icSpotShapePrinterDefault:
			return "Printer Default";
		case icSpotShapeRound:
			return "Round";
		case icSpotShapeDiamond:
			return "Diamond";
		case icSpotShapeEllipse:
			return "Ellipse";
		case icSpotShapeLine:
			return "Line";
		case icSpotShapeSquare:
			return "Square";
		case icSpotShapeCross:
			return "Cross";
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}

/* Standard Observer, used in the measurmentType tag */
static const char *string_StandardObserver(icStandardObserver sig) {
	static char buf[30];
	switch(sig) {
		case icStdObsUnknown:
			return "Unknown";
		case icStdObs1931TwoDegrees:
			return "1931 Two Degrees";
		case icStdObs1964TenDegrees:
			return "1964 Ten Degrees";
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}

/* Pre-defined illuminants, used in measurement and viewing conditions type */
static const char *string_Illuminant(icIlluminant sig) {
	static char buf[30];
	switch(sig) {
		case icIlluminantUnknown:
			return "Unknown";
		case icIlluminantD50:
			return "D50";
		case icIlluminantD65:
			return "D65";
		case icIlluminantD93:
			return "D93";
		case icIlluminantF2:
			return "F2";
		case icIlluminantD55:
			return "D55";
		case icIlluminantA:
			return "A";
		case icIlluminantEquiPowerE:
			return "Equi-Power(E)";
		case icIlluminantF8:
			return "F8";
		default:
			sprintf(buf,"Unrecognized - 0x%x",sig);
			return buf;
	}
}

/* Return a text abreviation of a color lookup algorithm */
static const char *string_LuAlg(icmLuAlgType alg) {
	static char buf[80];

	switch(alg) {
    	case icmMonoFwdType:
			return "MonoFwd";
    	case icmMonoBwdType:
			return "MonoBwd";
    	case icmMatrixFwdType:
			return "MatrixFwd";
    	case icmMatrixBwdType:
			return "MatrixBwd";
    	case icmLutType:
			return "Lut";
	default:
		sprintf(buf,"Unrecognized - %d",alg);
		return buf;
	}
}

/* Return a string description of the given enumeration value */
/* Public: */
const char *icm2str(icmEnumType etype, int enumval) {

	switch(etype) {
	    case icmScreenEncodings:
			return string_ScreenEncodings((unsigned int) enumval);
	    case icmDeviceAttributes:
			return string_DeviceAttributes((unsigned int) enumval);
		case icmProfileHeaderFlags:
			return string_ProfileHeaderFlags((unsigned int) enumval);
		case icmAsciiOrBinaryData:
			return string_AsciiOrBinaryData((unsigned int) enumval);
		case icmTagSignature:
			return string_TagSignature((icTagSignature) enumval);
		case icmTechnologySignature:
			return string_TechnologySignature((icTechnologySignature) enumval);
		case icmTypeSignature:
			return string_TypeSignature((icTagTypeSignature) enumval);
		case icmColorSpaceSignature:
			return string_ColorSpaceSignature((icColorSpaceSignature) enumval);
		case icmProfileClassSignature:
			return string_ProfileClassSignature((icProfileClassSignature) enumval);
		case icmPlatformSignature:
			return string_PlatformSignature((icPlatformSignature) enumval);
		case icmMeasurementGeometry:
			return string_MeasurementGeometry((icMeasurementGeometry) enumval);
		case icmRenderingIntent:
			return string_RenderingIntent((icRenderingIntent) enumval);
		case icmTransformLookupFunc:
			return string_LookupFunc((icmLookupFunc) enumval);
		case icmSpotShape:
			return string_SpotShape((icSpotShape) enumval);
		case icmStandardObserver:
			return string_StandardObserver((icStandardObserver) enumval);
		case icmIlluminant:
			return string_Illuminant((icIlluminant) enumval);
		case icmLuAlg:
			return string_LuAlg((icmLuAlgType) enumval);
		default:
			return "enum2str got unknown type";
	}
}

/* ========================================================== */
/* Object I/O routines                                        */
/* ========================================================== */
/* icmUnknown object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUnknown_get_size(
	icmBase *pp
) {
	icmUnknown *p = (icmUnknown *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 1);	/* 1 byte for each unknown data */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUnknown_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmUnknown *p = (icmUnknown *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmUnknown_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUnknown_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmUnknown_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 8)/1;		/* Number of elements in the array */

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	p->uttype = (icTagTypeSignature)read_SInt32Number(bp);
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 1) {
		p->data[i] = read_UInt8Number(bp);
	}
	icp->al->free(p->icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUnknown_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmUnknown *p = (icmUnknown *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmUnknown_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUnknown_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->uttype,bp)) != 0) {
		sprintf(icp->err,"icmUnknown_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp += 8;	/* Skip padding */

	/* Write all the data to the buffer */
	for (i = 0; i < p->size; i++, bp += 1) {
		if ((rv = write_UInt8Number(p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmUnknown_write: write_UInt8umber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmUnknown_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUnknown_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmUnknown *p = (icmUnknown *)pp;
	unsigned int i, ii, r, ph;

	if (verb <= 1)
		return;

	op->gprintf(op,"Unknown:\n");
	op->gprintf(op,"  Payload size in bytes = %u\n",p->size);

	/* Print one row of binary and ASCII interpretation if verb == 2, All if == 3 */
	/* else print all of it. */
	ii = i = ph = 0;
	for (r = 1;; r++) {		/* count rows */
		int c = 1;			/* Character location */

		c = 1;
		if (ph != 0) {	/* Print ASCII under binary */
			op->gprintf(op,"            ");
			i = ii;				/* Swap */
			c += 12;
		} else {
			op->gprintf(op,"    0x%04lx: ",i);
			ii = i;				/* Swap */
			c += 12;
		}
		while (i < p->size && c < 60) {
			if (ph == 0) 
				op->gprintf(op,"%02x ",p->data[i]);
			else {
				if (isprint(p->data[i]))
					op->gprintf(op,"%c  ",p->data[i]);
				else
					op->gprintf(op,"   ",p->data[i]);
			}
			c += 3;
			i++;
		}
		if (ph == 0 || i < p->size)
			op->gprintf(op,"\n");

		if (ph == 1 && i >= p->size) {
			op->gprintf(op,"\n");
			break;
		}
		if (ph == 1 && r > 1 && verb < 3) {
			op->gprintf(op,"    ...\n");
			break;			/* Print 1 row if not verbose */
		}

		if (ph == 0)
			ph = 1;
		else
			ph = 0;

	}
}

/* Allocate variable sized data elements */
static int icmUnknown_allocate(
	icmBase *pp
) {
	icmUnknown *p = (icmUnknown *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(unsigned char))) {
			sprintf(icp->err,"icmUnknown_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned char *) icp->al->calloc(icp->al, p->size, sizeof(unsigned char)))
		                                                                                == NULL) {
			sprintf(icp->err,"icmUnknown_alloc: malloc() of icmUnknown data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUnknown_delete(
	icmBase *pp
) {
	icmUnknown *p = (icmUnknown *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUnknown(
	icc *icp
) {
	icmUnknown *p;
	if ((p = (icmUnknown *) icp->al->calloc(icp->al,1,sizeof(icmUnknown))) == NULL)
		return NULL;
	p->ttype    = icmSigUnknownType;
	p->uttype   = icmSigUnknownType;
	p->refcount = 1;
	p->get_size = icmUnknown_get_size;
	p->read     = icmUnknown_read;
	p->write    = icmUnknown_write;
	p->dump     = icmUnknown_dump;
	p->allocate = icmUnknown_allocate;
	p->del      = icmUnknown_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt8Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUInt8Array_get_size(
	icmBase *pp
) {
	icmUInt8Array *p = (icmUInt8Array *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 1);	/* 1 byte for each UInt8 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUInt8Array_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmUInt8Array *p = (icmUInt8Array *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmUInt8Array_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUInt8Array_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmUInt8Array_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 8)/1;		/* Number of elements in the array */

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		icp->al->free(icp->al, buf);
		sprintf(icp->err,"icmUInt8Array_read: Wrong tag type for icmUInt8Array");
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 1) {
		p->data[i] = read_UInt8Number(bp);
	}
	icp->al->free(p->icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUInt8Array_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmUInt8Array *p = (icmUInt8Array *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmUInt8Array_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUInt8Array_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmUInt8Array_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp += 8;	/* Skip padding */

	/* Write all the data to the buffer */
	for (i = 0; i < p->size; i++, bp += 1) {
		if ((rv = write_UInt8Number(p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmUInt8Array_write: write_UInt8umber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmUInt8Array_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUInt8Array_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmUInt8Array *p = (icmUInt8Array *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"UInt8Array:\n");
	op->gprintf(op,"  No. elements = %lu\n",p->size);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->size; i++)
			op->gprintf(op,"    %lu:  %u\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmUInt8Array_allocate(
	icmBase *pp
) {
	icmUInt8Array *p = (icmUInt8Array *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(unsigned int))) {
			sprintf(icp->err,"icmUInt8Array_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned int *) icp->al->calloc(icp->al, p->size, sizeof(unsigned int)))
		                                                                              == NULL) {
			sprintf(icp->err,"icmUInt8Array_alloc: malloc() of icmUInt8Array data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUInt8Array_delete(
	icmBase *pp
) {
	icmUInt8Array *p = (icmUInt8Array *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt8Array(
	icc *icp
) {
	icmUInt8Array *p;
	if ((p = (icmUInt8Array *) icp->al->calloc(icp->al,1,sizeof(icmUInt8Array))) == NULL)
		return NULL;
	p->ttype    = icSigUInt8ArrayType;
	p->refcount = 1;
	p->get_size = icmUInt8Array_get_size;
	p->read     = icmUInt8Array_read;
	p->write    = icmUInt8Array_write;
	p->dump     = icmUInt8Array_dump;
	p->allocate = icmUInt8Array_allocate;
	p->del      = icmUInt8Array_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt16Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUInt16Array_get_size(
	icmBase *pp
) {
	icmUInt16Array *p = (icmUInt16Array *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 2);	/* 2 bytes for each UInt16 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUInt16Array_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmUInt16Array *p = (icmUInt16Array *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmUInt16Array_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUInt16Array_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmUInt16Array_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 8)/2;		/* Number of elements in the array */

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmUInt16Array_read: Wrong tag type for icmUInt16Array");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 2) {
		p->data[i] = read_UInt16Number(bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUInt16Array_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmUInt16Array *p = (icmUInt16Array *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmUInt16Array_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUInt16Array_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmUInt16Array_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->size; i++, bp += 2) {
		if ((rv = write_UInt16Number(p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmUInt16Array_write: write_UInt16umber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmUInt16Array_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUInt16Array_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmUInt16Array *p = (icmUInt16Array *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"UInt16Array:\n");
	op->gprintf(op,"  No. elements = %lu\n",p->size);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->size; i++)
			op->gprintf(op,"    %lu:  %u\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmUInt16Array_allocate(
	icmBase *pp
) {
	icmUInt16Array *p = (icmUInt16Array *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(unsigned int))) {
			sprintf(icp->err,"icmUInt16Array_alloc:: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned int *) icp->al->calloc(icp->al, p->size, sizeof(unsigned int)))
		                                                                              == NULL) {
			sprintf(icp->err,"icmUInt16Array_alloc: malloc() of icmUInt16Array data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUInt16Array_delete(
	icmBase *pp
) {
	icmUInt16Array *p = (icmUInt16Array *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt16Array(
	icc *icp
) {
	icmUInt16Array *p;
	if ((p = (icmUInt16Array *) icp->al->calloc(icp->al,1,sizeof(icmUInt16Array))) == NULL)
		return NULL;
	p->ttype    = icSigUInt16ArrayType;
	p->refcount = 1;
	p->get_size = icmUInt16Array_get_size;
	p->read     = icmUInt16Array_read;
	p->write    = icmUInt16Array_write;
	p->dump     = icmUInt16Array_dump;
	p->allocate = icmUInt16Array_allocate;
	p->del      = icmUInt16Array_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt32Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUInt32Array_get_size(
	icmBase *pp
) {
	icmUInt32Array *p = (icmUInt32Array *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 4);	/* 4 bytes for each UInt32 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUInt32Array_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmUInt32Array *p = (icmUInt32Array *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmUInt32Array_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUInt32Array_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmUInt32Array_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 8)/4;		/* Number of elements in the array */

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmUInt32Array_read: Wrong tag type for icmUInt32Array");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 4) {
		p->data[i] = read_UInt32Number(bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUInt32Array_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmUInt32Array *p = (icmUInt32Array *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmUInt32Array_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUInt32Array_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmUInt32Array_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->size; i++, bp += 4) {
		if ((rv = write_UInt32Number(p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmUInt32Array_write: write_UInt32umber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmUInt32Array_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUInt32Array_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmUInt32Array *p = (icmUInt32Array *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"UInt32Array:\n");
	op->gprintf(op,"  No. elements = %lu\n",p->size);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->size; i++)
			op->gprintf(op,"    %lu:  %u\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmUInt32Array_allocate(
	icmBase *pp
) {
	icmUInt32Array *p = (icmUInt32Array *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(unsigned int))) {
			sprintf(icp->err,"icmUInt32Array_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned int *) icp->al->calloc(icp->al, p->size, sizeof(unsigned int)))
		                                                                              == NULL) {
			sprintf(icp->err,"icmUInt32Array_alloc: malloc() of icmUInt32Array data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUInt32Array_delete(
	icmBase *pp
) {
	icmUInt32Array *p = (icmUInt32Array *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt32Array(
	icc *icp
) {
	icmUInt32Array *p;
	if ((p = (icmUInt32Array *) icp->al->calloc(icp->al,1,sizeof(icmUInt32Array))) == NULL)
		return NULL;
	p->ttype    = icSigUInt32ArrayType;
	p->refcount = 1;
	p->get_size = icmUInt32Array_get_size;
	p->read     = icmUInt32Array_read;
	p->write    = icmUInt32Array_write;
	p->dump     = icmUInt32Array_dump;
	p->allocate = icmUInt32Array_allocate;
	p->del      = icmUInt32Array_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUInt64Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUInt64Array_get_size(
	icmBase *pp
) {
	icmUInt64Array *p = (icmUInt64Array *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 8);	/* 8 bytes for each UInt64 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUInt64Array_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmUInt64Array *p = (icmUInt64Array *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmUInt64Array_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUInt64Array_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmUInt64Array_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 8)/8;		/* Number of elements in the array */

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmUInt64Array_read: Wrong tag type for icmUInt64Array");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 8) {
		read_UInt64Number(&p->data[i], bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUInt64Array_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmUInt64Array *p = (icmUInt64Array *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmUInt64Array_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUInt64Array_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmUInt64Array_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->size; i++, bp += 8) {
		if ((rv = write_UInt64Number(&p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmUInt64Array_write: write_UInt64umber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmUInt64Array_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUInt64Array_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmUInt64Array *p = (icmUInt64Array *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"UInt64Array:\n");
	op->gprintf(op,"  No. elements = %lu\n",p->size);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->size; i++)
			op->gprintf(op,"    %lu:  h=%lu, l=%lu\n",i,p->data[i].h,p->data[i].l);
	}
}

/* Allocate variable sized data elements */
static int icmUInt64Array_allocate(
	icmBase *pp
) {
	icmUInt64Array *p = (icmUInt64Array *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(icmUint64))) {
			sprintf(icp->err,"icmUInt64Array_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmUint64 *) icp->al->calloc(icp->al, p->size, sizeof(icmUint64)))
		                                                                        == NULL) {
			sprintf(icp->err,"icmUInt64Array_alloc: malloc() of icmUInt64Array data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUInt64Array_delete(
	icmBase *pp
) {
	icmUInt64Array *p = (icmUInt64Array *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUInt64Array(
	icc *icp
) {
	icmUInt64Array *p;
	if ((p = (icmUInt64Array *) icp->al->calloc(icp->al,1,sizeof(icmUInt64Array))) == NULL)
		return NULL;
	p->ttype    = icSigUInt64ArrayType;
	p->refcount = 1;
	p->get_size = icmUInt64Array_get_size;
	p->read     = icmUInt64Array_read;
	p->write    = icmUInt64Array_write;
	p->dump     = icmUInt64Array_dump;
	p->allocate = icmUInt64Array_allocate;
	p->del      = icmUInt64Array_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmU16Fixed16Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmU16Fixed16Array_get_size(
	icmBase *pp
) {
	icmU16Fixed16Array *p = (icmU16Fixed16Array *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 4);	/* 4 byte for each U16Fixed16 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmU16Fixed16Array_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmU16Fixed16Array *p = (icmU16Fixed16Array *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmU16Fixed16Array_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmU16Fixed16Array_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmU16Fixed16Array_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 8)/4;		/* Number of elements in the array */

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmU16Fixed16Array_read: Wrong tag type for icmU16Fixed16Array");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 4) {
		p->data[i] = read_U16Fixed16Number(bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmU16Fixed16Array_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmU16Fixed16Array *p = (icmU16Fixed16Array *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmU16Fixed16Array_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmU16Fixed16Array_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmU16Fixed16Array_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->size; i++, bp += 4) {
		if ((rv = write_U16Fixed16Number(p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmU16Fixed16Array_write: write_U16Fixed16umber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmU16Fixed16Array_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmU16Fixed16Array_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmU16Fixed16Array *p = (icmU16Fixed16Array *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"U16Fixed16Array:\n");
	op->gprintf(op,"  No. elements = %lu\n",p->size);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->size; i++)
			op->gprintf(op,"    %lu:  %.8f\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmU16Fixed16Array_allocate(
	icmBase *pp
) {
	icmU16Fixed16Array *p = (icmU16Fixed16Array *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(double))) {
			sprintf(icp->err,"icmU16Fixed16Array_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (double *) icp->al->calloc(icp->al, p->size, sizeof(double))) == NULL) {
			sprintf(icp->err,"icmU16Fixed16Array_alloc: malloc() of icmU16Fixed16Array data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmU16Fixed16Array_delete(
	icmBase *pp
) {
	icmU16Fixed16Array *p = (icmU16Fixed16Array *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmU16Fixed16Array(
	icc *icp
) {
	icmU16Fixed16Array *p;
	if ((p = (icmU16Fixed16Array *) icp->al->calloc(icp->al,1,sizeof(icmU16Fixed16Array))) == NULL)
		return NULL;
	p->ttype    = icSigU16Fixed16ArrayType;
	p->refcount = 1;
	p->get_size = icmU16Fixed16Array_get_size;
	p->read     = icmU16Fixed16Array_read;
	p->write    = icmU16Fixed16Array_write;
	p->dump     = icmU16Fixed16Array_dump;
	p->allocate = icmU16Fixed16Array_allocate;
	p->del      = icmU16Fixed16Array_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmS15Fixed16Array object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmS15Fixed16Array_get_size(
	icmBase *pp
) {
	icmS15Fixed16Array *p = (icmS15Fixed16Array *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 4);	/* 4 byte for each S15Fixed16 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmS15Fixed16Array_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmS15Fixed16Array *p = (icmS15Fixed16Array *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmS15Fixed16Array_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmS15Fixed16Array_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmS15Fixed16Array_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 8)/4;		/* Number of elements in the array */

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmS15Fixed16Array_read: Wrong tag type for icmS15Fixed16Array");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 4) {
		p->data[i] = read_S15Fixed16Number(bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmS15Fixed16Array_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmS15Fixed16Array *p = (icmS15Fixed16Array *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmS15Fixed16Array_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmS15Fixed16Array_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmS15Fixed16Array_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->size; i++, bp += 4) {
		if ((rv = write_S15Fixed16Number(p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmS15Fixed16Array_write: write_S15Fixed16umber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmS15Fixed16Array_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmS15Fixed16Array_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmS15Fixed16Array *p = (icmS15Fixed16Array *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"S15Fixed16Array:\n");
	op->gprintf(op,"  No. elements = %lu\n",p->size);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->size; i++)
			op->gprintf(op,"    %lu:  %.8f\n",i,p->data[i]);
	}
}

/* Allocate variable sized data elements */
static int icmS15Fixed16Array_allocate(
	icmBase *pp
) {
	icmS15Fixed16Array *p = (icmS15Fixed16Array *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(double))) {
			sprintf(icp->err,"icmS15Fixed16Array_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (double *) icp->al->calloc(icp->al, p->size, sizeof(double))) == NULL) {
			sprintf(icp->err,"icmS15Fixed16Array_alloc: malloc() of icmS15Fixed16Array data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmS15Fixed16Array_delete(
	icmBase *pp
) {
	icmS15Fixed16Array *p = (icmS15Fixed16Array *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmS15Fixed16Array(
	icc *icp
) {
	icmS15Fixed16Array *p;
	if ((p = (icmS15Fixed16Array *) icp->al->calloc(icp->al,1,sizeof(icmS15Fixed16Array))) == NULL)
		return NULL;
	p->ttype    = icSigS15Fixed16ArrayType;
	p->refcount = 1;
	p->get_size = icmS15Fixed16Array_get_size;
	p->read     = icmS15Fixed16Array_read;
	p->write    = icmS15Fixed16Array_write;
	p->dump     = icmS15Fixed16Array_dump;
	p->allocate = icmS15Fixed16Array_allocate;
	p->del      = icmS15Fixed16Array_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Data conversion support functions */
static int write_XYZNumber(icmXYZNumber *p, char *d) {
	int rv;
	if ((rv = write_S15Fixed16Number(p->X, d + 0)) != 0)
		return rv;
	if ((rv = write_S15Fixed16Number(p->Y, d + 4)) != 0)
		return rv;
	if ((rv = write_S15Fixed16Number(p->Z, d + 8)) != 0)
		return rv;
	return 0;
}

static int read_XYZNumber(icmXYZNumber *p, char *d) {
	p->X = read_S15Fixed16Number(d + 0);
	p->Y = read_S15Fixed16Number(d + 4);
	p->Z = read_S15Fixed16Number(d + 8);
	return 0;
}


/* Helper: Return a string that shows the XYZ number value */
static char *string_XYZNumber(icmXYZNumber *p) {
	static char buf[40];

	sprintf(buf,"%.8f, %.8f, %.8f", p->X, p->Y, p->Z);
	return buf;
}

/* Helper: Return a string that shows the XYZ number value, */
/* and the Lab D50 number in paren. Note the buffer will be re-used on every call. */
static char *string_XYZNumber_and_Lab(icmXYZNumber *p) {
	static char buf[100];
	double lab[3];
	lab[0] = p->X;
	lab[1] = p->Y;
	lab[2] = p->Z;
	icmXYZ2Lab(&icmD50, lab, lab);
	snprintf(buf,sizeof(buf),"%.8f, %.8f, %.8f    [Lab %f, %f, %f]", p->X, p->Y, p->Z, lab[0], lab[1], lab[2]);
	return buf;
}
			
/* icmXYZArray object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmXYZArray_get_size(
	icmBase *pp
) {
	icmXYZArray *p = (icmXYZArray *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 12);	/* 12 bytes for each XYZ */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmXYZArray_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmXYZArray *p = (icmXYZArray *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, size;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmXYZArray_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmXYZArray_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmXYZArray_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 8)/12;		/* Number of elements in the array */

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmXYZArray_read: Wrong tag type for icmXYZArray");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	/* Read all the data from the buffer */
	for (i = 0; i < size; i++, bp += 12) {
		read_XYZNumber(&p->data[i], bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmXYZArray_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmXYZArray *p = (icmXYZArray *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmXYZArray_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmXYZArray_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmXYZArray_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	for (i = 0; i < p->size; i++, bp += 12) {
		if ((rv = write_XYZNumber(&p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmXYZArray_write: write_XYZumber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmXYZArray_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmXYZArray_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmXYZArray *p = (icmXYZArray *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"XYZArray:\n");
	op->gprintf(op,"  No. elements = %lu\n",p->size);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->size; i++) {
			op->gprintf(op,"    %lu:  %s\n",i,string_XYZNumber_and_Lab(&p->data[i]));
			
		}
	}
}

/* Allocate variable sized data elements */
static int icmXYZArray_allocate(
	icmBase *pp
) {
	icmXYZArray *p = (icmXYZArray *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(icmXYZNumber))) {
			sprintf(icp->err,"icmXYZArray_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmXYZNumber *) icp->al->malloc(icp->al, sat_mul(p->size, sizeof(icmXYZNumber)))) == NULL) {
			sprintf(icp->err,"icmXYZArray_alloc: malloc() of icmXYZArray data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmXYZArray_delete(
	icmBase *pp
) {
	icmXYZArray *p = (icmXYZArray *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmXYZArray(
	icc *icp
) {
	icmXYZArray *p;
	if ((p = (icmXYZArray *) icp->al->calloc(icp->al,1,sizeof(icmXYZArray))) == NULL)
		return NULL;
	p->ttype    = icSigXYZArrayType;
	p->refcount = 1;
	p->get_size = icmXYZArray_get_size;
	p->read     = icmXYZArray_read;
	p->write    = icmXYZArray_write;
	p->dump     = icmXYZArray_dump;
	p->allocate = icmXYZArray_allocate;
	p->del      = icmXYZArray_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmCurve object */

/* Do a forward lookup through the curve */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmCurve_lookup_fwd(
	icmCurve *p,
	double *out,
	double *in
) {
	int rv = 0;
	if (p->flag == icmCurveLin) {
		*out = *in;
	} else if (p->flag == icmCurveGamma) {
		double val = *in;
		if (val <= 0.0)
			*out = 0.0;
		else
			*out = pow(val, p->data[0]);
	} else if (p->size == 0) { /* Table of 0 size */
		*out = *in;
	} else { /* Use linear interpolation */
		unsigned int ix;
		double val, w;
		double inputEnt_1 = (double)(p->size-1);

		val = *in * inputEnt_1;
		if (val < 0.0) {
			val = 0.0;
			rv |= 1;
		} else if (val > inputEnt_1) {
			val = inputEnt_1;
			rv |= 1;
		}
		ix = (unsigned int)floor(val);		/* Coordinate */
		if (ix > (p->size-2))
			ix = (p->size-2);
		w = val - (double)ix;		/* weight */
		val = p->data[ix];
		*out = val + w * (p->data[ix+1] - val);
	}
	return rv;
}

/* - - - - - - - - - - - - */
/* Support for reverse interpolation of 1D lookup tables */

/* Create a reverse curve lookup acceleration table */
/* return non-zero on error, 2 = malloc error. */
static int icmTable_setup_bwd(
	icc          *icp,			/* Base icc object */
	icmRevTable  *rt,			/* Reverse table data to setup */
	unsigned int size,			/* Size of fwd table */
	double       *data			/* Table */
) {
	unsigned int i;

	rt->size = size;		/* Stash pointers to these away */
	rt->data = data;
	
	/* Find range of output values */
	rt->rmin = 1e300;
	rt->rmax = -1e300;
	for (i = 0; i < rt->size; i++) {
		if (rt->data[i] > rt->rmax)
			rt->rmax = rt->data[i];
		if (rt->data[i] < rt->rmin)
			rt->rmin = rt->data[i];
	}

	/* Decide on reverse granularity */
	rt->rsize = sat_add(rt->size,2)/2;
	rt->qscale = (double)rt->rsize/(rt->rmax - rt->rmin);	/* Scale factor to quantize to */
	
	if (ovr_mul(rt->size, sizeof(unsigned int *))) {
		return 2;
	}
	/* Initialize the reverse lookup structures, and get overall min/max */
	if ((rt->rlists = (unsigned int **) icp->al->calloc(icp->al, rt->rsize, sizeof(unsigned int *))) == NULL) {
		return 2;
	}

	/* Assign each output value range bucket lists it intersects */
	for (i = 0; i < (rt->size-1); i++) {
		unsigned int s, e, j;	/* Start and end indexes (inclusive) */
		s = (unsigned int)((rt->data[i] - rt->rmin) * rt->qscale);
		e = (unsigned int)((rt->data[i+1] - rt->rmin) * rt->qscale);
		if (s >= rt->rsize)
			s = rt->rsize-1;
		if (e >= rt->rsize)
			e = rt->rsize-1;
		if (s > e) {	/* swap */
			unsigned int t;
			t = s; s = e; e = t;
		}

		/* For all buckets that may contain this output range, add index of this output */
		for (j = s; j <= e; j++) {
			unsigned int as;			/* Allocation size */
			unsigned int nf;			/* Next free slot */
			if (rt->rlists[j] == NULL) {	/* No allocation */
				as = 5;						/* Start with space for 5 */
				if ((rt->rlists[j] = (unsigned int *) icp->al->calloc(icp->al, as, sizeof(unsigned int))) == NULL) {
					return 2;
				}
				rt->rlists[j][0] = as;
				nf = rt->rlists[j][1] = 2;
			} else {
				as = rt->rlists[j][0];	/* Allocate space for this list */
				nf = rt->rlists[j][1];	/* Next free location in list */
				if (nf >= as) {			/* need to expand space */
					if ((as = sat_mul(as, 2)) == UINT_MAX
					 || ovr_mul(as, sizeof(unsigned int))) {
						return 2;
					}
					rt->rlists[j] = (unsigned int *) icp->al->realloc(icp->al,rt->rlists[j], as * sizeof(unsigned int));
					if (rt->rlists[j] == NULL) {
						return 2;
					}
					rt->rlists[j][0] = as;
				}
			}
			rt->rlists[j][nf++] = i;
			rt->rlists[j][1] = nf;
		}
	}
	rt->inited = 1;
	return 0;
}

/* Free up any data */
static void icmTable_delete_bwd(
	icc          *icp,			/* Base icc */
	icmRevTable  *rt			/* Reverse table data to setup */
) {
	if (rt->inited != 0) {
		while (rt->rsize > 0)
			icp->al->free(icp->al, rt->rlists[--rt->rsize]);
		icp->al->free(icp->al, rt->rlists);
		rt->size = 0;			/* Don't keep these */
		rt->data = NULL;
	}
}

/* Do a reverse lookup through the curve */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmTable_lookup_bwd(
	icmRevTable *rt,
	double *out,
	double *in
) {
	int rv = 0;
	unsigned int ix, k, i;
	double oval, ival = *in, val;
	double rsize_1;

	/* Find appropriate reverse list */
	rsize_1 = (double)(rt->rsize-1);
	val = ((ival - rt->rmin) * rt->qscale);
	if (val < 0.0)
		val = 0.0;
	else if (val > rsize_1)
		val = rsize_1;
	ix = (unsigned int)floor(val);		/* Coordinate */

	if (ix > (rt->size-2))
		ix = (rt->size-2);
	if (rt->rlists[ix] != NULL)  {		/* There is a list of fwd candidates */
		/* For each candidate forward range */
		for (i = 2; i < rt->rlists[ix][1]; i++)  {	/* For all fwd indexes */
			double lv,hv;
			k = rt->rlists[ix][i];					/* Base index */
			lv = rt->data[k];
			hv = rt->data[k+1];
			if ((ival >= lv && ival <= hv)	/* If this slot contains output value */
			 || (ival >= hv && ival <= lv)) {
				/* Reverse linear interpolation */
				if (hv == lv) {	/* Technically non-monotonic - due to quantization ? */
					oval = (k + 0.5)/(rt->size-1.0);
				} else
					oval = (k + ((ival - lv)/(hv - lv)))/(rt->size-1.0);
				/* If we kept looking, we would find multiple */
				/* solution for non-monotonic curve */
				*out = oval;
				return rv;
			}
		}
	}

	/* We have failed to find an exact value, so return the nearest value */
	/* (This is slow !) */
	val = fabs(ival - rt->data[0]);
	for (k = 0, i = 1; i < rt->size; i++) {
		double er;
		er = fabs(ival - rt->data[i]);
		if (er < val) {	/* new best */
			val = er;
			k = i;
		}
	}
	*out = k/(rt->size-1.0);
	rv |= 1;
	return rv;
}


/* - - - - - - - - - - - - */

/* Do a reverse lookup through the curve */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
/* (Note that clipping means mathematical clipping, and is not */
/*  set just because a device value is out of gamut. */ 
static int icmCurve_lookup_bwd(
	icmCurve *p,
	double *out,
	double *in
) {
	icc *icp = p->icp;
	int rv = 0;
	if (p->flag == icmCurveLin) {
		*out = *in;
	} else if (p->flag == icmCurveGamma) {
		double val = *in;
		if (val <= 0.0)
			*out = 0.0;
		else
			*out = pow(val, 1.0/p->data[0]);
	} else if (p->size == 0) { /* Table of 0 size */
		*out = *in;
	} else { /* Use linear interpolation */
		if (p->rt.inited == 0) {	
			rv = icmTable_setup_bwd(icp, &p->rt, p->size, p->data);
			if (rv != 0) {
				sprintf(icp->err,"icmCurve_lookup: Malloc failure in reverse lookup init.");
				return icp->errc = rv;
			}
		}
		rv = icmTable_lookup_bwd(&p->rt, out, in);
	}
	return rv;
}

/* Return the number of bytes needed to write this tag */
static unsigned int icmCurve_get_size(
	icmBase *pp
) {
	icmCurve *p = (icmCurve *)pp;
	unsigned int len = 0;
	len = sat_add(len, 12);				/* 12 bytes for tag, padding and count */
	len = sat_addmul(len, p->size, 2);	/* 2 bytes for each UInt16 */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmCurve_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmCurve *p = (icmCurve *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i;
	char *bp, *buf, *end;

	if (len < 12) {
		sprintf(icp->err,"icmCurve_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmCurve_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmCurve_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmCurve_read: Wrong tag type for icmCurve");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	p->size = read_UInt32Number(bp+8);
	bp = bp + 12;

	/* Set flag up before allocating */
	if (p->size == 0) {		/* Linear curve */
		p->flag = icmCurveLin;
	} else if (p->size == 1) {	/* Gamma curve */
		p->flag = icmCurveGamma;
	} else {
		p->flag = icmCurveSpec;
		if (p->size > (len - 12)/2) {
			sprintf(icp->err,"icmCurve_read: size overflow");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
	}

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	if (p->flag == icmCurveGamma) {	/* Gamma curve */
		if (bp > end || 1 > (end - bp)) {
			sprintf(icp->err,"icmCurve_read: Data too short for curve gamma");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		p->data[0] = read_U8Fixed8Number(bp);
	} else if (p->flag == icmCurveSpec) {
		/* Read all the data from the buffer */
		for (i = 0; i < p->size; i++, bp += 2) {
			if (bp > end || 2 > (end - bp)) {
				sprintf(icp->err,"icmCurve_read: Data too short for curve value");
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
			p->data[i] = read_DCS16Number(bp);
		}
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmCurve_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmCurve *p = (icmCurve *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmCurve_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmCurve_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmCurve_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write count */
	if ((rv = write_UInt32Number(p->size,bp+8)) != 0) {
		sprintf(icp->err,"icmCurve_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write all the data to the buffer */
	bp += 12;	/* Skip padding */
	if (p->flag == icmCurveLin) {
		if (p->size != 0) {
			sprintf(icp->err,"icmCurve_write: Must be exactly 0 entry for Linear");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
	} else if (p->flag == icmCurveGamma) {
		if (p->size != 1) {
			sprintf(icp->err,"icmCurve_write: Must be exactly 1 entry for Gamma");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		if ((rv = write_U8Fixed8Number(p->data[0],bp)) != 0) {
			sprintf(icp->err,"icmCurve_write: write_U8Fixed8umber(%.8f) failed",p->data[0]);
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	} else if (p->flag == icmCurveSpec) {
		if (p->size < 2) {
			sprintf(icp->err,"icmCurve_write: Must be 2 or more entries for Specified curve");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		for (i = 0; i < p->size; i++, bp += 2) {
			if ((rv = write_DCS16Number(p->data[i],bp)) != 0) {
				sprintf(icp->err,"icmCurve_write: write_UInt16umber(%.8f) failed",p->data[i]);
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmCurve_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmCurve_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmCurve *p = (icmCurve *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"Curve:\n");

	if (p->flag == icmCurveLin) {
		op->gprintf(op,"  Curve is linear\n");
	} else if (p->flag == icmCurveGamma) {
		op->gprintf(op,"  Curve is gamma of %.8f\n",p->data[0]);
	} else {
		op->gprintf(op,"  No. elements = %lu\n",p->size);
		if (verb >= 2) {
			unsigned int i;
			for (i = 0; i < p->size; i++)
				op->gprintf(op,"    %3lu:  %.8f\n",i,p->data[i]);
		}
	}
}

/* Allocate variable sized data elements */
static int icmCurve_allocate(
	icmBase *pp
) {
	icmCurve *p = (icmCurve *)pp;
	icc *icp = p->icp;

	if (p->flag == icmCurveUndef) {
		sprintf(icp->err,"icmCurve_alloc: flag not set");
		return icp->errc = 1;
	} else if (p->flag == icmCurveLin) {
		p->size = 0;
	} else if (p->flag == icmCurveGamma) {
		p->size = 1;
	}
	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(double))) {
			sprintf(icp->err,"icmCurve_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (double *) icp->al->calloc(icp->al, p->size, sizeof(double))) == NULL) {
			sprintf(icp->err,"icmCurve_alloc: malloc() of icmCurve data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmCurve_delete(
	icmBase *pp
) {
	icmCurve *p = (icmCurve *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icmTable_delete_bwd(icp, &p->rt);	/* Free reverse table info */
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmCurve(
	icc *icp
) {
	icmCurve *p;
	if ((p = (icmCurve *) icp->al->calloc(icp->al,1,sizeof(icmCurve))) == NULL)
		return NULL;
	p->ttype    = icSigCurveType;
	p->refcount = 1;
	p->get_size = icmCurve_get_size;
	p->read     = icmCurve_read;
	p->write    = icmCurve_write;
	p->dump     = icmCurve_dump;
	p->allocate = icmCurve_allocate;
	p->del      = icmCurve_delete;
	p->icp      = icp;

	p->lookup_fwd = icmCurve_lookup_fwd;
	p->lookup_bwd = icmCurve_lookup_bwd;

	p->rt.inited = 0;

	p->flag = icmCurveUndef;
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmData object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmData_get_size(
	icmBase *pp
) {
	icmData *p = (icmData *)pp;
	unsigned int len = 0;
	len = sat_add(len, 12);				/* 12 bytes for tag and padding */
	len = sat_addmul(len, p->size, 1);	/* 1 byte for each data element */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmData_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmData *p = (icmData *)pp;
	icc *icp = p->icp;
	int rv;
	unsigned size, f;
	char *bp, *buf;

	if (len < 12) {
		sprintf(icp->err,"icmData_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmData_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmData_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = size = (len - 12)/1;		/* Number of elements in the array */

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmData_read: Wrong tag type for icmData");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	/* Read the data type flag */
	f = read_UInt32Number(bp+8);
	if (f == 0) {
		p->flag = icmDataASCII;
	} else if (f == 1) {
		p->flag = icmDataBin;
#ifndef ICM_STRICT				/* Profile maker sometimes has a problem */
	} else if (f == 0x01000000) {
		p->flag = icmDataBin;
#endif
	} else {
		sprintf(icp->err,"icmData_read: Unknown flag value 0x%x",f);
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 12;	/* Skip padding and flag */

	if (p->size > 0) {
		if (p->flag == icmDataASCII) {
			if ((rv = check_null_string(bp,p->size)) == 1) {
				sprintf(icp->err,"icmData_read: ACSII is not null terminated");
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
			/* Haven't checked if rv == 2 is legal or not */
		}
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}

		memmove((void *)p->data, (void *)bp, p->size);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmData_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmData *p = (icmData *)pp;
	icc *icp = p->icp;
	unsigned int len, f;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmData_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmData_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmData_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	switch(p->flag) {
		case icmDataASCII:
			f = 0;
			break;
		case icmDataBin:
			f = 1;
			break;
		default:
			sprintf(icp->err,"icmData_write: Unknown Data Flag value");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
	}
	/* Write data flag descriptor to the buffer */
	if ((rv = write_UInt32Number(f,bp+8)) != 0) {
		sprintf(icp->err,"icmData_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	bp += 12;	/* Skip padding */

	if (p->data != NULL) {
		if (p->flag == icmDataASCII) {
			if ((rv = check_null_string((char *)p->data, p->size)) == 1) {
				sprintf(icp->err,"icmData_write: ASCII is not null terminated");
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
			/* Haven't checked if rv == 2 is legal or not */
		}
		memmove((void *)bp, (void *)p->data, p->size);
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmData_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmData_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmData *p = (icmData *)pp;
	unsigned int i, r, c, ii, size = 0;
	int ph = 0;		/* Phase */

	if (verb <= 0)
		return;

	op->gprintf(op,"Data:\n");
	switch(p->flag) {
		case icmDataASCII:
			op->gprintf(op,"  ASCII data\n");
			size = p->size > 0 ? p->size-1 : 0;
			break;
		case icmDataBin:
			op->gprintf(op,"  Binary data\n");
			size = p->size;
			break;
		case icmDataUndef:
			op->gprintf(op,"  Undefined data\n");
			size = p->size;
			break;
	}
	op->gprintf(op,"  No. elements = %lu\n",p->size);

	ii = i = 0;
	for (r = 1;; r++) {		/* count rows */
		if (i >= size) {
			op->gprintf(op,"\n");
			break;
		}
		if (r > 1 && verb < 2) {
			op->gprintf(op,"...\n");
			break;			/* Print 1 row if not verbose */
		}

		c = 1;
		if (ph != 0) {	/* Print ASCII under binary */
			op->gprintf(op,"           ");
			i = ii;
			c += 11;
		} else {
			op->gprintf(op,"    0x%04lx: ",i);
			ii = i;
			c += 10;
		}
		while (i < size && c < 75) {
			if (p->flag == icmDataASCII) {
				if (isprint(p->data[i])) {
					op->gprintf(op,"%c",p->data[i]);
					c++;
				} else {
					op->gprintf(op,"\\%03o",p->data[i]);
					c += 4;
				}
			} else {
				if (ph == 0) 
					op->gprintf(op,"%02x ",p->data[i]);
				else {
					if (isprint(p->data[i]))
						op->gprintf(op," %c ",p->data[i]);
					else
						op->gprintf(op,"   ",p->data[i]);
				}
				c += 3;
			}
			i++;
		}
		if (i < size)
			op->gprintf(op,"\n");
		if (verb > 2 && p->flag != icmDataASCII && ph == 0)
			ph = 1;
		else
			ph = 0;
	}
}

/* Allocate variable sized data elements */
static int icmData_allocate(
	icmBase *pp
) {
	icmData *p = (icmData *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(unsigned char))) {
			sprintf(icp->err,"icmData_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (unsigned char *) icp->al->calloc(icp->al, p->size, sizeof(unsigned char))) == NULL) {
			sprintf(icp->err,"icmData_alloc: malloc() of icmData data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmData_delete(
	icmBase *pp
) {
	icmData *p = (icmData *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmData(
	icc *icp
) {
	icmData *p;
	if ((p = (icmData *) icp->al->calloc(icp->al,1,sizeof(icmData))) == NULL)
		return NULL;
	p->ttype    = icSigDataType;
	p->refcount = 1;
	p->get_size = icmData_get_size;
	p->read     = icmData_read;
	p->write    = icmData_write;
	p->dump     = icmData_dump;
	p->allocate = icmData_allocate;
	p->del      = icmData_delete;
	p->icp      = icp;

	p->flag = icmDataUndef;
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmText object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmText_get_size(
	icmBase *pp
) {
	icmText *p = (icmText *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addmul(len, p->size, 1);	/* 1 byte for each character element (inc. null) */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmText_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmText *p = (icmText *)pp;
	icc *icp = p->icp;
	int rv;
	char *bp, *buf;

	if (len < 8) {
		sprintf(icp->err,"icmText_read: Tag too short to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmText_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmText_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->size = (len - 8)/1;		/* Number of elements in the array */

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmText_read: Wrong tag type for icmText");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp = bp + 8;

	if (p->size > 0) {
		if ((rv = check_null_string(bp,p->size)) == 1) {
			sprintf(icp->err,"icmText_read: text is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		/* Haven't checked if rv == 2 is legal or not */

		if ((rv = p->allocate((icmBase *)p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		memmove((void *)p->data, (void *)bp, p->size);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmText_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmText *p = (icmText *)pp;
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmText_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmText_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmText_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp = bp + 8;

	if (p->data != NULL) {
		if ((rv = check_null_string(p->data, p->size)) == 1) {
			sprintf(icp->err,"icmText_write: text is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		/* Haven't checked if rv == 2 is legal or not */
		memmove((void *)bp, (void *)p->data, p->size);
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmText_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmText_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmText *p = (icmText *)pp;
	unsigned int i, r, c, size;

	if (verb <= 0)
		return;

	op->gprintf(op,"Text:\n");
	op->gprintf(op,"  No. chars = %lu\n",p->size);

	size = p->size > 0 ? p->size-1 : 0;
	i = 0;
	for (r = 1;; r++) {		/* count rows */
		if (i >= size) {
			op->gprintf(op,"\n");
			break;
		}
		if (r > 1 && verb < 2) {
			op->gprintf(op,"...\n");
			break;			/* Print 1 row if not verbose */
		}
		c = 1;
		op->gprintf(op,"    0x%04lx: ",i);
		c += 10;
		while (i < size && c < 75) {
			if (isprint(p->data[i])) {
				op->gprintf(op,"%c",p->data[i]);
				c++;
			} else {
				op->gprintf(op,"\\%03o",p->data[i]);
				c += 4;
			}
			i++;
		}
		if (i < size)
			op->gprintf(op,"\n");
	}
}

/* Allocate variable sized data elements */
static int icmText_allocate(
	icmBase *pp
) {
	icmText *p = (icmText *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(char))) {
			sprintf(icp->err,"icmText_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (char *) icp->al->calloc(icp->al, p->size, sizeof(char))) == NULL) {
			sprintf(icp->err,"icmText_alloc: malloc() of icmText data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmText_delete(
	icmBase *pp
) {
	icmText *p = (icmText *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmText(
	icc *icp
) {
	icmText *p;
	if ((p = (icmText *) icp->al->calloc(icp->al,1,sizeof(icmText))) == NULL)
		return NULL;
	p->ttype    = icSigTextType;
	p->refcount = 1;
	p->get_size = icmText_get_size;
	p->read     = icmText_read;
	p->write    = icmText_write;
	p->dump     = icmText_dump;
	p->allocate = icmText_allocate;
	p->del      = icmText_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Data conversion support functions */
static int write_DateTimeNumber(icmDateTimeNumber *p, char *d) {
	int rv;
	if (p->year < 1900 || p->year > 3000
	 || p->month == 0 || p->month > 12
	 || p->day == 0 || p->day > 31
	 || p->hours > 23
	 || p->minutes > 59
	 || p->seconds > 59)
		return 1;

	if ((rv = write_UInt16Number(p->year,    d + 0)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->month,   d + 2)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->day,     d + 4)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->hours,   d + 6)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->minutes, d + 8)) != 0)
		return rv;
	if ((rv = write_UInt16Number(p->seconds, d + 10)) != 0)
		return rv;
	return 0;
}

static int read_DateTimeNumber(icmDateTimeNumber *p, char *d) {

	p->year    = read_UInt16Number(d + 0);
	p->month   = read_UInt16Number(d + 2);
	p->day     = read_UInt16Number(d + 4);
	p->hours   = read_UInt16Number(d + 6);
	p->minutes = read_UInt16Number(d + 8);
	p->seconds = read_UInt16Number(d + 10);

	/* Sanity check the date and time */
	if (p->year >= 1900 && p->year <= 3000
	 && p->month != 0 && p->month <= 12
	 && p->day != 0 && p->day <= 31
	 && p->hours <= 23
	 && p->minutes <= 59
	 && p->seconds <= 59)
		return 0;

#ifdef NEVER
	printf("Raw year = %d, month = %d, day = %d\n",p->year, p->month, p->day);
	printf("Raw hour = %d, minutes = %d, seconds = %d\n", p->hours, p->minutes, p->seconds);
#endif /* NEVER */

#ifdef ICM_STRICT	
	return 1;			/* Not legal */

#else
	/* Be more forgiving */

	/* Check for Adobe problem */
	if (p->month >= 1900 && p->month <= 3000
	 && p->year != 0 && p->year <= 12
	 && p->hours != 0 && p->hours <= 31
	 && p->day <= 23
	 && p->seconds <= 59
	 && p->minutes <= 59) {
		unsigned int tt; 

		/* Correct Adobe's faulty profile */
		tt = p->month; p->month = p->year; p->year = tt;
		tt = p->hours; p->hours = p->day; p->day = tt;
		tt = p->seconds; p->seconds = p->minutes; p->minutes = tt;

		return 0;
	}

	/* Hmm. some other sort of corruption. Limit values to sane */
	if (p->year < 1900) {
		if (p->year < 100)			/* Hmm. didn't use 4 digit year, guess it's 19xx ? */
			p->year += 1900;
		else
			p->year = 1900;
	} else if (p->year > 3000)
		p->year = 3000;

	if (p->month == 0)
		p->month = 1;
	else if (p->month > 12)
		p->month = 12;

	if (p->day == 0)
		p->day = 1;
	else if (p->day > 31)
		p->day = 31;

	if (p->hours > 23)
		p->hours = 23;

	if (p->minutes > 59)
		p->minutes = 59;

	if (p->seconds > 59)
		p->seconds = 59;

	return 0;
#endif
}

/* Return a string that shows the given date and time */
static char *string_DateTimeNumber(icmDateTimeNumber *p) {
	static const char *mstring[13] = {"Bad", "Jan","Feb","Mar","Apr","May","Jun",
					  "Jul","Aug","Sep","Oct","Nov","Dec"};
	static char buf[80];

	sprintf(buf,"%d %s %4d, %d:%02d:%02d", 
	                p->day, mstring[p->month > 12 ? 0 : p->month], p->year,
	                p->hours, p->minutes, p->seconds);
	return buf;
}

/* Set the DateTime structure to the current date and time */
static void setcur_DateTimeNumber(icmDateTimeNumber *p) {
	time_t cclk;
	struct tm *ctm;
	
	cclk = time(NULL);
	ctm = localtime(&cclk);
	
	p->year    = ctm->tm_year + 1900;
	p->month   = ctm->tm_mon + 1;
	p->day     = ctm->tm_mday;
	p->hours   = ctm->tm_hour;
	p->minutes = ctm->tm_min;
	p->seconds = ctm->tm_sec;
}

/* Return the number of bytes needed to write this tag */
static unsigned int icmDateTimeNumber_get_size(
	icmBase *pp
) {
	unsigned int len = 0;
	len = sat_add(len, 8);		/* 8 bytes for tag and padding */
	len = sat_add(len, 12);		/* 12 bytes for Date & Time */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmDateTimeNumber_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmDateTimeNumber *p = (icmDateTimeNumber *)pp;
	icc *icp = p->icp;
	int rv;
	char *bp, *buf;

	if (len < 20) {
		sprintf(icp->err,"icmDateTimeNumber_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmDateTimeNumber_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmDateTimeNumber_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmDateTimeNumber_read: Wrong tag type for icmDateTimeNumber");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	/* Read the time and date from buffer */
	if((rv = read_DateTimeNumber(p, bp)) != 0) {
		sprintf(icp->err,"icmDateTimeNumber_read: Corrupted DateTime");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmDateTimeNumber_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmDateTimeNumber *p = (icmDateTimeNumber *)pp;
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmDateTimeNumber_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmDateTimeNumber_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmDateTimeNumber_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write all the data to the buffer */
	bp += 8;	/* Skip padding */
	if ((rv = write_DateTimeNumber(p, bp)) != 0) {
		sprintf(icp->err,"icmDateTimeNumber_write: write_DateTimeNumber() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmDateTimeNumber_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmDateTimeNumber_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmDateTimeNumber *p = (icmDateTimeNumber *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"DateTimeNumber:\n");
	op->gprintf(op,"  Date = %s\n", string_DateTimeNumber(p));
}

/* Allocate variable sized data elements */
static int icmDateTimeNumber_allocate(
	icmBase *pp
) {
	/* Nothing to do */
	return 0;
}

/* Free all storage in the object */
static void icmDateTimeNumber_delete(
	icmBase *pp
) {
	icmDateTimeNumber *p = (icmDateTimeNumber *)pp;
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmDateTimeNumber(
	icc *icp
) {
	icmDateTimeNumber *p;
	if ((p = (icmDateTimeNumber *) icp->al->calloc(icp->al,1,sizeof(icmDateTimeNumber))) == NULL)
		return NULL;
	p->ttype    = icSigDateTimeType;
	p->refcount = 1;
	p->get_size = icmDateTimeNumber_get_size;
	p->read     = icmDateTimeNumber_read;
	p->write    = icmDateTimeNumber_write;
	p->dump     = icmDateTimeNumber_dump;
	p->allocate = icmDateTimeNumber_allocate;
	p->del      = icmDateTimeNumber_delete;
	p->icp      = icp;

	setcur_DateTimeNumber(p);	/* Default to current date and time */
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmLut object */

/* Check if the matrix is non-zero */
static int icmLut_nu_matrix(
	icmLut *p		/* Pointer to Lut object */
) {
	int i, j;
	
	for (j = 0; j < 3; j++) {		/* Rows */
		for (i = 0; i < 3; i++) {	/* Columns */
			if (   (i == j && p->e[j][i] != 1.0)
			    || (i != j && p->e[j][i] != 0.0))
				return 1;
		}
	}
	return 0;
}

/* return the locations of the minimum and */
/* maximum values of the given channel, in the clut */
static void icmLut_min_max(
	icmLut *p,		/* Pointer to Lut object */
	double *minp,	/* Return position of min/max */
	double *maxp,
	int chan		/* Channel, -1 for average of all */
) {
	double *tp;
	double minv, maxv;	/* Values */
	unsigned int e, ee, f;
	int gc[MAX_CHAN];	/* Grid coordinate */

	minv = 1e6;
	maxv = -1e6;

	for (e = 0; e < p->inputChan; e++)
		gc[e] = 0;	/* init coords */

	/* Search the whole table */
	for (tp = p->clutTable, e = 0; e < p->inputChan; tp += p->outputChan) {
		double v;
		if (chan == -1) {
			for (v = 0.0, f = 0; f < p->outputChan; f++)
				v += tp[f];
		} else {
			v = tp[chan];
		}
		if (v < minv) {
			minv = v;
			for (ee = 0; ee < p->inputChan; ee++)
				minp[ee] = gc[ee]/(p->clutPoints-1.0);
		}
		if (v > maxv) {
			maxv = v;
			for (ee = 0; ee < p->inputChan; ee++)
				maxp[ee] = gc[ee]/(p->clutPoints-1.0);
		}

		/* Increment coord */
		for (e = 0; e < p->inputChan; e++) {
			if (++gc[e] < p->clutPoints)
				break;	/* No carry */
			gc[e] = 0;
		}
	}
}

/* Convert XYZ throught Luts matrix */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmLut_lookup_matrix(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[outputChan] in ICC order - see Table 39 in 6.5.5 */
double *in		/* Input array[inputChan] */
) {
	double t0,t1;	/* Take care if out == in */
	t0     = p->e[0][0] * in[0] + p->e[0][1] * in[1] + p->e[0][2] * in[2];
	t1     = p->e[1][0] * in[0] + p->e[1][1] * in[1] + p->e[1][2] * in[2];
	out[2] = p->e[2][0] * in[0] + p->e[2][1] * in[1] + p->e[2][2] * in[2];
	out[0] = t0;
	out[1] = t1;

	return 0;
}

/* Convert normalized numbers though this Luts per channel input tables. */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmLut_lookup_input(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[inputChan] */
) {
	int rv = 0;
	unsigned int ix, n;
	double inputEnt_1 = (double)(p->inputEnt-1);
	double *table = p->inputTable;

	if (p->inputEnt == 0) {		/* Hmm. */
		for (n = 0; n < p->inputChan; n++)
			out[n] = in[n];
	} else {
		/* Use linear interpolation */
		for (n = 0; n < p->inputChan; n++, table += p->inputEnt) {
			double val, w;
			val = in[n] * inputEnt_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > inputEnt_1) {
				val = inputEnt_1;
				rv |= 1;
			}
			ix = (unsigned int)floor(val);		/* Grid coordinate */
			if (ix > (p->inputEnt-2))
				ix = (p->inputEnt-2);
			w = val - (double)ix;		/* weight */
			val = table[ix];
			out[n] = val + w * (table[ix+1] - val);
		}
	}
	return rv;
}

/* Convert normalized numbers though this Luts multi-dimensional table. */
/* using multi-linear interpolation. */
static int icmLut_lookup_clut_nl(
/* Return 0 on success, 1 if clipping occured, 2 on other error */
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	icc *icp = p->icp;
	int rv = 0;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	double *gw, GW[1 << 8];		/* weight for each grid cube corner */

	if (p->inputChan <= 8) {
		gw = GW;				/* Use stack allocation */
	} else {
		if ((gw = (double *) icp->al->malloc(icp->al, sat_mul((1 << p->inputChan), sizeof(double)))) == NULL) {
			sprintf(icp->err,"icmLut_lookup_clut: malloc() failed");
			return icp->errc = 2;
		}
	}

	/* We are using an multi-linear (ie. Trilinear for 3D input) interpolation. */
	/* The implementation here uses more multiplies that some other schemes, */
	/* (for instance, see "Tri-Linear Interpolation" by Steve Hill, */
	/* Graphics Gems IV, page 521), but has less involved bookeeping, */
	/* needs less local storage for intermediate output values, does fewer */
	/* output and intermediate value reads, and fp multiplies are fast on */
	/* todays processors! */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		double clutPoints_1 = (double)(p->clutPoints-1);
		int    clutPoints_2 = p->clutPoints-2;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= 1;
			}
			x = (unsigned int)floor(val);	/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
	/* Compute corner weights needed for interpolation */
	{
		unsigned int e;
		int i, g = 1;
		gw[0] = 1.0;
		for (e = 0; e < p->inputChan; e++) {
			for (i = 0; i < g; i++) {
				gw[g+i] = gw[i] * co[e];
				gw[i] *= (1.0 - co[e]);
			}
			g *= 2;
		}
	}
	/* Now compute the output values */
	{
		int i;
		unsigned int f;
		double w = gw[0];
		double *d = gp + p->dcube[0];
		for (f = 0; f < p->outputChan; f++)			/* Base of cube */
			out[f] = w * d[f];
		for (i = 1; i < (1 << p->inputChan); i++) {	/* For all other corners of cube */
			w = gw[i];				/* Strength reduce */
			d = gp + p->dcube[i];
			for (f = 0; f < p->outputChan; f++)
				out[f] += w * d[f];
		}
	}
	if (gw != GW)
		icp->al->free(icp->al, (void *)gw);
	return rv;
}

/* Convert normalized numbers though this Luts multi-dimensional table */
/* using simplex interpolation. */
static int icmLut_lookup_clut_sx(
/* Return 0 on success, 1 if clipping occured, 2 on other error */
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	int rv = 0;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	int    si[MAX_CHAN];		/* co[] Sort index, [0] = smalest */

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */
	/* This method is more appropriate for XYZ/RGB/CMYK input spaces, */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		double clutPoints_1 = (double)(p->clutPoints-1);
		int    clutPoints_2 = p->clutPoints-2;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= 1;
			}
			x = (unsigned int)floor(val);		/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
#ifdef NEVER
	/* Do selection sort on coordinates, smallest to largest. */
	{
		int e, f;
		for (e = 0; e < p->inputChan; e++)
			si[e] = e;						/* Initial unsorted indexes */
		for (e = 0; e < (p->inputChan-1); e++) {
			double cosn;
			cosn = co[si[e]];				/* Current smallest value */
			for (f = e+1; f < p->inputChan; f++) {	/* Check against rest */
				int tt;
				tt = si[f];
				if (cosn > co[tt]) {
					si[f] = si[e]; 			/* Exchange */
					si[e] = tt;
					cosn = co[tt];
				}
			}
		}
	}
#else
	/* Do insertion sort on coordinates, smallest to largest. */
	{
		int f, vf;
		unsigned int e;
		double v;
		for (e = 0; e < p->inputChan; e++)
			si[e] = e;						/* Initial unsorted indexes */

		for (e = 1; e < p->inputChan; e++) {
			f = e;
			v = co[si[f]];
			vf = f;
			while (f > 0 && co[si[f-1]] > v) {
				si[f] = si[f-1];
				f--;
			}
			si[f] = vf;
		}
	}
#endif
	/* Now compute the weightings, simplex vertices and output values */
	{
		unsigned int e, f;
		double w;		/* Current vertex weight */

		w = 1.0 - co[si[p->inputChan-1]];		/* Vertex at base of cell */
		for (f = 0; f < p->outputChan; f++)
			out[f] = w * gp[f];

		for (e = p->inputChan-1; e > 0; e--) {	/* Middle verticies */
			w = co[si[e]] - co[si[e-1]];
			gp += p->dinc[si[e]];				/* Move to top of cell in next largest dimension */
			for (f = 0; f < p->outputChan; f++)
				out[f] += w * gp[f];
		}

		w = co[si[0]];
		gp += p->dinc[si[0]];		/* Far corner from base of cell */
		for (f = 0; f < p->outputChan; f++)
			out[f] += w * gp[f];
	}
	return rv;
}

/* Convert normalized numbers though this Luts per channel output tables. */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
static int icmLut_lookup_output(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[outputChan] */
double *in		/* Input array[outputChan] */
) {
	int rv = 0;
	unsigned int ix, n;
	double outputEnt_1 = (double)(p->outputEnt-1);
	double *table = p->outputTable;

	if (p->outputEnt == 0) {		/* Hmm. */
		for (n = 0; n < p->outputChan; n++)
			out[n] = in[n];
	} else {
		/* Use linear interpolation */
		for (n = 0; n < p->outputChan; n++, table += p->outputEnt) {
			double val, w;
			val = in[n] * outputEnt_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > outputEnt_1) {
				val = outputEnt_1;
				rv |= 1;
			}
			ix = (unsigned int)floor(val);		/* Grid coordinate */
			if (ix > (p->outputEnt-2))
				ix = (p->outputEnt-2);
			w = val - (double)ix;		/* weight */
			val = table[ix];
			out[n] = val + w * (table[ix+1] - val);
		}
	}
	return rv;
}

/* ----------------------------------------------- */
/* Tune a single interpolated value. Based on lookup_clut functions (above) */

/* Helper function to fine tune a single value interpolation */
/* Return 0 on success, 1 if input clipping occured, 2 if output clipping occured */
int icmLut_tune_value_nl(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	icc *icp = p->icp;
	int rv = 0;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	double *gw, GW[1 << 8];		/* weight for each grid cube corner */
	double cout[MAX_CHAN];		/* Current output value */

	if (p->inputChan <= 8) {
		gw = GW;				/* Use stack allocation */
	} else {
		if ((gw = (double *) icp->al->malloc(icp->al, sat_mul((1 << p->inputChan), sizeof(double)))) == NULL) {
			sprintf(icp->err,"icmLut_lookup_clut: malloc() failed");
			return icp->errc = 2;
		}
	}

	/* We are using an multi-linear (ie. Trilinear for 3D input) interpolation. */
	/* The implementation here uses more multiplies that some other schemes, */
	/* (for instance, see "Tri-Linear Interpolation" by Steve Hill, */
	/* Graphics Gems IV, page 521), but has less involved bookeeping, */
	/* needs less local storage for intermediate output values, does fewer */
	/* output and intermediate value reads, and fp multiplies are fast on */
	/* todays processors! */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		double clutPoints_1 = (double)(p->clutPoints-1);
		int    clutPoints_2 = p->clutPoints-2;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= 1;
			}
			x = (unsigned int)floor(val);	/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
	/* Compute corner weights needed for interpolation */
	{
		unsigned int e;
		int i, g = 1;
		gw[0] = 1.0;
		for (e = 0; e < p->inputChan; e++) {
			for (i = 0; i < g; i++) {
				gw[g+i] = gw[i] * co[e];
				gw[i] *= (1.0 - co[e]);
			}
			g *= 2;
		}
	}
	/* Now compute the current output value, and distribute the correction */
	{
		int i;
		unsigned int f;
		double w, *d, ww = 0.0;
		for (f = 0; f < p->outputChan; f++)
			cout[f] = 0.0;
		for (i = 0; i < (1 << p->inputChan); i++) {	/* For all other corners of cube */
			w = gw[i];				/* Strength reduce */
			ww += w * w;			/* Sum of weights squared */
			d = gp + p->dcube[i];
			for (f = 0; f < p->outputChan; f++)
				cout[f] += w * d[f];
		}

		/* We distribute the correction needed in proportion to the */
		/* interpolation weighting, so the biggest correction is to the */
		/* closest vertex. */

		for (f = 0; f < p->outputChan; f++)
			cout[f] = (out[f] - cout[f])/ww;	/* Amount to distribute */

		for (i = 0; i < (1 << p->inputChan); i++) {	/* For all other corners of cube */
			w = gw[i];				/* Strength reduce */
			d = gp + p->dcube[i];
			for (f = 0; f < p->outputChan; f++) {
				d[f] += w * cout[f];			/* Apply correction */
				if (d[f] < 0.0) {
					d[f] = 0.0;
					rv |= 2;
				} else if (d[f] > 1.0) {
					d[f] = 1.0;
					rv |= 2;
				} 
			}
		}
	}

	if (gw != GW)
		icp->al->free(icp->al, (void *)gw);
	return rv;
}

/* Helper function to fine tune a single value interpolation */
/* Return 0 on success, 1 if input clipping occured, 2 if output clipping occured */
int icmLut_tune_value_sx(
icmLut *p,		/* Pointer to Lut object */
double *out,	/* Output array[inputChan] */
double *in		/* Input array[outputChan] */
) {
	int rv = 0;
	double *gp;					/* Pointer to grid cube base */
	double co[MAX_CHAN];		/* Coordinate offset with the grid cell */
	int    si[MAX_CHAN];		/* co[] Sort index, [0] = smalest */

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */
	/* This method is more appropriate for XYZ/RGB/CMYK input spaces, */

	/* Compute base index into grid and coordinate offsets */
	{
		unsigned int e;
		double clutPoints_1 = (double)(p->clutPoints-1);
		int    clutPoints_2 = p->clutPoints-2;
		gp = p->clutTable;		/* Base of grid array */

		for (e = 0; e < p->inputChan; e++) {
			unsigned int x;
			double val;
			val = in[e] * clutPoints_1;
			if (val < 0.0) {
				val = 0.0;
				rv |= 1;
			} else if (val > clutPoints_1) {
				val = clutPoints_1;
				rv |= 1;
			}
			x = (unsigned int)floor(val);		/* Grid coordinate */
			if (x > clutPoints_2)
				x = clutPoints_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			gp += x * p->dinc[e];		/* Add index offset for base of cube */
		}
	}
	/* Do insertion sort on coordinates, smallest to largest. */
	{
		int f, vf;
		unsigned int e;
		double v;
		for (e = 0; e < p->inputChan; e++)
			si[e] = e;						/* Initial unsorted indexes */

		for (e = 1; e < p->inputChan; e++) {
			f = e;
			v = co[si[f]];
			vf = f;
			while (f > 0 && co[si[f-1]] > v) {
				si[f] = si[f-1];
				f--;
			}
			si[f] = vf;
		}
	}
	/* Now compute the current output value, and distribute the correction */
	{
		unsigned int e, f;
		double w, ww = 0.0;		/* Current vertex weight, sum of weights squared */
		double cout[MAX_CHAN];		/* Current output value */
		double *ogp = gp;		/* Pointer to grid cube base */

		w = 1.0 - co[si[p->inputChan-1]];		/* Vertex at base of cell */
		ww += w * w;							/* Sum of weights squared */
		for (f = 0; f < p->outputChan; f++)
			cout[f] = w * gp[f];

		for (e = p->inputChan-1; e > 0; e--) {	/* Middle verticies */
			w = co[si[e]] - co[si[e-1]];
			ww += w * w;							/* Sum of weights squared */
			gp += p->dinc[si[e]];				/* Move to top of cell in next largest dimension */
			for (f = 0; f < p->outputChan; f++)
				cout[f] += w * gp[f];
		}

		w = co[si[0]];
		ww += w * w;							/* Sum of weights squared */
		gp += p->dinc[si[0]];					/* Far corner from base of cell */
		for (f = 0; f < p->outputChan; f++)
			cout[f] += w * gp[f];

		/* We distribute the correction needed in proportion to the */
		/* interpolation weighting, so the biggest correction is to the */
		/* closest vertex. */
		for (f = 0; f < p->outputChan; f++)
			cout[f] = (out[f] - cout[f])/ww;	/* Amount to distribute */

		gp = ogp;
		w = 1.0 - co[si[p->inputChan-1]];		/* Vertex at base of cell */
		for (f = 0; f < p->outputChan; f++) {
			gp[f] += w * cout[f];			/* Apply correction */
			if (gp[f] < 0.0) {
				gp[f] = 0.0;
				rv |= 2;
			} else if (gp[f] > 1.0) {
				gp[f] = 1.0;
				rv |= 2;
			}
		}

		for (e = p->inputChan-1; e > 0; e--) {	/* Middle verticies */
			w = co[si[e]] - co[si[e-1]];
			gp += p->dinc[si[e]];				/* Move to top of cell in next largest dimension */
			for (f = 0; f < p->outputChan; f++) {
				gp[f] += w * cout[f];			/* Apply correction */
				if (gp[f] < 0.0) {
					gp[f] = 0.0;
					rv |= 2;
				} else if (gp[f] > 1.0) {
					gp[f] = 1.0;
					rv |= 2;
				}
			}
		}

		w = co[si[0]];
		gp += p->dinc[si[0]];					/* Far corner from base of cell */
		for (f = 0; f < p->outputChan; f++) {
			gp[f] += w * cout[f];			/* Apply correction */
			if (gp[f] < 0.0) {
				gp[f] = 0.0;
				rv |= 2;
			} else if (gp[f] > 1.0) {
				gp[f] = 1.0;
				rv |= 2;
			}
		}
	}
	return rv;
}


/* ----------------------------------------------- */
/* Pseudo - Hilbert count sequencer */

/* This multi-dimensional count sequence is a distributed */
/* Gray code sequence, with direction reversal on every */
/* alternate power of 2 scale. */
/* It is intended to aid cache coherence in multi-dimensional */
/* regular sampling. It approximates the Hilbert curve sequence. */

/* Initialise, returns total usable count */
unsigned
psh_init(
psh *p,	/* Pointer to structure to initialise */
int      di,		/* Dimensionality */
unsigned int res,	/* Size per coordinate */
int co[]			/* Coordinates to initialise (May be NULL) */
) {
	int e;

	p->di = di;
	p->res = res;

	/* Compute bits */
	for (p->bits = 0; (1u << p->bits) < res; p->bits++)
		;

	/* Compute the total count mask */
	p->tmask = ((((unsigned)1) << (p->bits * di))-1);

	/* Compute usable count */
	p->count = 1;
	for (e = 0; e < di; e++)
		p->count *= res;

	p->ix = 0;

	if (co != NULL) {
		for (e = 0; e < di; e++)
			co[e] = 0;
	}

	return p->count;
}

/* Reset the counter */
void
psh_reset(
psh *p	/* Pointer to structure */
) {
	p->ix = 0;
}

/* Increment pseudo-hilbert coordinates */
/* Return non-zero if count rolls over to 0 */
int
psh_inc(
psh *p,	/* Pointer to structure */
int co[]		/* Coordinates to return */
) {
	int di = p->di;
	unsigned int res = p->res;
	unsigned int bits = p->bits;
	int e;

	do {
		unsigned int b;
		int gix;	/* Gray code index */
		
		p->ix = (p->ix + 1) & p->tmask;

		gix = p->ix ^ (p->ix >> 1);		/* Convert to gray code index */
	
		for (e = 0; e < di; e++) 
			co[e] = 0;
		
		for (b = 0; b < bits; b++) {	/* Distribute bits */
			if (b & 1) {
				for (e = di-1; e >= 0; e--)  {		/* In reverse order */
					co[e] |= (gix & 1) << b;
					gix >>= 1;
				}
			} else {
				for (e = 0; e < di; e++)  {			/* In normal order */
					co[e] |= (gix & 1) << b;
					gix >>= 1;
				}
			}
		}
	
		/* Convert from Gray to binary coordinates */
		for (e = 0; e < di; e++)  {
			unsigned int sh, tv;

			for(sh = 1, tv = co[e];; sh <<= 1) {
				unsigned ptv = tv;
				tv ^= (tv >> sh);
				if (ptv <= 1 || sh == 16)
					break;
			}
			if (tv >= res)	/* Dumbo filter - increment again if outside cube range */
				break;
			co[e] = tv;
		}

	} while (e < di);
	
	return (p->ix == 0);
}

/* ------------------------------------------------------- */

#ifndef COUNTERS_H

/* Macros for a multi-dimensional counter. */

/* Declare the counter name nn, maximum di mxdi, dimensions di, & count */
/* This counter can have each dimension range clipped */

#define FCOUNT(nn, mxdi, di) 				\
	int nn[mxdi];			/* counter value */				\
	int nn##_di = (di);		/* Number of dimensions */		\
	int nn##_stt[mxdi];		/* start count value */			\
	int nn##_res[mxdi]; 	/* last count +1 */				\
	int nn##_e				/* dimension index */

#define FRECONF(nn, start, endp1) 							\
	for (nn##_e = 0; nn##_e < nn##_di; nn##_e++) {			\
		nn##_stt[nn##_e] = (start);	/* start count value */	\
		nn##_res[nn##_e] = (endp1); /* last count +1 */		\
	}

/* Set the counter value to 0 */
#define FC_INIT(nn) 								\
{													\
	for (nn##_e = 0; nn##_e < nn##_di; nn##_e++)	\
		nn[nn##_e] = nn##_stt[nn##_e];				\
	nn##_e = 0;										\
}

/* Increment the counter value */
#define FC_INC(nn)									\
{													\
	for (nn##_e = 0; nn##_e < nn##_di; nn##_e++) {	\
		nn[nn##_e]++;								\
		if (nn[nn##_e] < nn##_res[nn##_e])			\
			break;	/* No carry */					\
		nn[nn##_e] = nn##_stt[nn##_e];				\
	}												\
}

/* After increment, expression is TRUE if counter is done */
#define FC_DONE(nn)								\
	(nn##_e >= nn##_di)

#endif /* COUNTERS_H */

/* Parameter to getNormFunc function */
typedef enum {
    icmFromLuti   = 0,  /* return "fromo Lut normalized index" conversion function */
    icmToLuti     = 1,  /* return "to Lut normalized index" conversion function */
    icmFromLutv   = 2,  /* return "from Lut normalized value" conversion function */
    icmToLutv     = 3   /* return "to Lut normalized value" conversion function */
} icmNormFlag;

/* Return an appropriate color space normalization function, */
/* given the color space and Lut type */
/* Return 0 on success, 1 on match failure */
static int getNormFunc(
	icc                   *icp,
	icColorSpaceSignature csig, 
	icTagTypeSignature    tagSig,
	icmNormFlag           flag,
	void               (**nfunc)(double *out, double *in)
);

#define CLIP_MARGIN 0.005		/* Margine to allow before reporting clipping = 0.5% */

/* NOTE that ICM_CLUT_SET_FILTER turns out to be not very useful, */
/* as it can result in reversals. Could #ifdef out the code ?? */

/* Helper function to set multiple Lut tables simultaneously. */
/* Note that these tables all have to be compatible in */
/* having the same configuration and resolution. */
/* Set errc and return error number in underlying icc */
/* Set warnc if there is clipping in the output values: */
/*  1 = input table, 2 = main clut, 3 = clut midpoint, 4 = midpoint interp, 5 = output table */
/* Note that clutfunc in[] value has "index under", ie: */
/* at ((int *)in)[-chan-1], and for primary grid is simply the */
/* grid index (ie. 5,3,8), and for the center of cells grid, is */
/* the -index-1, ie. -6,-3,-8 */
int icmSetMultiLutTables(
	int ntables,						/* Number of tables to be set, 1..n */
	icmLut **pp,						/* Pointer to array of Lut objects */
	int     flags,						/* Setting flags */
	void   *cbctx,						/* Opaque callback context pointer value */
	icColorSpaceSignature insig, 		/* Input color space */
	icColorSpaceSignature outsig, 		/* Output color space */
	void (*infunc)(void *cbctx, double *out, double *in),
							/* Input transfer function, inspace->inspace' (NULL = default) */
							/* Will be called ntables times for each input grid value */
	double *inmin, double *inmax,		/* Maximum range of inspace' values */
										/* (NULL = default) */
	void (*clutfunc)(void *cbntx, double *out, double *in),
							/* inspace' -> outspace[ntables]' transfer function */
							/* will be called once for each input' grid value, and */
							/* ntables output values should be written consecutively */
							/* to out[]. */
	double *clutmin, double *clutmax,	/* Maximum range of outspace' values */
										/* (NULL = default) */
	void (*outfunc)(void *cbntx, double *out, double *in),
								/* Output transfer function, outspace'->outspace (NULL = deflt) */
								/* Will be called ntables times on each output value */
	int *apxls_gmin, int *apxls_gmax	/* If not NULL, the grid indexes not to be affected */
										/* by ICM_CLUT_SET_APXLS, defaulting to 0..>clutPoints-1 */
) {
	icmLut *p, *pn;				/* Pointer to 0'th nd tn'th Lut object */
	icc *icp;					/* Pointer to common icc */
	int tn;
	unsigned int e, f, i, n;
	double **clutTable2 = NULL;		/* Cell center values for ICM_CLUT_SET_APXLS */ 
	double *clutTable3 = NULL;		/* Vertex smoothing radius values [ntables] per entry */
	int dinc3[MAX_CHAN];			/* Dimensional increment through clut3 (in doubles) */
	int dcube3[1 << MAX_CHAN];		/* Hyper cube offsets throught clut3 (in doubles) */
	int ii[MAX_CHAN];		/* Index value */
	psh counter;			/* Pseudo-Hilbert counter */
//	double _iv[4 * MAX_CHAN], *iv = &_iv[MAX_CHAN], *ivn;	/* Real index value/table value */
	int maxchan;			/* Actual max of input and output */
	double *_iv, *iv, *ivn;	/* Real index value/table value */
	double imin[MAX_CHAN], imax[MAX_CHAN];
	double omin[MAX_CHAN], omax[MAX_CHAN];
	int def_apxls_gmin[MAX_CHAN], def_apxls_gmax[MAX_CHAN];
	void (*ifromindex)(double *out, double *in);	/* Index to input color space function */
	void (*itoentry)(double *out, double *in);		/* Input color space to entry function */
	void (*ifromentry)(double *out, double *in);	/* Entry to input color space function */
	void (*otoentry)(double *out, double *in);		/* Output colorspace to table value function */
	void (*ofromentry)(double *out, double *in);	/* Table value to output color space function */
	int clip = 0;

	/* Check that everything is OK to proceed */
	if (ntables < 1 || ntables > MAX_CHAN) {
		if (ntables >= 1) {
			icp = pp[0]->icp;
			sprintf(icp->err,"icmSetMultiLutTables has illegal number of tables %d",ntables);
			return icp->errc = 1;
		} else {
			/* Can't write error message anywhere */
			return 1;
		}
	}

	p   = pp[0];
	icp = p->icp;

	for (tn = 1; tn < ntables; tn++) {
		if (pp[tn]->icp != icp) {
			sprintf(icp->err,"icmSetMultiLutTables Tables base icc is different");
			return icp->errc = 1;
		}
		if (pp[tn]->ttype != p->ttype) {
			sprintf(icp->err,"icmSetMultiLutTables Tables have different Tage Type");
			return icp->errc = 1;
		}

		if (pp[tn]->inputChan != p->inputChan) {
			sprintf(icp->err,"icmSetMultiLutTables Tables have different inputChan");
			return icp->errc = 1;
		}
		if (pp[tn]->outputChan != p->outputChan) {
			sprintf(icp->err,"icmSetMultiLutTables Tables have different outputChan");
			return icp->errc = 1;
		}
		if (pp[tn]->clutPoints != p->clutPoints) {
			sprintf(icp->err,"icmSetMultiLutTables Tables have different clutPoints");
			return icp->errc = 1;
		}
	}

	if (getNormFunc(icp, insig, p->ttype, icmFromLuti, &ifromindex) != 0) {
		sprintf(icp->err,"icmLut_set_tables index to input colorspace function lookup failed");
		return icp->errc = 1;
	}
	if (getNormFunc(icp, insig, p->ttype, icmToLutv, &itoentry) != 0) {
		sprintf(icp->err,"icmLut_set_tables input colorspace to table entry function lookup failed");
		return icp->errc = 1;
	}
	if (getNormFunc(icp, insig, p->ttype, icmFromLutv, &ifromentry) != 0) {
		sprintf(icp->err,"icmLut_set_tables table entry to input colorspace function lookup failed");
		return icp->errc = 1;
	}

	if (getNormFunc(icp, outsig, p->ttype, icmToLutv, &otoentry) != 0) {
		sprintf(icp->err,"icmLut_set_tables output colorspace to table entry function lookup failed");
		return icp->errc = 1;
	}
	if (getNormFunc(icp, outsig, p->ttype, icmFromLutv, &ofromentry) != 0) {
		sprintf(icp->err,"icmLut_set_tables table entry to output colorspace function lookup failed");
		return icp->errc = 1;
	}

	/* Allocate an array to hold the input and output values. */
	/* It needs to be able to hold di "index under valus as in[], */
	/* and ntables ICM_CLUT_SET_FILTER values as out[], so we assume maxchan >= di */
	maxchan = p->inputChan > p->outputChan ? p->inputChan : p->outputChan;
	if ((_iv = (double *) icp->al->malloc(icp->al, sizeof(double) * maxchan * (ntables+1)))
	                                                                              == NULL) {
		sprintf(icp->err,"icmLut_read: malloc() failed");
		return icp->errc = 2;
	}
	iv = _iv + maxchan;		/* Allow for "index under" and smoothing radius values */

	/* Setup input table value min-max */
	if (inmin == NULL || imax == NULL) {
#ifdef SYMETRICAL_DEFAULT_LAB_RANGE	/* Symetrical default range. */
		/* We are assuming V2 Lab16 encoding, since this is a lut16type that always uses */
		/* this encoding */
		if (insig == icSigLabData) { /* Special case Lab */
			double mn[3], mx[3];
			/* This is to ensure that Lab 100,0,0 maps exactly to a clut grid point. */
			/* This should work well if there is an odd grid resolution, */
			/* and icclib is being used, as input lookup will */
			/* be computed using floating point, so that the CLUT input value */
			/* 0.5 can be represented exactly. */
			/* Because the symetric range will cause slight clipping, */
			/* only do it if the input table has sufficient resolution */
			/* to represent the clipping faithfuly. */
			if (p->inputEnt >= 64) {
				if (p->ttype == icSigLut8Type) {
					mn[0] =   0.0, mn[1] = mn[2] = -127.0;
					mx[0] = 100.0, mx[1] = mx[2] =  127.0;
				} else {
					mn[0] =   0.0, mn[1] = mn[2] = -127.0 - 255.0/256.0;
					mx[0] = 100.0, mx[1] = mx[2] =  127.0 + 255.0/256.0;
				}
				itoentry(imin, mn);	/* Convert from input color space to table representation */
				itoentry(imax, mx);
			} else {
				for (e = 0; e < p->inputChan; e++) {
					imin[e] = 0.0;
					imax[e] = 1.0;
				}
			}
		} else
#endif
		{
			for (e = 0; e < p->inputChan; e++) {
				imin[e] = 0.0;		/* We are assuming this is true for all other color spaces. */
				imax[e] = 1.0;
			}
		}
	} else {
		itoentry(imin, inmin);	/* Convert from input color space to table representation */
		itoentry(imax, inmax);
	}

	/* Setup output table value min-max */
	if (clutmin == NULL || clutmax == NULL) {
#ifdef SYMETRICAL_DEFAULT_LAB_RANGE
		/* This really isn't doing much, since the full range encoding doesn't need */
		/* any adjustment to map a*b* 0 to an integer value. */
		/* We are tweaking the 16 bit L* = 100 to the last index into */
		/* the output table, which may help its accuracy slightly. */
		/* We are assuming V2 Lab16 encoding, since this is a lut16type that always uses */
		/* this encoding */
		if (outsig == icSigLabData) { /* Special case Lab */
			double mn[3], mx[3];
			/* The output of the CLUT will be an 8 or 16 bit value, and we want to */
			/* adjust the range so that the input grid point holding the white */
			/* point can encode 0.0 exactly. */
			/* Note that in the case of the a & b values, the range equates to */
			/* normalised 0.0 .. 1.0, since 0 can be represented exactly in it. */
			if (p->outputEnt >= 64) {
				if (p->ttype == icSigLut8Type) {
					mn[0] =   0.0, mn[1] = mn[2] = -128.0;
					mx[0] = 100.0, mx[1] = mx[2] = 127.0;
				} else {
					mn[0] =   0.0, mn[1] = mn[2] = -128.0;
					mx[0] = 100.0, mx[1] = mx[2] =  (65535.0 * 255.0)/65280.0 - 128.0;
				}
				otoentry(omin, mn);/* Convert from output color space to table representation */
				otoentry(omax, mx);
			} else {
				for (e = 0; e < p->inputChan; e++) {
					omin[e] = 0.0;
					omax[e] = 1.0;
				}
			}
		} else
#endif
		{
			for (f = 0; f < p->outputChan; f++) {
				omin[f] = 0.0;		/* We are assuming this is true for all other color spaces. */
				omax[f] = 1.0;
			}
		}
	} else {
		otoentry(omin, clutmin);/* Convert from output color space to table representation */
		otoentry(omax, clutmax);
	}

	/* Create the input table entry values */
	for (tn = 0; tn < ntables; tn++) {
		pn = pp[tn];
		for (n = 0; n < pn->inputEnt; n++) {
			double fv;
			fv = n/(pn->inputEnt-1.0);
			for (e = 0; e < pn->inputChan; e++)
				iv[e] = fv;

			ifromindex(iv,iv);			/* Convert from index value to input color space value */

			if (infunc != NULL)
				infunc(cbctx, iv, iv);	/* In colorspace -> input table -> In colorspace. */

			itoentry(iv,iv);			/* Convert from input color space value to table value */

			/* Expand used range to 0.0 - 1.0, and clip to legal values */
			/* Note that if the range is reduced, and clipping occurs, */
			/* then there should be enough resolution within the input */
			/* table, to represent the sharp edges of the clipping. */
			for (e = 0; e < pn->inputChan; e++) {
				double tt;
				tt = (iv[e] - imin[e])/(imax[e] - imin[e]);
				if (tt < 0.0) {
					DBGSLC(("iclip: tt = %f, iv = %f, omin = %f, omax = %f\n",tt,iv[e],omin[e],omax[e]));
					if (tt < -CLIP_MARGIN)
						clip = 1;
					tt = 0.0;
				} else if (tt > 1.0) {
					DBGSLC(("iclip: tt = %f, iv = %f, omin = %f, omax = %f\n",tt,iv[e],omin[e],omax[e]));
					if (tt > (1.0 + CLIP_MARGIN))
						clip = 1;
					tt = 1.0;
				}
				iv[e] = tt;
			}

			for (e = 0; e < pn->inputChan; e++) 		/* Input tables */
				pn->inputTable[e * pn->inputEnt + n] = iv[e];
		}
	}

	/* Allocate space for cell center value lookup */
	if (flags & ICM_CLUT_SET_APXLS) {
		if (apxls_gmin == NULL) {
			apxls_gmin = def_apxls_gmin;
			for (e = 0; e < p->inputChan; e++)
				apxls_gmin[e] = 0;
		}
		if (apxls_gmax == NULL) {
			apxls_gmax = def_apxls_gmax;
			for (e = 0; e < p->inputChan; e++)
				apxls_gmax[e] = p->clutPoints-1;
		}

		if ((clutTable2 = (double **) icp->al->calloc(icp->al,sizeof(double *), ntables)) == NULL) {
			sprintf(icp->err,"icmLut_set_tables malloc of cube center array failed");
			icp->al->free(icp->al, _iv);
			return icp->errc = 1;
		}
		for (tn = 0; tn < ntables; tn++) {
			if ((clutTable2[tn] = (double *) icp->al->calloc(icp->al,sizeof(double),
			                                               p->clutTable_size)) == NULL) {
				for (--tn; tn >= 0; tn--)
					icp->al->free(icp->al, clutTable2[tn]);
				icp->al->free(icp->al, _iv);
				icp->al->free(icp->al, clutTable2);
				sprintf(icp->err,"icmLut_set_tables malloc of cube center array failed");
				return icp->errc = 1;
			}
		}
	}

	/* Allocate space for smoothing radius values */
	if (flags & ICM_CLUT_SET_FILTER) {
		unsigned int j, g, size;

		/* Private: compute dimensional increment though clut3 */
		i = p->inputChan-1;
		dinc3[i--] = ntables;
		for (; i < p->inputChan; i--)
			dinc3[i] = dinc3[i+1] * p->clutPoints;
	
		/* Private: compute offsets from base of cube to other corners */
		for (dcube3[0] = 0, g = 1, j = 0; j < p->inputChan; j++) {
			for (i = 0; i < g; i++)
				dcube3[g+i] = dcube3[i] + dinc3[j];
			g *= 2;
		}

		if ((size = sat_mul(ntables, sat_pow(p->clutPoints,p->inputChan))) == UINT_MAX) {
			sprintf(icp->err,"icmLut_alloc size overflow");
			if (flags & ICM_CLUT_SET_APXLS) {
				for (tn = 0; tn < ntables; tn++)
					icp->al->free(icp->al, clutTable2[tn]);
			}
			icp->al->free(icp->al, clutTable2);
			icp->al->free(icp->al, _iv);
			return icp->errc = 1;
		}

		if ((clutTable3 = (double *) icp->al->calloc(icp->al,sizeof(double),
		                                               size)) == NULL) {
			if (flags & ICM_CLUT_SET_APXLS) {
				for (tn = 0; tn < ntables; tn++)
					icp->al->free(icp->al, clutTable2[tn]);
			}
			icp->al->free(icp->al, clutTable2);
			icp->al->free(icp->al, _iv);
			sprintf(icp->err,"icmLut_set_tables malloc of vertex smoothing value array failed");
			return icp->errc = 1;
		}
	}

	/* Create the multi-dimensional lookup table values */

	/* To make this clut function cache friendly, we use the pseudo-hilbert */
	/* count sequence. This keeps each point close to the last in the */
	/* multi-dimensional space. This is the point of setting multiple Luts at */ 
	/* once too - the assumption is that these tables are all related (different */
	/* gamut compressions for instance), and hence calling the clutfunc() with */
	/* close values will maximise reverse lookup cache hit rate. */

	psh_init(&counter, p->inputChan, p->clutPoints, ii);	/* Initialise counter */

	/* Itterate through all verticies in the grid */
	for (;;) {
		int ti, ti3;		/* Table indexes */
	
		for (ti = e = 0; e < p->inputChan; e++) { 	/* Input tables */
			ti += ii[e] * p->dinc[e];				/* Clut index */
			iv[e] = ii[e]/(p->clutPoints-1.0);		/* Vertex coordinates */
			iv[e] = iv[e] * (imax[e] - imin[e]) + imin[e]; /* Undo expansion to 0.0 - 1.0 */
			*((int *)&iv[-((int)e)-1]) = ii[e];		/* Trick to supply grid index in iv[] */
		}
	
		if (flags & ICM_CLUT_SET_FILTER) {
			for (ti3 = e = 0; e < p->inputChan; e++) 	/* Input tables */
				ti3 += ii[e] * dinc3[e];				/* Clut3 index */
		}
	
		DBGSL(("\nix %s\n",icmPiv(p->inputChan, ii)));
		DBGSL(("raw itv %s to iv'",icmPdv(p->inputChan, iv)));
		ifromentry(iv,iv);			/* Convert from table value to input color space */
		DBGSL((" %s\n",icmPdv(p->inputChan, iv)));
	
		/* Apply incolor -> outcolor function we want to represent for all tables */
		DBGSL(("iv: %s to ov'",icmPdv(p->inputChan, iv)));
		clutfunc(cbctx, iv, iv);
		DBGSL((" %s\n",icmPdv(p->outputChan, iv)));
	
		/* Save the results to the output tables */
		for (tn = 0, ivn = iv; tn < ntables; ivn += p->outputChan, tn++) {
			pn = pp[tn];
		
			DBGSL(("tn %d, ov' %s -> otv",tn,icmPdv(p->outputChan, ivn)));
			otoentry(ivn,ivn);			/* Convert from output color space value to table value */
			DBGSL((" %s\n  -> oval",icmPdv(p->outputChan, ivn)));
	
			/* Expand used range to 0.0 - 1.0, and clip to legal values */
			for (f = 0; f < pn->outputChan; f++) {
				double tt;
				tt = (ivn[f] - omin[f])/(omax[f] - omin[f]);
				if (tt < 0.0) {
					DBGSLC(("lclip: tt = %f, ivn= %f, omin = %f, omax = %f\n",tt,ivn[f],omin[f],omax[f]));
					if (tt < -CLIP_MARGIN)
						clip = 2;
					tt = 0.0;
				} else if (tt > 1.0) {
					DBGSLC(("lclip: tt = %f, ivn= %f, omin = %f, omax = %f\n",tt,ivn[f],omin[f],omax[f]));
					if (tt > (1.0 + CLIP_MARGIN))
						clip = 2;
					tt = 1.0;
				}
				ivn[f] = tt;
			}
		
			for (f = 0; f < pn->outputChan; f++) 	/* Output chans */
				pn->clutTable[ti + f] = ivn[f];
			DBGSL((" %s\n",icmPdv(pn->outputChan, ivn)));

			if (flags & ICM_CLUT_SET_FILTER) {
				clutTable3[ti3 + tn] = iv[-1 -tn];	/* Filter radiuses */
			}
		}
	
		/* Lookup cell center value if ICM_CLUT_SET_APXLS */
		if (clutTable2 != NULL) {

			for (e = 0; e < p->inputChan; e++) {
				if (ii[e] < apxls_gmin[e]
				 || ii[e] >= apxls_gmax[e])
					break;							/* Don't lookup outside least squares area */
				iv[e] = (ii[e] + 0.5)/(p->clutPoints-1.0);		/* Vertex coordinates + 0.5 */
				iv[e] = iv[e] * (imax[e] - imin[e]) + imin[e]; /* Undo expansion to 0.0 - 1.0 */
				*((int *)&iv[-((int)e)-1]) = -ii[e]-1;	/* Trick to supply -ve grid index in iv[] */
											    /* (Not this is only the base for +0.5 center) */
			}

			if (e >= p->inputChan) {	/* We're not on the last row */
		
				ifromentry(iv,iv);			/* Convert from table value to input color space */
			
				/* Apply incolor -> outcolor function we want to represent */
				clutfunc(cbctx, iv, iv);
			
				/* Save the results to the output tables */
				for (tn = 0, ivn = iv; tn < ntables; ivn += p->outputChan, tn++) {
					pn = pp[tn];
				
					otoentry(ivn,ivn);			/* Convert from output color space value to table value */
			
					/* Expand used range to 0.0 - 1.0, and clip to legal values */
					for (f = 0; f < pn->outputChan; f++) {
						double tt;
						tt = (ivn[f] - omin[f])/(omax[f] - omin[f]);
						if (tt < 0.0) {
							DBGSLC(("lclip: tt = %f, ivn= %f, omin = %f, omax = %f\n",tt,ivn[f],omin[f],omax[f]));
							if (tt < -CLIP_MARGIN)
								clip = 3;
							tt = 0.0;
						} else if (tt > 1.0) {
							DBGSLC(("lclip: tt = %f, ivn= %f, omin = %f, omax = %f\n",tt,ivn[f],omin[f],omax[f]));
							if (tt > (1.0 + CLIP_MARGIN))
								clip = 3;
							tt = 1.0;
						}
						ivn[f] = tt;
					}
				
					for (f = 0; f < pn->outputChan; f++) 	/* Output chans */
						clutTable2[tn][ti + f] = ivn[f];
				}
			}
		}

		/* Increment index within block (Reverse index significancd) */
		if (psh_inc(&counter, ii))
			break;
	}

#define APXLS_WHT 0.5
#define APXLS_DIFF_THRHESH 0.2
	/* Deal with cell center value, aproximate least squares adjustment. */
	/* Subtract some of the mean of the surrounding center values from each grid value. */
	/* Skip the range edges so that things like the white point or Video sync are not changed. */
	/* Avoid modifying the value if the difference between the */
	/* interpolated value and the current value is too great, */
	/* and there is the possibility of different color aliases. */
	if (clutTable2 != NULL) {
		int ti;				/* cube vertex table index */
		int ti2;			/* cube center table2 index */
		int ee;
		double cw = 1.0/(double)(1 << p->inputChan);		/* Weight for each cube corner */

		/* For each cell center point except last row because we access ii[e]+1 */  
		for (e = 0; e < p->inputChan; e++)
			ii[e] = apxls_gmin[e];	/* init coords */

		/* Compute linear interpolated value from center values */
		for (ee = 0; ee < p->inputChan;) {

			/* Compute base index for table2 */
			for (ti2 = e = 0; e < p->inputChan; e++)  	/* Input tables */
				ti2 += ii[e] * p->dinc[e];				/* Clut index */

			ti = ti2 + p->dcube[(1 << p->inputChan)-1];	/* +1 to each coord for vertex index */

			for (tn = 0; tn < ntables; tn++) {
				double mval[MAX_CHAN], vv;
				double maxd = 0.0;

				pn = pp[tn];
			
				/* Compute mean of center values */
				for (f = 0; f < pn->outputChan; f++) { 	/* Output chans */

					mval[f] = 0.0;
					for (i = 0; i < (1 << p->inputChan); i++) { /* For surrounding center values */
						mval[f] += clutTable2[tn][ti2 + p->dcube[i] + f];
					}
					mval[f] = pn->clutTable[ti + f] - mval[f] * cw;		/* Diff to mean */
					vv = fabs(mval[f]);
					if (vv > maxd)
						maxd = vv;
				}
			
				if (pn->outputChan <= 3 || maxd < APXLS_DIFF_THRHESH) {
					for (f = 0; f < pn->outputChan; f++) { 	/* Output chans */
				
						vv = pn->clutTable[ti + f] + APXLS_WHT * mval[f];
	
						/* Hmm. This is a bit crude. How do we know valid range is 0-1 ? */
						/* What about an ink limit ? */
						if (vv < 0.0) {
							vv = 0.0;
						} else if (vv > 1.0) {
							vv = 1.0;
						}
						pn->clutTable[ti + f] = vv;
					}
					DBGSL(("nix %s apxls ov %s\n",icmPiv(p->inputChan, ii), icmPdv(pn->outputChan, ivn)));
				}
			}

			/* Increment coord */
			for (ee = 0; ee < p->inputChan; ee++) {
				if (++ii[ee] < (apxls_gmax[ee]-1))		/* Stop short of upper row of clutTable2 */
					break;	/* No carry */
				ii[ee] = apxls_gmin[ee];
			}
		}

		/* Done with center values */
		for (tn = 0; tn < ntables; tn++)
			icp->al->free(icp->al, clutTable2[tn]);
		icp->al->free(icp->al, clutTable2);
	}

	/* Apply any smoothing in the clipped region to the resulting clutTable */
	/* !!! should avoid smoothing outside apxls_gmin[e] & apxls_gmax[e] region !!! */
	if (clutTable3 != NULL) {
		double *clutTable1;		/* Copy of current unfilted values */
		FCOUNT(cc, MAX_CHAN, p->inputChan);   /* Surrounding counter */
		
		if ((clutTable1 = (double *) icp->al->calloc(icp->al,sizeof(double),
		                                               p->clutTable_size)) == NULL) {
			icp->al->free(icp->al, clutTable3);
			icp->al->free(icp->al, _iv);
			sprintf(icp->err,"icmLut_set_tables malloc of grid copy failed");
			return icp->errc = 1;
		}

		for (tn = 0; tn < ntables; tn++) {
			int aa;
			int ee;
			int ti, ti3;		/* Table indexes */

			pn = pp[tn];

			/* For each pass */
			for (aa = 0; aa < 2; aa++) {
	
				/* Copy current values */
				memcpy(clutTable1, pn->clutTable, sizeof(double) * pn->clutTable_size);
	
				/* Filter each point */
				for (e = 0; e < pn->inputChan; e++)
					ii[e] = 0;	/* init coords */
	
				/* Compute linear interpolated error to actual cell center value */
				for (ee = 0; ee < pn->inputChan;) {
					double rr;		/* Filter radius */
					int ir;			/* Integer radius */
					double tw;		/* Total weight */
	
					/* Compute base index for this cell */
					for (ti3 = ti = e = 0; e < pn->inputChan; e++) {  	/* Input tables */
						ti += ii[e] * pn->dinc[e];				/* Clut index */
						ti3 += ii[e] * dinc3[e];				/* Clut3 index */
					}
					rr = clutTable3[ti3 + tn] * (pn->clutPoints-1.0);
					ir = (int)floor(rr + 0.5);			/* Don't bother unless 1/2 over vertex */
	
					if (ir < 1)
						goto next_vert;
	
					//FRECONF(cc, -ir, ir + 1);		/* Set size of surroundign grid */
	
					/* Clip scanning cube to be within grid */
					for (e = 0; e < pn->inputChan; e++) {
						int cr = ir;
						if ((ii[e] - ir) < 0)
							cr = ii[e];
						if ((ii[e] + ir) >= pn->clutPoints)
							cr = pn->clutPoints -1 -ii[e];
	
						cc_stt[e] = -cr;
						cc_res[e] = cr + 1;
					}
	
					for (f = 0; f < pn->outputChan; f++)
						pn->clutTable[ti + f] = 0.0;
					tw = 0.0;
	
					FC_INIT(cc)
					for (tw = 0.0; !FC_DONE(cc);) {
						double r;
						int tti;
	
						/* Radius of this cell */
						for (r = 0.0, tti = e = 0; e < pn->inputChan; e++) {
							int ix;
							r += cc[e] * cc[e];
							tti += (ii[e] + cc[e]) * p->dinc[e];
						}
						r = sqrt(r);
	
						if (r <= rr && e >= pn->inputChan) {
							double w = (rr - r)/rr;		/* Triangle weighting */
							w = sqrt(w);
							for (f = 0; f < pn->outputChan; f++) 
								pn->clutTable[ti+f] += w * clutTable1[tti + f];
							tw += w;
						}
						FC_INC(cc);
					}
					for (f = 0; f < pn->outputChan; f++) { 
						double vv = pn->clutTable[ti+f] / tw;
						if (vv < 0.0) {
							vv = 0.0;
						} else if (vv > 1.0) {
							vv = 1.0;
						}
						pn->clutTable[ti+f] = vv;
					}
	
					/* Increment coord */
				next_vert:;
					for (ee = 0; ee < pn->inputChan; ee++) {
						if (++ii[ee] < (pn->clutPoints-1))		/* Don't go through upper edge */
							break;	/* No carry */
						ii[ee] = 0;
					}
				}	/* Next grid point to filter */
			}	/* Next pass */
		}	/* Next table */

		icp->al->free(icp->al, clutTable1);
		icp->al->free(icp->al, clutTable3);
	}

	/* Create the 1D output table entry values */
	for (tn = 0; tn < ntables; tn++) {
		pn = pp[tn];
		for (n = 0; n < pn->outputEnt; n++) {
			double fv;
			fv = n/(pn->outputEnt-1.0);
			for (f = 0; f < pn->outputChan; f++)
				iv[f] = fv;

			/* Undo expansion to 0.0 - 1.0 */
			for (f = 0; f < pn->outputChan; f++) 		/* Output tables */
				iv[f] = iv[f] * (omax[f] - omin[f]) + omin[f];

			ofromentry(iv,iv);			/* Convert from table value to output color space value */

			if (outfunc != NULL)
				outfunc(cbctx, iv, iv);	/* Out colorspace -> output table -> out colorspace. */

			otoentry(iv,iv);			/* Convert from output color space value to table value */

			/* Clip to legal values */
			for (f = 0; f < pn->outputChan; f++) {
				double tt;
				tt = iv[f];
				if (tt < 0.0) {
					DBGSLC(("oclip: tt = %f\n",tt));
					if (tt < -CLIP_MARGIN)
						clip = 5;
					tt = 0.0;
				} else if (tt > 1.0) {
					DBGSLC(("oclip: tt = %f\n",tt));
					if (tt > (1.0 + CLIP_MARGIN))
						clip = 5;
					tt = 1.0;
				}
				iv[f] = tt;
			}

			for (f = 0; f < pn->outputChan; f++) 		/* Input tables */
				pn->outputTable[f * pn->outputEnt + n] = iv[f];
		}
	}

	icp->al->free(icp->al, _iv);

	icp->warnc = 0;
	if (clip) {
		DBGSLC(("Returning clip status = %d\n",clip));
		icp->warnc = clip;
	}
	
	return 0;
}

/* Helper function to initialize a Lut tables contents */
/* from supplied transfer functions. */
/* Set errc and return error number */
/* Set warnc if there is clipping in the output values */
static int icmLut_set_tables (
icmLut *p,							/* Pointer to Lut object */
int     flags,						/* Setting flags */
void   *cbctx,						/* Opaque callback context pointer value */
icColorSpaceSignature insig, 		/* Input color space */
icColorSpaceSignature outsig, 		/* Output color space */
void (*infunc)(void *cbcntx, double *out, double *in),
								/* Input transfer function, inspace->inspace' (NULL = default) */
double *inmin, double *inmax,		/* Maximum range of inspace' values (NULL = default) */
void (*clutfunc)(void *cbctx, double *out, double *in),
								/* inspace' -> outspace' transfer function */
double *clutmin, double *clutmax,	/* Maximum range of outspace' values (NULL = default) */
void (*outfunc)(void *cbctx, double *out, double *in),
									/* Output transfer function, outspace'->outspace (NULL = deflt) */
int *apxls_gmin, int *apxls_gmax	/* If not NULL, the grid indexes not to be affected */
									/* by ICM_CLUT_SET_APXLS, defaulting to 0..>clutPoints-1 */
) {
	struct _icmLut *pp[3];
	
	/* Simply call the multiple table function with one table */
	pp[0] = p;
	return icmSetMultiLutTables(1, pp, flags, 
	                            cbctx, insig, outsig,
	                            infunc,
	                            inmin, inmax,
	                            clutfunc,
	                            clutmin, clutmax,
	                            outfunc,
	                            apxls_gmin, apxls_gmax);
}

/* - - - - - - - - - - - - - - - - */
/* Return the number of bytes needed to write this tag */
static unsigned int icmLut_get_size(
	icmBase *pp
) {
	icmLut *p = (icmLut *)pp;
	unsigned int len = 0;

	if (p->ttype == icSigLut8Type) {
		len = sat_add(len, 48);			/* tag and header */
		len = sat_add(len, sat_mul3(1, p->inputChan, p->inputEnt));
		len = sat_add(len, sat_mul3(1, p->outputChan, sat_pow(p->clutPoints,p->inputChan)));
		len = sat_add(len, sat_mul3(1, p->outputChan, p->outputEnt));
	} else {
		len = sat_add(len, 52);			/* tag and header */
		len = sat_add(len, sat_mul3(2, p->inputChan, p->inputEnt));
		len = sat_add(len, sat_mul3(2, p->outputChan, sat_pow(p->clutPoints,p->inputChan)));
		len = sat_add(len, sat_mul3(2, p->outputChan, p->outputEnt));
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmLut_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmLut *p = (icmLut *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i, j, g, size;
	char *bp, *buf;

	if (len < 4) {
		sprintf(icp->err,"icmLut_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmLut_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmLut_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	p->ttype = (icTagTypeSignature)read_SInt32Number(bp);
	if (p->ttype != icSigLut8Type && p->ttype != icSigLut16Type) {
		sprintf(icp->err,"icmLut_read: Wrong tag type for icmLut");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	if (p->ttype == icSigLut8Type) {
		if (len < 48) {
			sprintf(icp->err,"icmLut_read: Tag too small to be legal");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
	} else {
		if (len < 52) {
			sprintf(icp->err,"icmLut_read: Tag too small to be legal");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
	}

	/* Read in the info common to 8 and 16 bit Lut */
	p->inputChan = read_UInt8Number(bp+8);
	p->outputChan = read_UInt8Number(bp+9);
	p->clutPoints = read_UInt8Number(bp+10);

	if (icp->allowclutPoints256 && p->clutPoints == 0)		/* Special case */
		p->clutPoints = 256;

	/* Read 3x3 transform matrix */
	for (j = 0; j < 3; j++) {		/* Rows */
		for (i = 0; i < 3; i++) {	/* Columns */
			p->e[j][i] = read_S15Fixed16Number(bp + 12 + ((j * 3 + i) * 4));
		}
	}
	/* Read 16 bit specific stuff */
	if (p->ttype == icSigLut8Type) {
		p->inputEnt  = 256;	/* By definition */
		p->outputEnt = 256;	/* By definition */
		bp = buf+48;
	} else {
		p->inputEnt  = read_UInt16Number(bp+48);
		p->outputEnt = read_UInt16Number(bp+50);
		bp = buf+52;
	}

	/* Sanity check tag size. This protects against */
	/* subsequent integer overflows involving the dimensions. */
	if ((size = icmLut_get_size((icmBase *)p)) == UINT_MAX
	 || size > len) {
		sprintf(icp->err,"icmLut_read: Tag wrong size for contents");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Sanity check the dimensions and resolution values agains limits, */
	/* allocate space for them and generate internal offset tables. */
	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read the input tables */
	size = (p->inputChan * p->inputEnt);
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1)
			p->inputTable[i] = read_DCS8Number(bp);
	} else {
		for (i = 0; i < size; i++, bp += 2)
			p->inputTable[i] = read_DCS16Number(bp);
	}

	/* Read the clut table */
	size = (p->outputChan * sat_pow(p->clutPoints,p->inputChan));
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1)
			p->clutTable[i] = read_DCS8Number(bp);
	} else {
		for (i = 0; i < size; i++, bp += 2)
			p->clutTable[i] = read_DCS16Number(bp);
	}

	/* Read the output tables */
	size = (p->outputChan * p->outputEnt);
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1)
			p->outputTable[i] = read_DCS8Number(bp);
	} else {
		for (i = 0; i < size; i++, bp += 2)
			p->outputTable[i] = read_DCS16Number(bp);
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmLut_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmLut *p = (icmLut *)pp;
	icc *icp = p->icp;
	unsigned int i,j;
	unsigned int len, size;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmLut_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmLut_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmLut_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write the info common to 8 and 16 bit Lut */
	if ((rv = write_UInt8Number(p->inputChan, bp+8)) != 0) {
		sprintf(icp->err,"icmLut_write: write_UInt8Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_UInt8Number(p->outputChan, bp+9)) != 0) {
		sprintf(icp->err,"icmLut_write: write_UInt8Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	
	if (icp->allowclutPoints256 && p->clutPoints == 256) {
		if ((rv = write_UInt8Number(0, bp+10)) != 0) {
			sprintf(icp->err,"icmLut_write: write_UInt8Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	} else {
		if ((rv = write_UInt8Number(p->clutPoints, bp+10)) != 0) {
			sprintf(icp->err,"icmLut_write: write_UInt8Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	write_UInt8Number(0, bp+11);		/* Set padding to 0 */

	/* Write 3x3 transform matrix */
	for (j = 0; j < 3; j++) {		/* Rows */
		for (i = 0; i < 3; i++) {	/* Columns */
			if ((rv = write_S15Fixed16Number(p->e[j][i],bp + 12 + ((j * 3 + i) * 4))) != 0) {
				sprintf(icp->err,"icmLut_write: write_S15Fixed16Number() failed");
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	}

	/* Write 16 bit specific stuff */
	if (p->ttype == icSigLut8Type) {
		if (p->inputEnt != 256 || p->outputEnt != 256) {
			sprintf(icp->err,"icmLut_write: 8 bit Input and Output tables must be 256 entries");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		bp = buf+48;
	} else {
		if (p->inputEnt > 4096 || p->outputEnt > 4096) {
			sprintf(icp->err,"icmLut_write: 16 bit Input and Output tables must each be less than 4096 entries");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		if ((rv = write_UInt16Number(p->inputEnt, bp+48)) != 0) {
			sprintf(icp->err,"icmLut_write: write_UInt16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_UInt16Number(p->outputEnt, bp+50)) != 0) {
			sprintf(icp->err,"icmLut_write: write_UInt16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		bp = buf+52;
	}

	/* Write the input tables */
	size = (p->inputChan * p->inputEnt);
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1) {
			if ((rv = write_DCS8Number(p->inputTable[i], bp)) != 0) {
				sprintf(icp->err,"icmLut_write: inputTable write_DCS8Number() failed");
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	} else {
		for (i = 0; i < size; i++, bp += 2) {
			if ((rv = write_DCS16Number(p->inputTable[i], bp)) != 0) {
				sprintf(icp->err,"icmLut_write: inputTable write_DCS16Number(%.8f) failed",p->inputTable[i]);
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	}

	/* Write the clut table */
	size = (p->outputChan * sat_pow(p->clutPoints,p->inputChan));
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1) {
			if ((rv = write_DCS8Number(p->clutTable[i], bp)) != 0) {
				sprintf(icp->err,"icmLut_write: clutTable write_DCS8Number() failed");
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	} else {
		for (i = 0; i < size; i++, bp += 2) {
			if ((rv = write_DCS16Number(p->clutTable[i], bp)) != 0) {
				sprintf(icp->err,"icmLut_write: clutTable write_DCS16Number(%.8f) failed",p->clutTable[i]);
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	}

	/* Write the output tables */
	size = (p->outputChan * p->outputEnt);
	if (p->ttype == icSigLut8Type) {
		for (i = 0; i < size; i++, bp += 1) {
			if ((rv = write_DCS8Number(p->outputTable[i], bp)) != 0) {
				sprintf(icp->err,"icmLut_write: outputTable write_DCS8Number() failed");
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	} else {
		for (i = 0; i < size; i++, bp += 2) {
			if ((rv = write_DCS16Number(p->outputTable[i], bp)) != 0) {
				sprintf(icp->err,"icmLut_write: outputTable write_DCS16Number(%.8f) failed",p->outputTable[i]);
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	}

	/* Write buffer to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmLut_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmLut_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmLut *p = (icmLut *)pp;
	if (verb <= 0)
		return;

	if (p->ttype == icSigLut8Type) {
		op->gprintf(op,"Lut8:\n");
	} else {
		op->gprintf(op,"Lut16:\n");
	}
	op->gprintf(op,"  Input Channels = %u\n",p->inputChan);
	op->gprintf(op,"  Output Channels = %u\n",p->outputChan);
	op->gprintf(op,"  CLUT resolution = %u\n",p->clutPoints);
	op->gprintf(op,"  Input Table entries = %u\n",p->inputEnt);
	op->gprintf(op,"  Output Table entries = %u\n",p->outputEnt);
	op->gprintf(op,"  XYZ matrix =  %.8f, %.8f, %.8f\n",p->e[0][0],p->e[0][1],p->e[0][2]);
	op->gprintf(op,"                %.8f, %.8f, %.8f\n",p->e[1][0],p->e[1][1],p->e[1][2]);
	op->gprintf(op,"                %.8f, %.8f, %.8f\n",p->e[2][0],p->e[2][1],p->e[2][2]);

	if (verb >= 2) {
		unsigned int i, j, size;
		unsigned int ii[MAX_CHAN];	/* maximum no of input channels */

		op->gprintf(op,"  Input table:\n");
		for (i = 0; i < p->inputEnt; i++) {
			op->gprintf(op,"    %3u: ",i);
			for (j = 0; j < p->inputChan; j++)
				op->gprintf(op," %1.10f",p->inputTable[j * p->inputEnt + i]);
			op->gprintf(op,"\n");
		}

		op->gprintf(op,"\n  CLUT table:\n");
		if (p->inputChan > MAX_CHAN) {
			op->gprintf(op,"  !!Can't dump > %d input channel CLUT table!!\n",MAX_CHAN);
		} else {
			size = (p->outputChan * sat_pow(p->clutPoints,p->inputChan));
			for (j = 0; j < p->inputChan; j++)
				ii[j] = 0;
			for (i = 0; i < size;) {
				unsigned int k;
				/* Print table entry index */
				op->gprintf(op,"   ");
				for (j = p->inputChan-1; j < p->inputChan; j--)
					op->gprintf(op," %2u",ii[j]);
				op->gprintf(op,":");
				/* Print table entry contents */
				for (k = 0; k < p->outputChan; k++, i++)
					op->gprintf(op," %1.10f",p->clutTable[i]);
				op->gprintf(op,"\n");
			
				for (j = 0; j < p->inputChan; j++) { /* Increment index */
					ii[j]++;
					if (ii[j] < p->clutPoints)
						break;	/* No carry */
					ii[j] = 0;
				}
			}
		}

		op->gprintf(op,"\n  Output table:\n");
		for (i = 0; i < p->outputEnt; i++) {
			op->gprintf(op,"    %3u: ",i);
			for (j = 0; j < p->outputChan; j++)
				op->gprintf(op," %1.10f",p->outputTable[j * p->outputEnt + i]);
			op->gprintf(op,"\n");
		}

	}
}

/* Sanity check the input & output dimensions, and */
/* allocate variable sized data elements, and */
/* generate internal dimension offset tables */
static int icmLut_allocate(
	icmBase *pp
) {
	unsigned int i, j, g, size;
	icmLut *p = (icmLut *)pp;
	icc *icp = p->icp;

	/* Sanity check, so that dinc[] comp. won't fail */
	if (p->inputChan < 1) {
		sprintf(icp->err,"icmLut_alloc: Can't handle %d input channels\n",p->inputChan);
		return icp->errc = 1;
	}

	if (p->inputChan > MAX_CHAN) {
		sprintf(icp->err,"icmLut_alloc: Can't handle > %d input channels\n",MAX_CHAN);
		return icp->errc = 1;
	}

	if (p->outputChan > MAX_CHAN) {
		sprintf(icp->err,"icmLut_alloc: Can't handle > %d output channels\n",MAX_CHAN);
		return icp->errc = 1;
	}

	if ((size = sat_mul(p->inputChan, p->inputEnt)) == UINT_MAX) {
		sprintf(icp->err,"icmLut_alloc size overflow");
		return icp->errc = 1;
	}
	if (size != p->inputTable_size) {
		if (ovr_mul(size, sizeof(double))) {
			sprintf(icp->err,"icmLut_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->inputTable != NULL)
			icp->al->free(icp->al, p->inputTable);
		if ((p->inputTable = (double *) icp->al->calloc(icp->al,size, sizeof(double))) == NULL) {
			sprintf(icp->err,"icmLut_alloc: calloc() of Lut inputTable data failed");
			return icp->errc = 2;
		}
		p->inputTable_size = size;
	}
	if ((size = sat_mul(p->outputChan, sat_pow(p->clutPoints,p->inputChan))) == UINT_MAX) {
		sprintf(icp->err,"icmLut_alloc size overflow");
		return icp->errc = 1;
	}
	if (size != p->clutTable_size) {
		if (ovr_mul(size, sizeof(double))) {
			sprintf(icp->err,"icmLut_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->clutTable != NULL)
			icp->al->free(icp->al, p->clutTable);
		if ((p->clutTable = (double *) icp->al->calloc(icp->al,size, sizeof(double))) == NULL) {
			sprintf(icp->err,"icmLut_alloc: calloc() of Lut clutTable data failed");
			return icp->errc = 2;
		}
		p->clutTable_size = size;
	}
	if ((size = sat_mul(p->outputChan, p->outputEnt)) == UINT_MAX) {
		sprintf(icp->err,"icmLut_alloc size overflow");
		return icp->errc = 1;
	}
	if (size != p->outputTable_size) {
		if (ovr_mul(size, sizeof(double))) {
			sprintf(icp->err,"icmLut_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->outputTable != NULL)
			icp->al->free(icp->al, p->outputTable);
		if ((p->outputTable = (double *) icp->al->calloc(icp->al,size, sizeof(double))) == NULL) {
			sprintf(icp->err,"icmLut_alloc: calloc() of Lut outputTable data failed");
			return icp->errc = 2;
		}
		p->outputTable_size = size;
	}

	/* Private: compute dimensional increment though clut */
	/* Note that first channel varies least rapidly. */
	i = p->inputChan-1;
	p->dinc[i--] = p->outputChan;
	for (; i < p->inputChan; i--)
		p->dinc[i] = p->dinc[i+1] * p->clutPoints;

	/* Private: compute offsets from base of cube to other corners */
	for (p->dcube[0] = 0, g = 1, j = 0; j < p->inputChan; j++) {
		for (i = 0; i < g; i++)
			p->dcube[g+i] = p->dcube[i] + p->dinc[j];
		g *= 2;
	}
	
	return 0;
}

/* Free all storage in the object */
static void icmLut_delete(
	icmBase *pp
) {
	icmLut *p = (icmLut *)pp;
	icc *icp = p->icp;
	int i;

	if (p->inputTable != NULL)
		icp->al->free(icp->al, p->inputTable);
	if (p->clutTable != NULL)
		icp->al->free(icp->al, p->clutTable);
	if (p->outputTable != NULL)
		icp->al->free(icp->al, p->outputTable);
	for (i = 0; i < p->inputChan; i++)
		icmTable_delete_bwd(icp, &p->rit[i]);
	for (i = 0; i < p->outputChan; i++)
		icmTable_delete_bwd(icp, &p->rot[i]);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmLut(
	icc *icp
) {
	int i,j;
	icmLut *p;
	if ((p = (icmLut *) icp->al->calloc(icp->al,1,sizeof(icmLut))) == NULL)
		return NULL;
	p->icp      = icp;

	p->ttype    = icSigLut16Type;
	p->refcount = 1;
	p->get_size = icmLut_get_size;
	p->read     = icmLut_read;
	p->write    = icmLut_write;
	p->dump     = icmLut_dump;
	p->allocate = icmLut_allocate;
	p->del      = icmLut_delete;

	/* Lookup methods */
	p->nu_matrix      = icmLut_nu_matrix;
	p->min_max        = icmLut_min_max;
	p->lookup_matrix  = icmLut_lookup_matrix;
	p->lookup_input   = icmLut_lookup_input;
	p->lookup_clut_nl = icmLut_lookup_clut_nl;
	p->lookup_clut_sx = icmLut_lookup_clut_sx;
	p->lookup_output  = icmLut_lookup_output;

	/* Set method */
	p->set_tables = icmLut_set_tables;
	p->tune_value = icmLut_tune_value_sx;		/* Default to most likely simplex */

	/* Set matrix to reasonable default */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			if (i == j)
				p->e[i][j] = 1.0;
			else
				p->e[i][j] = 0.0;
		}

	/* Init lookups to non-dangerous values */
	for (i = 0; i < MAX_CHAN; i++)
		p->dinc[i] = 0;

	for (i = 0; i < (1 << MAX_CHAN); i++)
		p->dcube[i] = 0;

	for (i = 0; i < MAX_CHAN; i++) {
		p->rit[i].inited = 0;
		p->rot[i].inited = 0;
	}

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* Measurement */

/* Return the number of bytes needed to write this tag */
static unsigned int icmMeasurement_get_size(
	icmBase *pp
) {
	unsigned int len = 0;
	len = sat_add(len, 8);		/* 8 bytes for tag and padding */
	len = sat_add(len, 4);		/* 4 for standard observer */
	len = sat_add(len, 12);		/* 12 for XYZ of measurement backing */
	len = sat_add(len, 4);		/* 4 for measurement geometry */
	len = sat_add(len, 4);		/* 4 for measurement flare */
	len = sat_add(len, 4);		/* 4 for standard illuminant */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmMeasurement_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmMeasurement *p = (icmMeasurement *)pp;
	icc *icp = p->icp;
	int rv;
	char *bp, *buf;

	if (len < 36) {
		sprintf(icp->err,"icmMeasurement_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmMeasurement_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmMeasurement_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmMeasurement_read: Wrong tag type for icmMeasurement");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read the encoded standard observer */
	p->observer = (icStandardObserver)read_SInt32Number(bp + 8);

	/* Read the XYZ values for measurement backing */
	if ((rv = read_XYZNumber(&p->backing, bp+12)) != 0) {
		sprintf(icp->err,"icmMeasurement: read_XYZNumber error");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Read the encoded measurement geometry */
	p->geometry = (icMeasurementGeometry)read_SInt32Number(bp + 24);

	/* Read the proportion of flare  */
	p->flare = read_U16Fixed16Number(bp + 28);

	/* Read the encoded standard illuminant */
	p->illuminant = (icIlluminant)read_SInt32Number(bp + 32);

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmMeasurement_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmMeasurement *p = (icmMeasurement *)pp;
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmMeasurement_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmMeasurement_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmMeasurement_write, type: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write the encoded standard observer */
	if ((rv = write_SInt32Number((int)p->observer, bp + 8)) != 0) {
		sprintf(icp->err,"icmMeasurementa_write, observer: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write the XYZ values for measurement backing */
	if ((rv = write_XYZNumber(&p->backing, bp+12)) != 0) {
		sprintf(icp->err,"icmMeasurement, backing: write_XYZNumber error");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write the encoded measurement geometry */
	if ((rv = write_SInt32Number((int)p->geometry, bp + 24)) != 0) {
		sprintf(icp->err,"icmMeasurementa_write, geometry: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write the proportion of flare */
	if ((rv = write_U16Fixed16Number(p->flare, bp + 28)) != 0) {
		sprintf(icp->err,"icmMeasurementa_write, flare: write_U16Fixed16Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write the encoded standard illuminant */
	if ((rv = write_SInt32Number((int)p->illuminant, bp + 32)) != 0) {
		sprintf(icp->err,"icmMeasurementa_write, illuminant: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmMeasurement_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmMeasurement_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmMeasurement *p = (icmMeasurement *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"Measurement:\n");
	op->gprintf(op,"  Standard Observer = %s\n", string_StandardObserver(p->observer));
	op->gprintf(op,"  XYZ for Measurement Backing = %s\n", string_XYZNumber_and_Lab(&p->backing));
	op->gprintf(op,"  Measurement Geometry = %s\n", string_MeasurementGeometry(p->geometry));
	op->gprintf(op,"  Measurement Flare = %5.1f%%\n", p->flare * 100.0);
	op->gprintf(op,"  Standard Illuminant = %s\n", string_Illuminant(p->illuminant));
}

/* Allocate variable sized data elements */
static int icmMeasurement_allocate(
	icmBase *pp
) {
	/* Nothing to do */
	return 0;
}

/* Free all storage in the object */
static void icmMeasurement_delete(
	icmBase *pp
) {
	icmMeasurement *p = (icmMeasurement *)pp;
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmMeasurement(
	icc *icp
) {
	icmMeasurement *p;
	if ((p = (icmMeasurement *) icp->al->calloc(icp->al,1,sizeof(icmMeasurement))) == NULL)
		return NULL;
	p->ttype    = icSigMeasurementType;
	p->refcount = 1;
	p->get_size = icmMeasurement_get_size;
	p->read     = icmMeasurement_read;
	p->write    = icmMeasurement_write;
	p->dump     = icmMeasurement_dump;
	p->allocate = icmMeasurement_allocate;
	p->del      = icmMeasurement_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Named color structure read/write support */
static int read_NamedColorVal(
	icmNamedColorVal *p,
	char *bp,
	char *end,
	icColorSpaceSignature pcs,		/* Header Profile Connection Space */
	unsigned int ndc				/* Number of device corrds */
) {
	icc *icp = p->icp;
	unsigned int i;
	unsigned int mxl;	/* Max possible string length */
	int rv;

	if (bp > end) {
		sprintf(icp->err,"icmNamedColorVal_read: Data too short to read");
		return icp->errc = 1;
	}
	mxl = (end - bp) < 32 ? (end - bp) : 32;
	if ((rv = check_null_string(bp,mxl)) == 1) {
		sprintf(icp->err,"icmNamedColorVal_read: Root name string not terminated");
		return icp->errc = 1;
	}
	/* Haven't checked if rv == 2 is legal or not */
	strcpy(p->root, bp);
	bp += strlen(p->root) + 1;
	if (bp > end || ndc > (end - bp)) {
		sprintf(icp->err,"icmNamedColorVal_read: Data too short to read device coords");
		return icp->errc = 1;
	}
	for (i = 0; i < ndc; i++) {
		p->deviceCoords[i] = read_DCS8Number(bp);
		bp += 1;
	}
	return 0;
}

static int read_NamedColorVal2(
	icmNamedColorVal *p,
	char *bp,
	char *end,
	icColorSpaceSignature pcs,		/* Header Profile Connection Space */
	unsigned int ndc				/* Number of device coords */
) {
	int rv;
	icc *icp = p->icp;
	unsigned int i;

	if (bp > end
	 || (32 + 6) > (end - bp)
	 || ndc > (end - bp - 32 - 6)/2) {
		sprintf(icp->err,"icmNamedColorVal2_read: Data too short to read");
		return icp->errc = 1;
	}
	if ((rv = check_null_string(bp,32)) == 1) {
		sprintf(icp->err,"icmNamedColorVal2_read: Root name string not terminated");
		return icp->errc = 1;
	}
	memmove((void *)p->root,(void *)(bp + 0),32);
	switch(pcs) {
		case icSigXYZData:
			read_PCSNumber(icp, icSigXYZData, p->pcsCoords, bp+32);
			break;
	   	case icSigLabData:
			/* namedColor2Type retains legacy Lab encoding */
			read_PCSNumber(icp, icmSigLabV2Data, p->pcsCoords, bp+32);
			break;
		default:
			return 1;		/* Unknown PCS */
	}
	for (i = 0; i < ndc; i++)
		p->deviceCoords[i] = read_DCS16Number(bp + 32 + 6 + 2 * i);
	return 0;
}

static int write_NamedColorVal(
	icmNamedColorVal *p,
	char *d,
	icColorSpaceSignature pcs,		/* Header Profile Connection Space */
	unsigned int ndc				/* Number of device corrds */
) {
	icc *icp = p->icp;
	unsigned int i;
	int rv;

	if ((rv = check_null_string(p->root,32)) == 1) {
		sprintf(icp->err,"icmNamedColorVal_write: Root string names is unterminated");
		return icp->errc = 1;
	}
	strcpy(d, p->root);
	d += strlen(p->root) + 1;
	for (i = 0; i < ndc; i++) {
		if ((rv = write_DCS8Number(p->deviceCoords[i], d)) != 0) {
			sprintf(icp->err,"icmNamedColorVal_write: write of device coord failed");
			return icp->errc = 1;
		}
		d += 1;
	}
	return 0;
}

static int write_NamedColorVal2(
	icmNamedColorVal *p,
	char *bp,
	icColorSpaceSignature pcs,		/* Header Profile Connection Space */
	unsigned int ndc				/* Number of device coords */
) {
	icc *icp = p->icp;
	unsigned int i;
	int rv;

	if ((rv = check_null_string(p->root,32)) == 1) {
		sprintf(icp->err,"icmNamedColorVal2_write: Root string names is unterminated");
		return icp->errc = 1;
	}
	rv = 0;
	memmove((void *)(bp + 0),(void *)p->root,32);
	switch(pcs) {
		case icSigXYZData:
			rv |= write_PCSNumber(icp, icSigXYZData, p->pcsCoords, bp+32);
			break;
	   	case icSigLabData:
			/* namedColor2Type retains legacy Lab encoding */
			rv |= write_PCSNumber(icp, icmSigLabV2Data, p->pcsCoords, bp+32);
			break;
		default:
			sprintf(icp->err,"icmNamedColorVal2_write: Unknown PCS");
			return icp->errc = 1;
	}
	if (rv) {
		sprintf(icp->err,"icmNamedColorVal2_write: write of PCS coord failed");
		return icp->errc = 1;
	}
	for (i = 0; i < ndc; i++) {
		if ((rv = write_DCS16Number(p->deviceCoords[i], bp + 32 + 6 + 2 * i)) != 0) {
			sprintf(icp->err,"icmNamedColorVal2_write: write of device coord failed");
			return icp->errc = 1;
		}
	}
	return 0;
}

/* - - - - - - - - - - - */
/* icmNamedColor object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmNamedColor_get_size(
	icmBase *pp
) {
	icmNamedColor *p = (icmNamedColor *)pp;
	unsigned int len = 0;
	if (p->ttype == icSigNamedColorType) {
		unsigned int i;
		len = sat_add(len, 8);			/* 8 bytes for tag and padding */
		len = sat_add(len, 4);			/* 4 for vendor specific flags */
		len = sat_add(len, 4);			/* 4 for count of named colors */
		len = sat_add(len, strlen(p->prefix) + 1); /* prefix of color names */
		len = sat_add(len, strlen(p->suffix) + 1); /* suffix of color names */
		for (i = 0; i < p->count; i++) {
			len = sat_add(len, strlen(p->data[i].root) + 1); /* color names */
			len = sat_add(len, p->nDeviceCoords * 1);	/* bytes for each named color */
		}
	} else {	/* Named Color 2 */
		len = sat_add(len, 8);			/* 8 bytes for tag and padding */
		len = sat_add(len, 4);			/* 4 for vendor specific flags */
		len = sat_add(len, 4);			/* 4 for count of named colors */
		len = sat_add(len, 4);			/* 4 for number of device coords */
		len = sat_add(len, 32);			/* 32 for prefix of color names */
		len = sat_add(len, 32);			/* 32 for suffix of color names */
		len = sat_add(len, sat_mul(p->count, (32 + 6 + p->nDeviceCoords * 2)));
		                                /* bytes for each named color */
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmNamedColor_read(
	icmBase *pp,
	unsigned int len,	/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmNamedColor *p = (icmNamedColor *)pp;
	icc *icp = p->icp;
	unsigned int i;
	char *bp, *buf, *end;
	int rv;

	if (len < 4) {
		sprintf(icp->err,"icmNamedColor_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmNamedColor_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmNamedColor_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	p->ttype = (icTagTypeSignature)read_SInt32Number(bp);
	if (p->ttype != icSigNamedColorType && p->ttype != icSigNamedColor2Type) {
		sprintf(icp->err,"icmNamedColor_read: Wrong tag type for icmNamedColor");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	if (p->ttype == icSigNamedColorType) {
		if (len < 16) {
			sprintf(icp->err,"icmNamedColor_read: Tag too small to be legal");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		/* Make sure that the number of device coords in known */
		p->nDeviceCoords = number_ColorSpaceSignature(icp->header->colorSpace);
		if (p->nDeviceCoords > MAX_CHAN) {
			sprintf(icp->err,"icmNamedColor_read: Can't handle more than %d device channels",MAX_CHAN);
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}

	} else {	/* icmNC2 */
		if (len < 84) {
			sprintf(icp->err,"icmNamedColor_read: Tag too small to be legal");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
	}

	/* Read vendor specific flag */
	p->vendorFlag = read_UInt32Number(bp+8);

	/* Read count of named colors */
	p->count = read_UInt32Number(bp+12);

	if (p->ttype == icSigNamedColorType) {
		unsigned int mxl;	/* Max possible string length */
		bp = bp + 16;

		/* Prefix for each color name */
		if (bp > end) {
			sprintf(icp->err,"icmNamedColor_read: Data too short to read");
			return icp->errc = 1;
		}
		mxl = (end - bp) < 32 ? (end - bp) : 32;
		if ((rv = check_null_string(bp,mxl)) == 1) {
			sprintf(icp->err,"icmNamedColor_read: Color prefix is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		/* Haven't checked if rv == 2 is legal or not */
		strcpy(p->prefix, bp);
		bp += strlen(p->prefix) + 1;
	
		/* Suffix for each color name */
		if (bp > end) {
			sprintf(icp->err,"icmNamedColor_read: Data too short to read");
			return icp->errc = 1;
		}
		mxl = (end - bp) < 32 ? (end - bp) : 32;
		if ((rv = check_null_string(bp,mxl)) == 1) {
			sprintf(icp->err,"icmNamedColor_read: Color suffix is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		/* Haven't checked if rv == 2 is legal or not */
		strcpy(p->suffix, bp);
		bp += strlen(p->suffix) + 1;
	
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	
		/* Read all the data from the buffer */
		for (i = 0; i < p->count; i++) {
			if ((rv = read_NamedColorVal(p->data+i, bp, end, icp->header->pcs, p->nDeviceCoords)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
			bp += strlen(p->data[i].root) + 1;
			bp += p->nDeviceCoords * 1;
		}
	} else {  /* icmNC2 */
		/* Number of device coords per color */
		p->nDeviceCoords = read_UInt32Number(bp+16);
		if (p->nDeviceCoords > MAX_CHAN) {
			sprintf(icp->err,"icmNamedColor_read: Can't handle more than %d device channels",MAX_CHAN);
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
	
		/* Prefix for each color name */
		memmove((void *)p->prefix, (void *)(bp + 20), 32);
		if ((rv = check_null_string(p->prefix,32)) == 1) {
			sprintf(icp->err,"icmNamedColor_read: Color prefix is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
	
		/* Suffix for each color name */
		memmove((void *)p->suffix, (void *)(bp + 52), 32);
		if ((rv = check_null_string(p->suffix,32)) == 1) {
			sprintf(icp->err,"icmNamedColor_read: Color suffix is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
	
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	
		/* Read all the data from the buffer */
		bp = bp + 84;
		for (i = 0; i < p->count; i++) {
			if ((rv = read_NamedColorVal2(p->data+i, bp, end, icp->header->pcs, p->nDeviceCoords)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
			bp += 32 + 6 + p->nDeviceCoords * 2;
		}
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmNamedColor_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmNamedColor *p = (icmNamedColor *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmNamedColor_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmNamedColor_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmNamedColor_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write vendor specific flag */
	if ((rv = write_UInt32Number(p->vendorFlag, bp+8)) != 0) {
		sprintf(icp->err,"icmNamedColor_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write count of named colors */
	if ((rv = write_UInt32Number(p->count, bp+12)) != 0) {
		sprintf(icp->err,"icmNamedColor_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	if (p->ttype == icSigNamedColorType) {
		bp = bp + 16;
	
		/* Prefix for each color name */
		if ((rv = check_null_string(p->prefix,32)) == 1) {
			sprintf(icp->err,"icmNamedColor_write: Color prefix is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		strcpy(bp, p->prefix);
		bp += strlen(p->prefix) + 1;
	
		/* Suffix for each color name */
		if ((rv = check_null_string(p->suffix,32)) == 1) {
			sprintf(icp->err,"icmNamedColor_write: Color sufix is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		strcpy(bp, p->suffix);
		bp += strlen(p->suffix) + 1;
	
		/* Write all the data to the buffer */

		for (i = 0; i < p->count; i++) {
			if ((rv = write_NamedColorVal(p->data+i, bp, icp->header->pcs, p->nDeviceCoords)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
			bp += strlen(p->data[i].root) + 1;
			bp += p->nDeviceCoords * 1;
		}
	} else {	/* icmNC2 */
		/* Number of device coords per color */
		if ((rv = write_UInt32Number(p->nDeviceCoords, bp+16)) != 0) {
			sprintf(icp->err,"icmNamedColor_write: write_UInt32Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	
		/* Prefix for each color name */
		if ((rv = check_null_string(p->prefix,32)) == 1) {
			sprintf(icp->err,"icmNamedColor_write: Color prefix is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		memmove((void *)(bp + 20), (void *)p->prefix, 32);
	
		/* Suffix for each color name */
		if ((rv = check_null_string(p->suffix,32)) == 1) {
			sprintf(icp->err,"icmNamedColor_write: Color sufix is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		memmove((void *)(bp + 52), (void *)p->suffix, 32);
	
		/* Write all the data to the buffer */
		bp = bp + 84;
		for (i = 0; i < p->count; i++, bp += (32 + 6 + p->nDeviceCoords * 2)) {
			if ((rv = write_NamedColorVal2(p->data+i, bp, icp->header->pcs, p->nDeviceCoords)) != 0) {
				icp->al->free(icp->al, buf);
				return rv;
			}
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmNamedColor_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmNamedColor_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmNamedColor *p = (icmNamedColor *)pp;
	icc *icp = p->icp;
	if (verb <= 0)
		return;

	if (p->ttype == icSigNamedColorType)
		op->gprintf(op,"NamedColor:\n");
	else
		op->gprintf(op,"NamedColor2:\n");
	op->gprintf(op,"  Vendor Flag = 0x%x\n",p->vendorFlag);
	op->gprintf(op,"  No. colors  = %u\n",p->count);
	op->gprintf(op,"  No. dev. coords = %u\n",p->nDeviceCoords);
	op->gprintf(op,"  Name prefix = '%s'\n",p->prefix);
	op->gprintf(op,"  Name suffix = '%s'\n",p->suffix);
	if (verb >= 2) {
		unsigned int i, n;
		icmNamedColorVal *vp;
		for (i = 0; i < p->count; i++) {
			vp = p->data + i;
			op->gprintf(op,"    Color %lu:\n",i);
			op->gprintf(op,"      Name root = '%s'\n",vp->root);

			if (p->ttype == icSigNamedColor2Type) {
				switch(icp->header->pcs) {
					case icSigXYZData:
							op->gprintf(op,"      XYZ = %.8f, %.8f, %.8f\n",
							        vp->pcsCoords[0],vp->pcsCoords[1],vp->pcsCoords[2]);
						break;
			    	case icSigLabData:
							op->gprintf(op,"      Lab = %f, %f, %f\n",
							        vp->pcsCoords[0],vp->pcsCoords[1],vp->pcsCoords[2]);
						break;
					default:
							op->gprintf(op,"      Unexpected PCS\n");
						break;
				}
			}
			if (p->nDeviceCoords > 0) {
				op->gprintf(op,"      Device Coords = ");
				for (n = 0; n < p->nDeviceCoords; n++) {
					if (n > 0)
						op->gprintf(op,", ");
					op->gprintf(op,"%.8f",vp->deviceCoords[n]);
				}
				op->gprintf(op,"\n");
			}
		}
	}
}

/* Allocate variable sized data elements */
static int icmNamedColor_allocate(
	icmBase *pp
) {
	icmNamedColor *p = (icmNamedColor *)pp;
	icc *icp = p->icp;

	if (p->count != p->_count) {
		unsigned int i;
		if (ovr_mul(p->count, sizeof(icmNamedColorVal))) {
			sprintf(icp->err,"icmNamedColor_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmNamedColorVal *) icp->al->calloc(icp->al,p->count, sizeof(icmNamedColorVal))) == NULL) {
			sprintf(icp->err,"icmNamedColor_alloc: malloc() of icmNamedColor data failed");
			return icp->errc = 2;
		}
		for (i = 0; i < p->count; i++) {
			p->data[i].icp = icp;	/* Do init */
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmNamedColor_delete(
	icmBase *pp
) {
	icmNamedColor *p = (icmNamedColor *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmNamedColor(
	icc *icp
) {
	icmNamedColor *p;
	if ((p = (icmNamedColor *) icp->al->calloc(icp->al,1,sizeof(icmNamedColor))) == NULL)
		return NULL;
	p->ttype    = icSigNamedColor2Type;
	p->refcount = 1;
	p->get_size = icmNamedColor_get_size;
	p->read     = icmNamedColor_read;
	p->write    = icmNamedColor_write;
	p->dump     = icmNamedColor_dump;
	p->allocate = icmNamedColor_allocate;
	p->del      = icmNamedColor_delete;
	p->icp      = icp;

	/* Default the the number of device coords appropriately for NamedColorType */
	p->nDeviceCoords = number_ColorSpaceSignature(icp->header->colorSpace);

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* Colorant table structure read/write support */
/* (Contribution from Piet Vandenborre) */

static int read_ColorantTableVal(
	icmColorantTableVal *p,
	char *bp,
	char *end,
	icColorSpaceSignature pcs		/* Header Profile Connection Space */
) {
	int rv;
	icc *icp = p->icp;
	if (bp > end || (32 + 6) > (end - bp)) {
		sprintf(icp->err,"icmColorantTableVal_read: Data too short to read");
		return icp->errc = 1;
	}
	if ((rv = check_null_string(bp,32)) == 1) {
		sprintf(icp->err,"icmColorantTableVal_read: Name string not terminated");
		return icp->errc = 1;
	}
	memmove((void *)p->name,(void *)(bp + 0),32);
	switch(pcs) {
		case icSigXYZData:
    	case icSigLabData:
			read_PCSNumber(icp, pcs, p->pcsCoords, bp+32);
			break;
		default:
			return 1;		/* Unknown PCS */
	}
	return 0;
}

static int write_ColorantTableVal(
	icmColorantTableVal *p,
	char *bp,
	icColorSpaceSignature pcs		/* Header Profile Connection Space */
) {
	int rv;
	icc *icp = p->icp;

	if ((rv = check_null_string(p->name,32)) == 1) {
		sprintf(icp->err,"icmColorantTableVal_write: Name string is unterminated");
		return icp->errc = 1;
	}
	memmove((void *)(bp + 0),(void *)p->name,32);
	rv = 0;
	switch(pcs) {
		case icSigXYZData:
    	case icSigLabData:
			rv |= write_PCSNumber(icp, pcs, p->pcsCoords, bp+32);
			break;
		default:
			sprintf(icp->err,"icmColorantTableVal_write: Unknown PCS");
			return icp->errc = 1;
	}
	if (rv) {
		sprintf(icp->err,"icmColorantTableVal_write: write of PCS coord failed");
		return icp->errc = 1;
	}
	return 0;
}

/* - - - - - - - - - - - */
/* icmColorantTable object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmColorantTable_get_size(
	icmBase *pp
) {
	icmColorantTable *p = (icmColorantTable *)pp;
	unsigned int len = 0;
	if (p->ttype == icSigColorantTableType
	 || p->ttype == icmSigAltColorantTableType) {
		unsigned int i;
		len = sat_add(len, 8);			/* 8 bytes for tag and padding */
		len = sat_add(len, 4);			/* 4 for count of colorants */
		for (i = 0; i < p->count; i++) {
			len = sat_add(len, 32); /* colorant names - 32 bytes*/
			len = sat_add(len, 6); /* colorant pcs value - 3 x 16bit number*/
		}
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmColorantTable_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmColorantTable *p = (icmColorantTable *)pp;
	icc *icp = p->icp;
	icColorSpaceSignature pcs;
	unsigned int i;
	char *bp, *buf, *end;
	int rv = 0;

	if (icp->header->deviceClass != icSigLinkClass)
		pcs = icp->header->pcs;
	else
		pcs = icSigLabData;

	if (len < 4) {
		sprintf(icp->err,"icmColorantTable_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmColorantTable_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmColorantTable_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	p->ttype = (icTagTypeSignature)read_SInt32Number(bp);
	if (p->ttype != icSigColorantTableType
	 && p->ttype != icmSigAltColorantTableType) {
		sprintf(icp->err,"icmColorantTable_read: Wrong tag type for icmColorantTable");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	if (len < 12) {
		sprintf(icp->err,"icmColorantTable_read: Tag too small to be legal");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read count of colorants */
	if (p->ttype == icmSigAltColorantTableType)
		p->count = read_UInt8Number(bp+8);		/* Hmm. This is Little Endian */
	else
		p->count = read_UInt32Number(bp+8);

	if (p->count > ((len - 12) / (32 + 6))) {
		sprintf(icp->err,"icmColorantTable_read count overflow, count %x, len %d",p->count,len);
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	bp = bp + 12;

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read all the data from the buffer */
	for (i = 0; i < p->count; i++, bp += (32 + 6)) {
		if (p->ttype == icmSigAltColorantTableType	/* Hack to reverse little endian */
		 && (end - bp) >= 38) {
			int tt;
			tt = *(bp + 32);
			*(bp+32) = *(bp+33);
			*(bp+33) = tt;
			tt = *(bp + 34);
			*(bp+34) = *(bp+35);
			*(bp+35) = tt;
			tt = *(bp + 36);
			*(bp+36) = *(bp+37);
			*(bp+37) = tt;
		}
		if ((rv = read_ColorantTableVal(p->data+i, bp, end, pcs)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	}

	icp->al->free(icp->al, buf);
	return rv;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmColorantTable_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmColorantTable *p = (icmColorantTable *)pp;
	icc *icp = p->icp;
	icColorSpaceSignature pcs;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	if (icp->header->deviceClass != icSigLinkClass)
		pcs = icp->header->pcs;
	else
		pcs = icSigLabData;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmColorantTable_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmColorantTable_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmColorantTable_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write count of colorants */
	if ((rv = write_UInt32Number(p->count, bp+8)) != 0) {
		sprintf(icp->err,"icmColorantTable_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	bp = bp + 12;
	
	/* Write all the data to the buffer */
	for (i = 0; i < p->count; i++, bp += (32 + 6)) {
		if ((rv = write_ColorantTableVal(p->data+i, bp, pcs)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmColorantTable_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmColorantTable_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmColorantTable *p = (icmColorantTable *)pp;
	icc *icp = p->icp;
	icColorSpaceSignature pcs;

	if (icp->header->deviceClass != icSigLinkClass)
		pcs = icp->header->pcs;
	else
		pcs = icSigLabData;

	if (verb <= 0)
		return;

	if (p->ttype == icSigColorantTableType
	 || p->ttype == icmSigAltColorantTableType)
		op->gprintf(op,"ColorantTable:\n");
	op->gprintf(op,"  No. colorants  = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		icmColorantTableVal *vp;
		for (i = 0; i < p->count; i++) {
			vp = p->data + i;
			op->gprintf(op,"    Colorant %lu:\n",i);
			op->gprintf(op,"      Name = '%s'\n",vp->name);

			if (p->ttype == icSigColorantTableType
			 || p->ttype == icmSigAltColorantTableType) {
			
				switch(pcs) {
					case icSigXYZData:
							op->gprintf(op,"      XYZ = %.8f, %.8f, %.8f\n",
							        vp->pcsCoords[0],vp->pcsCoords[1],vp->pcsCoords[2]);
						break;
			    	case icSigLabData:
							op->gprintf(op,"      Lab = %f, %f, %f\n",
							        vp->pcsCoords[0],vp->pcsCoords[1],vp->pcsCoords[2]);
						break;
					default:
							op->gprintf(op,"      Unexpected PCS\n");
						break;
				}
			}
		}
	}
}

/* Allocate variable sized data elements */
static int icmColorantTable_allocate(
	icmBase *pp
) {
	icmColorantTable *p = (icmColorantTable *)pp;
	icc *icp = p->icp;

	if (p->count != p->_count) {
		unsigned int i;
		if (ovr_mul(p->count, sizeof(icmColorantTableVal))) {
			sprintf(icp->err,"icmColorantTable_alloc: count overflow (%d of %lu bytes)",
			                        p->count,(unsigned long)sizeof(icmColorantTableVal));
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmColorantTableVal *) icp->al->calloc(icp->al,p->count, sizeof(icmColorantTableVal))) == NULL) {
			sprintf(icp->err,"icmColorantTable_alloc: malloc() of icmColorantTable data failed");
			return icp->errc = 2;
		}
		for (i = 0; i < p->count; i++) {
			p->data[i].icp = icp;	/* Do init */
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmColorantTable_delete(
	icmBase *pp
) {
	icmColorantTable *p = (icmColorantTable *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmColorantTable(
	icc *icp
) {
	icmColorantTable *p;
	if ((p = (icmColorantTable *) icp->al->calloc(icp->al,1,sizeof(icmColorantTable))) == NULL)
		return NULL;
	p->ttype    = icSigColorantTableType;
	p->refcount = 1;
	p->get_size = icmColorantTable_get_size;
	p->read     = icmColorantTable_read;
	p->write    = icmColorantTable_write;
	p->dump     = icmColorantTable_dump;
	p->allocate = icmColorantTable_allocate;
	p->del      = icmColorantTable_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* textDescription */

/* Return the number of bytes needed to write this tag */
static unsigned int icmTextDescription_get_size(
	icmBase *pp
) {
	icmTextDescription *p = (icmTextDescription *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);						/* 8 bytes for tag and padding */
	len = sat_addadd(len, 4, p->size);			/* Ascii string length + ascii string */
	len = sat_addaddmul(len, 8, 2, p->ucSize);	/* Unicode language code + length + string */
	len = sat_addadd(len, 3, 67);				/* ScriptCode code, length string */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmTextDescription_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmTextDescription *p = (icmTextDescription *)pp;
	icc *icp = p->icp;
	int rv;
	char *bp, *buf, *end;

#ifdef ICM_STRICT
	if (len < (8 + 4 + 8 + 3 /* + 67 */)) {
#else
	if (len < (8 + 4 + 8 + 3)) {
#endif
		sprintf(icp->err,"icmTextDescription_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmTextDescription_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmTextDescription_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read from the buffer into the structure */
	if ((rv = p->core_read(p, &bp, end)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* core read the object, return 0 on success, error code on fail */
static int icmTextDescription_core_read(
	icmTextDescription *p,
	char **bpp,				/* Pointer to buffer pointer, returns next after read */
	char *end				/* Pointer to past end of read buffer */
) {
	icc *icp = p->icp;
	int rv;
	char *bp = *bpp;

	if (bp > end || 8 > (end - bp)) {
		sprintf(icp->err,"icmTextDescription_read: Data too short to type descriptor");
		*bpp = bp;
		return icp->errc = 1;
	}

	p->size = read_UInt32Number(bp);
	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		*bpp = bp;
		sprintf(icp->err,"icmTextDescription_read: Wrong tag type ('%s') for icmTextDescription",
		        tag2str((icTagTypeSignature)read_SInt32Number(bp)));
		return icp->errc = 1;
	}
	bp = bp + 8;

	/* Read the Ascii string */
	if (bp > end || 4 > (end - bp)) {
		*bpp = bp;
		sprintf(icp->err,"icmTextDescription_read: Data too short to read Ascii header");
		return icp->errc = 1;
	}
	p->size = read_UInt32Number(bp);
	bp += 4;
	if (p->size > 0) {
		if (bp > end || p->size > (end - bp)) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: Data too short to read Ascii string");
			return icp->errc = 1;
		}
		if ((rv = check_null_string(bp,p->size)) == 1) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: ascii string is not terminated");
			return icp->errc = 1;
		}
#ifdef ICM_STRICT
		if (rv == 2) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: ascii string is shorter than count");
			return icp->errc = 1;
		}
#endif 
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			return rv;
		}
		strcpy(p->desc, bp);
		bp += p->size;
	}
	
	/* Read the Unicode string */
	if (bp > end || 8 > (end - bp)) {
		*bpp = bp;
		sprintf(icp->err,"icmTextDescription_read: Data too short to read Unicode string");
		return icp->errc = 1;
	}
	p->ucLangCode = read_UInt32Number(bp);
	bp += 4;
	p->ucSize = read_UInt32Number(bp);
	bp += 4;
	if (p->ucSize > 0) {
		ORD16 *up;
		char *tbp;
		if (bp > end || p->ucSize > (end - bp)/2) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: Data too short to read Unicode string");
			return icp->errc = 1;
		}
		if ((rv = check_null_string16(bp,p->ucSize)) == 1) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: Unicode string is not terminated");
			return icp->errc = 1;
		}
#ifdef ICM_STRICT
		if (rv == 2) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: Unicode string is shorter than count");
			return icp->errc = 1;
		}
#endif
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			return rv;
		}
		for (up = p->ucDesc, tbp = bp; tbp[0] != 0 || tbp[1] != 0; up++, tbp += 2)
			*up = read_UInt16Number(tbp);
		*up = 0;	/* Unicode null */
		bp += p->ucSize * 2;
	}
	
	/* Read the ScriptCode string */
	if (bp > end || 3 > (end - bp)) {
		*bpp = bp;
		sprintf(icp->err,"icmTextDescription_read: Data too short to read ScriptCode header");
		return icp->errc = 1;
	}
	p->scCode = read_UInt16Number(bp);
	bp += 2;
	p->scSize = read_UInt8Number(bp);
	bp += 1;
	if (p->scSize > 0) {
		if (p->scSize > 67) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: ScriptCode string too long");
			return icp->errc = 1;
		}
		if (bp > end || p->scSize > (end - bp)) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: Data too short to read ScriptCode string");
			return icp->errc = 1;
		}
		if ((rv = check_null_string(bp,p->scSize)) == 1) {
#ifdef ICM_STRICT
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_read: ScriptCode string is not terminated");
			return icp->errc = 1;
#else
			/* Patch it up */
			bp[p->scSize-1] = '\000';
#endif
		}
		memmove((void *)p->scDesc, (void *)bp, p->scSize);
	} else {
		memset((void *)p->scDesc, 0, 67);
	}
	bp += 67;
	
	*bpp = bp;
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmTextDescription_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmTextDescription *p = (icmTextDescription *)pp;
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmTextDescription_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmTextDescription_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write to the buffer from the structure */
	if ((rv = p->core_write(p, &bp)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmTextDescription_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Core write the contents of the object. Return 0 on sucess, error code on failure */
static int icmTextDescription_core_write(
	icmTextDescription *p,
	char **bpp				/* Pointer to buffer pointer, returns next after read */
) {
	icc *icp = p->icp;
	char *bp = *bpp;
	int rv;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmTextDescription_write: write_SInt32Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp = bp + 8;

	/* Write the Ascii string */
	if ((rv = write_UInt32Number(p->size,bp)) != 0) {
		sprintf(icp->err,"icmTextDescription_write: write_UInt32Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
	bp += 4;
	if (p->size > 0) {
		if ((rv = check_null_string(p->desc,p->size)) == 1) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_write: ascii string is not terminated");
			return icp->errc = 1;
		}
		if (rv == 2) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_write: ascii string is shorter than length");
			return icp->errc = 1;
		}
		strcpy(bp, p->desc);
		bp += strlen(p->desc) + 1;
	}
	
	/* Write the Unicode string */
	if ((rv = write_UInt32Number(p->ucLangCode, bp)) != 0) {
		sprintf(icp->err,"icmTextDescription_write: write_UInt32Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
	bp += 4;
	if ((rv = write_UInt32Number(p->ucSize, bp)) != 0) {
		sprintf(icp->err,"icmTextDescription_write: write_UInt32Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
	bp += 4;
	if (p->ucSize > 0) {
		ORD16 *up;
		if ((rv = check_null_string16((char *)p->ucDesc,p->ucSize)) == 1) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_write: Unicode string is not terminated");
			return icp->errc = 1;
		}
		if (rv == 2) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_write: Unicode string is shorter than length");
			return icp->errc = 1;
		}
		for(up = p->ucDesc; *up != 0; up++, bp += 2) {
			if ((rv = write_UInt16Number(((unsigned int)*up), bp)) != 0) {
				sprintf(icp->err,"icmTextDescription_write: write_UInt16Number() failed");
				*bpp = bp;
				return icp->errc = rv;
			}
		}
		bp[0] = 0;	/* null */
		bp[1] = 0;
		bp += 2;
	}
	
	/* Write the ScriptCode string */
	if ((rv = write_UInt16Number(p->scCode, bp)) != 0) {
		sprintf(icp->err,"icmTextDescription_write: write_UInt16Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
	bp += 2;
	if ((rv = write_UInt8Number(p->scSize, bp)) != 0) {
		sprintf(icp->err,"icmTextDescription_write: write_UInt8Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
	bp += 1;
	if (p->scSize > 0) {
		if (p->scSize > 67) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_write: ScriptCode string too long");
			return icp->errc = 1;
		}
		if ((rv = check_null_string((char *)p->scDesc,p->scSize)) == 1) {
			*bpp = bp;
			sprintf(icp->err,"icmTextDescription_write: ScriptCode string is not terminated");
			return icp->errc = 1;
		}
		memmove((void *)bp, (void *)p->scDesc, 67);
	} else {
		memset((void *)bp, 0, 67);
	}
	bp += 67;

	*bpp = bp;
	return 0;
}

/* Dump a text description of the object */
static void icmTextDescription_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmTextDescription *p = (icmTextDescription *)pp;
	unsigned int i, r, c;

	if (verb <= 0)
		return;

	op->gprintf(op,"TextDescription:\n");

	if (p->size > 0) {
		unsigned int size = p->size > 0 ? p->size-1 : 0;
		op->gprintf(op,"  ASCII data, length %lu chars:\n",p->size);

		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->gprintf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->gprintf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->gprintf(op,"    0x%04lx: ",i);
			c += 10;
			while (i < size && c < 75) {
				if (isprint(p->desc[i])) {
					op->gprintf(op,"%c",p->desc[i]);
					c++;
				} else {
					op->gprintf(op,"\\%03o",p->desc[i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->gprintf(op,"\n");
		}
	} else {
		op->gprintf(op,"  No ASCII data\n");
	}

	/* Can't dump Unicode or ScriptCode as text with portable code */
	if (p->ucSize > 0) {
		unsigned int size = p->ucSize;
		op->gprintf(op,"  Unicode Data, Language code 0x%x, length %lu chars\n",
		        p->ucLangCode, p->ucSize);
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->gprintf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->gprintf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->gprintf(op,"    0x%04lx: ",i);
			c += 10;
			while (i < size && c < 75) {
				op->gprintf(op,"%04x ",p->ucDesc[i]);
				c += 5;
				i++;
			}
			if (i < size)
				op->gprintf(op,"\n");
		}
	} else {
		op->gprintf(op,"  No Unicode data\n");
	}
	if (p->scSize > 0) {
		unsigned int size = p->scSize;
		op->gprintf(op,"  ScriptCode Data, Code 0x%x, length %lu chars\n",
		        p->scCode, p->scSize);
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->gprintf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->gprintf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->gprintf(op,"    0x%04lx: ",i);
			c += 10;
			while (i < size && c < 75) {
				op->gprintf(op,"%02x ",p->scDesc[i]);
				c += 3;
				i++;
			}
			if (i < size)
				op->gprintf(op,"\n");
		}
	} else {
		op->gprintf(op,"  No ScriptCode data\n");
	}
}

/* Allocate variable sized data elements */
static int icmTextDescription_allocate(
	icmBase *pp
) {
	icmTextDescription *p = (icmTextDescription *)pp;
	icc *icp = p->icp;

	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(char))) {
			sprintf(icp->err,"icmTextDescription_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->desc != NULL)
			icp->al->free(icp->al, p->desc);
		if ((p->desc = (char *) icp->al->calloc(icp->al, p->size, sizeof(char))) == NULL) {
			sprintf(icp->err,"icmTextDescription_alloc: malloc() of Ascii description failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	if (p->ucSize != p->uc_size) {
		if (ovr_mul(p->ucSize, sizeof(ORD16))) {
			sprintf(icp->err,"icmTextDescription_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->ucDesc != NULL)
			icp->al->free(icp->al, p->ucDesc);
		if ((p->ucDesc = (ORD16 *) icp->al->calloc(icp->al, p->ucSize, sizeof(ORD16))) == NULL) {
			sprintf(icp->err,"icmTextDescription_alloc: malloc() of Unicode description failed");
			return icp->errc = 2;
		}
		p->uc_size = p->ucSize;
	}
	return 0;
}

/* Free all variable sized elements */
static void icmTextDescription_unallocate(
	icmTextDescription *p
) {
	icc *icp = p->icp;

	if (p->desc != NULL)
		icp->al->free(icp->al, p->desc);
	if (p->ucDesc != NULL)
		icp->al->free(icp->al, p->ucDesc);
}

/* Free all storage in the object */
static void icmTextDescription_delete(
	icmBase *pp
) {
	icmTextDescription *p = (icmTextDescription *)pp;
	icc *icp = p->icp;

	icmTextDescription_unallocate(p);
	icp->al->free(icp->al, p);
}

/* Initialze a named object */
static void icmTextDescription_init(
	icmTextDescription *p,
	icc *icp
) {
	memset((void *)p, 0, sizeof(icmTextDescription));	/* Imitate calloc */

	p->ttype    = icSigTextDescriptionType;
	p->refcount = 1;
	p->get_size = icmTextDescription_get_size;
	p->read     = icmTextDescription_read;
	p->write    = icmTextDescription_write;
	p->dump     = icmTextDescription_dump;
	p->allocate = icmTextDescription_allocate;
	p->del      = icmTextDescription_delete;
	p->icp      = icp;

	p->core_read  = icmTextDescription_core_read;
	p->core_write = icmTextDescription_core_write;
}

/* Create an empty object. Return null on error */
static icmBase *new_icmTextDescription(
	icc *icp
) {
	icmTextDescription *p;
	if ((p = (icmTextDescription *) icp->al->calloc(icp->al,1,sizeof(icmTextDescription))) == NULL)
		return NULL;

	icmTextDescription_init(p,icp);
	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Support for icmDescStruct */

/* Return the number of bytes needed to write this tag */
static unsigned int icmDescStruct_get_size(
	icmDescStruct *p
) {
	unsigned int len = 0;
	len = sat_add(len, 20);				/* 20 bytes for header info */
	len = sat_add(len, p->device.get_size((icmBase *)&p->device));
	if (p->device.size == 0)
		len = sat_add(len, 1);			/* Extra 1 because of zero length desciption */
	len = sat_add(len, p->model.get_size((icmBase *)&p->model));
	if (p->model.size == 0)
		len = sat_add(len, 1);			/* Extra 1 because of zero length desciption */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmDescStruct_read(
	icmDescStruct *p,
	char **bpp,				/* Pointer to buffer pointer, returns next after read */
	char *end				/* Pointer to past end of read buffer */
) {
	icc *icp = p->icp;
	char *bp = *bpp;
	int rv = 0;

	if (bp > end || 20 > (end - bp)) {
		sprintf(icp->err,"icmDescStruct_read: Data too short read header");
		*bpp = bp;
		return icp->errc = 1;
	}

    p->deviceMfg = read_SInt32Number(bp + 0);
    p->deviceModel = read_UInt32Number(bp + 4);
    read_UInt64Number(&p->attributes, bp + 8);
	p->technology = (icTechnologySignature) read_UInt32Number(bp + 16);
	*bpp = bp += 20;

	/* Read the device text description */
	if ((rv = p->device.core_read(&p->device, bpp, end)) != 0) {
		return rv;
	}

	/* Read the model text description */
	if ((rv = p->model.core_read(&p->model, bpp, end)) != 0) {
		return rv;
	}
	
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmDescStruct_write(
	icmDescStruct *p,
	char **bpp				/* Pointer to buffer pointer, returns next after read */
) {
	icc *icp = p->icp;
	char *bp = *bpp;
	int rv = 0;
	char *ttd = NULL;
	unsigned int tts = 0;

    if ((rv = write_SInt32Number(p->deviceMfg, bp + 0)) != 0) {
		sprintf(icp->err,"icmDescStruct_write: write_SInt32Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
    if ((rv = write_UInt32Number(p->deviceModel, bp + 4)) != 0) {
		sprintf(icp->err,"icmDescStruct_write: write_UInt32Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
    if ((rv = write_UInt64Number(&p->attributes, bp + 8)) != 0) {
		sprintf(icp->err,"icmDescStruct_write: write_UInt64Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
	if ((rv = write_UInt32Number(p->technology, bp + 16)) != 0) {
		sprintf(icp->err,"icmDescStruct_write: write_UInt32Number() failed");
		*bpp = bp;
		return icp->errc = rv;
	}
	*bpp = bp += 20;

	/* Make sure the ASCII device text is a minimum size of 1, as per the spec. */
	ttd = p->device.desc;
	tts = p->device.size;

	if (p->device.size == 0) {
		p->device.desc = "";
		p->device.size = 1;
	}

	/* Write the device text description */
	if ((rv = p->device.core_write(&p->device, bpp)) != 0) {
		return rv;
	}

	p->device.desc = ttd;
	p->device.size = tts;

	/* Make sure the ASCII model text is a minimum size of 1, as per the spec. */
	ttd = p->model.desc;
	tts = p->model.size;

	if (p->model.size == 0) {
		p->model.desc = "";
		p->model.size = 1;
	}

	/* Write the model text description */
	if ((rv = p->model.core_write(&p->model, bpp)) != 0) {
		return rv;
	}

	p->model.desc = ttd;
	p->model.size = tts;

	/* Make sure the ASCII model text is a minimum size of 1, as per the spec. */
	ttd = p->device.desc;
	tts = p->device.size;

	return 0;
}

/* Dump a text description of the object */
static void icmDescStruct_dump(
	icmDescStruct *p,
	icmFile *op,	/* Output to dump to */
	int   verb,		/* Verbosity level */
	int   index		/* Description index */
) {
	if (verb <= 0)
		return;

	op->gprintf(op,"DescStruct %u:\n",index);
	if (verb >= 1) {
		op->gprintf(op,"  Dev. Mnfctr.    = %s\n",tag2str(p->deviceMfg));	/* ~~~ */
		op->gprintf(op,"  Dev. Model      = %s\n",tag2str(p->deviceModel));	/* ~~~ */
		op->gprintf(op,"  Dev. Attrbts    = %s\n", string_DeviceAttributes(p->attributes.l));
		op->gprintf(op,"  Dev. Technology = %s\n", string_TechnologySignature(p->technology));
		p->device.dump((icmBase *)&p->device, op,verb);
		p->model.dump((icmBase *)&p->model, op,verb);
		op->gprintf(op,"\n");
	}
}

/* Allocate variable sized data elements (ie. descriptions) */
static int icmDescStruct_allocate(
	icmDescStruct *p
) {
	int rv;

	if ((rv = p->device.allocate((icmBase *)&p->device)) != 0) {
		return rv;
	}
	if ((rv = p->model.allocate((icmBase *)&p->model)) != 0) {
		return rv;
	}
	return 0;
}

/* Free all storage in the object */
static void icmDescStruct_delete(
	icmDescStruct *p
) {
	icmTextDescription_unallocate(&p->device);
	icmTextDescription_unallocate(&p->model);
}

/* Init a DescStruct object */
static void icmDescStruct_init(
	icmDescStruct *p,
	icc *icp
) {

	p->allocate = icmDescStruct_allocate;
	p->icp = icp;

	icmTextDescription_init(&p->device, icp);
	icmTextDescription_init(&p->model, icp);
}

/* - - - - - - - - - - - - - - - */
/* icmProfileSequenceDesc object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmProfileSequenceDesc_get_size(
	icmBase *pp
) {
	icmProfileSequenceDesc *p = (icmProfileSequenceDesc *)pp;
	unsigned int len = 0;
	unsigned int i;
	len = sat_add(len, 12);				/* 12 bytes for tag, padding and count */
	for (i = 0; i < p->count; i++) {	/* All the description structures */
		len = sat_add(len, icmDescStruct_get_size(&p->data[i]));
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmProfileSequenceDesc_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmProfileSequenceDesc *p = (icmProfileSequenceDesc *)pp;
	icc *icp = p->icp;
	unsigned int i;
	char *bp, *buf, *end;
	int rv = 0;

	if (len < 12) {
		sprintf(icp->err,"icmProfileSequenceDesc_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmProfileSequenceDesc_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmProfileSequenceDesc_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmProfileSequenceDesc_read: Wrong tag type for icmProfileSequenceDesc");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp += 8;	/* Skip padding */

	p->count = read_UInt32Number(bp);	/* Number of sequence descriptions */
	bp += 4;

	/* Read all the sequence descriptions */
	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}
	for (i = 0; i < p->count; i++) {
		if ((rv = icmDescStruct_read(&p->data[i], &bp, end)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmProfileSequenceDesc_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmProfileSequenceDesc *p = (icmProfileSequenceDesc *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmProfileSequenceDesc_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmProfileSequenceDesc_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmProfileSequenceDesc_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	if ((rv = write_UInt32Number(p->count,bp+8)) != 0) {
		sprintf(icp->err,"icmProfileSequenceDesc_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	bp = bp + 12;

	/* Write all the description structures */
	for (i = 0; i < p->count; i++) {
		if ((rv = icmDescStruct_write(&p->data[i], &bp)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmProfileSequenceDesc_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmProfileSequenceDesc_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmProfileSequenceDesc *p = (icmProfileSequenceDesc *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"ProfileSequenceDesc:\n");
	op->gprintf(op,"  No. elements = %u\n",p->count);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->count; i++)
			icmDescStruct_dump(&p->data[i], op, verb-1, i);
	}
}

/* Allocate variable sized data elements (ie. count of profile descriptions) */
static int icmProfileSequenceDesc_allocate(
	icmBase *pp
) {
	icmProfileSequenceDesc *p = (icmProfileSequenceDesc *)pp;
	icc *icp = p->icp;
	unsigned int i;

	if (p->count != p->_count) {
		if (ovr_mul(p->count, sizeof(icmDescStruct))) {
			sprintf(icp->err,"icmProfileSequenceDesc_allocate: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmDescStruct *) icp->al->calloc(icp->al, p->count, sizeof(icmDescStruct))) == NULL) {
			sprintf(icp->err,"icmProfileSequenceDesc_allocate Allocation of DescStruct array failed");
			return icp->errc = 2;
		}
		/* Now init the DescStructs */
		for (i = 0; i < p->count; i++) {
			icmDescStruct_init(&p->data[i], icp);
		}
		p->_count = p->count;
	}
	return 0;
}

/* Free all storage in the object */
static void icmProfileSequenceDesc_delete(
	icmBase *pp
) {
	icmProfileSequenceDesc *p = (icmProfileSequenceDesc *)pp;
	icc *icp = p->icp;
	unsigned int i;

	for (i = 0; i < p->count; i++) {
		icmDescStruct_delete(&p->data[i]);	/* Free allocated contents */
	}
	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmProfileSequenceDesc(
	icc *icp
) {
	icmProfileSequenceDesc *p;
	if ((p = (icmProfileSequenceDesc *) icp->al->calloc(icp->al,1,sizeof(icmProfileSequenceDesc))) == NULL)
		return NULL;
	p->ttype    = icSigProfileSequenceDescType;
	p->refcount = 1;
	p->get_size = icmProfileSequenceDesc_get_size;
	p->read     = icmProfileSequenceDesc_read;
	p->write    = icmProfileSequenceDesc_write;
	p->dump     = icmProfileSequenceDesc_dump;
	p->allocate = icmProfileSequenceDesc_allocate;
	p->del      = icmProfileSequenceDesc_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* Signature */

/* Return the number of bytes needed to write this tag */
static unsigned int icmSignature_get_size(
	icmBase *pp
) {
	unsigned int len = 0;
	len = sat_add(len, 8);			/* 8 bytes for tag and padding */
	len = sat_add(len, 4);			/* 4 for signature */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmSignature_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmSignature *p = (icmSignature *)pp;
	icc *icp = p->icp;
	char *bp, *buf;

	if (len < 12) {
		sprintf(icp->err,"icmSignature_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmSignature_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmSignature_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmSignaturSignatureng tag type for icmSignature");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read the encoded measurement geometry */
	p->sig = (icTechnologySignature)read_SInt32Number(bp + 8);

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmSignature_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmSignature *p = (icmSignature *)pp;
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmSignature_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmSignature_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmSignature_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write the signature */
	if ((rv = write_SInt32Number((int)p->sig, bp + 8)) != 0) {
		sprintf(icp->err,"icmSignaturea_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmSignature_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmSignature_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmSignature *p = (icmSignature *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"Signature\n");
	op->gprintf(op,"  Technology = %s\n", string_TechnologySignature(p->sig));
}

/* Allocate variable sized data elements */
static int icmSignature_allocate(
	icmBase *pp
) {
	/* Nothing to do */
	return 0;
}

/* Free all storage in the object */
static void icmSignature_delete(
	icmBase *pp
) {
	icmSignature *p = (icmSignature *)pp;
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmSignature(
	icc *icp
) {
	icmSignature *p;
	if ((p = (icmSignature *) icp->al->calloc(icp->al,1,sizeof(icmSignature))) == NULL)
		return NULL;
	p->ttype    = icSigSignatureType;
	p->refcount = 1;
	p->get_size = icmSignature_get_size;
	p->read     = icmSignature_read;
	p->write    = icmSignature_write;
	p->dump     = icmSignature_dump;
	p->allocate = icmSignature_allocate;
	p->del      = icmSignature_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */

/* Data conversion support functions */
static int read_ScreeningData(icmScreeningData *p, char *d) {
	p->frequency = read_S15Fixed16Number(d + 0);
	p->angle     = read_S15Fixed16Number(d + 4);
	p->spotShape = (icSpotShape)read_SInt32Number(d + 8);
	return 0;
}

static int write_ScreeningData(icmScreeningData *p, char *d) {
	int rv;
	if ((rv = write_S15Fixed16Number(p->frequency, d + 0)) != 0)
		return rv;
	if ((rv = write_S15Fixed16Number(p->angle, d + 4)) != 0)
		return rv;
	if ((rv = write_SInt32Number((int)p->spotShape, d + 8)) != 0)
		return rv;
	return 0;
}


/* icmScreening object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmScreening_get_size(
	icmBase *pp
) {
	icmScreening *p = (icmScreening *)pp;
	unsigned int len = 0;
	len = sat_add(len, 16);					/* 16 bytes for tag, padding, flag & channeles */
	len = sat_addmul(len, p->channels, 12);	/* 12 bytes for each channel */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmScreening_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmScreening *p = (icmScreening *)pp;
	icc *icp = p->icp;
	int rv = 0;
	unsigned int i;
	char *bp, *buf, *end;

	if (len < 12) {
		sprintf(icp->err,"icmScreening_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmScreening_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmScreening_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmScreening_read: Wrong tag type for icmScreening");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->screeningFlag = read_UInt32Number(bp+8);		/* Flags */
	p->channels      = read_UInt32Number(bp+12);	/* Number of channels */
	bp = bp + 16;

	if ((rv = p->allocate((icmBase *)p)) != 0) {
		icp->al->free(icp->al, buf);
		return rv;
	}

	/* Read all the data from the buffer */
	for (i = 0; i < p->channels; i++, bp += 12) {
		if (bp > end || 12 > (end - bp)) {
			sprintf(icp->err,"icmScreening_read: Data too short to read Screening Data");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		read_ScreeningData(&p->data[i], bp);
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmScreening_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmScreening *p = (icmScreening *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmScreening_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmScreening_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmScreening_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	if ((rv = write_UInt32Number(p->screeningFlag,bp+8)) != 0) {
			sprintf(icp->err,"icmScreening_write: write_UInt32Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	if ((rv = write_UInt32Number(p->channels,bp+12)) != 0) {
			sprintf(icp->err,"icmScreening_write: write_UInt32NumberXYZumber() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	bp = bp + 16;

	/* Write all the data to the buffer */
	for (i = 0; i < p->channels; i++, bp += 12) {
		if ((rv = write_ScreeningData(&p->data[i],bp)) != 0) {
			sprintf(icp->err,"icmScreening_write: write_ScreeningData() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmScreening_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmScreening_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmScreening *p = (icmScreening *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"Screening:\n");
	op->gprintf(op,"  Flags = %s\n", string_ScreenEncodings(p->screeningFlag));
	op->gprintf(op,"  No. channels = %u\n",p->channels);
	if (verb >= 2) {
		unsigned int i;
		for (i = 0; i < p->channels; i++) {
			op->gprintf(op,"    %lu:\n",i);
			op->gprintf(op,"      Frequency:  %f\n",p->data[i].frequency);
			op->gprintf(op,"      Angle:      %f\n",p->data[i].angle);
			op->gprintf(op,"      Spot shape: %s\n", string_SpotShape(p->data[i].spotShape));
		}
	}
}

/* Allocate variable sized data elements */
static int icmScreening_allocate(
	icmBase *pp
) {
	icmScreening *p = (icmScreening *)pp;
	icc *icp = p->icp;

	if (p->channels != p->_channels) {
		if (ovr_mul(p->channels, sizeof(icmScreeningData))) {
			sprintf(icp->err,"icmScreening_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->data != NULL)
			icp->al->free(icp->al, p->data);
		if ((p->data = (icmScreeningData *) icp->al->malloc(icp->al, p->channels * sizeof(icmScreeningData))) == NULL) {
			sprintf(icp->err,"icmScreening_alloc: malloc() of icmScreening data failed");
			return icp->errc = 2;
		}
		p->_channels = p->channels;
	}
	return 0;
}

/* Free all storage in the object */
static void icmScreening_delete(
	icmBase *pp
) {
	icmScreening *p = (icmScreening *)pp;
	icc *icp = p->icp;

	if (p->data != NULL)
		icp->al->free(icp->al, p->data);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmScreening(
	icc *icp
) {
	icmScreening *p;
	if ((p = (icmScreening *) icp->al->calloc(icp->al,1,sizeof(icmScreening))) == NULL)
		return NULL;
	p->ttype    = icSigScreeningType;
	p->refcount = 1;
	p->get_size = icmScreening_get_size;
	p->read     = icmScreening_read;
	p->write    = icmScreening_write;
	p->dump     = icmScreening_dump;
	p->allocate = icmScreening_allocate;
	p->del      = icmScreening_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmUcrBg object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmUcrBg_get_size(
	icmBase *pp
) {
	icmUcrBg *p = (icmUcrBg *)pp;
	unsigned int len = 0;
	len = sat_add(len, 8);							/* 8 bytes for tag and padding */
	len = sat_addaddmul(len, 4, p->UCRcount, 2);	/* Undercolor Removal */
	len = sat_addaddmul(len, 4, p->BGcount, 2);		/* Black Generation */
	len = sat_add(len, p->size);					/* Description string */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmUcrBg_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmUcrBg *p = (icmUcrBg *)pp;
	icc *icp = p->icp;
	unsigned int i;
	int rv;
	char *bp, *buf, *end;

	if (len < 16) {
		sprintf(icp->err,"icmUcrBg_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUcrBg_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmUcrBg_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmUcrBg_read: Wrong tag type for icmUcrBg");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->UCRcount = read_UInt32Number(bp+8);	/* First curve count */
	bp = bp + 12;

	if (p->UCRcount > 0) {
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		for (i = 0; i < p->UCRcount; i++, bp += 2) {
			if (bp > end || 2 > (end - bp)) {
				sprintf(icp->err,"icmUcrBg_read: Data too short to read UCR Data");
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
			if (p->UCRcount == 1)	/* % */
				p->UCRcurve[i] = (double)read_UInt16Number(bp);
			else					/* 0.0 - 1.0 */
				p->UCRcurve[i] = read_DCS16Number(bp);
		}
	} else {
		p->UCRcurve = NULL;
	}

	if (bp > end || 4 > (end - bp)) {
		sprintf(icp->err,"icmData_read: Data too short to read Black Gen count");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->BGcount = read_UInt32Number(bp);	/* First curve count */
	bp += 4;

	if (p->BGcount > 0) {
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		for (i = 0; i < p->BGcount; i++, bp += 2) {
			if (bp > end || 2 > (end - bp)) {
				sprintf(icp->err,"icmUcrBg_read: Data too short to read BG Data");
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
			if (p->BGcount == 1)	/* % */
				p->BGcurve[i] = (double)read_UInt16Number(bp);
			else					/* 0.0 - 1.0 */
				p->BGcurve[i] = read_DCS16Number(bp);
		}
	} else {
		p->BGcurve = NULL;
	}

	p->size = end - bp;		/* Nominal string length */
	if (p->size > 0) {
		if ((rv = check_null_string(bp, p->size)) == 1) {
			sprintf(icp->err,"icmUcrBg_read: string is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		p->size = strlen(bp) + 1;
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		memmove((void *)p->string, (void *)bp, p->size);
		bp += p->size;
	} else {
		p->string = NULL;
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmUcrBg_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmUcrBg *p = (icmUcrBg *)pp;
	icc *icp = p->icp;
	unsigned int i;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmUcrBg_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmUcrBg_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmUcrBg_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp = bp + 8;

	/* Write UCR curve */
	if ((rv = write_UInt32Number(p->UCRcount,bp)) != 0) {
		sprintf(icp->err,"icmUcrBg_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	bp += 4;

	for (i = 0; i < p->UCRcount; i++, bp += 2) {
		if (p->UCRcount == 1) { /* % */
			if ((rv = write_UInt16Number((unsigned int)(p->UCRcurve[i]+0.5),bp)) != 0) {
				sprintf(icp->err,"icmUcrBg_write: write_UInt16umber() failed");
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		} else {
			if ((rv = write_DCS16Number(p->UCRcurve[i],bp)) != 0) {
				sprintf(icp->err,"icmUcrBg_write: write_DCS16umber(%.8f) failed",p->UCRcurve[i]);
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	}

	/* Write BG curve */
	if ((rv = write_UInt32Number(p->BGcount,bp)) != 0) {
		sprintf(icp->err,"icmUcrBg_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	bp += 4;

	for (i = 0; i < p->BGcount; i++, bp += 2) {
		if (p->BGcount == 1) { /* % */
			if ((rv = write_UInt16Number((unsigned int)(p->BGcurve[i]+0.5),bp)) != 0) {
				sprintf(icp->err,"icmUcrBg_write: write_UInt16umber() failed");
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		} else {
			if ((rv = write_DCS16Number(p->BGcurve[i],bp)) != 0) {
				sprintf(icp->err,"icmUcrBg_write: write_DCS16umber(%.8f) failed",p->BGcurve[i]);
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	}

	if (p->string != NULL) {
		if ((rv = check_null_string(p->string,p->size)) == 1) {
			sprintf(icp->err,"icmUcrBg_write: text is not null terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		if (rv == 2) {
			sprintf(icp->err,"icmUcrBg_write: text is shorter than length");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		memmove((void *)bp, (void *)p->string, p->size);
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmUcrBg_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmUcrBg_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmUcrBg *p = (icmUcrBg *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"Undercolor Removal Curve & Black Generation:\n");

	if (p->UCRcount == 0) {
		op->gprintf(op,"  UCR: Not specified\n");
	} else if (p->UCRcount == 1) {
		op->gprintf(op,"  UCR: %f%%\n",p->UCRcurve[0]);
	} else {
		op->gprintf(op,"  UCR curve no. elements = %u\n",p->UCRcount);
		if (verb >= 2) {
			unsigned int i;
			for (i = 0; i < p->UCRcount; i++)
				op->gprintf(op,"  %3lu:  %f\n",i,p->UCRcurve[i]);
		}
	}
	if (p->BGcount == 0) {
		op->gprintf(op,"  BG: Not specified\n");
	} else if (p->BGcount == 1) {
		op->gprintf(op,"  BG: %f%%\n",p->BGcurve[0]);
	} else {
		op->gprintf(op,"  BG curve no. elements = %u\n",p->BGcount);
		if (verb >= 2) {
			unsigned int i;
			for (i = 0; i < p->BGcount; i++)
				op->gprintf(op,"  %3lu:  %f\n",i,p->BGcurve[i]);
		}
	}

	{
		unsigned int i, r, c, size;
		op->gprintf(op,"  Description:\n");
		op->gprintf(op,"    No. chars = %lu\n",p->size);
	
		size = p->size > 0 ? p->size-1 : 0;
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->gprintf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->gprintf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->gprintf(op,"      0x%04lx: ",i);
			c += 10;
			while (i < size && c < 73) {
				if (isprint(p->string[i])) {
					op->gprintf(op,"%c",p->string[i]);
					c++;
				} else {
					op->gprintf(op,"\\%03o",p->string[i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->gprintf(op,"\n");
		}
	}
}

/* Allocate variable sized data elements */
static int icmUcrBg_allocate(
	icmBase *pp
) {
	icmUcrBg *p = (icmUcrBg *)pp;
	icc *icp = p->icp;

	if (p->UCRcount != p->UCR_count) {
		if (ovr_mul(p->UCRcount, sizeof(double))) {
			sprintf(icp->err,"icmUcrBg_allocate: size overflow");
			return icp->errc = 1;
		}
		if (p->UCRcurve != NULL)
			icp->al->free(icp->al, p->UCRcurve);
		if ((p->UCRcurve = (double *) icp->al->calloc(icp->al, p->UCRcount, sizeof(double))) == NULL) {
			sprintf(icp->err,"icmUcrBg_allocate: malloc() of UCR curve data failed");
			return icp->errc = 2;
		}
		p->UCR_count = p->UCRcount;
	}
	if (p->BGcount != p->BG_count) {
		if (ovr_mul(p->BGcount, sizeof(double))) {
			sprintf(icp->err,"icmUcrBg_allocate: size overflow");
			return icp->errc = 1;
		}
		if (p->BGcurve != NULL)
			icp->al->free(icp->al, p->BGcurve);
		if ((p->BGcurve = (double *) icp->al->calloc(icp->al, p->BGcount, sizeof(double))) == NULL) {
			sprintf(icp->err,"icmUcrBg_allocate: malloc() of BG curve data failed");
			return icp->errc = 2;
		}
		p->BG_count = p->BGcount;
	}
	if (p->size != p->_size) {
		if (ovr_mul(p->size, sizeof(char))) {
			sprintf(icp->err,"icmUcrBg_allocate: size overflow");
			return icp->errc = 1;
		}
		if (p->string != NULL)
			icp->al->free(icp->al, p->string);
		if ((p->string = (char *) icp->al->calloc(icp->al, p->size, sizeof(char))) == NULL) {
			sprintf(icp->err,"icmUcrBg_allocate: malloc() of string data failed");
			return icp->errc = 2;
		}
		p->_size = p->size;
	}
	return 0;
}

/* Free all storage in the object */
static void icmUcrBg_delete(
	icmBase *pp
) {
	icmUcrBg *p = (icmUcrBg *)pp;
	icc *icp = p->icp;

	if (p->UCRcurve != NULL)
		icp->al->free(icp->al, p->UCRcurve);
	if (p->BGcurve != NULL)
		icp->al->free(icp->al, p->BGcurve);
	if (p->string != NULL)
		icp->al->free(icp->al, p->string);
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmUcrBg(
	icc *icp
) {
	icmUcrBg *p;
	if ((p = (icmUcrBg *) icp->al->calloc(icp->al,1,sizeof(icmUcrBg))) == NULL)
		return NULL;
	p->ttype    = icSigUcrBgType;
	p->refcount = 1;
	p->get_size = icmUcrBg_get_size;
	p->read     = icmUcrBg_read;
	p->write    = icmUcrBg_write;
	p->dump     = icmUcrBg_dump;
	p->allocate = icmUcrBg_allocate;
	p->del      = icmUcrBg_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* VideoCardGamma (ColorSync 2.5 specific - c/o Neil Okamoto) */
/* 'vcgt' */

static unsigned int icmVideoCardGamma_get_size(
	icmBase *pp
) {
	icmVideoCardGamma *p = (icmVideoCardGamma *)pp;
	unsigned int len = 0;

	len = sat_add(len, 8);			/* 8 bytes for tag and padding */
	len = sat_add(len, 4);			/* 4 for gamma type */

	/* compute size of remainder */
	if (p->tagType == icmVideoCardGammaTableType) {
		len = sat_add(len, 2);       /* 2 bytes for channels */
		len = sat_add(len, 2);       /* 2 for entry count */
		len = sat_add(len, 2);       /* 2 for entry size */
		len = sat_add(len, sat_mul3(p->u.table.channels, /* compute table size */
		                            p->u.table.entryCount, p->u.table.entrySize));
	}
	else if (p->tagType == icmVideoCardGammaFormulaType) {
		len = sat_add(len, 12);		/* 4 bytes each for red gamma, min, & max */
		len = sat_add(len, 12);		/* 4 bytes each for green gamma, min & max */
		len = sat_add(len, 12);		/* 4 bytes each for blue gamma, min & max */
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmVideoCardGamma_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmVideoCardGamma *p = (icmVideoCardGamma *)pp;
	icc *icp = p->icp;
	int rv, c;
	char *bp, *buf;
	ORD8 *pchar;
	ORD16 *pshort;

	if (len < 18) {
		sprintf(icp->err,"icmVideoCardGamma_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmVideoCardGamma_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmVideoCardGamma_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmVideoCardGamma_read: Wrong tag type for icmVideoCardGamma");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read gamma format (eg. table or formula) from the buffer */
	p->tagType = (icmVideoCardGammaTagType)read_UInt32Number(bp+8);

	/* Read remaining gamma data based on format */
	if (p->tagType == icmVideoCardGammaTableType) {
		p->u.table.channels   = read_UInt16Number(bp+12);
		p->u.table.entryCount = read_UInt16Number(bp+14);
		p->u.table.entrySize  = read_UInt16Number(bp+16);
		if ((len-18) < sat_mul3(p->u.table.channels, p->u.table.entryCount,
		                        p->u.table.entrySize)) {
			sprintf(icp->err,"icmVideoCardGamma_read: Tag too small to be legal");
			return icp->errc = 1;
		}
		if ((rv = pp->allocate(pp)) != 0) {  /* make space for table */
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		/* ~~~~ This should be a table of doubles like the rest of icclib ! ~~~~ */
		pchar = (ORD8 *)p->u.table.data;
		pshort = (ORD16 *)p->u.table.data;
		for (c=0, bp=bp+18; c<p->u.table.channels*p->u.table.entryCount; c++) {
			switch (p->u.table.entrySize) {
			case 1:
				*pchar++ = read_UInt8Number(bp);
				bp++;
				break;
			case 2:
				*pshort++ = read_UInt16Number(bp);
				bp+=2;
				break;
			default:
				sprintf(icp->err,"icmVideoCardGamma_read: unsupported table entry size");
				pp->del(pp);
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
		}
	} else if (p->tagType == icmVideoCardGammaFormulaType) {
		if (len < 48) {
			sprintf(icp->err,"icmVideoCardGamma_read: Tag too small to be legal");
			return icp->errc = 1;
		}
		p->u.table.channels     = 3;	/* Always 3 for formula */
		p->u.formula.redGamma   = read_S15Fixed16Number(bp+12);
		p->u.formula.redMin     = read_S15Fixed16Number(bp+16);
		p->u.formula.redMax     = read_S15Fixed16Number(bp+20);
		p->u.formula.greenGamma = read_S15Fixed16Number(bp+24);
		p->u.formula.greenMin   = read_S15Fixed16Number(bp+28);
		p->u.formula.greenMax   = read_S15Fixed16Number(bp+32);
		p->u.formula.blueGamma  = read_S15Fixed16Number(bp+36);
		p->u.formula.blueMin    = read_S15Fixed16Number(bp+40);
		p->u.formula.blueMax    = read_S15Fixed16Number(bp+44);
	} else {
		sprintf(icp->err,"icmVideoCardGammaTable_read: Unknown gamma format for icmVideoCardGamma");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmVideoCardGamma_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmVideoCardGamma *p = (icmVideoCardGamma *)pp;
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0, c;
	ORD8 *pchar;
	ORD16 *pshort;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmViewingConditions_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmViewingConditions_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmVideoCardGamma_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write gamma format (eg. table of formula) */
	if ((rv = write_UInt32Number(p->tagType,bp+8)) != 0) {
		sprintf(icp->err,"icmVideoCardGamma_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write remaining gamma data based on format */
	if (p->tagType == icmVideoCardGammaTableType) {
		if ((rv = write_UInt16Number(p->u.table.channels,bp+12)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_UInt16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_UInt16Number(p->u.table.entryCount,bp+14)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_UInt16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_UInt16Number(p->u.table.entrySize,bp+16)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_UInt16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		pchar = (ORD8 *)p->u.table.data;
		pshort = (ORD16 *)p->u.table.data;
		for (c=0, bp=bp+18; c<p->u.table.channels*p->u.table.entryCount; c++) {
			switch (p->u.table.entrySize) {
			case 1:
				write_UInt8Number(*pchar++,bp);
				bp++;
				break;
			case 2:
				write_UInt16Number(*pshort++,bp);
				bp+=2;
				break;
			default:
				sprintf(icp->err,"icmVideoCardGamma_write: unsupported table entry size");
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
		}
	} else if (p->tagType == icmVideoCardGammaFormulaType) {
		if ((rv = write_S15Fixed16Number(p->u.formula.redGamma,bp+12)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.redMin,bp+16)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.redMax,bp+20)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.greenGamma,bp+24)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.greenMin,bp+28)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.greenMax,bp+32)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.blueGamma,bp+36)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.blueMin,bp+40)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		if ((rv = write_S15Fixed16Number(p->u.formula.blueMax,bp+44)) != 0) {
			sprintf(icp->err,"icmVideoCardGamma_write: write_S15Fixed16Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
	} else {
		sprintf(icp->err,"icmVideoCardGammaTable_write: Unknown gamma format for icmVideoCardGamma");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmViewingConditions_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmVideoCardGamma_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmVideoCardGamma *p = (icmVideoCardGamma *)pp;
	int c,i;

	if (verb <= 0)
		return;

	if (p->tagType == icmVideoCardGammaTableType) {
		op->gprintf(op,"VideoCardGammaTable:\n");
		op->gprintf(op,"  channels  = %d\n", p->u.table.channels);
		op->gprintf(op,"  entries   = %d\n", p->u.table.entryCount);
		op->gprintf(op,"  entrysize = %d\n", p->u.table.entrySize);
		if (verb >= 2) {
			/* dump array contents also */
			for (c=0; c<p->u.table.channels; c++) {
				op->gprintf(op,"  channel #%d\n",c);
				for (i=0; i<p->u.table.entryCount; i++) {
					if (p->u.table.entrySize == 1) {
						op->gprintf(op,"    %d: %d\n",i,((ORD8 *)p->u.table.data)[c*p->u.table.entryCount+i]);
					}
					else if (p->u.table.entrySize == 2) {
						op->gprintf(op,"    %d: %d\n",i,((ORD16 *)p->u.table.data)[c*p->u.table.entryCount+i]);
					}
				}
			}
		}
	} else if (p->tagType == icmVideoCardGammaFormulaType) {
		op->gprintf(op,"VideoCardGammaFormula:\n");
		op->gprintf(op,"  red gamma   = %.8f\n", p->u.formula.redGamma);
		op->gprintf(op,"  red min     = %.8f\n", p->u.formula.redMin);
		op->gprintf(op,"  red max     = %.8f\n", p->u.formula.redMax);
		op->gprintf(op,"  green gamma = %.8f\n", p->u.formula.greenGamma);
		op->gprintf(op,"  green min   = %.8f\n", p->u.formula.greenMin);
		op->gprintf(op,"  green max   = %.8f\n", p->u.formula.greenMax);
		op->gprintf(op,"  blue gamma  = %.8f\n", p->u.formula.blueGamma);
		op->gprintf(op,"  blue min    = %.8f\n", p->u.formula.blueMin);
		op->gprintf(op,"  blue max    = %.8f\n", p->u.formula.blueMax);
	} else {
		op->gprintf(op,"  Unknown tag format\n");
	}
}

/* Allocate variable sized data elements */
static int icmVideoCardGamma_allocate(
	icmBase *pp
) {
	icmVideoCardGamma *p = (icmVideoCardGamma *)pp;
	icc *icp = p->icp;
	unsigned int size;

	/* note: allocation is only relevant for table type
	 * and in that case the channels, entryCount, and entrySize
	 * fields must all be set prior to getting here
	 */

	if (p->tagType == icmVideoCardGammaTableType) {
		size = sat_mul(p->u.table.channels, p->u.table.entryCount);
		switch (p->u.table.entrySize) {
			case 1:
				size = sat_mul(size, sizeof(ORD8));
				break;
			case 2:
				size = sat_mul(size, sizeof(unsigned short));
				break;
			default:
				sprintf(icp->err,"icmVideoCardGamma_alloc: unsupported table entry size");
				return icp->errc = 1;
		}
		if (size == UINT_MAX) {
			sprintf(icp->err,"icmVideoCardGamma_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->u.table.data != NULL)
			icp->al->free(icp->al, p->u.table.data);
		if ((p->u.table.data = (void*) icp->al->malloc(icp->al, size)) == NULL) {
			sprintf(icp->err,"icmVideoCardGamma_alloc: malloc() of table data failed");
			return icp->errc = 2;
		}
	}

	return 0;
}

/* Read a value */
static double icmVideoCardGamma_lookup(
	icmVideoCardGamma *p,
	int chan,		/* Channel, 0, 1 or 2 */
	double iv		/* Input value 0.0 - 1.0 */
) {
	double ov = 0.0;

	if (chan < 0 || chan > (p->u.table.channels-1)
	 || iv < 0.0 || iv > 1.0)
		return iv;

	if (p->tagType == icmVideoCardGammaTableType && p->u.table.entryCount == 0) {
		/* Deal with siliness */
		ov = iv;
	} else if (p->tagType == icmVideoCardGammaTableType) {
		/* Use linear interpolation */
		unsigned int ix;
		double val0, val1, w;
		double inputEnt_1 = (double)(p->u.table.entryCount-1);

		val0 = iv * inputEnt_1;
		if (val0 < 0.0)
			val0 = 0.0;
		else if (val0 > inputEnt_1)
			val0 = inputEnt_1;
		ix = (unsigned int)floor(val0);		/* Coordinate */
		if (ix > (p->u.table.entryCount-2))
			ix = (p->u.table.entryCount-2);
		w = val0 - (double)ix;		/* weight */
		if (p->u.table.entrySize == 1) {
			val0 = ((ORD8 *)p->u.table.data)[chan * p->u.table.entryCount + ix]/255.0;
			val1 = ((ORD8 *)p->u.table.data)[chan * p->u.table.entryCount + ix + 1]/255.0;
		} else if (p->u.table.entrySize == 2) {
			val0 = ((ORD16 *)p->u.table.data)[chan * p->u.table.entryCount + ix]/65535.0;
			val1 = ((ORD16 *)p->u.table.data)[chan * p->u.table.entryCount + ix + 1]/65535.0;
		} else {
			val0 = val1 = iv;
		}
		ov = val0 + w * (val1 - val0);

	} else if (p->tagType == icmVideoCardGammaFormulaType) {
		double min, max, gam;

		if (iv == 0) {
			min = p->u.formula.redMin;
			max = p->u.formula.redMax;
			gam = p->u.formula.redGamma;
		} else if (iv == 1) {
			min = p->u.formula.greenMin;
			max = p->u.formula.greenMax;
			gam = p->u.formula.greenGamma;
		} else {
			min = p->u.formula.blueMin;
			max = p->u.formula.blueMax;
			gam = p->u.formula.blueGamma;
		}

		/* The Apple OSX doco confirms this is the formula */
		ov = min + (max - min) * pow(iv, gam);
	}
	return ov;
}

/* Free all storage in the object */
static void icmVideoCardGamma_delete(
	icmBase *pp
) {
	icmVideoCardGamma *p = (icmVideoCardGamma *)pp;
	icc *icp = p->icp;

	if (p->tagType == icmVideoCardGammaTableType && p->u.table.data != NULL)
		icp->al->free(icp->al, p->u.table.data);

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmVideoCardGamma(
	icc *icp
) {
	icmVideoCardGamma *p;
	if ((p = (icmVideoCardGamma *) icp->al->calloc(icp->al,1,sizeof(icmVideoCardGamma))) == NULL)
		return NULL;
	p->ttype    = icSigVideoCardGammaType;
	p->refcount = 1;
	p->get_size = icmVideoCardGamma_get_size;
	p->read     = icmVideoCardGamma_read;
	p->write    = icmVideoCardGamma_write;
	p->lookup   = icmVideoCardGamma_lookup;
	p->dump     = icmVideoCardGamma_dump;
	p->allocate = icmVideoCardGamma_allocate;
	p->del      = icmVideoCardGamma_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* ViewingConditions */

/* Return the number of bytes needed to write this tag */
static unsigned int icmViewingConditions_get_size(
	icmBase *pp
) {
	unsigned int len = 0;
	len = sat_add(len, 8);			/* 8 bytes for tag and padding */
	len = sat_add(len, 12);			/* 12 for XYZ of illuminant */
	len = sat_add(len, 12);			/* 12 for XYZ of surround */
	len = sat_add(len, 4);			/* 4 for illuminant type */
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmViewingConditions_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmViewingConditions *p = (icmViewingConditions *)pp;
	icc *icp = p->icp;
	int rv;
	char *bp, *buf;

	if (len < 36) {
		sprintf(icp->err,"icmViewingConditions_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmViewingConditions_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmViewingConditions_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmViewingConditions_read: Wrong tag type for icmViewingConditions");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read the XYZ values for the illuminant */
	if ((rv = read_XYZNumber(&p->illuminant, bp+8)) != 0) {
		sprintf(icp->err,"icmViewingConditions: read_XYZNumber error");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Read the XYZ values for the surround */
	if ((rv = read_XYZNumber(&p->surround, bp+20)) != 0) {
		sprintf(icp->err,"icmViewingConditions: read_XYZNumber error");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Read the encoded standard illuminant */
	p->stdIlluminant = (icIlluminant)read_SInt32Number(bp + 32);

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmViewingConditions_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmViewingConditions *p = (icmViewingConditions *)pp;
	icc *icp = p->icp;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmViewingConditions_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmViewingConditions_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmViewingConditions_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */

	/* Write the XYZ values for the illuminant */
	if ((rv = write_XYZNumber(&p->illuminant, bp+8)) != 0) {
		sprintf(icp->err,"icmViewingConditions: write_XYZNumber error");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write the XYZ values for the surround */
	if ((rv = write_XYZNumber(&p->surround, bp+20)) != 0) {
		sprintf(icp->err,"icmViewingConditions: write_XYZNumber error");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write the encoded standard illuminant */
	if ((rv = write_SInt32Number((int)p->stdIlluminant, bp + 32)) != 0) {
		sprintf(icp->err,"icmViewingConditionsa_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmViewingConditions_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmViewingConditions_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmViewingConditions *p = (icmViewingConditions *)pp;
	if (verb <= 0)
		return;

	op->gprintf(op,"Viewing Conditions:\n");
	op->gprintf(op,"  XYZ value of illuminant in cd/m^2 = %s\n", string_XYZNumber(&p->illuminant));
	op->gprintf(op,"  XYZ value of surround in cd/m^2   = %s\n", string_XYZNumber(&p->surround));
	op->gprintf(op,"  Illuminant type = %s\n", string_Illuminant(p->stdIlluminant));
}

/* Allocate variable sized data elements */
static int icmViewingConditions_allocate(
	icmBase *pp
) {
	/* Nothing to do */
	return 0;
}

/* Free all storage in the object */
static void icmViewingConditions_delete(
	icmBase *pp
) {
	icmViewingConditions *p = (icmViewingConditions *)pp;
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmViewingConditions(
	icc *icp
) {
	icmViewingConditions *p;
	if ((p = (icmViewingConditions *) icp->al->calloc(icp->al,1,sizeof(icmViewingConditions))) == NULL)
		return NULL;
	p->ttype    = icSigViewingConditionsType;
	p->refcount = 1;
	p->get_size = icmViewingConditions_get_size;
	p->read     = icmViewingConditions_read;
	p->write    = icmViewingConditions_write;
	p->dump     = icmViewingConditions_dump;
	p->allocate = icmViewingConditions_allocate;
	p->del      = icmViewingConditions_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ---------------------------------------------------------- */
/* icmCrdInfo object */

/* Return the number of bytes needed to write this tag */
static unsigned int icmCrdInfo_get_size(
	icmBase *pp
) {
	icmCrdInfo *p = (icmCrdInfo *)pp;
	unsigned int len = 0, t;
	len = sat_add(len, 8);				/* 8 bytes for tag and padding */
	len = sat_addadd(len, 4, p->ppsize);	/* Postscript product name */
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		len = sat_addadd(len, 4, p->crdsize[t]);	/* crd names */ 
	}
	return len;
}

/* read the object, return 0 on success, error code on fail */
static int icmCrdInfo_read(
	icmBase *pp,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icmCrdInfo *p = (icmCrdInfo *)pp;
	icc *icp = p->icp;
	unsigned int t;
	int rv;
	char *bp, *buf, *end;

	if (len < 28) {
		sprintf(icp->err,"icmCrdInfo_read: Tag too small to be legal");
		return icp->errc = 1;
	}

	/* Allocate a file read buffer */
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmCrdInfo_read: malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;
	end = buf + len;

	/* Read portion of file into buffer */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, bp, 1, len) != len) {
		sprintf(icp->err,"icmCrdInfo_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Read type descriptor from the buffer */
	if (((icTagTypeSignature)read_SInt32Number(bp)) != p->ttype) {
		sprintf(icp->err,"icmCrdInfo_read: Wrong tag type for icmCrdInfo");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	bp = bp + 8;

	/* Postscript product name */
	if (bp > end || 4 > (end - bp)) {
		sprintf(icp->err,"icmCrdInfo_read: Data too short to read Postscript product name");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	p->ppsize = read_UInt32Number(bp);
	bp += 4;
	if (p->ppsize > 0) {
		if (p->ppsize > (end - bp)) {
			sprintf(icp->err,"icmCrdInfo_read: Data to short to read Postscript product string");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		if ((rv = check_null_string(bp,p->ppsize)) == 1) {
			sprintf(icp->err,"icmCrdInfo_read: Postscript product name is not terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		/* Haven't checked if rv == 2 is legal or not */
		if ((rv = p->allocate((icmBase *)p)) != 0) {
			icp->al->free(icp->al, buf);
			return rv;
		}
		memmove((void *)p->ppname, (void *)bp, p->ppsize);
		bp += p->ppsize;
	}
	
	/* CRD names for the four rendering intents */
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		if (bp > end || 4 > (end - bp)) {
			sprintf(icp->err,"icmCrdInfo_read: Data too short to read CRD%d name",t);
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		p->crdsize[t] = read_UInt32Number(bp);
		bp += 4;
		if (p->crdsize[t] > 0) {
			if (p->crdsize[t] > (end - bp)) {
				sprintf(icp->err,"icmCrdInfo_read: Data to short to read CRD%d string",t);
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
			if ((rv = check_null_string(bp,p->crdsize[t])) == 1) {
				sprintf(icp->err,"icmCrdInfo_read: CRD%d name is not terminated",t);
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
			/* Haven't checked if rv == 2 is legal or not */
			if ((rv = p->allocate((icmBase *)p)) != 0) { 
				icp->al->free(icp->al, buf);
				return rv;
			}
			memmove((void *)p->crdname[t], (void *)bp, p->crdsize[t]);
			bp += p->crdsize[t];
		}
	}

	icp->al->free(icp->al, buf);
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmCrdInfo_write(
	icmBase *pp,
	unsigned int of			/* File offset to write from */
) {
	icmCrdInfo *p = (icmCrdInfo *)pp;
	icc *icp = p->icp;
	unsigned int t;
	unsigned int len;
	char *bp, *buf;		/* Buffer to write from */
	int rv;

	/* Allocate a file write buffer */
	if ((len = p->get_size((icmBase *)p)) == UINT_MAX) {
		sprintf(icp->err,"icmCrdInfo_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmCrdInfo_write malloc() failed");
		return icp->errc = 2;
	}
	bp = buf;

	/* Write type descriptor to the buffer */
	if ((rv = write_SInt32Number((int)p->ttype,bp)) != 0) {
		sprintf(icp->err,"icmCrdInfo_write: write_SInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	write_SInt32Number(0,bp+4);			/* Set padding to 0 */
	bp = bp + 8;

	/* Postscript product name */
	if ((rv = write_UInt32Number(p->ppsize,bp)) != 0) {
		sprintf(icp->err,"icmCrdInfo_write: write_UInt32Number() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	bp += 4;
	if (p->ppsize > 0) {
		if ((rv = check_null_string(p->ppname,p->ppsize)) == 1) {
			sprintf(icp->err,"icmCrdInfo_write: Postscript product name is not terminated");
			icp->al->free(icp->al, buf);
			return icp->errc = 1;
		}
		/* Haven't checked if rv == 2 is legal or not */
		memmove((void *)bp, (void *)p->ppname, p->ppsize);
		bp += p->ppsize;
	}

	/* CRD names for the four rendering intents */
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		if ((rv = write_UInt32Number(p->crdsize[t],bp)) != 0) {
			sprintf(icp->err,"icmCrdInfo_write: write_UInt32Number() failed");
			icp->al->free(icp->al, buf);
			return icp->errc = rv;
		}
		bp += 4;
		if (p->ppsize > 0) {
			if ((rv = check_null_string(p->crdname[t],p->crdsize[t])) == 1) {
				sprintf(icp->err,"icmCrdInfo_write: CRD%d name is not terminated",t);
				icp->al->free(icp->al, buf);
				return icp->errc = 1;
			}
			/* Haven't checked if rv == 2 is legal or not */
			memmove((void *)bp, (void *)p->crdname[t], p->crdsize[t]);
			bp += p->crdsize[t];
		}
	}

	/* Write to the file */
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmCrdInfo_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}
	icp->al->free(icp->al, buf);
	return 0;
}

/* Dump a text description of the object */
static void icmCrdInfo_dump(
	icmBase *pp,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	icmCrdInfo *p = (icmCrdInfo *)pp;
	unsigned int i, r, c, size, t;

	if (verb <= 0)
		return;

	op->gprintf(op,"PostScript Product name and CRD names:\n");

	op->gprintf(op,"  Product name:\n");
	op->gprintf(op,"    No. chars = %lu\n",p->ppsize);
	
	size = p->ppsize > 0 ? p->ppsize-1 : 0;
	i = 0;
	for (r = 1;; r++) {		/* count rows */
		if (i >= size) {
			op->gprintf(op,"\n");
			break;
		}
		if (r > 1 && verb < 2) {
			op->gprintf(op,"...\n");
			break;			/* Print 1 row if not verbose */
		}
		c = 1;
		op->gprintf(op,"      0x%04lx: ",i);
		c += 10;
		while (i < size && c < 73) {
			if (isprint(p->ppname[i])) {
				op->gprintf(op,"%c",p->ppname[i]);
				c++;
			} else {
				op->gprintf(op,"\\%03o",p->ppname[i]);
				c += 4;
			}
			i++;
		}
		if (i < size)
			op->gprintf(op,"\n");
	}

	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		op->gprintf(op,"  CRD%ld name:\n",t);
		op->gprintf(op,"    No. chars = %lu\n",p->crdsize[t]);
		
		size = p->crdsize[t] > 0 ? p->crdsize[t]-1 : 0;
		i = 0;
		for (r = 1;; r++) {		/* count rows */
			if (i >= size) {
				op->gprintf(op,"\n");
				break;
			}
			if (r > 1 && verb < 2) {
				op->gprintf(op,"...\n");
				break;			/* Print 1 row if not verbose */
			}
			c = 1;
			op->gprintf(op,"      0x%04lx: ",i);
			c += 10;
			while (i < size && c < 73) {
				if (isprint(p->crdname[t][i])) {
					op->gprintf(op,"%c",p->crdname[t][i]);
					c++;
				} else {
					op->gprintf(op,"\\%03o",p->crdname[t][i]);
					c += 4;
				}
				i++;
			}
			if (i < size)
				op->gprintf(op,"\n");
		}
	}
}

/* Allocate variable sized data elements */
static int icmCrdInfo_allocate(
	icmBase *pp
) {
	icmCrdInfo *p = (icmCrdInfo *)pp;
	icc *icp = p->icp;
	unsigned int t;

	if (p->ppsize != p->_ppsize) {
		if (ovr_mul(p->ppsize, sizeof(char))) {
			sprintf(icp->err,"icmCrdInfo_alloc: size overflow");
			return icp->errc = 1;
		}
		if (p->ppname != NULL)
			icp->al->free(icp->al, p->ppname);
		if ((p->ppname = (char *) icp->al->calloc(icp->al, p->ppsize, sizeof(char))) == NULL) {
			sprintf(icp->err,"icmCrdInfo_alloc: malloc() of string data failed");
			return icp->errc = 2;
		}
		p->_ppsize = p->ppsize;
	}
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		if (p->crdsize[t] != p->_crdsize[t]) {
			if (ovr_mul(p->crdsize[t], sizeof(char))) {
				sprintf(icp->err,"icmCrdInfo_alloc: size overflow");
				return icp->errc = 1;
			}
			if (p->crdname[t] != NULL)
				icp->al->free(icp->al, p->crdname[t]);
			if ((p->crdname[t] = (char *) icp->al->calloc(icp->al, p->crdsize[t], sizeof(char))) == NULL) {
				sprintf(icp->err,"icmCrdInfo_alloc: malloc() of CRD%d name string failed",t);
				return icp->errc = 2;
			}
			p->_crdsize[t] = p->crdsize[t];
		}
	}
	return 0;
}

/* Free all storage in the object */
static void icmCrdInfo_delete(
	icmBase *pp
) {
	icmCrdInfo *p = (icmCrdInfo *)pp;
	icc *icp = p->icp;
	unsigned int t;

	if (p->ppname != NULL)
		icp->al->free(icp->al, p->ppname);
	for (t = 0; t < 4; t++) {	/* For all 4 intents */
		if (p->crdname[t] != NULL)
			icp->al->free(icp->al, p->crdname[t]);
	}
	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmBase *new_icmCrdInfo(
	icc *icp
) {
	icmCrdInfo *p;
	if ((p = (icmCrdInfo *) icp->al->calloc(icp->al,1,sizeof(icmCrdInfo))) == NULL)
		return NULL;
	p->ttype    = icSigCrdInfoType;
	p->refcount = 1;
	p->get_size = icmCrdInfo_get_size;
	p->read     = icmCrdInfo_read;
	p->write    = icmCrdInfo_write;
	p->dump     = icmCrdInfo_dump;
	p->allocate = icmCrdInfo_allocate;
	p->del      = icmCrdInfo_delete;
	p->icp      = icp;

	return (icmBase *)p;
}

/* ========================================================== */
/* icmHeader object */
/* ========================================================== */

/* Return the number of bytes needed to write this tag */
static unsigned int icmHeader_get_size(
	icmHeader *p
) {
	return 128;		/* By definition */
}

/* read the object, return 0 on success, error code on fail */
static int icmHeader_read(
	icmHeader *p,
	unsigned int len,		/* tag length */
	unsigned int of		/* start offset within file */
) {
	icc *icp = p->icp;
	char *buf;
	unsigned int tt;
	int rv = 0;
	
	if (len != 128) {
		sprintf(icp->err,"icmHeader_read: Length expected to be 128");
		return icp->errc = 1;
	}

	if ((buf = (char *) icp->al->malloc(icp->al, len)) == NULL) {
		sprintf(icp->err,"icmHeader_read: malloc() failed");
		return icp->errc = 2;
	}
	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->read(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmHeader_read: fseek() or fread() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Check that the magic number is right */
	tt = read_SInt32Number(buf+36);
	if (tt != icMagicNumber) {				/* Check magic number */
		sprintf(icp->err,"icmHeader_read: wrong magic number 0x%x",tt);
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}

	/* Fill in the in-memory structure */
	p->size  = read_UInt32Number(buf + 0);	/* Profile size in bytes */
	if (p->size < (128 + 4)) {
		sprintf(icp->err,"icmHeader_read: file size %d too small to be legal",p->size);
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
    p->cmmId = read_SInt32Number(buf + 4);	/* CMM for profile */
	tt       = read_UInt8Number(buf + 8);	/* Raw major version number */
    p->majv  = (tt >> 4) * 10 + (tt & 0xf);	/* Integer major version number */
	tt       = read_UInt8Number(buf + 9);	/* Raw minor & bug fix version numbers */
    p->minv  = (tt >> 4);					/* Integer minor version number */
    p->bfv   = (tt & 0xf);					/* Integer bug fix version number */
	if (p->majv < 3) {						/* Set version class */
    	if (p->minv >= 4)
			icp->ver = icmVersion2_4;
		else if (p->minv >= 3)
			icp->ver = icmVersion2_3;
		else
			icp->ver = icmVersionDefault;
	} else
		icp->ver = icmVersion4_1;
	p->deviceClass = (icProfileClassSignature)
	           read_SInt32Number(buf + 12);	/* Type of profile */
    p->colorSpace = (icColorSpaceSignature)
	           read_SInt32Number(buf + 16);	/* Color space of data */
    p->pcs = (icColorSpaceSignature)
	           read_SInt32Number(buf + 20);	/* PCS: XYZ or Lab */
	if ((rv = read_DateTimeNumber(&p->date, buf + 24)) != 0) {	/* Creation Date */
		sprintf(icp->err,"icmHeader_read: read_DateTimeNumber corrupted");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
    p->platform = (icPlatformSignature)
	           read_SInt32Number(buf + 40);			/* Primary platform */
    p->flags = read_UInt32Number(buf + 44);			/* Various bits */
    p->manufacturer = read_SInt32Number(buf + 48); /* Dev manufacturer */
    p->model = read_SInt32Number(buf + 52);			/* Dev model */
    read_UInt64Number(&p->attributes, buf + 56);	/* Device attributes */
	p->renderingIntent = (icRenderingIntent)
	           read_SInt32Number(buf + 64);	/* Rendering intent */
	if ((rv = read_XYZNumber(&p->illuminant, buf + 68)) != 0) {	/* Profile illuminant */
		sprintf(icp->err,"icmHeader_read: read_XYZNumber error");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
    p->creator = read_SInt32Number(buf + 80);	/* Profile creator */

	for (tt = 0; tt < 16; tt++)					/* Profile ID */
		p->id[tt] = icp->ver >= icmVersion4_1 ? read_UInt8Number(buf + 84 + tt) : 0;

	icp->al->free(icp->al, buf);

#ifndef ENABLE_V4
	if (icp->ver >= icmVersion4_1) {
		sprintf(icp->err,"icmHeader_read: ICC V4 not supported!");
		return icp->errc = 1;
	}
#endif
	return 0;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
static int icmHeader_write(
	icmHeader *p,
	unsigned int of,		/* File offset to write from */
	int doid				/* Flag, nz = writing to compute ID */
) {
	icc *icp = p->icp;
	char *buf;		/* Buffer to write from */
	unsigned int len;
	unsigned int tt;
	int rv = 0;

	/* Allocate a file write buffer */
	if ((len = p->get_size(p)) == UINT_MAX) {
		sprintf(icp->err,"icmHeader_write get_size overflow");
		return icp->errc = 1;
	}
	if ((buf = (char *) icp->al->calloc(icp->al,1,len)) == NULL) {	/* Zero it - some CMS are fussy */
		sprintf(icp->err,"icmHeader_write calloc() failed");
		return icp->errc = 2;
	}

	/* Fill in the write buffer */
	if ((rv = write_UInt32Number(p->size, buf + 0)) != 0) {	/* Profile size in bytes */
		sprintf(icp->err,"icmHeader_write: profile size");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}

    if ((rv = write_SInt32Number(p->cmmId, buf + 4)) != 0) {	/* CMM for profile */
		sprintf(icp->err,"icmHeader_write: cmmId");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if (p->majv < 0 || p->majv > 99			/* Sanity check version numbers */
	 || p->minv < 0 || p->minv > 9
	 || p->bfv  < 0 || p->bfv  > 9) {
		sprintf(icp->err,"icmHeader_write: version number");
		icp->al->free(icp->al, buf);
		return icp->errc = 1;
	}
	// ~~~ Hmm. We're not checking ->ver is >= corresponding header version number ~~
	tt = ((p->majv/10) << 4) + (p->majv % 10);
	if ((rv = write_UInt8Number(tt, buf + 8)) != 0) {	/* Raw major version number */
		sprintf(icp->err,"icmHeader_write: Uint8Number major version");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
    tt = (p->minv << 4) + p->bfv;
	if ((rv = write_UInt8Number(tt, buf + 9)) != 0) {	/* Raw minor/bug fix version numbers */
		sprintf(icp->err,"icmHeader_write: Uint8Number minor/bug fix");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_SInt32Number((int)p->deviceClass, buf + 12)) != 0) {	/* Type of profile */
		sprintf(icp->err,"icmHeader_write: SInt32Number deviceClass");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_SInt32Number((int)p->colorSpace, buf + 16)) != 0) {	/* Color space of data */
		sprintf(icp->err,"icmHeader_write: SInt32Number data color space");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_SInt32Number((int)p->pcs, buf + 20)) != 0) {		/* PCS: XYZ or Lab */
		sprintf(icp->err,"icmHeader_write: SInt32Number PCS");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_DateTimeNumber(&p->date, buf + 24)) != 0) {		/* Creation Date */
		sprintf(icp->err,"icmHeader_write: DateTimeNumber creation");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_SInt32Number(icMagicNumber, buf+36)) != 0) {		/* Magic number */
		sprintf(icp->err,"icmHeader_write: SInt32Number magic");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_SInt32Number((int)p->platform, buf + 40)) != 0) {	/* Primary platform */
		sprintf(icp->err,"icmHeader_write: SInt32Number platform");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
    if ((rv = write_UInt32Number(doid ? 0 : p->flags, buf + 44)) != 0) { /* Various flag bits */
		sprintf(icp->err,"icmHeader_write: UInt32Number flags");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
    if ((rv = write_SInt32Number(p->manufacturer, buf + 48)) != 0) { /* Dev manufacturer */
		sprintf(icp->err,"icmHeader_write: SInt32Number manufaturer");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
    if ((write_SInt32Number(p->model, buf + 52)) != 0) {				/* Dev model */
		sprintf(icp->err,"icmHeader_write: SInt32Number model");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
    if ((rv = write_UInt64Number(&p->attributes, buf + 56)) != 0) {	/* Device attributes */
		sprintf(icp->err,"icmHeader_write: UInt64Number attributes");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_SInt32Number(doid ? 0 : (int)p->renderingIntent, buf + 64)) != 0) { /* Rendering intent */
		sprintf(icp->err,"icmHeader_write: SInt32Number rendering intent");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if ((rv = write_XYZNumber(&p->illuminant, buf + 68)) != 0) {		/* Profile illuminant */
		sprintf(icp->err,"icmHeader_write: XYZNumber illuminant");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
    if ((rv = write_SInt32Number(p->creator, buf + 80)) != 0) {		/* Profile creator */
		sprintf(icp->err,"icmHeader_write: SInt32Number creator");
		icp->al->free(icp->al, buf);
		return icp->errc = rv;
	}
	if (doid == 0 && icp->ver >= icmVersion4_1) {	/* ID is V4.0+ feature */
		for (tt = 0; tt < 16; tt++) {
		    if ((rv = write_UInt8Number(p->id[tt], buf + 84 + tt)) != 0) { /* Profile ID */
				sprintf(icp->err,"icmHeader_write: UInt8Number creator");
				icp->al->free(icp->al, buf);
				return icp->errc = rv;
			}
		}
	}

	if (   icp->fp->seek(icp->fp, of) != 0
	    || icp->fp->write(icp->fp, buf, 1, len) != len) {
		sprintf(icp->err,"icmHeader_write fseek() or fwrite() failed");
		icp->al->free(icp->al, buf);
		return icp->errc = 2;
	}

	icp->al->free(icp->al, buf);
	return rv;
}

static void icmHeader_dump(
	icmHeader *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	int i;
	if (verb <= 0)
		return;

	op->gprintf(op,"Header:\n");
	op->gprintf(op,"  size         = %d bytes\n",p->size);
	op->gprintf(op,"  CMM          = %s\n",tag2str(p->cmmId));
	op->gprintf(op,"  Version      = %d.%d.%d\n",p->majv, p->minv, p->bfv);
	op->gprintf(op,"  Device Class = %s\n", string_ProfileClassSignature(p->deviceClass));
	op->gprintf(op,"  Color Space  = %s\n", string_ColorSpaceSignature(p->colorSpace));
	op->gprintf(op,"  Conn. Space  = %s\n", string_ColorSpaceSignature(p->pcs));
	op->gprintf(op,"  Date, Time   = %s\n", string_DateTimeNumber(&p->date));
	op->gprintf(op,"  Platform     = %s\n", string_PlatformSignature(p->platform));
	op->gprintf(op,"  Flags        = %s\n", string_ProfileHeaderFlags(p->flags));
	op->gprintf(op,"  Dev. Mnfctr. = %s\n", tag2str(p->manufacturer));	/* ~~~ */
	op->gprintf(op,"  Dev. Model   = %s\n", tag2str(p->model));	/* ~~~ */
	op->gprintf(op,"  Dev. Attrbts = %s\n", string_DeviceAttributes(p->attributes.l));
	op->gprintf(op,"  Rndrng Intnt = %s\n", string_RenderingIntent(p->renderingIntent));
	op->gprintf(op,"  Illuminant   = %s\n", string_XYZNumber_and_Lab(&p->illuminant));
	op->gprintf(op,"  Creator      = %s\n", tag2str(p->creator));	/* ~~~ */
	if (p->icp->ver >= icmVersion4_1) {	/* V4.0+ feature */
		for (i = 0; i < 16; i++) {		/* Check if ID has been set */
			if (p->id[i] != 0)
				break;
		}
		if (i < 16)
			op->gprintf(op,"  ID           = %02X%02X%02X%02X%02X%02X%02X%02X"
			                               "%02X%02X%02X%02X%02X%02X%02X%02X\n",
				p->id[0], p->id[1], p->id[2], p->id[3], p->id[4], p->id[5], p->id[6], p->id[7],
				p->id[8], p->id[9], p->id[10], p->id[11], p->id[12], p->id[13], p->id[14], p->id[15]);
		else
			op->gprintf(op,"  ID           = <Not set>\n");
	}
	op->gprintf(op,"\n");
}

static void icmHeader_delete(
	icmHeader *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* Create an empty object. Return null on error */
static icmHeader *new_icmHeader(
	icc *icp
) {
	icmHeader *p;
	if ((p = (icmHeader *) icp->al->calloc(icp->al,1,sizeof(icmHeader))) == NULL)
		return NULL;
	p->icp      = icp;
	p->get_size = icmHeader_get_size;
	p->read     = icmHeader_read;
	p->write    = icmHeader_write;
	p->dump     = icmHeader_dump;
	p->del      = icmHeader_delete;

	return p;
}

/* ---------------------------------------------------------- */
/* Type vector table. Match the Tag type against the object creator */
static struct {
	icTagTypeSignature  ttype;			/* The tag type signature */
	icmBase *              (*new_obj)(icc *icp);
} typetable[] = {
	{icSigColorantTableType,       new_icmColorantTable},
	{icmSigAltColorantTableType,   new_icmColorantTable},
	{icSigCrdInfoType,             new_icmCrdInfo},
	{icSigCurveType,               new_icmCurve},
	{icSigDataType,                new_icmData},
	{icSigDateTimeType,            new_icmDateTimeNumber},
	{icSigLut16Type,               new_icmLut},
	{icSigLut8Type,                new_icmLut},
	{icSigMeasurementType,         new_icmMeasurement},
	{icSigNamedColorType,          new_icmNamedColor},
	{icSigNamedColor2Type,         new_icmNamedColor},
	{icSigProfileSequenceDescType, new_icmProfileSequenceDesc},
	{icSigS15Fixed16ArrayType,     new_icmS15Fixed16Array},
	{icSigScreeningType,           new_icmScreening},
	{icSigSignatureType,           new_icmSignature},
	{icSigTextDescriptionType,     new_icmTextDescription},
	{icSigTextType,                new_icmText},
	{icSigU16Fixed16ArrayType,     new_icmU16Fixed16Array},
	{icSigUcrBgType,               new_icmUcrBg},
	{icSigVideoCardGammaType,      new_icmVideoCardGamma},
	{icSigUInt16ArrayType,         new_icmUInt16Array},
	{icSigUInt32ArrayType,         new_icmUInt32Array},
	{icSigUInt64ArrayType,         new_icmUInt64Array},
	{icSigUInt8ArrayType,          new_icmUInt8Array},
	{icSigViewingConditionsType,   new_icmViewingConditions},
	{icSigXYZArrayType,            new_icmXYZArray},
	{icMaxEnumType,                NULL}
}; 

/* Table that lists the legal Types for each Tag Signature */
static struct {
	icTagSignature      sig;
	icTagTypeSignature  ttypes[4];			/* Arbitrary max of 4 */
} sigtypetable[] = {
	{icSigAToB0Tag,					{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigAToB1Tag,					{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigAToB2Tag,					{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigBlueColorantTag,			{icSigXYZType,icMaxEnumType}},
	{icSigBlueTRCTag,				{icSigCurveType,icMaxEnumType}},
	{icSigBToA0Tag,					{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigBToA1Tag,					{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigBToA2Tag,					{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigCalibrationDateTimeTag,	{icSigDateTimeType,icMaxEnumType}},
	{icSigChromaticAdaptationTag,	{icSigS15Fixed16ArrayType,icMaxEnumType}},
	{icSigCharTargetTag,			{icSigTextType,icMaxEnumType}},
	{icSigColorantTableTag,         {icSigColorantTableType,icmSigAltColorantTableType,
									                                     icMaxEnumType}},
	{icSigColorantTableOutTag,      {icSigColorantTableType,icmSigAltColorantTableType,
									                                     icMaxEnumType}},
	{icSigCopyrightTag,				{icSigTextType,icMaxEnumType}},
	{icSigCrdInfoTag,				{icSigCrdInfoType,icMaxEnumType}},
	{icSigDeviceMfgDescTag,			{icSigTextDescriptionType,icMaxEnumType}},
	{icSigDeviceModelDescTag,		{icSigTextDescriptionType,icMaxEnumType}},
	{icSigGamutTag,					{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigGrayTRCTag,				{icSigCurveType,icMaxEnumType}},
	{icSigGreenColorantTag,			{icSigXYZType,icMaxEnumType}},
	{icSigGreenTRCTag,				{icSigCurveType,icMaxEnumType}},
	{icSigLuminanceTag,				{icSigXYZType,icMaxEnumType}},
	{icSigMeasurementTag,			{icSigMeasurementType,icMaxEnumType}},
	{icSigMediaBlackPointTag,		{icSigXYZType,icMaxEnumType}},
	{icSigMediaWhitePointTag,		{icSigXYZType,icMaxEnumType}},
	{icSigNamedColorTag,			{icSigNamedColorType,icMaxEnumType}},
	{icSigNamedColor2Tag,			{icSigNamedColor2Type,icMaxEnumType}},
	{icSigPreview0Tag,				{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigPreview1Tag,				{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigPreview2Tag,				{icSigLut8Type,icSigLut16Type,icMaxEnumType}},
	{icSigProfileDescriptionTag,	{icSigTextDescriptionType,icMaxEnumType}},
	{icSigProfileSequenceDescTag,	{icSigProfileSequenceDescType,icMaxEnumType}},
	{icSigPs2CRD0Tag,				{icSigDataType,icMaxEnumType}},
	{icSigPs2CRD1Tag,				{icSigDataType,icMaxEnumType}},
	{icSigPs2CRD2Tag,				{icSigDataType,icMaxEnumType}},
	{icSigPs2CRD3Tag,				{icSigDataType,icMaxEnumType}},
	{icSigPs2CSATag,				{icSigDataType,icMaxEnumType}},
	{icSigPs2RenderingIntentTag,	{icSigDataType,icMaxEnumType}},
	{icSigRedColorantTag,			{icSigXYZType,icMaxEnumType}},
	{icSigRedTRCTag,				{icSigCurveType,icMaxEnumType}},
	{icSigScreeningDescTag,			{icSigTextDescriptionType,icMaxEnumType}},
	{icSigScreeningTag,				{icSigScreeningType,icMaxEnumType}},
	{icSigTechnologyTag,			{icSigSignatureType,icMaxEnumType}},
	{icSigUcrBgTag,					{icSigUcrBgType,icMaxEnumType}},
	{icSigVideoCardGammaTag,		{icSigVideoCardGammaType,icMaxEnumType}},
	{icSigViewingCondDescTag,		{icSigTextDescriptionType,icMaxEnumType}},
	{icSigViewingConditionsTag,		{icSigViewingConditionsType,icMaxEnumType}},

	{icmSigAbsToRelTransSpace,		{icSigS15Fixed16ArrayType,icMaxEnumType}},

	{icMaxEnumTag,					{icMaxEnumType}}
}; 

/* Fake color tag for specifying PCS */
#define icSigPCSData  ((icColorSpaceSignature) 0x50435320L)

/* Table that lists the required tags for various profiles */
static struct {
	icProfileClassSignature sig;		/* Profile signature */
	int      			    chans;		/* Data Color channels, -ve for match but try next, */
										/*          -100 for ignore, -200 for ignore and try next */
	icColorSpaceSignature   colsig;		/* Data Color space signature, icMaxEnumData for ignore, */
										/*                           icSigPCSData for XYZ of Lab */
	icColorSpaceSignature   pcssig;		/* PCS Color space signature, icMaxEnumData for ignore, */
										/*                          icSigPCSData for XYZ or Lab */
	icTagSignature          tags[12];	/* Arbitrary max of 12 */
} tagchecktable[] = {
    {icSigInputClass,      -1, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigGrayTRCTag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigInputClass,      -3, icMaxEnumData, icSigXYZData,
	 	{icSigProfileDescriptionTag,
		 icSigRedColorantTag,
		 icSigGreenColorantTag,
		 icSigBlueColorantTag,
		 icSigRedTRCTag,
		 icSigGreenTRCTag,
		 icSigBlueTRCTag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigInputClass,     -100, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigAToB0Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigDisplayClass,     -1, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigGrayTRCTag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigDisplayClass,     -3, icSigRgbData, icSigXYZData,	/* Rgb or any 3 component space ?? */
	 	{icSigProfileDescriptionTag,
		 icSigRedColorantTag,
		 icSigGreenColorantTag,
		 icSigBlueColorantTag,
		 icSigRedTRCTag,
		 icSigGreenTRCTag,
		 icSigBlueTRCTag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

	/* Non-3 component Display device */
    {icSigDisplayClass,     -100, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigAToB0Tag,					/* BToA doesn't seem to be required, which is strange... */
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigOutputClass,     -1, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigGrayTRCTag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigOutputClass,     -1, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigAToB0Tag,
		 icSigBToA0Tag,
		 icSigGamutTag,
		 icSigAToB1Tag,
		 icSigBToA1Tag,
		 icSigAToB2Tag,
		 icSigBToA2Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigOutputClass,     -2, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigAToB0Tag,
		 icSigBToA0Tag,
		 icSigGamutTag,
		 icSigAToB1Tag,
		 icSigBToA1Tag,
		 icSigAToB2Tag,
		 icSigBToA2Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigOutputClass,     -3, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigAToB0Tag,
		 icSigBToA0Tag,
		 icSigGamutTag,
		 icSigAToB1Tag,
		 icSigBToA1Tag,
		 icSigAToB2Tag,
		 icSigBToA2Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigOutputClass,     -4, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigAToB0Tag,
		 icSigBToA0Tag,
		 icSigGamutTag,
		 icSigAToB1Tag,
		 icSigBToA1Tag,
		 icSigAToB2Tag,
		 icSigBToA2Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigOutputClass,     -100, icMaxEnumData, icSigPCSData,	/* Assumed from Hexachrome examples */
	 	{icSigProfileDescriptionTag,
		 icSigBToA0Tag,
		 icSigBToA1Tag,
		 icSigBToA2Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigLinkClass,      -100, icMaxEnumData, icMaxEnumData,
	 	{icSigProfileDescriptionTag,
		 icSigAToB0Tag,
		 icSigProfileSequenceDescTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigColorSpaceClass,       -100, icMaxEnumData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigBToA0Tag,
		 icSigAToB0Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigAbstractClass,      -100, icSigPCSData, icSigPCSData,
	 	{icSigProfileDescriptionTag,
		 icSigAToB0Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigNamedColorClass,        -200, icMaxEnumData, icMaxEnumData,
	 	{icSigProfileDescriptionTag,
		 icSigNamedColorTag,				/* Not strictly V3.4 */
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

    {icSigNamedColorClass,        -100, icMaxEnumData, icMaxEnumData,
	 	{icSigProfileDescriptionTag,
		 icSigNamedColor2Tag,
		 icSigMediaWhitePointTag,
		 icSigCopyrightTag, icMaxEnumTag}},

	{icMaxEnumClass,-1,icMaxEnumData, icMaxEnumData,		{icMaxEnumTag}}
}; 

/* ------------------------------------------------------------- */

/* Return the current read fp (if any) */
static icmFile *icc_get_rfp(icc *p) {
	return p->fp;
}

/* Change the version to be non-default (ie. not 2.2.0), */
/* e.g. ICC V4 (used for creation) */
/* Return 0 if OK */
/* Return 1 on error */
static int icc_set_version(icc *p, icmICCVersion ver) {

	if (p->header == NULL) {
		sprintf(p->err,"icc_set_version: Header is missing");
		return p->errc = 1;
	}

	switch (ver) {
		case icmVersionDefault:
		    p->header->majv = 2;
			p->header->minv = 2;
			p->header->bfv  = 0;
			break;
		case icmVersion2_3:
		    p->header->majv = 2;
			p->header->minv = 3;
			p->header->bfv  = 0;
			break;
		case icmVersion2_4:
		    p->header->majv = 2;
			p->header->minv = 4;
			p->header->bfv  = 0;
			break;
#ifdef ENABLE_V4
		case icmVersion4_1:
		    p->header->majv = 4;
			p->header->minv = 1;
			p->header->bfv  = 0;
			break;
#endif
		default:
			sprintf(p->err,"icc_set_version: Unsupported version 0x%x",ver);
			return p->errc = 1;
	}
	return 0;
}


/* Check that the ICC profile looks like it will be legal. */
/* Return non-zero and set error string if not */
static int check_icc_legal(
	icc *p
) {
	int i, j;
	icProfileClassSignature  sig;
	icColorSpaceSignature colsig;
	icColorSpaceSignature pcssig;
	int      	          dchans;

	if (p->header == NULL) {
		sprintf(p->err,"icc_check_legal: Header is missing");
		return p->errc = 1;
	}

	sig = p->header->deviceClass;
	colsig = p->header->colorSpace;
	dchans = number_ColorSpaceSignature(colsig);
	pcssig = p->header->pcs;

	/* Find a matching table entry */
	for (i = 0; tagchecktable[i].sig != icMaxEnumType; i++) {
		if (    tagchecktable[i].sig   == sig
		 && (   tagchecktable[i].chans == dchans	/* Exactly matches */
		     || tagchecktable[i].chans == -dchans	/* Exactly matches, but can try next table */
		     || tagchecktable[i].chans < -99)		/* Doesn't have to match or try next table */
		 && (   tagchecktable[i].colsig == colsig
		     || (tagchecktable[i].colsig == icSigPCSData
		         && (colsig == icSigXYZData || colsig == icSigLabData))
		     || tagchecktable[i].colsig == icMaxEnumData)
		 && (   tagchecktable[i].pcssig == pcssig
		     || (tagchecktable[i].pcssig == icSigPCSData
		         && (pcssig == icSigXYZData || pcssig == icSigLabData))
		     || tagchecktable[i].pcssig == icMaxEnumData)) {

			/* Found entry, so now check that all the required tags are present. */
			for (j = 0; tagchecktable[i].tags[j] != icMaxEnumType; j++) {
				if (p->find_tag(p, tagchecktable[i].tags[j]) != 0) {	/* Not present! */
#ifdef NEVER
					printf("icc_check_legal: deviceClass %s is missing required tag %s\n", tag2str(sig), tag2str(tagchecktable[i].tags[j]));
#endif
					if (tagchecktable[i].chans == -200
					 || tagchecktable[i].chans == -dchans) {	/* But can try next table */
						break;
					}
					/* ~~99 Hmm. Should report all possible missing tags from */
					/* previous failed tables ~~~999 */
					sprintf(p->err,"icc_check_legal: deviceClass %s is missing required tag %s",
					               tag2str(sig), tag2str(tagchecktable[i].tags[j]));
					return p->errc = 1;
				}
			}
			if (tagchecktable[i].tags[j] == icMaxEnumType) {
				break;		/* Fount all required tags */
			}
		}
	}

	/* According to the spec. if the deviceClass is:
		an Abstract Class: both in and out color spaces should be PCS
		an Link Class: both in and out color spaces can be any, and should
		    be the input space of the first profile in the link, and the
		    input space of the last profile in the link respectively.
		a Named Class: in and out color spaces are not defined in the spec.
		Input, Display, Output and ColorSpace Classes, input color
		    space can be any, and the output space must be PCS.
		~~ should check this here ???
	*/
	
	return 0;	/* Assume anything is ok */
}


/* read the object, return 0 on success, error code on fail */
/* NOTE: this doesn't read the tag types, they should be read on demand. */
/* NOTE: fp ownership is taken even if the function fails. */
static int icc_read_x(
	icc *p,
	icmFile *fp,			/* File to read from */
	unsigned int of,		/* File offset to read from */
	int take_fp				/* NZ if icc is to take ownership of fp */
) {
	char tcbuf[4];			/* Tag count read buffer */
	unsigned int i, len;
	unsigned int minoff, maxoff;		/* Minimum and maximum offsets of tag data */
	int er = 0;				/* Error code */
	
	p->fp = fp;
	if (take_fp)
		p->del_fp = 1;
	p->of = of;
	if (p->header == NULL) {
		sprintf(p->err,"icc_read: No header defined");
		return p->errc = 1;
	}

	/* Read the header */
	if (p->header->read(p->header, 128, of)) {
		return 1;
	}

	/* Read the tag count */
	if (   p->fp->seek(p->fp, of + 128) != 0
	    || p->fp->read(p->fp, tcbuf, 1, 4) != 4) {
		sprintf(p->err,"icc_read: fseek() or fread() failed on tag count");
		return p->errc = 1;
	}
	p->count = read_UInt32Number(tcbuf);

	/* Sanity check it */
	if (p->count > 357913940	/* (2^32-5)/12 */
	 || (p->count > ((p->header->size - 128 - 4) / 12))) {
		sprintf(p->err,"icc_read: tag count %d is too large to be legal",p->count);
		return p->errc = 1;
	}
	minoff = 128 + 4 + p->count * 12;
	maxoff = p->header->size;

	if (p->count > 0) {
		char *bp, *buf;

		if (ovr_mul(p->count, sizeof(icmTag))) {
			sprintf(p->err,"icc_read: size overflow");
			return p->errc = 1;
		}

		/* Read the table into memory */
		if ((p->data = (icmTag *) p->al->calloc(p->al, p->count, sizeof(icmTag))) == NULL) {
			sprintf(p->err,"icc_read: Tag table malloc() failed");
			return p->errc = 2;
		}
	
		len = sat_mul(p->count, 12);
		if ((buf = (char *) p->al->malloc(p->al, len)) == NULL) {
			sprintf(p->err,"icc_read: Tag table read buffer malloc() failed");
			p->al->free(p->al, p->data);
			p->data = NULL;
			return p->errc = 2;
		}
		if (   p->fp->seek(p->fp, of + 128 + 4) != 0
		    || p->fp->read(p->fp, buf, 1, len) != len) {
			sprintf(p->err,"icc_read: fseek() or fread() failed on tag table");
			p->al->free(p->al, p->data);
			p->data = NULL;
			p->al->free(p->al, buf);
			return p->errc = 1;
		}

		/* Fill in the tag table structure for each tag */
		for (bp = buf, i = 0; i < p->count; i++, bp += 12) {
	    	p->data[i].sig = (icTagSignature)read_SInt32Number(bp + 0);	
	    	p->data[i].offset = read_UInt32Number(bp + 4);
	    	p->data[i].size = read_UInt32Number(bp + 8);	
		}
		p->al->free(p->al, buf);

		/* Check that each tag lies within the nominated space available, */
		/* and has a reasonable size. */
		for (i = 0; i < p->count; i++) {
			if (p->data[i].offset < minoff
			 || p->data[i].offset > maxoff
			 || p->data[i].size < 4
			 || p->data[i].size > (maxoff - minoff)
			 || (p->data[i].offset + p->data[i].size) < p->data[i].offset	/* Overflow */
			 || (p->data[i].offset + p->data[i].size) > p->header->size) {
				sprintf(p->err,"icc_read: tag %d sig %s offset %d size %d is out of range of the nominated file size %d",i,tag2str(p->data[i].sig),p->data[i].offset,p->data[i].size,maxoff);
				p->al->free(p->al, p->data);
				p->data = NULL;
				return p->errc = 1;
			}
		}

		/* Read each tag type */
		for (i = 0; i < p->count; i++) {
			if (   p->fp->seek(p->fp, of + p->data[i].offset) != 0
			    || p->fp->read(p->fp, tcbuf, 1, 4) != 4) {
				sprintf(p->err,"icc_read: fseek() or fread() failed on tag headers");
				p->al->free(p->al, p->data);
				p->data = NULL;
				return p->errc = 1;
			}
	    	p->data[i].ttype = (icTagTypeSignature) read_SInt32Number(tcbuf);	/* Tag type */
	    	p->data[i].objp = NULL;							/* Read on demand */
		}
	}	/* p->count > 0 */

	/* Check if there is an ArgyllCMS 'arts' tag, and setup the wpchtmx[][] matrix from it. */
	{
		icmS15Fixed16Array *artsTag;
	
		if ((artsTag = (icmS15Fixed16Array *)p->read_tag(p, icmSigAbsToRelTransSpace)) != NULL
		 && artsTag->ttype == icSigS15Fixed16ArrayType
		 && artsTag->size >= 9) {
		
			p->wpchtmx[0][0] = artsTag->data[0];
			p->wpchtmx[0][1] = artsTag->data[1];
			p->wpchtmx[0][2] = artsTag->data[2];
			p->wpchtmx[1][0] = artsTag->data[3];
			p->wpchtmx[1][1] = artsTag->data[4];
			p->wpchtmx[1][2] = artsTag->data[5];
			p->wpchtmx[2][0] = artsTag->data[6];
			p->wpchtmx[2][1] = artsTag->data[7];
			p->wpchtmx[2][2] = artsTag->data[8];

			icmInverse3x3(p->iwpchtmx, p->wpchtmx);

			p->useArts = 1;		/* Save it if it was in profile */

		} else {
			/* If an ArgyllCMS created profile, or if it's a Display profile, */
			/* use Bradford. This makes sRGB and AdobeRGB etc. work correctly */
			/* for absolute colorimetic. Note that for display profiles that set */
			/* the WP to D50 and store their chromatic transform in the 'chad' tag, */
			/* (i.e. some V2 profiles and all V4 profiles) this will have no effect */
			/* on the Media Relative WP Transformation since D50 -> D50, and */
			/* the 'chad' tag will be used to set the internal MediaWhite value */
			/* and transform matrix. */
			if (p->header->creator == str2tag("argl")
			 || p->header->deviceClass == icSigDisplayClass) {

				icmCpy3x3(p->wpchtmx, icmBradford);
				icmInverse3x3(p->iwpchtmx, p->wpchtmx);
	
			/* Default to ICC standard Wrong Von Kries */
			/* for non-ArgyllCMS, non-Display profiles. */
			} else {
				icmCpy3x3(p->wpchtmx, icmWrongVonKries);
				icmCpy3x3(p->iwpchtmx, icmWrongVonKries);
			}
		
			p->useArts = 0;		/* Don't save it if it wasn't in profile */
		}

		p->autoWpchtmx = 0;		/* It's been set on reading - don't set automatically */
	}

	/* If this is a Display profile, check if there is a 'chad' tag, and read it */
	/* in if it exists. We will use this latter. */
	{
		icmS15Fixed16Array *chadTag;

	 	if (p->header->deviceClass == icSigDisplayClass
		 && (chadTag = (icmS15Fixed16Array *)p->read_tag(p, icSigChromaticAdaptationTag)) != NULL
		 && chadTag->ttype == icSigS15Fixed16ArrayType
		 && chadTag->size == 9) {
			
			p->chadmx[0][0] = chadTag->data[0];
			p->chadmx[0][1] = chadTag->data[1];
			p->chadmx[0][2] = chadTag->data[2];
			p->chadmx[1][0] = chadTag->data[3];
			p->chadmx[1][1] = chadTag->data[4];
			p->chadmx[1][2] = chadTag->data[5];
			p->chadmx[2][0] = chadTag->data[6];
			p->chadmx[2][1] = chadTag->data[7];
			p->chadmx[2][2] = chadTag->data[8];

			p->chadValid = 1;
	
			p->useChad = 1;		/* Use it when writing */
		}
	}

	return er;
}

/* read the object, return 0 on success, error code on fail */
/* NOTE: this doesn't read the tag types, they should be read on demand. */
/* (backward compatible version) */
static int icc_read(
	icc *p,
	icmFile *fp,			/* File to read from */
	unsigned int of		/* File offset to read from */
) {
	return icc_read_x(p, fp, of, 0);
}

/* Check the profiles ID. We assume the file has already been read. */
/* Return 0 if OK, 1 if no ID to check, 2 if doesn't match, 3 if some other error. */
/* NOTE: this reads the whole file again, to compute the checksum. */
static int icc_check_id(
	icc *p,
	ORD8 *rid		/* Optionaly return computed ID */
) {
	unsigned char buf[128];
	ORD8 id[16];
	icmMD5 *md5 = NULL;
	unsigned int len;
	
	if (p->header == NULL) {
		sprintf(p->err,"icc_check_id: No header defined");
		return p->errc = 3;
	}
	len = p->header->size;		/* Claimed size of profile */

	/* See if there is an ID to compare against */
	for (len = 0; len < 16; len++) {
		if (p->header->id[len] != 0)
			break;
	}
	if (len >= 16) {
		return 1; 
	}

	if ((md5 = new_icmMD5_a(p->al)) == NULL) {
		sprintf(p->err,"icc_check_id: new_icmMD5 failed");
		return p->errc = 3;
	}
		
	/* Check the header */
	if (   p->fp->seek(p->fp, p->of) != 0
	    || p->fp->read(p->fp, buf, 1, 128) != 128) {
		sprintf(p->err,"icc_check_id: fseek() or fread() failed");
		return p->errc = 3;
	}

	/* Zero the appropriate bytes in the header */
	buf[44] = buf[45] = buf[46] = buf[47] = 0;
	buf[64] = buf[65] = buf[66] = buf[67] = 0;
	buf[84] = buf[85] = buf[86] = buf[87] =
	buf[88] = buf[89] = buf[90] = buf[91] =
	buf[92] = buf[93] = buf[94] = buf[95] =
	buf[96] = buf[97] = buf[98] = buf[99] = 0;

	md5->add(md5, buf, 128);

	/* Suck in the rest of the profile */
	for (;len > 0;) {
		unsigned int rsize = 128;
		if (rsize > len)
			rsize = len;
		if (p->fp->read(p->fp, buf, 1, rsize) != rsize) {
			sprintf(p->err,"icc_check_id: fread() failed");
			return p->errc = 3;
		}
		md5->add(md5, buf, rsize);
		len -= rsize;
	}

	md5->get(md5, id);
	md5->del(md5);

	if (rid != NULL) {
		for (len = 0; len < 16; len++)
			rid[len] = id[len];
	}

	/* Check the ID */
	for (len = 0; len < 16; len++) {
		if (p->header->id[len] != id[len])
			break;
	}
	if (len >= 16) {
		return 0;		/* Matched */ 
	}
	return 2;			/* Didn't match */
}

void quantize3x3S15Fixed16(double targ[3], double mat[3][3], double in[3]);

/* Add any automatically created tags. */
/* (Hmm. Should we remove them if they shouldn't be there ?) */
static int icc_add_auto_tags(icc *p) {

	/* If we're using the ArgyllCMS 'arts' tag to record the chromatic */
	/* adapation cone matrix used for the Media Relative WP Transformation, */ 
	/* create it and set it from the wpchtmx[][] matrix. */
	/* Don't write it if there is to 'wtpt' tag (i.e. it's a device link) */
	if (p->useArts
	 && p->find_tag(p, icSigMediaWhitePointTag) == 0)  {
		int rv;
		icmS15Fixed16Array *artsTag;

		/* Make sure no 'arts' tag currently exists */
		if (p->delete_tag(p, icmSigAbsToRelTransSpace) != 0
		 && p->errc != 2) {
			sprintf(p->err,"icc_write: Deleting existing 'arts' tag failed");
			return p->errc = 1;
		}

		/* Add one */
		if ((artsTag = (icmS15Fixed16Array *)p->add_tag(p, icmSigAbsToRelTransSpace,
			                                     icSigS15Fixed16ArrayType)) == NULL) {
			sprintf(p->err,"icc_write: Adding 'arts' tag failed");
			return p->errc = 1;
		}
		artsTag->size = 9;
		if ((rv = artsTag->allocate((icmBase *)artsTag)	) != 0) {
			sprintf(p->err,"icc_write: Allocating 'arts' tag failed");
			return p->errc = 1;
		}

		/* The cone matrix is assumed to be arranged conventionaly for matrix */
		/* times vector multiplication. */
		/* Consistent with ICC usage, the dimension corresponding to the matrix */
		/* rows varies least rapidly while the one corresponding to the matrix */
		/* columns varies most rapidly. */
		artsTag->data[0] = p->wpchtmx[0][0];
		artsTag->data[1] = p->wpchtmx[0][1];
		artsTag->data[2] = p->wpchtmx[0][2];
		artsTag->data[3] = p->wpchtmx[1][0];
		artsTag->data[4] = p->wpchtmx[1][1];
		artsTag->data[5] = p->wpchtmx[1][2];
		artsTag->data[6] = p->wpchtmx[2][0];
		artsTag->data[7] = p->wpchtmx[2][1];
		artsTag->data[8] = p->wpchtmx[2][2];
	}

	/* If this is a Display profile, and we have been told to save it in */
	/* ICCV4 style, then set the media white point tag to D50 and save */
	/* the chromatic adapation matrix to the 'chad' tag. */
	{
		int rv;
		icmXYZArray *whitePointTag;
		icmS15Fixed16Array *chadTag;
		
	 	if (p->header->deviceClass == icSigDisplayClass
		 && p->useChad
		 && (whitePointTag = (icmXYZArray *)p->read_tag(p, icSigMediaWhitePointTag)) != NULL
		 && whitePointTag->ttype == icSigXYZType
		 && whitePointTag->size >= 1) {
	
			/* If we've set this profile, not just read it, */
			/* compute the fromAbs/chad matrix from media white point and cone matrix */
			if (!p->chadValid) {
				double wp[3];
				p->chromAdaptMatrix(p, ICM_CAM_NONE, icmD50, whitePointTag->data[0], p->chadmx);  

				/* Optimally quantize chad matrix to preserver white point */
				icmXYZ2Ary(wp, whitePointTag->data[0]);
				quantize3x3S15Fixed16(icmD50_ary3, p->chadmx, wp);
			}
	
			/* Make sure no 'chad' tag currently exists */
			if (p->delete_tag(p, icSigChromaticAdaptationTag) != 0
			 && p->errc != 2) {
				sprintf(p->err,"icc_write: Deleting existing 'chad' tag failed");
				return p->errc = 1;
			}
	
			/* Add one */
			if ((chadTag = (icmS15Fixed16Array *)p->add_tag(p, icSigChromaticAdaptationTag,
				                                     icSigS15Fixed16ArrayType)) == NULL) {
				sprintf(p->err,"icc_write: Adding 'chad' tag failed");
				return p->errc = 1;
			}
			chadTag->size = 9;
			if ((rv = chadTag->allocate((icmBase *)chadTag)	) != 0) {
				sprintf(p->err,"icc_write: Allocating 'chad' tag failed");
				return p->errc = 1;
			}
	
			/* Save in ICC matrix order */
			chadTag->data[0] = p->chadmx[0][0];
			chadTag->data[1] = p->chadmx[0][1];
			chadTag->data[2] = p->chadmx[0][2];
			chadTag->data[3] = p->chadmx[1][0];
			chadTag->data[4] = p->chadmx[1][1];
			chadTag->data[5] = p->chadmx[1][2];
			chadTag->data[6] = p->chadmx[2][0];
			chadTag->data[7] = p->chadmx[2][1];
			chadTag->data[8] = p->chadmx[2][2];

			/* Set the media white point to D50 */
			whitePointTag->data[0] = icmD50;
		}
	}
	return 0;
}

/* Return the total size needed for the profile. */
/* This will add any automatic tags such as 'arts' tag, */
/* so the current information needs to be final enough */
/* for the automatic tag creation to be correct. */
/* Return 0 on error. */
static unsigned int icc_get_size(
	icc *p
) {
	unsigned int i, size = 0;

	/* Ignore any errors this time */
	icc_add_auto_tags(p);

#ifdef ICM_STRICT
	/* Check that the right tags etc. are present for a legal ICC profile */
	if (check_icc_legal(p) != 0) {
		return 0;
	}
#endif /* ICM_STRICT */

	/* Compute the total size and tag element data offsets */
	if (p->header == NULL) {
		sprintf(p->err,"icc_get_size: No header defined");
		p->errc = 1;
		return 0;
	}

	size = sat_add(size, p->header->get_size(p->header));
	/* Assume header is aligned */
	size = sat_addaddmul(size, 4, p->count, 12);	/* Tag table length */
	size = sat_align(ALIGN_SIZE, size);

	if (size == UINT_MAX) {
		sprintf(p->err,"icc_get_size: size overflow");
		return p->errc = 1;
	}

	/* Reset touched flag for each tag type */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].objp == NULL) {
			sprintf(p->err,"icc_get_size: Internal error - NULL tag element");
			p->errc = 1;
			return 0;
		}
		p->data[i].objp->touched = 0;
	}
	/* Get size for each tag type, skipping links */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].objp->touched == 0) { /* Not alllowed for previously */
			size = sat_add(size, p->data[i].objp->get_size(p->data[i].objp));
			size = sat_align(ALIGN_SIZE, size);
			p->data[i].objp->touched = 1;	/* Don't account for this again */
		}
	}

	return size;	/* Total size needed, or UINT_MAX if overflow */
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
/* NOTE: fp ownership is taken even if the function fails. */
static int icc_write_x(
	icc *p,
	icmFile *fp,		/* File to write to */
	unsigned int of,	/* File offset to write to */
	int take_fp			/* NZ if icc is to take ownership of fp */
) {
	char *bp, *buf;		/* tag table buffer */
	unsigned int len;
	int rv = 0;
	unsigned int i, size = 0;
	unsigned char pbuf[ALIGN_SIZE];

	if ((rv = icc_add_auto_tags(p)) != 0)
		return rv;

	p->fp = fp;			/* Open file pointer */
	if (take_fp)
		p->del_fp = 1;
	p->of = of;			/* Offset of ICC profile */

	/* Compute the total size and tag element data offsets */
	if (p->header == NULL) {
		sprintf(p->err,"icc_write: No header defined");
		return p->errc = 1;
	}

	/* Check that the right tags etc. are present for a legal ICC profile */
	if ((rv = check_icc_legal(p)) != 0) {
		return rv;
	}

	for (i = 0; i < ALIGN_SIZE; i++)
		pbuf[i] = 0;

	size = sat_add(size, p->header->get_size(p->header));
	/* Assume header is aligned */
	len = sat_addmul(4, p->count, 12);	/* Tag table length */
	len = sat_sub(sat_align(ALIGN_SIZE, sat_add(size, len)), size);	/* Aligned size */
	size = sat_align(ALIGN_SIZE, sat_add(size, len));
	
	if (len == UINT_MAX) {
		sprintf(p->err,"icc_write get_size overflow");
		return p->errc = 1;
	}

	/* Allocate memory buffer for tag table */
	if ((buf = (char *) p->al->calloc(p->al, 1, len)) == NULL) {
		sprintf(p->err,"icc_write calloc() failed");
		return p->errc = 2;
	}
	bp = buf;

    if ((rv = write_UInt32Number(p->count, bp)) != 0) {		/* Tag count */
		sprintf(p->err,"icc_write: write_UInt32Number() failed on tag count");
		p->al->free(p->al, buf);
		return p->errc = rv;
	}
	bp += 4;
	/* Reset touched flag for each tag type */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].objp == NULL) {
			sprintf(p->err,"icc_write: Internal error - NULL tag element");
			p->al->free(p->al, buf);
			return p->errc = 1;
		}
		p->data[i].objp->touched = 0;
	}
	/* Set the offset and size for each tag type, create the tag table write data */
	/* and compute the total profile size. */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].objp->touched == 0) {	/* Allocate space for tag type */
			p->data[i].offset = size;			/* Profile relative target */
			p->data[i].size = p->data[i].objp->get_size(p->data[i].objp);
			size = sat_add(size, p->data[i].size);
			p->data[i].pad = sat_sub(sat_align(ALIGN_SIZE, size), size);
			size = sat_align(ALIGN_SIZE, size);
			p->data[i].objp->touched = 1;	/* Allocated space for it */
			if (size == UINT_MAX) {
				sprintf(p->err,"icc_write: size overflow");
				return p->errc = 1;
			}
		} else { /* must be linked - copy allocation */
			unsigned int k;
			for (k = 0; k < p->count; k++) {	/* Find linked tag */
				if (p->data[k].objp == p->data[i].objp)
					break;
			}
			if (k == p->count) {
				sprintf(p->err,"icc_write: corrupted link"); 
				return p->errc = 2;
			}
		    p->data[i].offset = p->data[k].offset;
		    p->data[i].size   = p->data[k].size;
		    p->data[i].pad    = p->data[k].pad;
		}
		/* Write tag table entry for this tag */
		if ((rv = write_SInt32Number((int)p->data[i].sig,bp + 0)) != 0) {
			sprintf(p->err,"icc_write: write_SInt32Number() failed on tag signature");
			p->al->free(p->al, buf);
			return p->errc = rv;
		}
		if ((rv = write_UInt32Number(p->data[i].offset, bp + 4)) != 0) {
			sprintf(p->err,"icc_write: write_UInt32Number() failed on tag offset");
			p->al->free(p->al, buf);
			return p->errc = rv;
		}
		if ((rv = write_UInt32Number(p->data[i].size, bp + 8)) != 0) {
			sprintf(p->err,"icc_write: write_UInt32Number() failed on tag size");
			p->al->free(p->al, buf);
			return p->errc = rv;
		}
		bp += 12;
	}
	p->header->size = size;		/* Record total icc padded size */
	

	/* If V4.0+, Compute the MD5 id for the profile. */
	/* We do this by writing to a fake icmFile */
	if (p->ver >= icmVersion4_1) {
		icmMD5 *md5 = NULL;
		icmFile *ofp, *dfp = NULL;

		if ((md5 = new_icmMD5_a(p->al)) == NULL) {
			sprintf(p->err,"icc_write: new_icmMD5 failed");
			p->al->free(p->al, buf);
			return p->errc = 2;
		}
		
		if ((dfp = new_icmFileMD5_a(md5, p->al)) == NULL) {
			sprintf(p->err,"icc_write: new_icmFileMD5 failed");
			md5->del(md5);
			p->al->free(p->al, buf);
			return p->errc = 2;
		}
		
		ofp = p->fp;
		p->fp = dfp;

		/* Dummy write the header */
		if ((rv = p->header->write(p->header, 0, 1)) != 0) {
			p->al->free(p->al, buf);
			return rv;
		}
	
		/* Dummy write the tag table */
		if (   p->fp->seek(p->fp, 128) != 0
		    || p->fp->write(p->fp, buf, 1, len) != len) {
			sprintf(p->err,"icc_write: seek() or write() failed");
			p->al->free(p->al, buf);
			return p->errc = 1;
		}
	
		/* Dummy write all the tag element data */
		/* (We invert meaning of touched here) */
		for (i = 0; i < p->count; i++) {	/* For all the tag element data */
			if (p->data[i].objp->touched == 0)
				continue;		/* Must be linked, and we've already written it */
			if ((rv = p->data[i].objp->write(p->data[i].objp, p->data[i].offset)) != 0) {
				p->al->free(p->al, buf);
				return rv;
			}
			/* Pad with 0 to next boundary */
			if (p->data[i].pad > 0) {
				if (p->fp->write(p->fp, pbuf, 1, p->data[i].pad) != p->data[i].pad) {
					sprintf(p->err,"icc_write: write() failed");
					p->al->free(p->al, buf);
					return p->errc = 1;
				}
			}
			p->data[i].objp->touched = 0;	/* Written it, so don't write it again. */
		}
	
		if (p->fp->flush(p->fp) != 0) {
			sprintf(p->err,"icc_write flush() failed");
			p->al->free(p->al, buf);
			return p->errc = 1;
		}

		if ((p->errc = ((icmFileMD5 *)dfp)->get_errc(dfp)) != 0) {
			sprintf(p->err,"icc_write compute ID failed with code %d", p->errc);
			p->al->free(p->al, buf);
			return p->errc;
		}

		/* Get the MD5 checksum ID */
		md5->get(md5, p->header->id);

		dfp->del(dfp);
		md5->del(md5);
		p->fp = ofp;

		/* Reset the touched flags */
		for (i = 0; i < p->count; i++)
			p->data[i].objp->touched = 1;
	}

	/* Now write out the profile for real. */
	/* Although it may appear like we're seeking for each element, */
	/* in fact elements will be written in file order. */

	/* Write the header */
	if ((rv = p->header->write(p->header, of, 0)) != 0) {
		p->al->free(p->al, buf);
		return rv;
	}

	/* Write the tag table */
	if (   p->fp->seek(p->fp, of + 128) != 0
	    || p->fp->write(p->fp, buf, 1, len) != len) {
		sprintf(p->err,"icc_write: seek() or write() failed");
		p->al->free(p->al, buf);
		return p->errc = 1;
	}
	p->al->free(p->al, buf);

	/* Write all the tag element data */
	/* (We invert the meaning of touched here) */
	for (i = 0; i < p->count; i++) {	/* For all the tag element data */
		if (p->data[i].objp->touched == 0)
			continue;		/* Must be linked, and we've already written it */
		if ((rv = p->data[i].objp->write(p->data[i].objp, of + p->data[i].offset)) != 0) {
			return rv;
		}
		/* Pad with 0 to next boundary */
		if (p->data[i].pad > 0) {
			if (p->fp->write(p->fp, pbuf, 1, p->data[i].pad) != p->data[i].pad) {
				sprintf(p->err,"icc_write: write() failed");
				return p->errc = 1;
			}
		}
		p->data[i].objp->touched = 0;	/* Written it, so don't write it again. */
	}

	if (p->fp->flush(p->fp) != 0) {
		sprintf(p->err,"icc_write flush() failed");
		return p->errc = 1;
	}

	return rv;
}

/* Write the contents of the object. Return 0 on sucess, error code on failure */
/* (backwards compatible version) */
static int icc_write(
	icc *p,
	icmFile *fp,		/* File to write to */
	unsigned int of	/* File offset to write to */
) {
	return icc_write_x(p, fp, of, 0);
}

/* Create and add a tag with the given signature. */
/* Returns a pointer to the element object */
/* Returns NULL if error - icc->errc will contain */
/* 2 on system error, */
/* 3 if unknown tag */
/* 4 if duplicate tag */
/* NOTE: that we prevent tag duplication */
/* NOTE: to create a tag type icmSigUnknownType, set ttype to icmSigUnknownType, */
/* and set the actual tag type in icmSigUnknownType->uttype */
static icmBase *icc_add_tag(
	icc *p,
    icTagSignature sig,			/* Tag signature - may be unknown */
	icTagTypeSignature  ttype	/* Tag type */
) {
	icmBase *tp;
	icmBase *nob;
	int i = 0, ok = 1;
	unsigned int j;

	if (ttype != icmSigUnknownType) {   /* Check only for possibly known types */

		/* Check that a known signature has an acceptable type */
		for (i = 0; sigtypetable[i].sig != icMaxEnumType; i++) {
			if (sigtypetable[i].sig == sig) {	/* recognized signature */
				ok = 0;
				for (j = 0; sigtypetable[i].ttypes[j] != icMaxEnumType; j++) {
					if (sigtypetable[i].ttypes[j] == ttype)	/* recognized type */
						ok = 1;
				}
				break;
			}
		}
		if (!ok) {
			sprintf(p->err,"icc_add_tag: wrong tag type for signature");
			p->errc = 1;
			return NULL;
		}

		/* Check that we know how to handle this type */
		for (i = 0; typetable[i].ttype != icMaxEnumType; i++) {
			if (typetable[i].ttype == ttype)
				break;
		}
		if (typetable[i].ttype == icMaxEnumType) {
			sprintf(p->err,"icc_add_tag: unsupported tag type");
			p->errc = 1;
			return NULL;
		}
	}

	/* Check that this tag doesn't already exist */
	/* (Perhaps we should simply replace it, rather than erroring ?) */
	for (j = 0; j < p->count; j++) {
		if (p->data[j].sig == sig) {
			sprintf(p->err,"icc_add_tag: Already have tag '%s' in profile",tag2str(p->data[j].sig)); 
			p->errc = 4;
			return NULL;
		}
	}

	/* Make space in tag table for new tag item */
	if (ovr_mul(sat_add(p->count,1), sizeof(icmTag))) {
		sprintf(p->err,"icc_add_tag: size overflow");
		p->errc = 1;
		return NULL;
	}
	if (p->data == NULL)
		tp = (icmBase *)p->al->malloc(p->al, (p->count+1) * sizeof(icmTag));
	else
		tp = (icmBase *)p->al->realloc(p->al, (void *)p->data, (p->count+1) * sizeof(icmTag));
	if (tp == NULL) {
		sprintf(p->err,"icc_add_tag: Tag table realloc() failed");
		p->errc = 2;
		return NULL;
	}
	p->data = (icmTag *)tp;

	if (ttype == icmSigUnknownType) {
		if ((nob = new_icmUnknown(p)) == NULL)
			return NULL;
	} else {
		/* Allocate the empty object */
		if ((nob = typetable[i].new_obj(p)) == NULL)
			return NULL;
	}

	/* Fill out our tag table entry */
    p->data[p->count].sig = sig;		/* The tag signature */
	p->data[p->count].ttype = nob->ttype = ttype;	/* The tag type signature */
    p->data[p->count].offset = 0;		/* Unknown offset yet */
    p->data[p->count].size = 0;			/* Unknown size yet */
    p->data[p->count].objp = nob;		/* Empty object */
	p->count++;

	return nob;
}

/* Create and add a tag which is a link to an existing tag. */
/* Returns a pointer to the element object */
/* Returns NULL if error - icc->errc will contain */
/* 3 if incompatible tag */
/* NOTE: that we prevent tag duplication */
static icmBase *icc_link_tag(
	icc *p,
    icTagSignature sig,			/* Tag signature - may be unknown */
    icTagSignature ex_sig		/* Tag signature of tag to link to */
) {
	icmBase *tp;
	unsigned int j, exi;
	int i, ok = 1;

	/* Search for existing signature */
	for (exi = 0; exi < p->count; exi++) {
		if (p->data[exi].sig == ex_sig)		/* Found it */
			break;
	}
	if (exi == p->count) {
		sprintf(p->err,"icc_link_tag: Can't find existing tag '%s'",tag2str(ex_sig)); 
		p->errc = 1;
		return NULL;
	}

    if (p->data[exi].objp == NULL) {
		sprintf(p->err,"icc_link_tag: Existing tag '%s' isn't loaded",tag2str(ex_sig)); 
		p->errc = 1;
		return NULL;
	}

	/* Check that a known signature has an acceptable type */
	for (i = 0; sigtypetable[i].sig != icMaxEnumType; i++) {
		if (sigtypetable[i].sig == sig) {	/* recognized signature */
			ok = 0;
			for (j = 0; sigtypetable[i].ttypes[j] != icMaxEnumType; j++) {
				if (sigtypetable[i].ttypes[j] == p->data[exi].ttype)	/* recognized type */
					ok = 1;
			}
			break;
		}
	}
	if (!ok) {
		sprintf(p->err,"icc_link_tag: wrong tag type for signature");
		p->errc = 1;
		return NULL;
	}

	/* Check that this tag doesn't already exits */
	for (j = 0; j < p->count; j++) {
		if (p->data[j].sig == sig) {
			sprintf(p->err,"icc_link_tag: Already have tag '%s' in profile",tag2str(p->data[j].sig)); 
			p->errc = 1;
			return NULL;
		}
	}

	/* Make space in tag table for new tag item */
	if (p->data == NULL)
		tp = (icmBase *)p->al->malloc(p->al, (p->count+1) * sizeof(icmTag));
	else
		tp = (icmBase *)p->al->realloc(p->al, (void *)p->data, (p->count+1) * sizeof(icmTag));
	if (tp == NULL) {
		sprintf(p->err,"icc_link_tag: Tag table realloc() failed");
		p->errc = 2;
		return NULL;
	}
	p->data = (icmTag *)tp;

	/* Fill out our tag table entry */
    p->data[p->count].sig  = sig;		/* The tag signature */
	p->data[p->count].ttype  = p->data[exi].ttype;	/* The tag type signature */
    p->data[p->count].offset = p->data[exi].offset;	/* Same offset (may not be allocated yet) */
    p->data[p->count].size = p->data[exi].size;		/* Same size (may not be allocated yet) */
    p->data[p->count].objp = p->data[exi].objp;		/* Shared object */
	p->data[exi].objp->refcount++;					/* Bump reference count on tag type */
	p->count++;

	return p->data[exi].objp;
}

/* Search for tag signature */
/* return: */
/* 0 if found */
/* 1 if found but not handled type */
/* 2 if not found */
/* NOTE: doesn't set icc->errc or icc->err[] */
/* NOTE: we don't handle tag duplication - you'll always get the first in the file. */
static int icc_find_tag(
	icc *p,
    icTagSignature sig			/* Tag signature - may be unknown */
) {
	unsigned int i;
	int j;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i == p->count)
		return 2;

	/* See if we can handle this type */
	for (j = 0; typetable[j].ttype != icMaxEnumType; j++) {
		if (typetable[j].ttype == p->data[i].ttype)
			break;
	}
	if (typetable[j].ttype == icMaxEnumType)
		return 1;

	return 0;
}

/* Read the specific tag element data, and return a pointer to the object */
/* (This is an internal function)                  */
/* Returns NULL if error - icc->errc will contain: */
/* 2 if not found                                  */
/* Returns an icmSigUnknownType object if the tag type isn't handled by a */
/* specific object and alow_unk is NZ */
/* NOTE: we don't handle tag duplication - you'll always get the first in the file */
static icmBase *icc_read_tag_ix(
	icc *p,
	unsigned int i,				/* Index from 0.. p->count-1 */
	int alow_unk				/* NZ to allow unknown tag to load */
) {
	icTagTypeSignature ttype;	/* Tag type we will create */
	icmBase *nob;
	unsigned int k;
	int j;

	if (i >= p->count) {
		sprintf(p->err,"icc_read_tag_ix: index %d is out of range",i);
		p->errc = 2;
		return NULL;
	}

	/* See if it's already been read */
    if (p->data[i].objp != NULL) {
		return p->data[i].objp;		/* Just return it */
	}
	
	/* See if this should be a link */
	for (k = 0; k < p->count; k++) {
		if (i == k)
			continue;
	    if (p->data[i].ttype  == p->data[k].ttype	/* Exact match and already read */
	     && p->data[i].offset == p->data[k].offset
	     && p->data[i].size   == p->data[k].size
	     && p->data[k].objp != NULL)
			break;
	}
	if (k < p->count) {		/* Make this a link */
		p->data[i].objp = p->data[k].objp;
		p->data[k].objp->refcount++;	/* Bump reference count */
		return p->data[k].objp;			/* Done */
	}

	/* See if we can handle this type */
	for (j = 0; typetable[j].ttype != icMaxEnumType; j++) {
		if (typetable[j].ttype == p->data[i].ttype)
			break;
	}

	if (typetable[j].ttype == icMaxEnumType) {
		if (!alow_unk) {
			sprintf(p->err,"icc_read_tag_ix: found unknown tag");
			p->errc = 2;
			return NULL;
		}
		ttype = icmSigUnknownType; /* Use the Unknown type to handle an unknown tag type */
	} else {
		ttype = p->data[i].ttype;	/* We known this type */
	}
	
	/* Create and read in the object */
	if (ttype == icmSigUnknownType)
		nob = new_icmUnknown(p);
	else
		nob = typetable[j].new_obj(p);

	if (nob == NULL)
		return NULL;

	if ((nob->read(nob, p->data[i].size, p->of + p->data[i].offset)) != 0) {
		nob->del(nob);		/* Failed, so destroy it */
		return NULL;
	}
    p->data[i].objp = nob;
	return nob;
}

/* Read the tag element data of the first matching, and return a pointer to the object */
/* Returns NULL if error - icc->errc will contain: */
/* 2 if not found */
/* Doesn't read uknown type tags */
static icmBase *icc_read_tag(
	icc *p,
    icTagSignature sig			/* Tag signature - may be unknown */
) {
	unsigned int i;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i >= p->count) {
		sprintf(p->err,"icc_read_tag: Tag '%s' not found",string_TagSignature(sig));
		p->errc = 2;
		return NULL;
	}

	/* Let read_tag_ix do all the work */
	return icc_read_tag_ix(p, i, 0);
}

/* Read the tag element data of the first matching, and return a pointer to the object */
/* Returns NULL if error. */
/* Returns an icmSigUnknownType object if the tag type isn't handled by a specific object. */
/* NOTE: we don't handle tag duplication - you'll always get the first in the file. */
static icmBase *icc_read_tag_any(
	icc *p,
    icTagSignature sig			/* Tag signature - may be unknown */
) {
	unsigned int i;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i >= p->count) {
		sprintf(p->err,"icc_read_tag: Tag '%s' not found",string_TagSignature(sig));
		p->errc = 2;
		return NULL;
	}

	/* Let read_tag_ix do all the work */
	return icc_read_tag_ix(p, i, 1);
}

/* Rename a tag signature */
static int icc_rename_tag(
	icc *p,
    icTagSignature sig,			/* Existing Tag signature - may be unknown */
    icTagSignature sigNew		/* New Tag signature - may be unknown */
) {
	unsigned int k;
	int i, j, ok = 1;

	/* Search for signature */
	for (k = 0; k < p->count; k++) {
		if (p->data[k].sig == sig)		/* Found it */
			break;
	}
	if (k >= p->count) {
		sprintf(p->err,"icc_rename_tag: Tag '%s' not found",string_TagSignature(sig));
		return p->errc = 2;
	}

	/* Check that a known new signature has an acceptable type */
	for (i = 0; sigtypetable[i].sig != icMaxEnumType; i++) {
		if (sigtypetable[i].sig == sigNew) {	/* recognized signature */
			ok = 0;
			for (j = 0; sigtypetable[i].ttypes[j] != icMaxEnumType; j++) {
				if (sigtypetable[i].ttypes[j] == p->data[k].ttype)	/* recognized type */
					ok = 1;
			}
			break;
		}
	}

	if (!ok) {
		sprintf(p->err,"icc_rename_tag: wrong signature for tag type");
		p->errc = 1;
		return p->errc;
	}

	/* change its signature */
	p->data[k].sig = sigNew;

	return 0;
}

/* Unread a specific tag, and free the underlying tag type data */
/* if this was the last reference to it. */
/* (This is an internal function) */
/* Returns non-zero on error: */
/* tag not found - icc->errc will contain 2 */
/* tag not read - icc->errc will contain 2 */
static int icc_unread_tag_ix(
	icc *p,
	unsigned int i				/* Index from 0.. p->count-1 */
) {
	if (i >= p->count) {
		sprintf(p->err,"icc_unread_tag_ix: index %d is out of range",i);
		return p->errc = 2;
	}

	/* See if it's been read */
    if (p->data[i].objp == NULL) {
		sprintf(p->err,"icc_unread_tag: Tag '%s' not currently loaded",string_TagSignature(p->data[i].sig));
		return p->errc = 2;
	}
	
	if (--(p->data[i].objp->refcount) == 0)			/* decrement reference count */
			(p->data[i].objp->del)(p->data[i].objp);	/* Last reference */
  	p->data[i].objp = NULL;

	return 0;
}

/* Unread the tag, and free the underlying tag type */
/* if this was the last reference to it. */
/* Returns non-zero on error: */
/* tag not found - icc->errc will contain 2 */
/* tag not read - icc->errc will contain 2 */
/* NOTE: we don't handle tag duplication - you'll always get the first in the file */
static int icc_unread_tag(
	icc *p,
    icTagSignature sig		/* Tag signature - may be unknown */
) {
	unsigned int i;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i >= p->count) {
		sprintf(p->err,"icc_unread_tag: Tag '%s' not found",string_TagSignature(sig));
		return p->errc = 2;
	}

	return icc_unread_tag(p, i);
}

/* Delete the tag, and free the underlying tag type, */
/* if this was the last reference to it. */
/* Note this finds the first tag with a matching signature. */
/* Returns non-zero on error: */
/* tag not found - icc->errc will contain 2 */
static int icc_delete_tag_ix(
	icc *p,
	unsigned int i				/* Index from 0.. p->count-1 */
) {
	if (i >= p->count) {
		sprintf(p->err,"icc_delete_tag_ix: index %d of range",i);
		return p->errc = 2;
	}

	/* If it's been read into memory, decrement the reference count */
    if (p->data[i].objp != NULL) {
		if (--(p->data[i].objp->refcount) == 0)			/* decrement reference count */
			(p->data[i].objp->del)(p->data[i].objp);	/* Last reference */
  		p->data[i].objp = NULL;
	}
	
	/* Now remove it from the tag list */
	for (; i < (p->count-1); i++)
		p->data[i] = p->data[i+1];	/* Copy the structure down */

	p->count--;		/* One less tag in list */

	return 0;
}

/* Delete the tag, and free the underlying tag type, */
/* if this was the last reference to it. */
/* Note this finds the first tag with a matching signature. */
/* Returns non-zero on error: */
/* tag not found - icc->errc will contain 2 */
static int icc_delete_tag(
	icc *p,
    icTagSignature sig		/* Tag signature - may be unknown */
) {
	unsigned int i;

	/* Search for signature */
	for (i = 0; i < p->count; i++) {
		if (p->data[i].sig == sig)		/* Found it */
			break;
	}
	if (i >= p->count) {
		sprintf(p->err,"icc_delete_tag: Tag '%s' not found",string_TagSignature(sig));
		return p->errc = 2;
	}

	return icc_delete_tag_ix(p, i);
}

/* Read all the tags into memory, including unknown types. */
/* Returns non-zero on error. */
static int icc_read_all_tags(
	icc *p
) {
	unsigned int i;

	for (i = 0; i < p->count; i++) {	/* For all the tag element data */
		if (icc_read_tag_ix(p, i, 1) == NULL)
			return p->errc;
	}
	return 0;
}


static void icc_dump(
	icc *p,
	icmFile *op,	/* Output to dump to */
	int   verb		/* Verbosity level */
) {
	unsigned int i;
	if (verb <= 0)
		return;

	op->gprintf(op,"icc:\n");

	/* Dump the header */
	if (p->header != NULL)
		p->header->dump(p->header,op,verb);

	/* Dump all the tag elements */
	for (i = 0; i < p->count; i++) {	/* For all the tag element data */
		icmBase *ob;
		int tr;
		op->gprintf(op,"tag %d:\n",i);
		op->gprintf(op,"  sig      %s\n",tag2str(p->data[i].sig)); 
		op->gprintf(op,"  type     %s\n",tag2str(p->data[i].ttype)); 
		op->gprintf(op,"  offset   %d\n", p->data[i].offset);
		op->gprintf(op,"  size     %d\n", p->data[i].size);
		tr = 0;
		if (p->data[i].objp == NULL) {
			/* The object is not loaded, so load it then free it */
			if (icc_read_tag_ix(p, i, 1) == NULL)
				op->gprintf(op,"Unable to read: %d, %s\n",p->errc,p->err);
			tr = 1;
		}
		if ((ob = p->data[i].objp) != NULL) {
			/* op->gprintf(op,"  refcount %d\n", ob->refcount); */
			ob->dump(ob,op,verb-1);

			if (tr != 0) {	/* Cleanup if temporary */
				icc_unread_tag_ix(p, i);
			}
		}
		op->gprintf(op,"\n");
	}
}

static void icc_delete(
	icc *p
) {
	unsigned int i;
	icmAlloc *al = p->al;
	int del_al   = p->del_al;

	/* Free up the header */
	if (p->header != NULL)
		(p->header->del)(p->header);

	/* Free up the tag data objects */
	if (p->data != NULL) {
		for (i = 0; i < p->count; i++) {
			if (p->data[i].objp != NULL) {
				if (--(p->data[i].objp->refcount) == 0)	/* decrement reference count */
					(p->data[i].objp->del)(p->data[i].objp);	/* Last reference */
		  	  	p->data[i].objp = NULL;
			}
		}
		/* Free tag table */
		al->free(al, p->data);
	}

	/* We are responsible for deleting the file object */
	if (p->del_fp && p->fp != NULL)
		p->fp->del(p->fp);

	/* This object */
	al->free(al, p);

	if (del_al)			/* We are responsible for deleting allocator */
		al->del(al);
}

/* ================================================== */
/* Lut Color normalizing and de-normalizing functions */

/* As a rule, I am representing Lut in memory as values in machine form as real */
/* numbers in the range 0.0 - 1.0. For many color spaces (ie. RGB, Gray, */
/* hsv, hls, cmyk and other device coords), this is entirely appropriate. */
/* For CIE based spaces though, this is not correct, since (I assume!) the binary */
/* representation will be consistent with the encoding in Annex A, page 74 */
/* of the standard. Note that the standard doesn't specify the encoding of */
/* many color spaces (ie. Yuv, Yxy etc.), and is unclear about PCS. */

/* The following functions convert to and from the CIE base spaces */
/* and the real Lut input/output values. These are used to convert real color */
/* space values into/out of the raw lut 0.0-1.0 representation (which subsequently */
/* get converted to ICC integer values in the obvious way as a mapping to 0 .. 2^n-1). */

/* This is used internally to support the Lut->lookup() function, */
/* and can also be used by someone writing a Lut based profile to determine */
/* the colorspace range that the input lut indexes cover, as well */
/* as processing the output luts values into normalized form ready */
/* for writing. */

/* These functions should be accessed by calling icc.getNormFuncs() */ 

/* - - - - - - - - - - - - - - - - */
/* According to 6.5.5 and 6.5.6 of the spec., */
/* XYZ index values are represented the same as their table */
/* values, ie. as a u1.15 representation, with a value */
/* range from 0.0 ->  1.999969482422 */

/* Convert Lut index/value to XYZ coord. */ 
static void Lut_Lut2XYZ(double *out, double *in) {
	out[0] = in[0] * (1.0 + 32767.0/32768); /* X */
	out[1] = in[1] * (1.0 + 32767.0/32768); /* Y */
	out[2] = in[2] * (1.0 + 32767.0/32768); /* Z */
}

/* Convert XYZ coord to Lut index/value. */ 
static void Lut_XYZ2Lut(double *out, double *in) {
	out[0] = in[0] * (1.0/(1.0 + 32767.0/32768));
	out[1] = in[1] * (1.0/(1.0 + 32767.0/32768));
	out[2] = in[2] * (1.0/(1.0 + 32767.0/32768));
}

/* Convert Lut index/value to Y coord. */ 
static void Lut_Lut2Y(double *out, double *in) {
	out[0] = in[0] * (1.0 + 32767.0/32768); /* Y */
}

/* Convert Y coord to Lut index/value. */ 
static void Lut_Y2Lut(double *out, double *in) {
	out[0] = in[0] * (1.0/(1.0 + 32767.0/32768));
}

/* - - - - - - - - - - - - - - - - */
/* Convert 8 bit Lab to Lut numbers */
/* Annex A specifies 8 and 16 bit encoding, but is */
/* silent on the Lut index normalization. */
/* Following Michael Bourgoin's 1998 SIGGRAPH course comment on this, */
/* we assume here that the index encoding is the same as the */
/* value encoding. */

/* Convert Lut8 table index/value to Lab */
static void Lut_Lut2Lab_8(double *out, double *in) {
	out[0] = in[0] * 100.0;				/* L */
	out[1] = (in[1] * 255.0) - 128.0;	/* a */
	out[2] = (in[2] * 255.0) - 128.0;	/* b */
}

/* Convert Lab to Lut8 table index/value */
static void Lut_Lab2Lut_8(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
	out[1] = (in[1] + 128.0) * 1.0/255.0;	/* a */
	out[2] = (in[2] + 128.0) * 1.0/255.0;	/* b */
}

/* Convert Lut8 table index/value to L */
static void Lut_Lut2L_8(double *out, double *in) {
	out[0] = in[0] * 100.0;				/* L */
}

/* Convert L to Lut8 table index/value */
static void Lut_L2Lut_8(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
}

/* - - - - - - - - - - - - - - - - */
/* Convert 16 bit Lab to Lut numbers, V2 */

/* Convert Lut16 table index/value to Lab */
static void Lut_Lut2LabV2_16(double *out, double *in) {
	out[0] = in[0] * (100.0 * 65535.0)/65280.0;			/* L */
	out[1] = (in[1] * (255.0 * 65535.0)/65280) - 128.0;	/* a */
	out[2] = (in[2] * (255.0 * 65535.0)/65280) - 128.0;	/* b */
}

/* Convert Lab to Lut16 table index/value */
static void Lut_Lab2LutV2_16(double *out, double *in) {
	out[0] = in[0] * 65280.0/(100.0 * 65535.0);				/* L */
	out[1] = (in[1] + 128.0) * 65280.0/(255.0 * 65535.0);	/* a */
	out[2] = (in[2] + 128.0) * 65280.0/(255.0 * 65535.0);	/* b */
}

/* Convert Lut16 table index/value to L */
static void Lut_Lut2LV2_16(double *out, double *in) {
	out[0] = in[0] * (100.0 * 65535.0)/65280.0;			/* L */
}

/* Convert Lab to Lut16 table index/value */
static void Lut_L2LutV2_16(double *out, double *in) {
	out[0] = in[0] * 65280.0/(100.0 * 65535.0);				/* L */
}

/* - - - - - - - - - - - - - - - - */
/* Convert 16 bit Lab to Lut numbers, V4 */

/* Convert Lut16 table index/value to Lab */
static void Lut_Lut2LabV4_16(double *out, double *in) {
	out[0] = in[0] * 100.0;				/* L */
	out[1] = (in[1] * 255.0) - 128.0;	/* a */
	out[2] = (in[2] * 255.0) - 128.0;	/* b */
}

/* Convert Lab to Lut16 table index/value */
static void Lut_Lab2LutV4_16(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
	out[1] = (in[1] + 128.0) * 1.0/255.0;	/* a */
	out[2] = (in[2] + 128.0) * 1.0/255.0;	/* b */
}

/* Convert Lut16 table index/value to L */
static void Lut_Lut2LV4_16(double *out, double *in) {
	out[0] = in[0] * 100.0;				/* L */
}

/* Convert L to Lut16 table index/value */
static void Lut_L2LutV4_16(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;				/* L */
}

/* - - - - - - - - - - - - - - - - */
/* Convert Luv to Lut number */
/* This data normalization is taken from Apples */
/* Colorsync specification. */
/* As per other color spaces, we assume that the index */
/* normalization is the same as the data normalization. */

/* Convert Lut table index/value to Luv */
static void Lut_Lut2Luv(double *out, double *in) {
	out[0] = in[0] * 100.0;						/* L */
	out[1] = (in[1] * 65535.0/256.0) - 128.0;	/* u */
	out[2] = (in[2] * 65535.0/256.0) - 128.0;	/* v */
}

/* Convert Luv to Lut table index/value */
static void Lut_Luv2Lut(double *out, double *in) {
	out[0] = in[0] * 1.0/100.0;					/* L */
	out[1] = (in[1] + 128.0) * 256.0/65535.0;	/* u */
	out[2] = (in[2] + 128.0) * 256.0/65535.0;	/* v */
}

/* - - - - - - - - - - - - - - - - */
/* Convert YCbCr to Lut number */
/* We are assuming full range here. foot/head scaling */
/* should be done outside the icc profile. */

/* Convert Lut table index/value to YCbCr */
static void Lut_Lut2YCbCr(double *out, double *in) {
	out[0] = in[0];			/* Y */
	out[1] = in[1] - 0.5;	/* Cb */
	out[2] = in[2] - 0.5;	/* Cr */
}

/* Convert YCbCr to Lut table index/value */
static void Lut_YCbCr2Lut(double *out, double *in) {
	out[0] = in[0];			/* Y */
	out[1] = in[1] + 0.5;	/* Cb */
	out[2] = in[2] + 0.5;	/* Cr */
}

/* - - - - - - - - - - - - - - - - */
/* Default N component conversions */
static void Lut_N(double *out, double *in, int nc) {
	for (--nc; nc >= 0; nc--)
		out[nc] = in[nc];
}

/* 1 */
static void Lut_1(double *out, double *in) {
	out[0] = in[0];
}

/* 2 */
static void Lut_2(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
}

/* 3 */
static void Lut_3(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

/* 4 */
static void Lut_4(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
}

/* 5 */
static void Lut_5(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	out[4] = in[4];
}

/* 6 */
static void Lut_6(double *out, double *in) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	out[4] = in[4];
	out[5] = in[5];
}

/* 7 */
static void Lut_7(double *out, double *in) {
	Lut_N(out, in, 7);
}

/* 8 */
static void Lut_8(double *out, double *in) {
	Lut_N(out, in, 8);
}

/* 9 */
static void Lut_9(double *out, double *in) {
	Lut_N(out, in, 9);
}

/* 10 */
static void Lut_10(double *out, double *in) {
	Lut_N(out, in, 10);
}

/* 11 */
static void Lut_11(double *out, double *in) {
	Lut_N(out, in, 11);
}

/* 12 */
static void Lut_12(double *out, double *in) {
	Lut_N(out, in, 12);
}

/* 13 */
static void Lut_13(double *out, double *in) {
	Lut_N(out, in, 13);
}

/* 14 */
static void Lut_14(double *out, double *in) {
	Lut_N(out, in, 14);
}

/* 15 */
static void Lut_15(double *out, double *in) {
	Lut_N(out, in, 15);
}

/* Function table - match conversions to color spaces. */
/* Anything not here, we don't know how to convert. */
static struct {
	icColorSpaceSignature csig;
	void (*fromLut)(double *out, double *in);	/* from Lut index/entry */
	void (*toLut)(double *out, double *in);		/* to Lut index/entry */
} colnormtable[] = {
	{icSigXYZData,     Lut_Lut2XYZ,      Lut_XYZ2Lut },
	{icmSigYData,      Lut_Lut2Y,        Lut_Y2Lut },
	{icmSigLab8Data,   Lut_Lut2Lab_8,    Lut_Lab2Lut_8 },
	{icmSigLabV2Data,  Lut_Lut2LabV2_16, Lut_Lab2LutV2_16 },
	{icmSigLabV4Data,  Lut_Lut2LabV4_16, Lut_Lab2LutV4_16 },
	{icmSigL8Data,     Lut_Lut2L_8,      Lut_L2Lut_8 },
	{icmSigLV2Data,    Lut_Lut2LV2_16,   Lut_L2LutV2_16 },
	{icmSigLV4Data,    Lut_Lut2LV4_16,   Lut_L2LutV4_16 },
	{icSigLuvData,     Lut_Lut2Luv,      Lut_Luv2Lut },
	{icSigYCbCrData,   Lut_Lut2YCbCr,    Lut_YCbCr2Lut },
	{icSigYxyData,     Lut_3,            Lut_3 },
	{icSigRgbData,     Lut_3,            Lut_3 },
	{icSigGrayData,    Lut_1,            Lut_1 },
	{icSigHsvData,     Lut_3,            Lut_3 },
	{icSigHlsData,     Lut_3,            Lut_3 },
	{icSigCmykData,    Lut_4,            Lut_4 },
	{icSigCmyData,     Lut_3,            Lut_3 },
	{icSigMch6Data,    Lut_6,            Lut_6 },
	{icSig2colorData,  Lut_2,            Lut_2 },
	{icSig3colorData,  Lut_3,            Lut_3 },
	{icSig4colorData,  Lut_4,            Lut_4 },
	{icSig5colorData,  Lut_5,            Lut_5 },
	{icSig6colorData,  Lut_6,            Lut_6 },
	{icSig7colorData,  Lut_7,            Lut_7 },
	{icSigMch5Data,    Lut_5,            Lut_5 },
	{icSigMch6Data,    Lut_6,            Lut_6 },
	{icSigMch7Data,    Lut_7,            Lut_7 },
	{icSigMch8Data,    Lut_8,            Lut_8 },
	{icSig8colorData,  Lut_8,            Lut_8 },
	{icSig9colorData,  Lut_9,            Lut_9 },
	{icSig10colorData, Lut_10,           Lut_10 },
	{icSig11colorData, Lut_11,           Lut_11 },
	{icSig12colorData, Lut_12,           Lut_12 },
	{icSig13colorData, Lut_13,           Lut_13 },
	{icSig14colorData, Lut_14,           Lut_14 },
	{icSig15colorData, Lut_15,           Lut_15 },
	{icMaxEnumData,    NULL,             NULL   }
};
	
/*
	Legacy Lab encoding:

	The V4 specificatins are misleading on this, since they assume in this
	instance that all devices spaces, however labeled, have no defined
	ICC encoding. The end result is simple enough though:

	ICC V2 Lab encoding should be used in all PCS encodings in
	a icSigLut16Type or icSigNamedColor2Type tag, and can be used
	for device space Lab encoding for these tags.

	ICC V4 Lab encoding should be used in all PCS encodings in
	all other situations, and can be used for device space Lab encoding
	for all other situtaions.

	[ Since the ICC spec. doesn't cover device spaces labeled as Lab,
      these are ripe for mis-matches between different implementations.]

	This logic has yet to be fully implemented here.
*/

/* Find appropriate conversion functions from the normalised */
/* Lut data range 0.0 - 1.0 to/from a given colorspace value, */
/* given the color space and Lut type. */
/* Return 0 on success, 1 on match failure. */
/* NOTE: doesn't set error value, message etc.! */
static int getNormFunc(
	icc *icp,
//	icProfileClassSignature psig,		/* Profile signature to use */
	icColorSpaceSignature   csig, 		/* Colorspace to use. */
//  int                     lutin,		/* 0 if this is for a icSigLut16Type input, nz for output */
//	icTagSignature          tagSig,		/* Tag signature involved (AtoB or B2A etc.) */
	icTagTypeSignature      tagType,	/* icSigLut8Type or icSigLut16Type or V4 lut */
	icmNormFlag             flag,		/* icmFromLuti, icmFromLutv or icmToLuti, icmToLutv */
	void                 (**nfunc)(double *out, double *in)
) {
	int i;
	if (tagType == icSigLut8Type && csig == icSigLabData) {
		csig = icmSigLab8Data;
	}
	if (csig == icSigLabData) {
		if (tagType == icSigLut16Type)	/* Lut16 retains legacy encoding */
			csig = icmSigLabV2Data;
		else {							/* Other tag types use version specific encoding */
			if (icp->ver >= icmVersion4_1)
				csig = icmSigLabV4Data;
			else
				csig = icmSigLabV2Data;
		}
	}

	for (i = 0; colnormtable[i].csig != icMaxEnumData; i++) {
		if (colnormtable[i].csig == csig)
			break;	/* Found it */
	}
	if (colnormtable[i].csig == icMaxEnumData) {	/* Oops */
		*nfunc   = NULL;
		return 1;
	}

	if (flag == icmFromLuti || flag == icmFromLutv) {	/* Table index/value decoding functions */
		*nfunc = colnormtable[i].fromLut;
		return 0;
	} else if (flag == icmToLuti || flag == icmToLutv) {	/* Table index/value encoding functions */
		*nfunc = colnormtable[i].toLut;
		return 0;
	}
	*nfunc   = NULL;
	return 1;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Colorspace ranges - used instead of norm/denorm by Mono, Matrix and */
/* override PCS */

/* Function table - match ranges to color spaces. */
/* Anything not here, we don't know how to convert. */
/* Hmm. we're not handling Lab8 properly ?? ~~~8888 */
static struct {
	icColorSpaceSignature csig;
	int same;				/* Non zero if first entry applies to all channels */
	double min[3];			/* Minimum value for this colorspace */
	double max[3];			/* Maximum value for this colorspace */
} colorrangetable[] = {
	{icSigXYZData,     1, { 0.0 } , { 1.0 + 32767.0/32768.0 } },
	{icmSigLab8Data,   0, { 0.0, -128.0, -128.0 }, { 100.0, 127.0,  127.0 } }, 
	{icmSigLabV2Data,  0, { 0.0, -128.0, -128.0 },
	                      { 100.0 + 25500.0/65280.0, 127.0 + 255.0/256.0, 127.0 + 255.0/256.0 } }, 
	{icmSigLabV4Data,  0, { 0.0, -128.0, -128.0 }, { 100.0, 127.0,  127.0 } }, 
	{icmSigYData,      1, { 0.0 }, { 1.0 + 32767.0/32768.0 } },
	{icmSigL8Data,     1, { 0.0 }, { 100.0 } }, 
	{icmSigLV2Data,    1, { 0.0 }, { 100.0 + 25500.0/65280.0 } }, 
	{icmSigLV4Data,    1, { 0.0 }, { 100.0 } }, 
	{icSigLuvData,     0, { 0.0, -128.0, -128.0 },
	                      { 100.0, 127.0 + 255.0/256.0, 127.0 + 255.0/256.0 } },
	{icSigYCbCrData,   0, { 0.0, -0.5, -0.5 }, { 1.0, 0.5, 0.5 } },		/* Full range */
	{icSigYxyData,     1, { 0.0 }, { 1.0 } },			/* ??? */
	{icSigRgbData,     1, { 0.0 }, { 1.0 } },
	{icSigGrayData,    1, { 0.0 }, { 1.0 } },
	{icSigHsvData,     1, { 0.0 }, { 1.0 } },
	{icSigHlsData,     1, { 0.0 }, { 1.0 } },
	{icSigCmykData,    1, { 0.0 }, { 1.0 } },
	{icSigCmyData,     1, { 0.0 }, { 1.0 } },
	{icSigMch6Data,    1, { 0.0 }, { 1.0 } },
	{icSig2colorData,  1, { 0.0 }, { 1.0 } },
	{icSig3colorData,  1, { 0.0 }, { 1.0 } },
	{icSig4colorData,  1, { 0.0 }, { 1.0 } },
	{icSig5colorData,  1, { 0.0 }, { 1.0 } },
	{icSig6colorData,  1, { 0.0 }, { 1.0 } },
	{icSig7colorData,  1, { 0.0 }, { 1.0 } },
	{icSig8colorData,  1, { 0.0 }, { 1.0 } },
	{icSigMch5Data,    1, { 0.0 }, { 1.0 } },
	{icSigMch6Data,    1, { 0.0 }, { 1.0 } },
	{icSigMch7Data,    1, { 0.0 }, { 1.0 } },
	{icSigMch8Data,    1, { 0.0 }, { 1.0 } },
	{icSig9colorData,  1, { 0.0 }, { 1.0 } },
	{icSig10colorData, 1, { 0.0 }, { 1.0 } },
	{icSig11colorData, 1, { 0.0 }, { 1.0 } },
	{icSig12colorData, 1, { 0.0 }, { 1.0 } },
	{icSig13colorData, 1, { 0.0 }, { 1.0 } },
	{icSig14colorData, 1, { 0.0 }, { 1.0 } },
	{icSig15colorData, 1, { 0.0 }, { 1.0 } },
	{icMaxEnumData,    1, { 0.0 }, { 1.0 } }
};
	
/* Find appropriate typical encoding ranges for a */
/* colorspace given the color space. */
/* Return 0 on success, 1 on match failure */
static int getRange(
	icc *icp,
//	icProfileClassSignature psig,		/* Profile signature to use */
	icColorSpaceSignature   csig, 		/* Colorspace to use. */
//  int                     lutin,		/* 0 if this is for a icSigLut16Type input, nz for output */
//	icTagSignature          tagSig,		/* Tag signature involved (AtoB or B2A etc.) */
	icTagTypeSignature      tagType,	/* icSigLut8Type or icSigLut16Type or V4 lut */
	double *min, double *max			/* Return range values */
) {
	int i, e, ee;

	if (tagType == icSigLut8Type && csig == icSigLabData) {
		csig = icmSigLab8Data;
	}
	if (csig == icSigLabData) {
		if (tagType == icSigLut16Type)	/* Lut16 retains legacy encoding */
			csig = icmSigLabV2Data;
		else {							/* Other tag types use version specific encoding */
			if (icp->ver >= icmVersion4_1)
				csig = icmSigLabV4Data;
			else
				csig = icmSigLabV2Data;
		}
	}

	for (i = 0; colorrangetable[i].csig != icMaxEnumData; i++) {
		if (colorrangetable[i].csig == csig)
			break;	/* Found it */
	}
	if (colorrangetable[i].csig == icMaxEnumData) {	/* Oops */
		return 1;
	}
	ee = number_ColorSpaceSignature(csig);		/* Get number of components */

	if (colorrangetable[i].same) {		/* All channels are the same */
		for (e = 0; e < ee; e++) {
			if (min != NULL)
				min[e] = colorrangetable[i].min[0];
			if (max != NULL)
				max[e] = colorrangetable[i].max[0];
		}
	} else {
		for (e = 0; e < ee; e++) {
			if (min != NULL)
				min[e] = colorrangetable[i].min[e];
			if (max != NULL)
				max[e] = colorrangetable[i].max[e];
		}
	}
	return 0;
}

/* =============================================================== */
/* Misc. support functions.                                        */

/* Clamp a 3 vector to be +ve */
void icmClamp3(double out[3], double in[3]) {
	int i;
	for (i = 0; i < 3; i++)
		out[i] = in[i] < 0.0 ? 0.0 : in[i];
}

/* Add two 3 vectors */
void icmAdd3(double out[3], double in1[3], double in2[3]) {
	out[0] = in1[0] + in2[0];
	out[1] = in1[1] + in2[1];
	out[2] = in1[2] + in2[2];
}

/* Subtract two 3 vectors, out = in1 - in2 */
void icmSub3(double out[3], double in1[3], double in2[3]) {
	out[0] = in1[0] - in2[0];
	out[1] = in1[1] - in2[1];
	out[2] = in1[2] - in2[2];
}

/* Divide two 3 vectors, out = in1/in2 */
void icmDiv3(double out[3], double in1[3], double in2[3]) {
	out[0] = in1[0]/in2[0];
	out[1] = in1[1]/in2[1];
	out[2] = in1[2]/in2[2];
}

/* Multiply two 3 vectors, out = in1 * in2 */
void icmMul3(double out[3], double in1[3], double in2[3]) {
	out[0] = in1[0] * in2[0];
	out[1] = in1[1] * in2[1];
	out[2] = in1[2] * in2[2];
}

/* Take absolute of a 3 vector */
void icmAbs3(double out[3], double in[3]) {
	out[0] = fabs(in[0]);
	out[1] = fabs(in[1]);
	out[2] = fabs(in[2]);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Set a 3x3 matrix to unity */
void icmSetUnity3x3(double mat[3][3]) {
	int i, j;
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			if (i == j)
				mat[j][i] = 1.0;
			else
				mat[j][i] = 0.0;
		}
	}
	
}

/* Copy a 3x3 transform matrix */
void icmCpy3x3(double dst[3][3], double src[3][3]) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src[j][i];
	}
}

/* Scale each element of a 3x3 transform matrix */
void icmScale3x3(double dst[3][3], double src[3][3], double scale) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src[j][i] * scale;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* 

  mat     in    out

[     ]   []    []
[     ] * [] => []
[     ]   []    []

 */

/* Multiply 3 array by 3x3 transform matrix */
void icmMulBy3x3(double out[3], double mat[3][3], double in[3]) {
	double tt[3];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1] + mat[0][2] * in[2];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1] + mat[1][2] * in[2];
	tt[2] = mat[2][0] * in[0] + mat[2][1] * in[1] + mat[2][2] * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Add one 3x3 to another */
/* dst = src1 + src2 */
void icmAdd3x3(double dst[3][3], double src1[3][3], double src2[3][3]) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src1[j][i] + src2[j][i];
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Tensor product. Multiply two 3 vectors to form a 3x3 matrix */
/* src1[] forms the colums, and src2[] forms the rows in the result */
void icmTensMul3(double dst[3][3], double src1[3], double src2[3]) {
	int i, j;
	
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src1[j] * src2[i];
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Multiply one 3x3 with another */
/* dst = src * dst */
void icmMul3x3(double dst[3][3], double src[3][3]) {
	int i, j, k;
	double td[3][3];		/* Temporary dest */

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			double tt = 0.0;
			for (k = 0; k < 3; k++)
				tt += src[j][k] * dst[k][i];
			td[j][i] = tt;
		}
	}

	/* Copy result out */
	for (j = 0; j < 3; j++)
		for (i = 0; i < 3; i++)
			dst[j][i] = td[j][i];
}

/* Multiply one 3x3 with another #2 */
/* dst = src1 * src2 */
void icmMul3x3_2(double dst[3][3], double src1[3][3], double src2[3][3]) {
	int i, j, k;
	double td[3][3];		/* Temporary dest */

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			double tt = 0.0;
			for (k = 0; k < 3; k++)
				tt += src1[j][k] * src2[k][i];
			td[j][i] = tt;
		}
	}

	/* Copy result out */
	for (j = 0; j < 3; j++)
		for (i = 0; i < 3; i++)
			dst[j][i] = td[j][i];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* 
	Matrix Inversion
	by Richard Carling
	from "Graphics Gems", Academic Press, 1990
*/

/* 
 *   adjoint( original_matrix, inverse_matrix )
 * 
 *     calculate the adjoint of a 3x3 matrix
 *
 *      Let  a   denote the minor determinant of matrix A obtained by
 *           ij
 *
 *      deleting the ith row and jth column from A.
 *
 *                    i+j
 *     Let  b   = (-1)    a
 *          ij            ji
 *
 *    The matrix B = (b  ) is the adjoint of A
 *                     ij
 */

#define det2x2(a, b, c, d) (a * d - b * c)

static void adjoint(
double out[3][3],
double in[3][3]
) {
    double a1, a2, a3, b1, b2, b3, c1, c2, c3;

    /* assign to individual variable names to aid  */
    /* selecting correct values  */

	a1 = in[0][0]; b1 = in[0][1]; c1 = in[0][2];
	a2 = in[1][0]; b2 = in[1][1]; c2 = in[1][2];
	a3 = in[2][0]; b3 = in[2][1]; c3 = in[2][2];

    /* row column labeling reversed since we transpose rows & columns */

    out[0][0]  =   det2x2(b2, b3, c2, c3);
    out[1][0]  = - det2x2(a2, a3, c2, c3);
    out[2][0]  =   det2x2(a2, a3, b2, b3);
        
    out[0][1]  = - det2x2(b1, b3, c1, c3);
    out[1][1]  =   det2x2(a1, a3, c1, c3);
    out[2][1]  = - det2x2(a1, a3, b1, b3);
        
    out[0][2]  =   det2x2(b1, b2, c1, c2);
    out[1][2]  = - det2x2(a1, a2, c1, c2);
    out[2][2]  =   det2x2(a1, a2, b1, b2);
}

/*
 * double = icmDet3x3(  a1, a2, a3, b1, b2, b3, c1, c2, c3 )
 * 
 * calculate the determinant of a 3x3 matrix
 * in the form
 *
 *     | a1,  b1,  c1 |
 *     | a2,  b2,  c2 |
 *     | a3,  b3,  c3 |
 */

double icmDet3x3(double in[3][3]) {
    double a1, a2, a3, b1, b2, b3, c1, c2, c3;
    double ans;

	a1 = in[0][0]; b1 = in[0][1]; c1 = in[0][2];
	a2 = in[1][0]; b2 = in[1][1]; c2 = in[1][2];
	a3 = in[2][0]; b3 = in[2][1]; c3 = in[2][2];

    ans = a1 * det2x2(b2, b3, c2, c3)
        - b1 * det2x2(a2, a3, c2, c3)
        + c1 * det2x2(a2, a3, b2, b3);
    return ans;
}

#define ICM_SMALL_NUMBER	1.e-8
/* 
 *   inverse( original_matrix, inverse_matrix )
 * 
 *    calculate the inverse of a 4x4 matrix
 *
 *     -1     
 *     A  = ___1__ adjoint A
 *         det A
 */

/* Return non-zero if not invertable */
int icmInverse3x3(
double out[3][3],
double in[3][3]
) {
    int i, j;
    double det;

    /*  calculate the 3x3 determinant
     *  if the determinant is zero, 
     *  then the inverse matrix is not unique.
     */
    det = icmDet3x3(in);

    if ( fabs(det) < ICM_SMALL_NUMBER)
        return 1;

    /* calculate the adjoint matrix */
    adjoint(out, in);

    /* scale the adjoint matrix to get the inverse */
    for (i = 0; i < 3; i++)
        for(j = 0; j < 3; j++)
		    out[i][j] /= det;
	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Transpose a 3x3 matrix */
void icmTranspose3x3(double out[3][3], double in[3][3]) {
	int i, j;
	if (out != in) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				out[i][j] = in[j][i];
	} else {
		double tt[3][3];
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				tt[i][j] = in[j][i];
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				out[i][j] = tt[i][j];
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the dot product of two 3 vectors */
double icmDot3(double in1[3], double in2[3]) {
	return in1[0] * in2[0] + in1[1] * in2[1] + in1[2] * in2[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the cross product of two 3D vectors, out = in1 x in2 */
void icmCross3(double out[3], double in1[3], double in2[3]) {
	double tt[3];
    tt[0] = (in1[1] * in2[2]) - (in1[2] * in2[1]);
    tt[1] = (in1[2] * in2[0]) - (in1[0] * in2[2]);
    tt[2] = (in1[0] * in2[1]) - (in1[1] * in2[0]);
	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the norm (length) squared of a 3 vector */
double icmNorm3sq(double in[3]) {
	return in[0] * in[0] + in[1] * in[1] + in[2] * in[2];
}

/* Compute the norm (length) of a 3 vector */
double icmNorm3(double in[3]) {
	return sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
}

/* Scale a 3 vector by the given ratio */
void icmScale3(double out[3], double in[3], double rat) {
	out[0] = in[0] * rat;
	out[1] = in[1] * rat;
	out[2] = in[2] * rat;
}

/* Compute a blend between in0 and in1 */
void icmBlend3(double out[3], double in0[3], double in1[3], double bf) {
	out[0] = (1.0 - bf) * in0[0] + bf * in1[0];
	out[1] = (1.0 - bf) * in0[1] + bf * in1[1];
	out[2] = (1.0 - bf) * in0[2] + bf * in1[2];
}

/* Clip a vector to the range 0.0 .. 1.0 */
void icmClip3(double out[3], double in[3]) {
	int j;
	for (j = 0; j < 3; j++) {
		out[j] = in[j];
		if (out[j] < 0.0)
			out[j] = 0.0;
		else if (out[j] > 1.0)
			out[j] = 1.0;
	}
}

/* Normalise a 3 vector to the given length. Return nz if not normalisable */
int icmNormalize3(double out[3], double in[3], double len) {
	double tt = sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
	
	if (tt < ICM_SMALL_NUMBER)
		return 1;
	tt = len/tt;
	out[0] = in[0] * tt;
	out[1] = in[1] * tt;
	out[2] = in[2] * tt;
	return 0;
}

/* Compute the norm (length) squared of a vector define by two points */
double icmNorm33sq(double in1[3], double in0[3]) {
	int j;
	double rv;
	for (rv = 0.0, j = 0; j < 3; j++) {
		double tt = in1[j] - in0[j];
		rv += tt * tt;
	}
	return rv;
}

/* Compute the norm (length) of a vector define by two points */
double icmNorm33(double in1[3], double in0[3]) {
	int j;
	double rv;
	for (rv = 0.0, j = 0; j < 3; j++) {
		double tt = in1[j] - in0[j];
		rv += tt * tt;
	}
	return sqrt(rv);
}

/* Scale a two point vector by the given ratio. in0[] is the origin */
void icmScale33(double out[3], double in1[3], double in0[3], double rat) {
	out[0] = in0[0] + (in1[0] - in0[0]) * rat;
	out[1] = in0[1] + (in1[1] - in0[1]) * rat;
	out[2] = in0[2] + (in1[2] - in0[2]) * rat;
}

/* Normalise a vector from 0->1 to the given length. */
/* The new location of in1[] is returned in out[]. */
/* Return nz if not normalisable */
int icmNormalize33(double out[3], double in1[3], double in0[3], double len) {
	int j;
	double vl;

	for (vl = 0.0, j = 0; j < 3; j++) {
		double tt = in1[j] - in0[j];
		vl += tt * tt;
	}
	vl = sqrt(vl);
	if (vl < ICM_SMALL_NUMBER)
		return 1;
	
	vl = len/vl;
	for (j = 0; j < 3; j++) {
		out[j] = in0[j] + (in1[j] - in0[j]) * vl;
	}
	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given two 3D points, create a matrix that rotates */
/* and scales one onto the other about the origin 0,0,0. */
/* The maths is from page 52 of Tomas Moller and Eric Haines "Real-Time Rendering". */
/* s is source vector, t is target vector. */
/* Usage of icmRotMat: */
/*	t[0] == mat[0][0] * s[0] + mat[0][1] * s[1] + mat[0][2] * s[2]; */
/*	t[1] == mat[1][0] * s[0] + mat[1][1] * s[1] + mat[1][2] * s[2]; */
/*	t[2] == mat[2][0] * s[0] + mat[2][1] * s[1] + mat[2][2] * s[2]; */
/* i.e. use icmMulBy3x3 */
void icmRotMat(double m[3][3], double s[3], double t[3]) {
	double sl, tl, sn[3], tn[3];
	double v[3];		/* Cross product */
	double e;			/* Dot product */
	double h;			/* 1-e/Cross product dot squared */

	/* Normalise input vectors */
	/* The rotation will be about 0,0,0 */
	sl = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
	tl = sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);

	if (sl < 1e-12 || tl < 1e-12) {	/* Hmm. Do nothing */
		m[0][0] = 1.0;
		m[0][1] = 0.0;
		m[0][2] = 0.0;
		m[1][0] = 0.0;
		m[1][1] = 1.0;
		m[1][2] = 0.0;
		m[2][0] = 0.0;
		m[2][1] = 0.0;
		m[2][2] = 1.0;
		return;
	}

	sn[0] = s[0]/sl; sn[1] = s[1]/sl; sn[2] = s[2]/sl;
	tn[0] = t[0]/tl; tn[1] = t[1]/tl; tn[2] = t[2]/tl;

	/* Compute the cross product */
    v[0] = (sn[1] * tn[2]) - (sn[2] * tn[1]);
    v[1] = (sn[2] * tn[0]) - (sn[0] * tn[2]);
    v[2] = (sn[0] * tn[1]) - (sn[1] * tn[0]);

	/* Compute the dot product */
    e = sn[0] * tn[0] + sn[1] * tn[1] + sn[2] * tn[2];

	/* Cross product dot squared */
    h = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];

	/* If the two input vectors are close to being parallel, */
	/* then h will be close to zero. */
	if (fabs(h) < 1e-12) {

		/* Make sure scale is the correct sign */
		if (s[0] * t[0] + s[1] * t[1] + s[2] * t[2] < 0.0)
			tl = -tl;

		m[0][0] = tl/sl;
		m[0][1] = 0.0;
		m[0][2] = 0.0;
		m[1][0] = 0.0;
		m[1][1] = tl/sl;
		m[1][2] = 0.0;
		m[2][0] = 0.0;
		m[2][1] = 0.0;
		m[2][2] = tl/sl;
	} else {
		/* 1-e/Cross product dot squared */
	    h = (1.0 - e) / h;

		m[0][0] = tl/sl * (e + h * v[0] * v[0]);
		m[0][1] = tl/sl * (h * v[0] * v[1] - v[2]);
		m[0][2] = tl/sl * (h * v[0] * v[2] + v[1]);
		m[1][0] = tl/sl * (h * v[0] * v[1] + v[2]);
		m[1][1] = tl/sl * (e + h * v[1] * v[1]);
		m[1][2] = tl/sl * (h * v[1] * v[2] - v[0]);
		m[2][0] = tl/sl * (h * v[0] * v[2] - v[1]);
		m[2][1] = tl/sl * (h * v[1] * v[2] + v[0]);
		m[2][2] = tl/sl * (e + h * v[2] * v[2]);
	}

#ifdef NEVER		/* Check result */
	{
		double tt[3];

		icmMulBy3x3(tt, m, s);

		if (icmLabDEsq(t, tt) > 1e-4) {
			printf("icmRotMat error t, is %f %f %f\n",tt[0],tt[1],tt[2]);
			printf("            should be %f %f %f\n",t[0],t[1],t[2]);
		}
	}
#endif /* NEVER */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Multiply 2 array by 2x2 transform matrix */
void icmMulBy2x2(double out[2], double mat[2][2], double in[2]) {
	double tt[2];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1];

	out[0] = tt[0];
	out[1] = tt[1];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Copy a 3x4 transform matrix */
void icmCpy3x4(double dst[3][4], double src[3][4]) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 4; i++)
			dst[j][i] = src[j][i];
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Multiply 3 array by 3x4 transform matrix */
void icmMul3By3x4(double out[3], double mat[3][4], double in[3]) {
	double tt[3];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1] + mat[0][2] * in[2] + mat[0][3];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1] + mat[1][2] * in[2] + mat[1][3];
	tt[2] = mat[2][0] * in[0] + mat[2][1] * in[1] + mat[2][2] * in[2] + mat[2][3];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given two 3D vectors, create a matrix that translates, */
/* rotates and scales one onto the other. */
/* The maths is from page 52 of Tomas Moller and Eric Haines */
/* "Real-Time Rendering". */
/* s0 -> s1 is source vector, t0 -> t1 is target vector. */
/* Usage of icmRotMat: */
/*	t[0] = mat[0][0] * s[0] + mat[0][1] * s[1] + mat[0][2] * s[2] + mat[0][3]; */
/*	t[1] = mat[1][0] * s[0] + mat[1][1] * s[1] + mat[1][2] * s[2] + mat[1][3]; */
/*	t[2] = mat[2][0] * s[0] + mat[2][1] * s[1] + mat[2][2] * s[2] + mat[2][3]; */
/* i.e. use icmMul3By3x4 */
void icmVecRotMat(double m[3][4], double s1[3], double s0[3], double t1[3], double t0[3]) {
	int i, j;
	double ss[3], tt[3], rr[3][3];

	/* Create the rotation matrix: */
	for (i = 0; i < 3; i++) {
		ss[i] = s1[i] - s0[i];
		tt[i] = t1[i] - t0[i];
	}
	icmRotMat(rr, ss, tt);
	
	/* Create rotated version of s0: */
	icmMulBy3x3(ss, rr, s0);

	/* Create 4x4 matrix */
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 4; i++) {
			if (i < 3 && j < 3)
				m[j][i] = rr[j][i];
			else if (i == 3 && j < 3)
				m[j][i] = t0[j] - ss[j];
			else if (i == j)
				m[j][i] = 1.0;
			else
				m[j][i] = 0.0;
		}
	}

#ifdef NEVER		/* Check result */
	{
		double tt0[3], tt1[3];

		icmMul3By3x4(tt0, m, s0);

		if (icmLabDEsq(t0, tt0) > 1e-4) {
			printf("icmVecRotMat error t0, is %f %f %f\n",tt0[0],tt0[1],tt0[2]);
			printf("                should be %f %f %f\n",t0[0],t0[1],t0[2]);
		}

		icmMul3By3x4(tt1, m, s1);

		if (icmLabDEsq(t1, tt1) > 1e-4) {
			printf("icmVecRotMat error t1, is %f %f %f\n",tt1[0],tt1[1],tt1[2]);
			printf("                should be %f %f %f\n",t1[0],t1[1],t1[2]);
		}
	}
#endif /* NEVER */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the 3D intersection of a vector and a plane */
/* return nz if there is no intersection */
int icmVecPlaneIsect(
double isect[3],		/* return intersection point */
double pl_const,		/* Plane equation constant */
double pl_norm[3],		/* Plane normal vector */
double ve_1[3],			/* Point on line */
double ve_0[3]			/* Second point on line */
) {
	double den;			/* denominator */
	double rv;			/* Vector parameter value */
	double vvec[3];		/* Vector vector */
	double ival[3];		/* Intersection value */

	/* Compute vector between the two points */
	vvec[0] = ve_1[0] - ve_0[0];
	vvec[1] = ve_1[1] - ve_0[1];
	vvec[2] = ve_1[2] - ve_0[2];

	/* Compute the denominator */
	den = pl_norm[0] * vvec[0] + pl_norm[1] * vvec[1] + pl_norm[2] * vvec[2];

	/* Too small to intersect ? */
	if (fabs(den) < 1e-12) {
		return 1;
	}

	/* Compute the parameterized intersction point */
	rv = -(pl_norm[0] * ve_0[0] + pl_norm[1] * ve_0[1] + pl_norm[2] * ve_0[2] + pl_const)/den;

	/* Compute the actual intersection point */
	isect[0] = ve_0[0] + rv * vvec[0];
	isect[1] = ve_0[1] + rv * vvec[1];
	isect[2] = ve_0[2] + rv * vvec[2];

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the closest point on a line to a point. */
/* Return closest point and parameter value if not NULL. */
/* Return nz if the line length is zero */
int icmLinePointClosest(double cp[3], double *pa,
                       double la0[3], double la1[3], double pp[3]) {
	double va[3], vp[3];
	double val;				/* Vector length */
	double a;				/* Parameter value */

	icmSub3(va, la1, la0);	/* Line vector */
	val = icmNorm3(va);		/* Vector length */

	if (val < 1e-12)
		return 1;

	icmSub3(vp, pp, la0);	/* Point vector to line base */

	a = icmDot3(vp, va) / val;		/* Normalised dist of point projected onto line */

	if (cp != NULL)
		icmBlend3(cp, la0, la1, a);

	if (pa != NULL)
		*pa = a;

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the closest points between two lines a and b. */
/* Return closest points and parameter values if not NULL. */
/* Return nz if they are paralel. */
/* The maths is from page 338 of Tomas Moller and Eric Haines "Real-Time Rendering". */
int icmLineLineClosest(double ca[3], double cb[3], double *pa, double * pb,
                       double la0[3], double la1[3], double lb0[3], double lb1[3]) {
	double va[3], vb[3];
	double vvab[3], vvabns;		/* Cross product of va and vb and its norm squared */
	double vba0[3];				/* lb0 - la0 */
	double tt[3];
	double a, b;				/* Parameter values */

	icmSub3(va, la1, la0);
	icmSub3(vb, lb1, lb0);
	icmCross3(vvab, va, vb);
	vvabns = icmNorm3sq(vvab);

	if (vvabns < 1e-12)
		return 1;

	icmSub3(vba0, lb0, la0);
	icmCross3(tt, vba0, vb);
	a = icmDot3(tt, vvab) / vvabns;

	icmCross3(tt, vba0, va);
	b = icmDot3(tt, vvab) / vvabns;

	if (pa != NULL)
		*pa = a;

	if (pb != NULL)
		*pb = b;

	if (ca != NULL) {
		ca[0] = la0[0] + a * va[0];
		ca[1] = la0[1] + a * va[1];
		ca[2] = la0[2] + a * va[2];
	}

	if (cb != NULL) {
		cb[0] = lb0[0] + b * vb[0];
		cb[1] = lb0[1] + b * vb[1];
		cb[2] = lb0[2] + b * vb[2];
	}

#ifdef NEVER	/* Verify  */
	{
		double vab[3];		/* Vector from ca to cb */

		vab[0] = lb0[0] + b * vb[0] - la0[0] - a * va[0];
		vab[1] = lb0[1] + b * vb[1] - la0[1] - a * va[1];
		vab[2] = lb0[2] + b * vb[2] - la0[2] - a * va[2];
		
		if (icmDot3(va, vab) > 1e-6
		 || icmDot3(vb, vab) > 1e-6)
			warning("icmLineLineClosest verify failed\n");
	}
#endif

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given 3 3D points, compute a plane equation. */
/* The normal will be right handed given the order of the points */
/* The plane equation will be the 3 normal components and the constant. */
/* Return nz if any points are cooincident or co-linear */
int icmPlaneEqn3(double eq[4], double p0[3], double p1[3], double p2[3]) {
	double ll, v1[3], v2[3];

	/* Compute vectors along edges */
	v2[0] = p1[0] - p0[0];
	v2[1] = p1[1] - p0[1];
	v2[2] = p1[2] - p0[2];

	v1[0] = p2[0] - p0[0];
	v1[1] = p2[1] - p0[1];
	v1[2] = p2[2] - p0[2];

	/* Compute cross products v1 x v2, which will be the normal */
	eq[0] = v1[1] * v2[2] - v1[2] * v2[1];
	eq[1] = v1[2] * v2[0] - v1[0] * v2[2];
	eq[2] = v1[0] * v2[1] - v1[1] * v2[0];

	/* Normalise the equation */
	ll = sqrt(eq[0] * eq[0] + eq[1] * eq[1] + eq[2] * eq[2]);
	if (ll < 1e-10) {
		return 1;
	}
	eq[0] /= ll;
	eq[1] /= ll;
	eq[2] /= ll;

	/* Compute the plane equation constant */
	eq[3] = - (eq[0] * p0[0])
	        - (eq[1] * p0[1])
	        - (eq[2] * p0[2]);

	return 0;
}

/* Given a 3D point and a plane equation, return the signed */
/* distance from the plane */
double icmPlaneDist3(double eq[4], double p[3]) {
	double rv;

	rv = eq[0] * p[0]
	   + eq[1] * p[1]
	   + eq[2] * p[2]
	   + eq[3];

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given 2 2D points, compute a plane equation. */
/* The normal will be right handed given the order of the points */
/* The plane equation will be the 2 normal components and the constant. */
/* Return nz if any points are cooincident or co-linear */
int icmPlaneEqn2(double eq[3], double p0[2], double p1[2]) {
	double ll, v1[3];

	/* Compute vectors along edge */
	v1[0] = p1[0] - p0[0];
	v1[1] = p1[1] - p0[1];

	/* Normal to vector */
	eq[0] =  v1[1];
	eq[1] = -v1[0];

	/* Normalise the equation */
	ll = sqrt(eq[0] * eq[0] + eq[1] * eq[1]);
	if (ll < 1e-10) {
		return 1;
	}
	eq[0] /= ll;
	eq[1] /= ll;

	/* Compute the plane equation constant */
	eq[2] = - (eq[0] * p0[0])
	        - (eq[1] * p0[1]);

	return 0;
}

/* Given a 2D point and a plane equation, return the signed */
/* distance from the plane. The distance will be +ve if the point */
/* is to the right of the plane formed by two points in order */
double icmPlaneDist2(double eq[3], double p[2]) {
	double rv;

	rv = eq[0] * p[0]
	   + eq[1] * p[1]
	   + eq[2];

	return rv;
}

/* Given two infinite 2D lines define by 4 points, compute the intersection. */
/* Return nz if there is no intersection (lines are parallel) */
int icmLineIntersect2(double res[2], double p1[2], double p2[2], double p3[2], double p4[2]) {
	/* Compute by determinants */
	double x1y2_y1x2 = p1[0] * p2[1] - p1[1] * p2[0];
	double x3y4_y3x4 = p3[0] * p4[1] - p3[1] * p4[0];
	double x1_x2     = p1[0] - p2[0];
	double y1_y2     = p1[1] - p2[1];
	double x3_x4     = p3[0] - p4[0];
	double y3_y4     = p3[1] - p4[1];
	double num;							/* Numerator */

	num = x1_x2 * y3_y4 - y1_y2 * x3_x4;

	if (fabs(num) < 1e-10)
		return 1;

	res[0] = (x1y2_y1x2 * x3_x4 - x1_x2 * x3y4_y3x4)/num;
	res[1] = (x1y2_y1x2 * y3_y4 - y1_y2 * x3y4_y3x4)/num;

	return 0;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* CIE Y (range 0 .. 1) to perceptual CIE 1976 L* (range 0 .. 100) */
double
icmY2L(double val) {
	if (val > 0.008856451586)
		val = pow(val,1.0/3.0);
	else
		val = 7.787036979 * val + 16.0/116.0;

	val = (116.0 * val - 16.0);
	return val;
}

/* Perceptual CIE 1976 L* (range 0 .. 100) to CIE Y (range 0 .. 1) */
double
icmL2Y(double val) {
	val = (val + 16.0)/116.0;

	if (val > 24.0/116.0)
		val = pow(val,3.0);
	else
		val = (val - 16.0/116.0)/7.787036979;
	return val;
}

/* CIE XYZ to perceptual CIE 1976 L*a*b* */
void
icmXYZ2Lab(icmXYZNumber *w, double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double x,y,z,fx,fy,fz;

	x = X/w->X;
	y = Y/w->Y;
	z = Z/w->Z;

	if (x > 0.008856451586)
		fx = pow(x,1.0/3.0);
	else
		fx = 7.787036979 * x + 16.0/116.0;

	if (y > 0.008856451586)
		fy = pow(y,1.0/3.0);
	else
		fy = 7.787036979 * y + 16.0/116.0;

	if (z > 0.008856451586)
		fz = pow(z,1.0/3.0);
	else
		fz = 7.787036979 * z + 16.0/116.0;

	out[0] = 116.0 * fy - 16.0;
	out[1] = 500.0 * (fx - fy);
	out[2] = 200.0 * (fy - fz);
}

/* Perceptual CIE 1976 L*a*b* to CIE XYZ */
void
icmLab2XYZ(icmXYZNumber *w, double *out, double *in) {
	double L = in[0], a = in[1], b = in[2];
	double x,y,z,fx,fy,fz;

	fy = (L + 16.0)/116.0;
	fx = a/500.0 + fy;
	fz = fy - b/200.0;

	if (fy > 24.0/116.0)
		y = pow(fy,3.0);
	else
		y = (fy - 16.0/116.0)/7.787036979;

	if (fx > 24.0/116.0)
		x = pow(fx,3.0);
	else
		x = (fx - 16.0/116.0)/7.787036979;

	if (fz > 24.0/116.0)
		z = pow(fz,3.0);
	else
		z = (fz - 16.0/116.0)/7.787036979;

	out[0] = x * w->X;
	out[1] = y * w->Y;
	out[2] = z * w->Z;
}


/* LCh to Lab */
void icmLCh2Lab(double *out, double *in) {
	double C, h;

	C = in[1];
	h = 3.14159265359/180.0 * in[2];

	out[0] = in[0];
	out[1] = C * cos(h);
	out[2] = C * sin(h);
}

/* Lab to LCh (general to polar, works with Luv too) */
void icmLab2LCh(double *out, double *in) {
	double C, h;

	C = sqrt(in[1] * in[1] + in[2] * in[2]);

    h = (180.0/3.14159265359) * atan2(in[2], in[1]);
	h = (h < 0.0) ? h + 360.0 : h;

	out[0] = in[0];
	out[1] = C;
	out[2] = h;
}

/* XYZ to Yxy */
extern ICCLIB_API void icmXYZ2Yxy(double *out, double *in) {
	double sum = in[0] + in[1] + in[2];
	double Y, x, y;

	if (sum < 1e-9) {
		Y = 0.0;
		y = 0.333333333;
		x = 0.333333333;
	} else {
		Y = in[1];
		x = in[0]/sum;
		y = in[1]/sum;
	}
	out[0] = Y;
	out[1] = x;
	out[2] = y;
}

/* Yxy to XYZ */
extern ICCLIB_API void icmYxy2XYZ(double *out, double *in) {
	double Y = in[0];
	double x = in[1];
	double y = in[2];
	double z = 1.0 - x - y;
	double sum;
	if (y < 1e-9) {
		out[0] = out[1] = out[2] = 0.0;
	} else {
		sum = Y/y;
		out[0] = x * sum;
		out[1] = Y;
		out[2] = z * sum;
	}
}


/* CIE XYZ to perceptual CIE 1976 L*u*v* */
extern ICCLIB_API void icmXYZ2Luv(icmXYZNumber *w, double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double un, vn, u, v, fl, fu, fv;

	un = (4.0 * w->X) / (w->X + 15.0 * w->Y + 3.0 * w->Z);
	vn = (9.0 * w->Y) / (w->X + 15.0 * w->Y + 3.0 * w->Z);
	u = (4.0 * X) / (X + 15.0 * Y + 3.0 * Z);
	v = (9.0 * Y) / (X + 15.0 * Y + 3.0 * Z);
	
	Y /= w->Y;

	if (Y > 0.008856451586)
		fl = pow(Y,1.0/3.0);
	else
		fl = 7.787036979 * Y + 16.0/116.0;

	fu = u - un;
	fv = v - vn;

	out[0] = 116.0 * fl - 16.0;
	out[1] = 13.0 * out[0] * fu;
	out[2] = 13.0 * out[0] * fv;
}

/* Perceptual CIE 1976 L*u*v* to CIE XYZ */
extern ICCLIB_API void icmLuv2XYZ(icmXYZNumber *w, double *out, double *in) {
	double un, vn, u, v, fl, fu, fv, sum, X, Y, Z;

	fl = (in[0] + 16.0)/116.0;
	fu = in[1] / (13.0 * in[0]);
	fv = in[2] / (13.0 * in[0]);

	un = (4.0 * w->X) / (w->X + 15.0 * w->Y + 3.0 * w->Z);
	vn = (9.0 * w->Y) / (w->X + 15.0 * w->Y + 3.0 * w->Z);

	u = fu + un;
	v = fv + vn;

	if (fl > 24.0/116.0)
		Y = pow(fl,3.0);
	else
		Y = (fl - 16.0/116.0)/7.787036979;
	Y *= w->Y;

	sum = (9.0 * Y)/v;
	X = (u * sum)/4.0;
	Z = (sum - X - 15.0 * Y)/3.0;

	out[0] = X;
	out[1] = Y;
	out[2] = Z;
}

/* CIE XYZ to perceptual CIE 1976 UCS diagram Yu'v'*/
/* (Yu'v' is a better chromaticity space than Yxy) */
extern ICCLIB_API void icmXYZ21976UCS(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double den, u, v;

	den = (X + 15.0 * Y + 3.0 * Z);

	if (den < 1e-9) {
		Y = 0.0;
		u = 4.0/19.0;
		v = 9.0/19.0;
	} else {
		u = (4.0 * X) / den;
		v = (9.0 * Y) / den;
	}
	
	out[0] = Y;
	out[1] = u;
	out[2] = v;
}

/* Perceptual CIE 1976 UCS diagram Yu'v' to CIE XYZ */
extern ICCLIB_API void icm1976UCS2XYZ(double *out, double *in) {
	double u, v, fl, fu, fv, sum, X, Y, Z;

	Y = in[0];
	u = in[1];
	v = in[2];

	if (v < 1e-9) {
		X = Y = Z = 0.0;
	} else {
		X = ((9.0 * u * Y)/(4.0 * v));
		Z = -(((20.0 * v + 3.0 * u - 12.0) * Y)/(4.0 * v));
	}

	out[0] = X;
	out[1] = Y;
	out[2] = Z;
}

/* CIE XYZ to perceptual CIE 1960 UCS */
/* (This was obsoleted by the 1976UCS, but is still used */
/*  in computing color temperatures.) */
extern ICCLIB_API void icmXYZ21960UCS(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double den, u, v;

	den = (X + 15.0 * Y + 3.0 * Z);

	if (den < 1e-9) {
		Y = 0.0;
		u = 4.0/19.0;
		v = 6.0/19.0;
	} else {
		u = (4.0 * X) / den; 
		v = (6.0 * Y) / den;
	}
	
	out[0] = Y;
	out[1] = u;
	out[2] = v;
}

/* Perceptual CIE 1960 UCS to CIE XYZ */
extern ICCLIB_API void icm1960UCS2XYZ(double *out, double *in) {
	double u, v, fl, fu, fv, sum, X, Y, Z;

	Y = in[0];
	u = in[1];
	v = in[2];

	if (v < 1e-9) {
		X = Y = Z = 0.0;
	} else {
		X = ((3.0 * u * Y)/(2.0 * v));
		Z = -(((10.0 * v + u - 4.0) * Y)/(2.0 * v));
	}

	out[0] = X;
	out[1] = Y;
	out[2] = Z;
}

/* CIE XYZ to perceptual CIE 1964 WUV (U*V*W*) */
/* (This is obsolete but still used in computing CRI) */
extern ICCLIB_API void icmXYZ21964WUV(icmXYZNumber *w, double *out, double *in) {
	double W, U, V;
	double wucs[3];
	double iucs[3];

	icmXYZ2Ary(wucs, *w);
	icmXYZ21960UCS(wucs, wucs);
	icmXYZ21960UCS(iucs, in);

	W = 25.0 * pow(iucs[0] * 100.0/wucs[0], 1.0/3.0) - 17.0;
	U = 13.0 * W * (iucs[1] - wucs[1]);
	V = 13.0 * W * (iucs[2] - wucs[2]);

	out[0] = W;
	out[1] = U;
	out[2] = V;
}

/* Perceptual CIE 1964 WUV (U*V*W*) to CIE XYZ */
extern ICCLIB_API void icm1964WUV2XYZ(icmXYZNumber *w, double *out, double *in) {
	double W, U, V;
	double wucs[3];
	double iucs[3];

	icmXYZ2Ary(wucs, *w);
	icmXYZ21960UCS(wucs, wucs);

	W = in[0];
	U = in[1];
	V = in[2];

	iucs[0] = pow((W + 17.0)/25.0, 3.0) * wucs[0]/100.0;
	iucs[1] = U / (13.0 * W) + wucs[1];
	iucs[2] = V / (13.0 * W) + wucs[2];

	icm1960UCS2XYZ(out, iucs);
}

/* CIE CIE1960 UCS to perceptual CIE 1964 WUV (U*V*W*) */
/* (This is used in computing CRI) */
extern ICCLIB_API void icm1960UCS21964WUV(icmXYZNumber *w, double *out, double *in) {
	double W, U, V;
	double wucs[3];

	icmXYZ2Ary(wucs, *w);
	icmXYZ21960UCS(wucs, wucs);

	W = 25.0 * pow(in[0] * 100.0/wucs[0], 1.0/3.0) - 17.0;
	U = 13.0 * W * (in[1] - wucs[1]);
	V = 13.0 * W * (in[2] - wucs[2]);

	out[0] = W;
	out[1] = U;
	out[2] = V;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* NOTE :- that these values are for the 1931 standard observer. */
/* Since they are an arbitrary 4 decimal place accuracy, we round */
/* them to be exactly the same as ICC header encoded values, */
/* to avoid any slight discrepancy with PCS white from profiles. */

/* available D50 Illuminant */
icmXYZNumber icmD50 = { 		/* Profile illuminant - D50 */
    RND_S15FIXED16(0.9642),
	RND_S15FIXED16(1.0000),
	RND_S15FIXED16(0.8249)
};

icmXYZNumber icmD50_100 = {		/* Profile illuminant - D50, scaled to 100 */
    RND_S15FIXED16(0.9642) * 100.0,
	RND_S15FIXED16(1.0000) * 100.0,
	RND_S15FIXED16(0.8249) * 100.0
};

double icmD50_ary3[3] = { 		/* Profile illuminant - D50 */
    RND_S15FIXED16(0.9642),
	RND_S15FIXED16(1.0000),
	RND_S15FIXED16(0.8249)
};

double icmD50_100_ary3[3] = {	/* Profile illuminant - D50, scaled to 100 */
    RND_S15FIXED16(0.9642) * 100.0,
	RND_S15FIXED16(1.0000) * 100.0,
	RND_S15FIXED16(0.8249) * 100.0
};

/* available D65 Illuminant */
icmXYZNumber icmD65 = { 		/* Profile illuminant - D65 */
    RND_S15FIXED16(0.9505),
	RND_S15FIXED16(1.0000),
	RND_S15FIXED16(1.0890)
};

icmXYZNumber icmD65_100 = { 	/* Profile illuminant - D65, scaled to 100 */
    RND_S15FIXED16(0.9505) * 100.0,
	RND_S15FIXED16(1.0000) * 100.0,
	RND_S15FIXED16(1.0890) * 100.0
};

double icmD65_ary3[3] = { 		/* Profile illuminant - D65 */
    RND_S15FIXED16(0.9505),
	RND_S15FIXED16(1.0000),
	RND_S15FIXED16(1.0890)
};

double icmD65_100_ary3[3] = { 	/* Profile illuminant - D65, scaled to 100 */
    RND_S15FIXED16(0.9505) * 100.0,
	RND_S15FIXED16(1.0000) * 100.0,
	RND_S15FIXED16(1.0890) * 100.0
};


/* Default black point */
icmXYZNumber icmBlack = {
    0.0000, 0.0000, 0.0000
};

/* The Standard ("wrong Von-Kries") chromatic transform matrix */
double icmWrongVonKries[3][3] = {
	{ 1.0000, 0.0000, 0.0000 },
	{ 0.0000, 1.0000, 0.0000 },
	{ 0.0000, 0.0000, 1.0000 }
};

/* The Bradford chromatic transform matrix */
double icmBradford[3][3] = {
	{ RND_S15FIXED16( 0.8951), RND_S15FIXED16( 0.2664), RND_S15FIXED16(-0.1614) },
	{ RND_S15FIXED16(-0.7502), RND_S15FIXED16( 1.7135), RND_S15FIXED16( 0.0367) },
	{ RND_S15FIXED16( 0.0389), RND_S15FIXED16(-0.0685), RND_S15FIXED16( 1.0296) }
};

/* Return the normal Delta E given two Lab values */
double icmLabDE(double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	rv += tt * tt;
	
	return sqrt(rv);
}

/* Return the normal Delta E squared, given two Lab values */
double icmLabDEsq(double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	rv += tt * tt;
	
	return rv;
}

/* Return the normal Delta E given two XYZ values */
extern ICCLIB_API double icmXYZLabDE(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3], rv;

	icmXYZ2Lab(w, lab0, in0);
	icmXYZ2Lab(w, lab1, in1);
	rv = icmLabDE(lab0, lab1);
	return rv;
}

/* (Note that CIE94 can give odd results for very large delta E's, */
/* when one of the two points is near the neutral axis: */
/* ie DE(A,B + del) != DE(A,B) + DE(del) */
#ifdef NEVER
{
	double w1[3] = { 99.996101, -0.058417, -0.010557 };
	double c1[3] = { 60.267956, 98.845863, -61.163277 };
	double w2[3] = { 100.014977, -0.138339, 0.089744 };
	double c2[3] = { 60.294464, 98.117104, -60.843159 };
	
	printf("DE   1 = %f, 2 = %f\n", icmLabDE(c1, w1), icmLabDE(c2, w2));
	printf("DE94 1 = %f, 2 = %f\n", icmCIE94(c1, w1), icmCIE94(c2, w2));
}
#endif


/* Return the CIE94 Delta E color difference measure, squared */
double icmCIE94sq(double Lab0[3], double Lab1[3]) {
	double desq, dhsq;
	double dlsq, dcsq;
	double c12;

	{
		double dl, da, db;
		dl = Lab0[0] - Lab1[0];
		dlsq = dl * dl;		/* dl squared */
		da = Lab0[1] - Lab1[1];
		db = Lab0[2] - Lab1[2];

		/* Compute normal Lab delta E squared */
		desq = dlsq + da * da + db * db;
	}

	{
		double c1, c2, dc;

		/* Compute chromanance for the two colors */
		c1 = sqrt(Lab0[1] * Lab0[1] + Lab0[2] * Lab0[2]);
		c2 = sqrt(Lab1[1] * Lab1[1] + Lab1[2] * Lab1[2]);
		c12 = sqrt(c1 * c2);	/* Symetric chromanance */

		/* delta chromanance squared */
		dc = c1 - c2;
		dcsq = dc * dc;
	}

	/* Compute delta hue squared */
	if ((dhsq = desq - dlsq - dcsq) < 0.0)
		dhsq = 0.0;
	{
		double sc, sh;

		/* Weighting factors for delta chromanance & delta hue */
		sc = 1.0 + 0.045 * c12;
		sh = 1.0 + 0.015 * c12;
		return dlsq + dcsq/(sc * sc) + dhsq/(sh * sh);
	}
}

/* Return the CIE94 Delta E color difference measure */
double icmCIE94(double Lab0[3], double Lab1[3]) {
	return sqrt(icmCIE94sq(Lab0, Lab1));
}

/* Return the CIE94 Delta E color difference measure for two XYZ values */
extern ICCLIB_API double icmXYZCIE94(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3];

	icmXYZ2Lab(w, lab0, in0);
	icmXYZ2Lab(w, lab1, in1);
	return sqrt(icmCIE94sq(lab0, lab1));
}


/* From the paper "The CIEDE2000 Color-Difference Formula: Implementation Notes, */
/* Supplementary Test Data, and Mathematical Observations", by */
/* Gaurav Sharma, Wencheng Wu and Edul N. Dalal, */
/* Color Res. Appl., vol. 30, no. 1, pp. 21-30, Feb. 2005. */

/* Return the CIEDE2000 Delta E color difference measure squared, for two Lab values */
double icmCIE2Ksq(double *Lab0, double *Lab1) {
	double C1, C2;
	double h1, h2;
	double dL, dC, dH;
	double dsq;

	/* The trucated value of PI is needed to ensure that the */
	/* test cases pass, as one of them lies on the edge of */
	/* a mathematical discontinuity. The precision is still */
	/* enough for any practical use. */
#define RAD2DEG(xx) (180.0/3.14159265358979 * (xx))
#define DEG2RAD(xx) (3.14159265358979/180.0 * (xx))

	/* Compute Cromanance and Hue angles */
	{
		double C1ab, C2ab;
		double Cab, Cab7, G;
		double a1, a2;

		C1ab = sqrt(Lab0[1] * Lab0[1] + Lab0[2] * Lab0[2]);
		C2ab = sqrt(Lab1[1] * Lab1[1] + Lab1[2] * Lab1[2]);
		Cab = 0.5 * (C1ab + C2ab);
		Cab7 = pow(Cab,7.0);
		G = 0.5 * (1.0 - sqrt(Cab7/(Cab7 + 6103515625.0)));
		a1 = (1.0 + G) * Lab0[1];
		a2 = (1.0 + G) * Lab1[1];
		C1 = sqrt(a1 * a1 + Lab0[2] * Lab0[2]);
		C2 = sqrt(a2 * a2 + Lab1[2] * Lab1[2]);

		if (C1 < 1e-9)
			h1 = 0.0;
		else {
			h1 = RAD2DEG(atan2(Lab0[2], a1));
			if (h1 < 0.0)
				h1 += 360.0;
		}

		if (C2 < 1e-9)
			h2 = 0.0;
		else {
			h2 = RAD2DEG(atan2(Lab1[2], a2));
			if (h2 < 0.0)
				h2 += 360.0;
		}
	}

	/* Compute delta L, C and H */
	{
		double dh;

		dL = Lab1[0] - Lab0[0];
		dC = C2 - C1;
		if (C1 < 1e-9 || C2 < 1e-9) {
			dh = 0.0;
		} else {
			dh = h2 - h1;
			if (dh > 180.0)
				dh -= 360.0;
			else if (dh < -180.0)
				dh += 360.0;
		}

		dH = 2.0 * sqrt(C1 * C2) * sin(DEG2RAD(0.5 * dh));
	}

	{
		double L, C, h, T;
		double hh, ddeg;
		double C7, RC, L50sq, SL, SC, SH, RT;
		double dLsq, dCsq, dHsq, RCH;

		L = 0.5 * (Lab0[0]  + Lab1[0]);
		C = 0.5 * (C1 + C2);

		if (C1 < 1e-9 || C2 < 1e-9) {
			h = h1 + h2;
		} else {
			h = h1 + h2;
			if (fabs(h1 - h2) > 180.0) {
				if (h < 360.0)
					h += 360.0;
				else if (h >= 360.0)
					h -= 360.0;
			}
			h *= 0.5;
		}

		T = 1.0 - 0.17 * cos(DEG2RAD(h-30.0)) + 0.24 * cos(DEG2RAD(2.0 * h))
		  + 0.32 * cos(DEG2RAD(3.0 * h + 6.0)) - 0.2 * cos(DEG2RAD(4.0 * h - 63.0));
		L50sq = (L - 50.0) * (L - 50.0);

		SL = 1.0 + (0.015 * L50sq)/sqrt(20.0 + L50sq);
		SC = 1.0 + 0.045 * C;
		SH = 1.0 + 0.015 * C * T;

		dLsq = dL/SL;
		dCsq = dC/SC;
		dHsq = dH/SH;

		hh = (h - 275.0)/25.0;
		ddeg = 30.0 * exp(-hh * hh);
		C7 = pow(C, 7.0);
		RC = 2.0 * sqrt(C7/(C7 + 6103515625.0));
		RT = -sin(DEG2RAD(2 * ddeg)) * RC;

		RCH = RT * dCsq * dHsq;

		dLsq *= dLsq;
		dCsq *= dCsq;
		dHsq *= dHsq;

		dsq = dLsq + dCsq + dHsq + RCH;
	}

	return dsq;

#undef RAD2DEG
#undef DEG2RAD
}

/* Return the CIE2DE000 Delta E color difference measure for two Lab values */
double icmCIE2K(double *Lab0, double *Lab1) {
	return sqrt(icmCIE2Ksq(Lab0, Lab1));
}

/* Return the CIEDE2000 Delta E color difference measure for two XYZ values */
ICCLIB_API double icmXYZCIE2K(icmXYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3];

	icmXYZ2Lab(w, lab0, in0);
	icmXYZ2Lab(w, lab1, in1);
	return sqrt(icmCIE2Ksq(lab0, lab1));
}



/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Independent chromatic adaptation transform utility. */
/* Return a 3x3 chromatic adaptation matrix */
/* Use icmMulBy3x3(dst, mat, src) */
/* NOTE that to transform primaries they */
/* must be mat[XYZ][RGB] format! */
void icmChromAdaptMatrix(
	int flags,				/* Use Bradford flag, Transform given matrix flag */
	icmXYZNumber d_wp,		/* Destination white point */
	icmXYZNumber s_wp,		/* Source white point */
	double mat[3][3]		/* Destination matrix */
) {
	double dst[3], src[3];			/* Source & destination white points */
	double vkmat[3][3];				/* Von Kries matrix */
	static int inited = 0;			/* Compute inverse bradford once */
	static double ibradford[3][3];	/* Inverse Bradford */

	/* Set initial matrix to unity if creating from scratch */
	if (!(flags & ICM_CAM_MULMATRIX)) {
		icmSetUnity3x3(mat);
	}

	icmXYZ2Ary(src, s_wp);
	icmXYZ2Ary(dst, d_wp);

	if (flags & ICM_CAM_BRADFORD) {
		icmMulBy3x3(src, icmBradford, src);
		icmMulBy3x3(dst, icmBradford, dst);
	}

	/* Setup the Von Kries white point adaptation matrix */
	vkmat[0][0] = dst[0]/src[0];
	vkmat[1][1] = dst[1]/src[1];
	vkmat[2][2] = dst[2]/src[2];
	vkmat[0][1] = vkmat[0][2] = 0.0;
	vkmat[1][0] = vkmat[1][2] = 0.0;
	vkmat[2][0] = vkmat[2][1] = 0.0;

	/* Transform to Bradford space if requested */
	if (flags & ICM_CAM_BRADFORD) {
		icmMul3x3(mat, icmBradford);
	}

	/* Apply chromatic adaptation */
	icmMul3x3(mat, vkmat);

	/* Transform from Bradford space */
	if (flags & ICM_CAM_BRADFORD) {
		if (inited == 0) {
			icmInverse3x3(ibradford, icmBradford);
			inited = 1;
		}
		icmMul3x3(mat, ibradford);
	}

	/* We're done */
}

/* Setup the wpchtmx appropriately for creating profile */
static void icc_setup_wpchtmx(icc *p) {
	int useBradford = 1;		/* Default use Bradford */

	if (!p->autoWpchtmx)
		return;					/* Reading profile has set wpchtmx[][] */

	/* If we should use ICC standard Wrong Von Kries for white point chromatic adapation */
	if (p->header->deviceClass == icSigOutputClass
	 && p->useLinWpchtmx) {
		useBradford = 0;
	} 

	if (useBradford) {
		icmCpy3x3(p->wpchtmx, icmBradford);
		icmInverse3x3(p->iwpchtmx, p->wpchtmx);
	} else {
		icmCpy3x3(p->wpchtmx, icmWrongVonKries);
		icmCpy3x3(p->iwpchtmx, icmWrongVonKries);
	}

	/* This is set for this profile class now */
	p->wpchtmx_class = p->header->deviceClass;
}

/* icc Chromatic adaptation transform utility using */
/* the current Absolute to Media Relative Transformation Space wpchtmx. */
/* Return a 3x3 chromatic adaptation matrix */
/* Use icmMulBy3x3(dst, mat, src) */
/* NOTE that to transform primaries they */
/* must be mat[XYZ][RGB] format! */
/* NOTE that this resets the chadValid flag (i.e. we assume that if */
/* this method gets called, that we are discarding any 'chad' tag */
/* and creating our own chromatic adapation) */
static void icc_chromAdaptMatrix(
	icc *p,
	int flags,				/* Transform given matrix flag */
	icmXYZNumber d_wp,		/* Destination white point */
	icmXYZNumber s_wp,		/* Source white point */
	double mat[3][3]		/* Destination matrix */
) {
	double dst[3], src[3];			/* Source & destination white points */
	double vkmat[3][3];				/* Von Kries matrix */

	if (p->header->deviceClass == icMaxEnumClass) {
		fprintf(stderr,"icc_chromAdaptMatrix called with no deviceClass!\n");
	}

	/* See if the profile type has changed, re-evaluate wpchtmx */
	if (p->wpchtmx_class != p->header->deviceClass) {
		icc_setup_wpchtmx(p);
	}

	/* Set initial matrix to unity if creating from scratch */
	if (!(flags & ICM_CAM_MULMATRIX)) {
		icmSetUnity3x3(mat);
	}

	/* Take a copy of src/dst */
	icmXYZ2Ary(src, s_wp);
	icmXYZ2Ary(dst, d_wp);

	/* Transform src/dst to cone space */
	icmMulBy3x3(src, p->wpchtmx, src);
	icmMulBy3x3(dst, p->wpchtmx, dst);

	/* Transform incoming matrix cone space */
	icmMul3x3(mat, p->wpchtmx);

	/* Setup the Von Kries white point adaptation matrix */
	vkmat[0][0] = dst[0]/src[0];
	vkmat[1][1] = dst[1]/src[1];
	vkmat[2][2] = dst[2]/src[2];
	vkmat[0][1] = vkmat[0][2] = 0.0;
	vkmat[1][0] = vkmat[1][2] = 0.0;
	vkmat[2][0] = vkmat[2][1] = 0.0;

	/* Apply chromatic adaptation */
	icmMul3x3(mat, vkmat);

	/* Transform from con space */
	icmMul3x3(mat, p->iwpchtmx);

	p->chadValid = 0;		/* Don't use this now */

	/* We're done */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* RGB XYZ primaries device to RGB->XYZ transform matrix. */
/* We assume that the device is perfectly additive, but that */
/* there may be a scale factor applied to the channels to */
/* match the white point at RGB = 1. */
/* Use icmMulBy3x3(dst, mat, src) */
/* Return non-zero if matrix would be singular */
int icmRGBXYZprim2matrix(
	double red[3],		/* Red colorant */
	double green[3],	/* Green colorant */
	double blue[3],		/* Blue colorant */
	double white[3],	/* White point */
	double mat[3][3]	/* Destination matrix[RGB][XYZ] */
) {
	double tmat[3][3];
	double t[3];

	/* Assemble the colorants into a matrix */
	tmat[0][0] = red[0];
	tmat[0][1] = green[0];
	tmat[0][2] = blue[0];
	tmat[1][0] = red[1];
	tmat[1][1] = green[1];
	tmat[1][2] = blue[1];
	tmat[2][0] = red[2];
	tmat[2][1] = green[2];
	tmat[2][2] = blue[2];

	/* Compute the inverse */
	if (icmInverse3x3(mat, tmat))
		return 1;

	/* Compute scale vector that maps colorants to white point */
	t[0] = mat[0][0] * white[0]
	     + mat[0][1] * white[1] 
	     + mat[0][2] * white[2]; 
	t[1] = mat[1][0] * white[0]
	     + mat[1][1] * white[1] 
	     + mat[1][2] * white[2]; 
	t[2] = mat[2][0] * white[0]
	     + mat[2][1] * white[1] 
	     + mat[2][2] * white[2]; 

	/* Now formulate the transform matrix */
	mat[0][0] = red[0]   * t[0];
	mat[0][1] = green[0] * t[1];
	mat[0][2] = blue[0]  * t[2];
	mat[1][0] = red[1]   * t[0];
	mat[1][1] = green[1] * t[1];
	mat[1][2] = blue[1]  * t[2];
	mat[2][0] = red[2]   * t[0];
	mat[2][1] = green[2] * t[1];
	mat[2][2] = blue[2]  * t[2];

	return 0;
}

/* RGB Yxy primaries to device to RGB->XYZ transform matrix */
/* Return non-zero if matrix would be singular */
/* Use icmMulBy3x3(dst, mat, src) */
int icmRGBYxyprim2matrix(
	double red[3],		/* Red colorant */
	double green[3],	/* Green colorant */
	double blue[3],		/* Blue colorant */
	double white[3],	/* White point */
	double mat[3][3],	/* Return matrix[RGB][XYZ] */
	double wXYZ[3]		/* Return white XYZ */
) {
	double r[3], g[3], b[3];
	
	icmYxy2XYZ(r, red);
	icmYxy2XYZ(g, green);
	icmYxy2XYZ(b, blue);
	icmYxy2XYZ(wXYZ, white);

	return icmRGBXYZprim2matrix(r, g, b, wXYZ, mat);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Pre-round a 3x3 matrix to ensure that the product of */
/* the matrix and the input value is the same as */
/* the quantized matrix product. This is used to improve accuracy */
/* of 'chad' tag in computing absolute white point. */ 
void quantize3x3S15Fixed16(
	double targ[3],			/* Target of product */
	double mat[3][3],		/* matrix[][] to be quantized */
	double in[3]			/* Input of product - must not be 0.0! */
) {
	int i, j;
	double sum[3];			/* == target */
	double tmp[3];			/* Uncorrected sum */

	printf("In     = %.8f %.8f %.8f\n",in[0], in[1], in[2]);
	printf("Target = %.8f %.8f %.8f\n",targ[0], targ[1], targ[2]);

	for (j = 0; j < 3; j++)
		sum[j] = targ[j];

	/* Pre-quantize the matrix, and then ensure that the */
	/* sum of the product of the quantized values is the quantized */
	/* sum by assigning the rounding error to the largest component. */
	for (i = 0; i < 3; i++) {
		int bix = 0;
		double bval = -1e9;

		/* locate the largest and quantize each matrix component */
		for (j = 0; j < 3; j++) {
			if (fabs(mat[i][j]) > bval) {	/* Locate largest */
				bix = j;
				bval = fabs(mat[i][j]);
			} 
			mat[i][j] = round_S15Fixed16Number(mat[i][j]);
		}

		/* Check the product of the uncorrected quantized values */
		tmp[i] = 0.0;
		for (j = 0; j < 3; j++)
			tmp[i] += mat[i][j] * in[j];

		/* Compute the value the largest has to be */
		/* to ensure that sum of the quantized mat[][] times in[] is */
		/* equal to the quantized sum. */
		for (j = 0; j < 3; j++) {
			if (j == bix)
				continue;
			sum[i] -= mat[i][j] * in[j];
		}
		mat[i][bix] = round_S15Fixed16Number(sum[i]/in[i]);

		/* Check the product of the corrected quantized values */
		sum[i] = 0.0;
		for (j = 0; j < 3; j++)
			sum[i] += mat[i][j] * in[j];
	}
	printf("Q Sum     = %.8f %.8f %.8f\n",tmp[0], tmp[1], tmp[2]);
	printf("Q cor Sum = %.8f %.8f %.8f\n",sum[0], sum[1], sum[2]);
}

/* Pre-round RGB device primary values to ensure that */
/* the sum of the quantized primaries is the same as */
/* the quantized sum. */
/* [Note matrix is transposed compared to quantize3x3S15Fixed16() ] */  
void quantizeRGBprimsS15Fixed16(
	double mat[3][3]		/* matrix[RGB][XYZ] */
) {
	int i, j;
	double sum[3];

//	printf("D50 = %f %f %f\n",icmD50.X, icmD50.Y, icmD50.Z);

	/* Compute target sum of primary XYZ */
	for (i = 0; i < 3; i++) {
		sum[i] = 0.0;
		for (j = 0; j < 3; j++)
			sum[i] += mat[j][i];
	}
//	printf("Sum = %f %f %f\n",sum[0], sum[1], sum[2]);

	/* Pre-quantize the primary XYZ's, and then ensure that the */
	/* sum of the quantized values is the quantized sum by assigning */
	/* the rounding error to the largest component. */
	for (i = 0; i < 3; i++) {
		int bix = 0;
		double bval = -1e9;

		/* locate the largest and quantize each component */
		for (j = 0; j < 3; j++) {
			if (fabs(mat[j][i]) > bval) {	/* Locate largest */
				bix = j;
				bval = fabs(mat[j][i]);
			} 
			mat[j][i] = round_S15Fixed16Number(mat[j][i]);
		}

		/* Compute the value the largest has to be */
		/* to ensure that sum of the quantized values is */
		/* equal to the quantized sum */
		for (j = 0; j < 3; j++) {
			if (j == bix)
				continue;
			sum[i] -= mat[j][i];
		}
		mat[bix][i] = round_S15Fixed16Number(sum[i]);

		/* Check the sum of the quantized values */
//		sum[i] = 0.0;
//		for (j = 0; j < 3; j++)
//			sum[i] += mat[j][i];
	}
//	printf("Q cor Sum = %f %f %f\n",sum[0], sum[1], sum[2]);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Some PCS utility functions */

/* Clip Lab, while maintaining hue angle. */
/* Return nz if clipping occured */
int icmClipLab(double out[3], double in[3]) {
	double C;

	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];

	if (out[0] >= 0.0 && out[0] <= 100.0
	 && out[1] >= -128.0 && out[1] <= 127.0
	 && out[2] >= -128.0 && out[2] <= 127.0)
		return 0;

	/* Clip L */
	if (out[0] < 0.0)
		out[0] = 0.0;
	else if (out[0] > 100.0)
		out[0] = 100.0;
	
	C = out[1];
	if (fabs(out[2]) > fabs(C))
		C = out[2];
	if (C < -128.0 || C > 127.0) {
		if (C < 0.0)
			C = -128.0/C;
		else
			C = 127.0/C;
		out[1] *= C;
		out[2] *= C;
	}

	return 1;
}

/* Clip XYZ, while maintaining hue angle */
/* Return nz if clipping occured */
int icmClipXYZ(double out[3], double in[3]) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];

	if (out[0] >= 0.0 && out[0] <= 1.9999
	 && out[1] >= 0.0 && out[1] <= 1.9999
	 && out[2] >= 0.0 && out[2] <= 1.9999)
		return 0;

	/* Clip Y and scale X and Z similarly */

	if (out[1] > 1.9999) {
		out[0] *= 1.9999/out[1];
		out[2] *= 1.9999/out[1];
		out[1] = 1.9999;
	} else if (out[1] < 0.0) {
		out[0] = 0.0;
		out[1] = 0.0;
		out[2] = 0.0;
	}

	if (out[0] < 0.0 || out[0] > 1.9999
	 || out[2] < 0.0 || out[2] > 1.9999) {
		double D50[3] = { 0.9642, 1.0000, 0.8249 };
		double bb = 0.0;

		/* Scale the D50 so that it has the same Y value as our color */
		D50[0] *= out[1];
		D50[1] *= out[1];
		D50[2] *= out[1];

		/* Figure out what blend factor with Y scaled D50, brings our */
		/* color X and Z values into range */

		if (out[0] < 0.0) {
			double b;
			b = (0.0 - out[0])/(D50[0] - out[0]);
			if (b > bb)
				bb = b;
		} else if (out[0] > 1.9999) {
			double b;
			b = (1.9999 - out[0])/(D50[0] - out[0]);
			if (b > bb)
				bb = b;
		}
		if (out[2] < 0.0) {
			double b;
			b = (0.0 - out[2])/(D50[2] - out[2]);
			if (b > bb)
				bb = b;
		} else if (out[2] > 1.9999) {
			double b;
			b = (1.9999 - out[2])/(D50[2] - out[2]);
			if (b > bb)
				bb = b;
		}
		/* Do the desaturation */
		out[0] = D50[0] * bb + (1.0 - bb) * out[0];
		out[2] = D50[2] * bb + (1.0 - bb) * out[2];
	}
	return 1;
}

/* --------------------------------------------------------------- */
/* Some video specific functions */

/* Should add ST.2048 log functions */

/* Convert Lut table index/value to YPbPr */
/* (Same as Lut_Lut2YPbPr() ) */ 
void icmLut2YPbPr(double *out, double *in) {
	out[0] = in[0];			/* Y */
	out[1] = in[1] - 0.5;	/* Cb */
	out[2] = in[2] - 0.5;	/* Cr */
}

/* Convert YPbPr to Lut table index/value */
/* (Same as Lut_YPbPr2Lut() ) */ 
void icmYPbPr2Lut(double *out, double *in) {
	out[0] = in[0];			/* Y */
	out[1] = in[1] + 0.5;	/* Cb */
	out[2] = in[2] + 0.5;	/* Cr */
}

/* Convert Rec601 RGB' into YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* [From the Rec601 spec. ] */
void icmRec601_RGBd_2_YPbPr(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 0.299 * in[0] + 0.587 * in[1] + 0.114 * in[2];

	tt[1] =     -0.299 /1.772 * in[0]
	      +     -0.587 /1.772 * in[1]
          + (1.0-0.114)/1.772 * in[2];

	tt[2] = (1.0-0.299)/1.402 * in[0]
	      +     -0.587 /1.402 * in[1]
          +     -0.114 /1.402 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec601 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* [Inverse of above] */
void icmRec601_YPbPr_2_RGBd(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 1.000000000 * in[0] +  0.000000000 * in[1] +  1.402000000 * in[2];
	tt[1] = 1.000000000 * in[0] + -0.344136286 * in[1] + -0.714136286 * in[2];
	tt[2] = 1.000000000 * in[0] +  1.772000000 * in[1] +  0.000000000 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}


/* Convert Rec709 1150/60/2:1 RGB' into YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* (This is for digital Rec709 and is very close to analog  Rec709 60Hz.) */
/* [From the Rec709 specification] */
void icmRec709_RGBd_2_YPbPr(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 0.2126 * in[0] + 0.7152 * in[1] + 0.0722 * in[2];

	tt[1] = 1.0/1.8556 * -0.2126      * in[0]
	      + 1.0/1.8556 * -0.7152      * in[1]
          + 1.0/1.8556 * (1.0-0.0722) * in[2];

	tt[2] = 1.0/1.5748 * (1.0-0.2126) * in[0]
	      + 1.0/1.5748 * -0.7152      * in[1]
          + 1.0/1.5748 * -0.0722      * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec709 1150/60/2:1 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* (This is for digital Rec709 and is very close to analog  Rec709 60Hz.) */
/* [Inverse of above] */
void icmRec709_YPbPr_2_RGBd(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 1.000000000 * in[0] +  0.000000000 * in[1] +  1.574800000 * in[2];
	tt[1] = 1.000000000 * in[0] + -0.187324273 * in[1] + -0.468124273 * in[2];
	tt[2] = 1.000000000 * in[0] +  1.855600000 * in[1] +  0.000000000 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec709 1250/50/2:1 RGB' into YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* (This is for analog Rec709 50Hz) */
/* [From the Rec709 specification] */
void icmRec709_50_RGBd_2_YPbPr(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 0.299 * in[0] + 0.587 * in[1] + 0.114 * in[2];

	tt[1] = 0.564 * -0.299      * in[0]
	      + 0.564 * -0.587      * in[1]
          + 0.564 * (1.0-0.114) * in[2];

	tt[2] = 0.713 * (1.0-0.299) * in[0]
	      + 0.713 * -0.587      * in[1]
          + 0.713 * -0.114      * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec709 1250/50/2:1 YPbPr to RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* (This is for analog Rec709 50Hz) */
/* [Inverse of above] */
void icmRec709_50_YPbPr_2_RGBd(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 1.000000000 * in[0] +  0.000000000 * in[1] +  1.402524544 * in[2];
	tt[1] = 1.000000000 * in[0] + -0.344340136 * in[1] + -0.714403473 * in[2];
	tt[2] = 1.000000000 * in[0] +  1.773049645 * in[1] +  0.000000000 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}


/* Convert Rec2020 RGB' into Non-constant liminance YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* [From the Rec2020 specification] */
void icmRec2020_NCL_RGBd_2_YPbPr(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 0.2627 * in[0] + 0.6780 * in[1] + 0.0593 * in[2];

	tt[1] = 1/1.8814 * -0.2627      * in[0]
	      + 1/1.8814 * -0.6780      * in[1]
          + 1/1.8814 * (1.0-0.0593) * in[2];

	tt[2] = 1/1.4746 * (1.0-0.2627) * in[0]
	      + 1/1.4746 * -0.6780      * in[1]
          + 1/1.4746 * -0.0593      * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec2020 Non-constant liminance YPbPr into RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* [Inverse of above] */
void icmRec2020_NCL_YPbPr_2_RGBd(double out[3], double in[3]) {
	double tt[3];

	tt[0] = 1.000000000 * in[0] +  0.000000000 * in[1] +  1.474600000 * in[2];
	tt[1] = 1.000000000 * in[0] + -0.164553127 * in[1] + -0.571353127 * in[2];
	tt[2] = 1.000000000 * in[0] +  1.881400000 * in[1] +  0.000000000 * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec2020 RGB' into Constant liminance YPbPr, or "full range YCbCr" */
/* where input 0..1, output 0..1, -0.5 .. 0.5, -0.5 .. 0.5 */
/* [From the Rec2020 specification] */
void icmRec2020_CL_RGBd_2_YPbPr(double out[3], double in[3]) {
	int i;
	double tt[3];

	/* Convert RGB' to RGB */
	for (i = 0; i < 3; i++) {
		if (in[i] < (4.5 * 0.0181))
			tt[i] = in[i]/4.5;
		else
			tt[i] = pow((in[i] + 0.0993)/1.0993, 1.0/0.45);
	}

	/* Y value */
	tt[0] = 0.2627 * tt[0] + 0.6780 * tt[1] + 0.0593 * tt[2];

	/* Y' value */
	if (tt[0] < 0.0181)
		tt[0] = tt[0] * 4.5;
	else
		tt[0] = 1.0993 * pow(tt[0], 0.45) - 0.0993;

	tt[1] = in[2] - tt[0];
	if (tt[1] <= 0.0)
		tt[1] /= 1.9404;
	else
		tt[1] /= 1.5816;

	tt[2] = in[0] - tt[0];
	if (tt[2] <= 0.0)
		tt[2] /= 1.7184;
	else
		tt[2] /= 0.9936;

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

/* Convert Rec2020 Constant liminance YPbPr into RGB' (== "full range YCbCr") */
/* where input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, output 0.0 .. 1 */
/* [Inverse of above] */
void icmRec2020_CL_YPbPr_2_RGBd(double out[3], double in[3]) {
	int i;
	double tin[3], tt[3];

	/* Y' */
	tin[0] = in[0];

	/* B' - Y' */
	if (in[1] <= 0.0)
		tin[1] = 1.9404 * in[1];
	else
		tin[1] = 1.5816 * in[1];

	/* R' - Y' */
	if (in[2] <= 0.0)
		tin[2] = 1.7184 * in[2];
	else
		tin[2] = 0.9936 * in[2];


	/* R' */
	tt[0] = tin[2] + tin[0];

	/* Y' */
	tt[1] = tin[0];

	/* B' */
	tt[2] = tin[1] + tin[0];

	/* Convert RYB' to RYB */
	for (i = 0; i < 3; i++) {
		if (tt[i] < (4.5 * 0.0181))
			tin[i] = tt[i]/4.5;
		else
			tin[i] = pow((tt[i] + 0.0993)/1.0993, 1.0/0.45);
	}
	
	/* G */
	tt[1] = (tin[1] - 0.2627 * tin[0] - 0.0593 * tin[2])/0.6780;

	/* G' */
	if (tt[1] < 0.0181)
		tt[1] = tt[1] * 4.5;
	else
		tt[1] = 1.0993 * pow(tt[1], 0.45) - 0.0993;
	
	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}


/* Convert Rec601/Rec709/Rec2020 YPbPr to YCbCr Video range. */
/* input 0..1, -0.5 .. 0.5, -0.5 .. 0.5, */
/* output 16/255 .. 235/255, 16/255 .. 240/255, 16/255 .. 240/255 */ 
void icmRecXXX_YPbPr_2_YCbCr(double out[3], double in[3]) {
	out[0] = ((235.0 - 16.0) * in[0] + 16.0)/255.0;
	out[1] = ((128.0 - 16.0) * 2.0 * in[1] + 128.0)/255.0;
	out[2] = ((128.0 - 16.0) * 2.0 * in[2] + 128.0)/255.0;
}

/* Convert Rec601/Rec709/Rec2020 Video YCbCr to YPbPr range. */
/* input 16/255 .. 235/255, 16/255 .. 240/255, 16/255 .. 240/255 */ 
/* output 0..1, -0.5 .. 0.5, -0.5 .. 0.5, */
void icmRecXXX_YCbCr_2_YPbPr(double out[3], double in[3]) {
	out[0] = (255.0 * in[0] - 16.0)/(235.0 - 16.0);
	out[1] = (255.0 * in[1] - 128.0)/(2.0 * (128.0 - 16.0));
	out[2] = (255.0 * in[2] - 128.0)/(2.0 * (128.0 - 16.0));
}

/* Convert full range RGB to Video range 16..235 RGB */
void icmRGB_2_VidRGB(double out[3], double in[3]) {
	out[0] = ((235.0 - 16.0) * in[0] + 16.0)/255.0;
	out[1] = ((235.0 - 16.0) * in[1] + 16.0)/255.0;
	out[2] = ((235.0 - 16.0) * in[2] + 16.0)/255.0;
}

/* Convert Video range 16..235 RGB to full range RGB */
/* Return nz if outside RGB range */
void icmVidRGB_2_RGB(double out[3], double in[3]) {
	out[0] = (255.0 * in[0] - 16.0)/(235.0 - 16.0);
	out[1] = (255.0 * in[1] - 16.0)/(235.0 - 16.0);
	out[2] = (255.0 * in[2] - 16.0)/(235.0 - 16.0);
}

/* =============================================================== */
/* PS 3.14-2009, Digital Imaging and Communications in Medicine */
/* (DICOM) Part 14: Grayscale Standard Display Function */

/* JND index value 1..1023 to L 0.05 .. 3993.404 cd/m^2 */
static double icmDICOM_fwd_nl(double jnd) {
	double a = -1.3011877;
	double b = -2.5840191e-2;
	double c =  8.0242636e-2;
	double d = -1.0320229e-1;
	double e =  1.3646699e-1;
	double f =  2.8745620e-2;
	double g = -2.5468404e-2;
	double h = -3.1978977e-3;
	double k =  1.2992634e-4;
	double m =  1.3635334e-3;
	double jj, num, den, rv;

	jj = jnd = log(jnd);

	num = a;
	den = 1.0;
	num += c * jj;
	den += b * jj;
	jj *= jnd;
	num += e * jj;
	den += d * jj;
	jj *= jnd;
	num += g * jj;
	den += f * jj;
	jj *= jnd;
	num += m * jj;
	den += h * jj;
	jj *= jnd;
	den += k * jj;
	
	rv = pow(10.0, num/den);

	return rv;
}

/* JND index value 1..1023 to L 0.05 .. 3993.404 cd/m^2 */
double icmDICOM_fwd(double jnd) {

	if (jnd < 0.5)
		jnd = 0.5;
	if (jnd > 1024.0)
		jnd = 1024.0;

	return icmDICOM_fwd_nl(jnd);
}

/* L 0.05 .. 3993.404 cd/m^2 to JND index value 1..1023 */
/* This is not super accurate -  typically to 0.03 .. 0.1 jne. */
static double icmDICOM_bwd_apx(double L) {
	double A = 71.498068;
	double B = 94.593053;
	double C = 41.912053;
	double D =  9.8247004;
	double E =  0.28175407;
	double F = -1.1878455;
	double G = -0.18014349;
	double H =  0.14710899;
	double I = -0.017046845;
	double rv, LL;

	if (L < 0.049982) {			/* == jnd 0.5 */
		return 0.5;
	}
	if (L > 4019.354716)		/* == jnd 1024 */
		L = 4019.354716;

	LL = L = log10(L);
	rv = A;
	rv += B * LL;
	LL *= L;
	rv += C * LL;
	LL *= L;
	rv += D * LL;
	LL *= L;
	rv += E * LL;
	LL *= L;
	rv += F * LL;
	LL *= L;
	rv += G * LL;
	LL *= L;
	rv += H * LL;
	LL *= L;
	rv += I * LL;

	return rv;
}

/* L 0.05 .. 3993.404 cd/m^2 to JND index value 1..1023 */
/* Polish the aproximate solution twice using Newton's itteration */
double icmDICOM_bwd(double L) {
	double rv, Lc, prv, pLc, de;
	int i;

	if (L < 0.045848) 			/* == jnd 0.5 */
		L = 0.045848;
	if (L > 4019.354716)		/* == jnd 1024 */
		L = 4019.354716;

	/* Approx solution */
	rv = icmDICOM_bwd_apx(L);

	/* Compute aprox derivative */
	Lc = icmDICOM_fwd_nl(rv);

	prv = rv + 0.01;
	pLc = icmDICOM_fwd_nl(prv);

	do {
		de = (rv - prv)/(Lc - pLc);
		prv = rv;
		rv -= (Lc - L) * de;
		pLc = Lc;
		Lc = icmDICOM_fwd_nl(rv);
	} while (fabs(Lc - L) > 1e-8);

	return rv;
}


/* =============================================================== */

/* Object for computing RFC 1321 MD5 checksums. */
/* Derived from Colin Plumb's 1993 public domain code. */

/* Reset the checksum */
static void icmMD5_reset(icmMD5 *p) {
	p->tlen = 0;

	p->sum[0] = 0x67452301;
	p->sum[1] = 0xefcdab89;
	p->sum[2] = 0x98badcfe;
	p->sum[3] = 0x10325476;

	p->fin = 0;
}

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

#define MD5STEP(f, w, x, y, z, pp, xtra, s) \
		data = (pp)[0] + ((pp)[3] << 24) + ((pp)[2] << 16) + ((pp)[1] << 8); \
		w += f(x, y, z) + data + xtra; \
		w = (w << s) | (w >> (32-s)); \
		w += x;

/* Add another 64 bytes to the checksum */
static void icmMD5_accume(icmMD5 *p, ORD8 *in) {
	ORD32 data, a, b, c, d;

	a = p->sum[0];
	b = p->sum[1];
	c = p->sum[2];
	d = p->sum[3];

	MD5STEP(F1, a, b, c, d, in + (4 * 0),  0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 1),  0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 2),  0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 3),  0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 4),  0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 5),  0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 6),  0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 7),  0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 8),  0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 9),  0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 10), 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 11), 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 12), 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 13), 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 14), 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 15), 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in + (4 * 1),  0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 6),  0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 11), 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 0),  0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 5),  0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 10), 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 15), 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 4),  0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 9),  0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 14), 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 3),  0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 8),  0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 13), 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 2),  0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 7),  0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 12), 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in + (4 * 5),  0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 8),  0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 11), 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 14), 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 1),  0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 4),  0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 7),  0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 10), 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 13), 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 0),  0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 3),  0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 6),  0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 9),  0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 12), 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 15), 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 2),  0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in + (4 * 0),  0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 7),  0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 14), 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 5),  0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 12), 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 3),  0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 10), 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 1),  0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 8),  0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 15), 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 6),  0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 13), 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 4),  0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 11), 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 2),  0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 9),  0xeb86d391, 21);

	p->sum[0] += a;
	p->sum[1] += b;
	p->sum[2] += c;
	p->sum[3] += d;
}

#undef F1
#undef F2
#undef F3
#undef F4
#undef MD5STEP

/* Add some bytes */
static void icmMD5_add(icmMD5 *p, ORD8 *ibuf, unsigned int len) {
	unsigned int bs;

	if (p->fin)
		return;				/* This is actually an error */

	bs = p->tlen;			/* Current bytes added */
	p->tlen = bs + len;		/* Update length after adding this buffer */
	bs &= 0x3f;				/* Bytes already in buffer */

	/* Deal with any existing partial bytes in p->buf */
	if (bs) {
		ORD8 *np = (ORD8 *)p->buf + bs;	/* Next free location in partial buffer */

		bs = 64 - bs;		/* Free space in partial buffer */

		if (len < bs) {		/* Not enought new to make a full buffer */
			memmove(np, ibuf, len);
			return;
		}

		memmove(np, ibuf, bs);	/* Now got one full buffer */
		icmMD5_accume(p, np);
		ibuf += bs;
		len -= bs;
	}

	/* Deal with input data 64 bytes at a time */
	while (len >= 64) {
		icmMD5_accume(p, ibuf);
		ibuf += 64;
		len -= 64;
	}

	/* Deal with any remaining bytes */
	memmove(p->buf, ibuf, len);
}

/* Finalise the checksum and return the result. */
static void icmMD5_get(icmMD5 *p, ORD8 chsum[16]) {
	int i;
	unsigned count;
	ORD32 bits1, bits0;
	ORD8 *pp;

	if (p->fin == 0) {
	
		/* Compute number of bytes processed mod 64 */
		count = p->tlen & 0x3f;

		/* Set the first char of padding to 0x80.  This is safe since there is
		   always at least one byte free */
		pp = p->buf + count;
		*pp++ = 0x80;

		/* Bytes of padding needed to make 64 bytes */
		count = 64 - 1 - count;

		/* Pad out to 56 mod 64, allowing 8 bytes for length in bits. */
		if (count < 8) {	/* Not enough space for padding and length */

			memset(pp, 0, count);
			icmMD5_accume(p, p->buf);

			/* Now fill the next block with 56 bytes */
			memset(p->buf, 0, 56);
		} else {
			/* Pad block to 56 bytes */
			memset(pp, 0, count - 8);
		}

		/* Compute number of bits */
		bits1 = 0x7 & (p->tlen >> (32 - 3)); 
		bits0 = p->tlen << 3;

		/* Append number of bits */
		p->buf[64 - 8] = bits0 & 0xff; 
		p->buf[64 - 7] = (bits0 >> 8) & 0xff; 
		p->buf[64 - 6] = (bits0 >> 16) & 0xff; 
		p->buf[64 - 5] = (bits0 >> 24) & 0xff; 
		p->buf[64 - 4] = bits1 & 0xff; 
		p->buf[64 - 3] = (bits1 >> 8) & 0xff; 
		p->buf[64 - 2] = (bits1 >> 16) & 0xff; 
		p->buf[64 - 1] = (bits1 >> 24) & 0xff; 

		icmMD5_accume(p, p->buf);

		p->fin = 1;
	}

	/* Return the result, lsb to msb */
	pp = chsum;
	for (i = 0; i < 4; i++) {
		*pp++ = p->sum[i] & 0xff; 
		*pp++ = (p->sum[i] >> 8) & 0xff; 
		*pp++ = (p->sum[i] >> 16) & 0xff; 
		*pp++ = (p->sum[i] >> 24) & 0xff; 
	}
}


/* Delete the instance */
static void icmMD5_del(icmMD5 *p) {
	icmAlloc *al = p->al;
	int del_al   = p->del_al;

	/* This object */
	al->free(al, p);

	if (del_al)			/* We are responsible for deleting allocator */
		al->del(al);
}

/* Create a new MD5 checksumming object, with a reset checksum value */
/* Return it or NULL if there is an error */
extern icmMD5 *new_icmMD5_a(icmAlloc *al) {
	icmMD5 *p;

	if ((p = (icmMD5 *)al->calloc(al,1,sizeof(icmMD5))) == NULL)
		return NULL;

	p->al       = al;
	p->reset    = icmMD5_reset;
	p->add      = icmMD5_add;
	p->get      = icmMD5_get;
	p->del      = icmMD5_del;

	p->reset(p);

	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Dumy icmFile used to compute MD5 checksum on write */

/* Get the size of the file (Only valid for reading file. */
static size_t icmFileMD5_get_size(icmFile *pp) {
	icmFileMD5 *p = (icmFileMD5 *)pp;

	return p->size;
}

/* Set current position to offset. Return 0 on success, nz on failure. */
/* Seek can't be supported for MD5, so and seek must be to current location. */
static int icmFileMD5_seek(
icmFile *pp,
unsigned int offset
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;

	if (p->of != offset) {
		p->errc = 1;
	}
	if (p->of > p->size)
		p->size = p->of;
	return 0;
}

/* Read count items of size length. Return number of items successfully read. */
/* Read is not implemented */
static size_t icmFileMD5_read(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	return 0;
}

/* write count items of size length. Return number of items successfully written. */
/* Simply pass to MD5 to compute checksum */
static size_t icmFileMD5_write(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;
	size_t len = size * count;

	p->md5->add(p->md5, (ORD8 *)buffer, len);
	p->of += len;
	if (p->of > p->size)
		p->size = p->of;
	return count;
}

/* do a printf */
/* Not implemented */
static int icmFileMD5_printf(
icmFile *pp,
const char *format,
...
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;
	p->errc = 2;
	return 0;
}

/* flush all write data out to secondary storage. Return nz on failure. */
static int icmFileMD5_flush(
icmFile *pp
) {
	return 0;
}

/* we're done with the file object, return nz on failure */
static int icmFileMD5_delete(
icmFile *pp
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;

	p->al->free(p->al, p);	/* Free object */
	return 0;
}

/* Return the error code. Error code will usually be set */
/* if we did a seek to other than the current location, */
/* or did a printf. */
static int icmFileMD5_geterrc(
icmFile *pp
) {
	icmFileMD5 *p = (icmFileMD5 *)pp;

	return p->errc;
}

/* Create a checksum dump file access class with allocator */
icmFile *new_icmFileMD5_a(
icmMD5 *md5,		/* MD5 object to use */
icmAlloc *al		/* heap allocator */
) {
	icmFileMD5 *p;

	if ((p = (icmFileMD5 *) al->calloc(al, 1, sizeof(icmFileMD5))) == NULL) {
		return NULL;
	}
	p->md5      = md5;			/* MD5 compute object */
	p->al       = al;			/* Heap allocator */
	p->get_size = icmFileMD5_get_size;
	p->seek     = icmFileMD5_seek;
	p->read     = icmFileMD5_read;
	p->write    = icmFileMD5_write;
	p->gprintf   = icmFileMD5_printf;
	p->flush    = icmFileMD5_flush;
	p->del      = icmFileMD5_delete;
	p->get_errc = icmFileMD5_geterrc;

	p->of = 0;
	p->errc = 0;

	return (icmFile *)p;
}


/* ============================================= */
/* Implementation of color transform lookups.    */

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Methods common to all transforms (icmLuBase) : */

/* Return information about the native lut in/out/pcs colorspaces. */
/* Any pointer may be NULL if value is not to be returned */
static void
icmLutSpaces(
	struct _icmLuBase *p,			/* This */
	icColorSpaceSignature *ins,		/* Return Native input color space */
	int *inn,						/* Return number of input components */
	icColorSpaceSignature *outs,	/* Return Native output color space */
	int *outn,						/* Return number of output components */
	icColorSpaceSignature *pcs		/* Return Native PCS color space */
									/* (this will be the same as ins or outs */
									/* depending on the lookup direction) */
) {
	if (ins != NULL)
		*ins = p->inSpace;
	if (inn != NULL)
		*inn = (int)number_ColorSpaceSignature(p->inSpace);

	if (outs != NULL)
		*outs = p->outSpace;
	if (outn != NULL)
		*outn = (int)number_ColorSpaceSignature(p->outSpace);
	if (pcs != NULL)
		*pcs = p->pcs;
}

/* Return information about the effective lookup in/out colorspaces, */
/* including allowance for PCS override. */
/* Any pointer may be NULL if value is not to be returned */
static void
icmLuSpaces(
	struct _icmLuBase *p,			/* This */
	icColorSpaceSignature *ins,		/* Return effective input color space */
	int *inn,						/* Return number of input components */
	icColorSpaceSignature *outs,	/* Return effective output color space */
	int *outn,						/* Return number of output components */
	icmLuAlgType *alg,				/* Return type of lookup algorithm used */
    icRenderingIntent *intt,		/* Return the intent being implented */
    icmLookupFunc *fnc,				/* Return the profile function being implemented */
	icColorSpaceSignature *pcs,		/* Return the profile effective PCS */
	icmLookupOrder *ord				/* return the search Order */
) {
	if (ins != NULL)
		*ins = p->e_inSpace;
	if (inn != NULL)
		*inn = (int)number_ColorSpaceSignature(p->e_inSpace);

	if (outs != NULL)
		*outs = p->e_outSpace;
	if (outn != NULL)
		*outn = (int)number_ColorSpaceSignature(p->e_outSpace);

	if (alg != NULL)
		*alg = p->ttype;

    if (intt != NULL)
		*intt = p->intent;

	if (fnc != NULL)
		*fnc = p->function;

	if (pcs != NULL)
		*pcs = p->e_pcs;

	if (ord != NULL)
		*ord = p->order;
}

/* Relative to Absolute for this WP in XYZ */
static void icmLuXYZ_Rel2Abs(icmLuBase *p, double *out, double *in) {
	icmMulBy3x3(out, p->toAbs, in);
}

/* Absolute to Relative for this WP in XYZ */
static void icmLuXYZ_Abs2Rel(icmLuBase *p, double *out, double *in) {
	icmMulBy3x3(out, p->fromAbs, in);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Methods common to all non-named transforms (icmLuBase) : */

/* Initialise the LU white and black points from the ICC tags, */
/* and the corresponding absolute<->relative conversion matrices */
/* return nz on error */
static int icmLuInit_Wh_bk(
struct _icmLuBase *lup
) {
	icmXYZArray *whitePointTag, *blackPointTag;
	icc *p = lup->icp;

	if ((whitePointTag = (icmXYZArray *)p->read_tag(p, icSigMediaWhitePointTag)) == NULL
        || whitePointTag->ttype != icSigXYZType || whitePointTag->size < 1) {
		if (p->header->deviceClass != icSigLinkClass 
		 && (lup->intent == icAbsoluteColorimetric
		  || lup->intent == icmAbsolutePerceptual
		  || lup->intent == icmAbsoluteSaturation)) {
			sprintf(p->err,"icc_lookup: Profile is missing Media White Point Tag");
			p->errc = 1;
			return 1;
		}
		p->err[0] = '\000';
		p->errc = 0;
		lup->whitePoint = icmD50;					/* safe value */
	} else
		lup->whitePoint = whitePointTag->data[0];	/* Copy structure */

	if ((blackPointTag = (icmXYZArray *)p->read_tag(p, icSigMediaBlackPointTag)) == NULL
        || blackPointTag->ttype != icSigXYZType || blackPointTag->size < 1) {
		p->err[0] = '\000';
		p->errc = 0;
		lup->blackPoint = icmBlack;					/* default */
		lup->blackisassumed = 1;					/* We assumed the default */
	} else  {
		lup->blackPoint = blackPointTag->data[0];	/* Copy structure */
		lup->blackisassumed = 0;					/* The black is from the tag */
	}

	/* If this is a Display profile, check if there is a 'chad' tag, and setup the */
	/* white point and toAbs/fromAbs matricies from that, so as to implement an */
	/* effective Absolute Colorimetric intent for such profiles. */
 	if (p->header->deviceClass == icSigDisplayClass
	 && p->chadValid) {
		double wp[3];

		icmCpy3x3(lup->fromAbs, p->chadmx);
		icmInverse3x3(lup->toAbs, lup->fromAbs);

		/* Compute absolute white point. We deliberately ignore what's in the white point tag */
		/* and assume D50, since dealing with a non-D50 white point tag is contrary to ICCV4 */
		/* and full of ambiguity (i.e. is it a separate "media" white different to the */
		/* display white and not D50, or has the profile creator mistakenly put the display */
		/* white in the white point tag ?) */
		icmMulBy3x3(wp, lup->toAbs, icmD50_ary3); 
		icmAry2XYZ(lup->whitePoint, wp);

		DBLLL(("toAbs and fromAbs created from 'chad' tag\n"));
		DBLLL(("computed wp %.8f %.8f %.8f\n", lup->whitePoint.X,
		                               lup->whitePoint.Y, lup->whitePoint.Z));

	} else {
		/* Create absolute <-> relative conversion matricies */
		p->chromAdaptMatrix(p, ICM_CAM_NONE, lup->whitePoint, icmD50, lup->toAbs);
		p->chromAdaptMatrix(p, ICM_CAM_NONE, icmD50, lup->whitePoint,  lup->fromAbs);
		DBLLL(("toAbs and fromAbs created from wp %f %f %f and D50 %f %f %f\n", lup->whitePoint.X,
		                       lup->whitePoint.Y, lup->whitePoint.Z, icmD50.X, icmD50.Y, icmD50.Z));
	}

	DBLLL(("toAbs   = %f %f %f\n          %f %f %f\n          %f %f %f\n",
	        lup->toAbs[0][0], lup->toAbs[0][1], lup->toAbs[0][2],
	        lup->toAbs[1][0], lup->toAbs[1][1], lup->toAbs[1][2],
	        lup->toAbs[2][0], lup->toAbs[2][1], lup->toAbs[2][2]));
	DBLLL(("fromAbs = %f %f %f\n          %f %f %f\n          %f %f %f\n",
	        lup->fromAbs[0][0], lup->fromAbs[0][1], lup->fromAbs[0][2],
	        lup->fromAbs[1][0], lup->fromAbs[1][1], lup->fromAbs[1][2],
	        lup->fromAbs[2][0], lup->fromAbs[2][1], lup->fromAbs[2][2]));

	return 0;
}

/* Return the media white and black points in absolute XYZ space. */
/* Note that if not in the icc, the black point will be returned as 0, 0, 0, */
/* and the function will return nz. */ 
/* Any pointer may be NULL if value is not to be returned */
static int icmLuWh_bk_points(
struct _icmLuBase *p,
double *wht,
double *blk
) {
	if (wht != NULL) {
		icmXYZ2Ary(wht,p->whitePoint);
	}

	if (blk != NULL) {
		icmXYZ2Ary(blk,p->blackPoint);
	}
	if (p->blackisassumed)
		return 1;
	return 0;
}

/* Get the LU white and black points in LU PCS space, converted to XYZ. */
/* (ie. white and black will be relative if LU is relative intent etc.) */
/* Return nz if the black point is being assumed to be 0,0,0 rather */
/* than being from the tag. */															\
static int icmLuLu_wh_bk_points(
struct _icmLuBase *p,
double *wht,
double *blk
) {
	if (wht != NULL) {
		icmXYZ2Ary(wht,p->whitePoint);
	}

	if (blk != NULL) {
		icmXYZ2Ary(blk,p->blackPoint);
	}
	if (p->intent != icAbsoluteColorimetric
	 && p->intent != icmAbsolutePerceptual
	 && p->intent != icmAbsoluteSaturation) {
		if (wht != NULL)
			icmMulBy3x3(wht, p->fromAbs, wht);
		if (blk != NULL)
			icmMulBy3x3(blk, p->fromAbs, blk);
	}
	if (p->blackisassumed)
		return 1;
	return 0;
}

/* Get the native (internal) ranges for the Monochrome or Matrix profile */
/* Arguments may be NULL */
static void
icmLu_get_lutranges (
	struct _icmLuBase *p,
	double *inmin, double *inmax,		/* Return maximum range of inspace values */
	double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	icTagTypeSignature tagType;

	if (p->ttype == icmLutType) {
		icmLuLut *pp = (icmLuLut *)p;
		tagType = pp->lut->ttype;
	} else {
		tagType = icMaxEnumType;
	}

	/* Hmm. we have no way of handling an error from getRange. */
	/* It shouldn't ever return one unless there is a mismatch between */
	/* getRange and Lu creation... */
	getRange(p->icp, p->inSpace, tagType, inmin, inmax);
	getRange(p->icp, p->outSpace, tagType, outmin, outmax);
}

/* Get the effective (externally visible) ranges for the all profile types */
/* Arguments may be NULL */
static void
icmLu_get_ranges (
	struct _icmLuBase *p,
	double *inmin, double *inmax,		/* Return maximum range of inspace values */
	double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	icTagTypeSignature tagType;

	if (p->ttype == icmLutType) {
		icmLuLut *pp = (icmLuLut *)p;
		tagType = pp->lut->ttype;
	} else {
		tagType = icMaxEnumType;
	}
	/* Hmm. we have no way of handling an error from getRange. */
	/* It shouldn't ever return one unless there is a mismatch between */
	/* getRange and Lu creation... */
	getRange(p->icp, p->e_inSpace, tagType, inmin, inmax);
	getRange(p->icp, p->e_outSpace, tagType, outmin, outmax);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Forward and Backward Monochrome type methods: */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* Individual components of Fwd conversion: */

/* Actual device to linearised device */
static int
icmLuMonoFwd_curve (
icmLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;
	int rv = 0;

	/* Translate from device to PCS scale */
	if ((rv |= p->grayCurve->lookup_fwd(p->grayCurve,&out[0],&in[0])) > 1) {
		sprintf(icp->err,"icc_lookup: Curve->lookup_fwd() failed");
		icp->errc = rv;
		return 2;
	}

	return rv;
}

/* Linearised device to relative PCS */
static int
icmLuMonoFwd_map (
icmLuMono *p,		/* This */
double *out,		/* Vector of output values (native space) */
double *in			/* Vector of input values (native space) */
) {
	int rv = 0;
	double Y = in[0];		/* In case out == in */

	out[0] = p->pcswht.X;
	out[1] = p->pcswht.Y;
	out[2] = p->pcswht.Z;
	if (p->pcs == icSigLabData)
		icmXYZ2Lab(&p->pcswht, out, out);	/* in Lab */

	/* Scale linearized device level to PCS white */
	out[0] *= Y;
	out[1] *= Y;
	out[2] *= Y;

	return rv;
}

/* relative PCS to absolute PCS (if required) */
static int
icmLuMonoFwd_abs (	/* Abs comes last in Fwd conversion */
icmLuMono *p,		/* This */
double *out,		/* Vector of output values in Effective PCS */
double *in			/* Vector of input values in Native PCS */
) {
	int rv = 0;

	if (out != in) {	/* Don't alter input values */
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	/* Do absolute conversion */
	if (p->intent == icAbsoluteColorimetric
	 || p->intent == icmAbsolutePerceptual
	 || p->intent == icmAbsoluteSaturation) {

		if (p->pcs == icSigLabData) 	/* Convert L to Y */
			icmLab2XYZ(&p->pcswht, out, out);
		
		/* Convert from Relative to Absolute colorimetric */
		icmMulBy3x3(out, p->toAbs, out);
		
		if (p->e_pcs == icSigLabData)
			icmXYZ2Lab(&p->pcswht, out, out);

	} else {

		/* Convert from Native to Effective output space */
		if (p->pcs == icSigLabData && p->e_pcs == icSigXYZData)
			icmLab2XYZ(&p->pcswht, out, out);
		else if (p->pcs == icSigXYZData && p->e_pcs == icSigLabData)
			icmXYZ2Lab(&p->pcswht, out, out);
	}

	return rv;
}


/* Overall Fwd conversion routine (Dev->PCS) */
static int
icmLuMonoFwd_lookup (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Input value */
) {
	int rv = 0;
	icmLuMono *p = (icmLuMono *)pp;
	rv |= icmLuMonoFwd_curve(p, out, in);
	rv |= icmLuMonoFwd_map(p, out, out);
	rv |= icmLuMonoFwd_abs(p, out, out);
	return rv;
}

/* Three stage conversion routines */
static int
icmLuMonoFwd_lookup_in(
icmLuBase *pp,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMono *p = (icmLuMono *)pp;
	rv |= icmLuMonoFwd_curve(p, out, in);
	return rv;
}

static int
icmLuMonoFwd_lookup_core(
icmLuBase *pp,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMono *p = (icmLuMono *)pp;
	rv |= icmLuMonoFwd_map(p, out, in);
	rv |= icmLuMonoFwd_abs(p, out, out);
	return rv;
}

static int
icmLuMonoFwd_lookup_out(
icmLuBase *pp,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	out[0] = in[0]; 
	out[1] = in[1]; 
	out[2] = in[2]; 
	return rv;
}


/* -  -  -  -  -  -  -  -  -  -  -  -  -  - */
/* Individual components of Bwd conversion: */

/* Convert from relative PCS to absolute PCS (if required) */
static int
icmLuMonoBwd_abs (	/* Abs comes first in Bwd conversion */
icmLuMono *p,		/* This */
double *out,		/* Vector of output values in Native PCS */
double *in			/* Vector of input values in Effective PCS */
) {
	int rv = 0;

	if (out != in) {	/* Don't alter input values */
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	/* Force to monochrome locus in correct space */
	if (p->e_pcs == icSigLabData) {
		double wp[3];

		if (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation) {
			wp[0] = p->whitePoint.X;
			wp[1] = p->whitePoint.Y;
			wp[2] = p->whitePoint.Z;
		} else {
			wp[0] = p->pcswht.X;
			wp[1] = p->pcswht.Y;
			wp[2] = p->pcswht.Z;
		}
		icmXYZ2Lab(&p->pcswht, wp, wp);	/* Convert to Lab white point */
		out[1] = out[0]/wp[0] * wp[1];
		out[2] = out[0]/wp[0] * wp[2];

	} else {
		if (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation) {
			out[0] = out[1]/p->whitePoint.Y * p->whitePoint.X;
			out[2] = out[1]/p->whitePoint.Y * p->whitePoint.Z;
		} else {
			out[0] = out[1]/p->pcswht.Y * p->pcswht.X;
			out[2] = out[1]/p->pcswht.Y * p->pcswht.Z;
		}
	}

	/* Do absolute conversion, and conversion to effective PCS */
	if (p->intent == icAbsoluteColorimetric
	 || p->intent == icmAbsolutePerceptual
	 || p->intent == icmAbsoluteSaturation) {

		if (p->e_pcs == icSigLabData)
			icmLab2XYZ(&p->pcswht, out, out);

		icmMulBy3x3(out, p->fromAbs, out);

		/* Convert from Effective to Native input space */
		if (p->pcs == icSigLabData)
			icmXYZ2Lab(&p->pcswht, out, out);

	} else {

		/* Convert from Effective to Native input space */
		if (p->e_pcs == icSigLabData && p->pcs == icSigXYZData)
			icmLab2XYZ(&p->pcswht, out, out);
		else if (p->e_pcs == icSigXYZData && p->pcs == icSigLabData)
			icmXYZ2Lab(&p->pcswht, out, out);
	}

	return rv;
}

/* Map from relative PCS to linearised device */
static int
icmLuMonoBwd_map (
icmLuMono *p,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values (native space) */
) {
	int rv = 0;
	double pcsw[3];

	pcsw[0] = p->pcswht.X;
	pcsw[1] = p->pcswht.Y;
	pcsw[2] = p->pcswht.Z;
	if (p->pcs == icSigLabData)
		icmXYZ2Lab(&p->pcswht, pcsw, pcsw);	/* in Lab (should be 100.0!) */

	/* Divide linearized device level into PCS white luminence */
	if (p->pcs == icSigLabData)
		out[0] = in[0]/pcsw[0];
	else
		out[0] = in[1]/pcsw[1];

	return rv;
}

/* Map from linearised device to actual device */
static int
icmLuMonoBwd_curve (
icmLuMono *p,		/* This */
double *out,		/* Output value */
double *in			/* Input value */
) {
	icc *icp = p->icp;
	int rv = 0;

	/* Convert to device value through curve */
	if ((rv = p->grayCurve->lookup_bwd(p->grayCurve,&out[0],&in[0])) > 1) {
		sprintf(icp->err,"icc_lookup: Curve->lookup_bwd() failed");
		icp->errc = rv;
		return 2;
	}

	return rv;
}

/* Overall Bwd conversion routine (PCS->Dev) */
static int
icmLuMonoBwd_lookup (
icmLuBase *pp,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	double temp[3];
	int rv = 0;
	icmLuMono *p = (icmLuMono *)pp;
	rv |= icmLuMonoBwd_abs(p, temp, in);
	rv |= icmLuMonoBwd_map(p, out, temp);
	rv |= icmLuMonoBwd_curve(p, out, out);
	return rv;
}

/* Three stage conversion routines */
static int
icmLuMonoBwd_lookup_in(
icmLuBase *pp,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	return rv;
}

static int
icmLuMonoBwd_lookup_core(
icmLuBase *pp,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	double temp[3];
	int rv = 0;
	icmLuMono *p = (icmLuMono *)pp;
	rv |= icmLuMonoBwd_abs(p, temp, in);
	rv |= icmLuMonoBwd_map(p, out, temp);
	return rv;
}

static int
icmLuMonoBwd_lookup_out(
icmLuBase *pp,		/* This */
double *out,		/* Output value */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMono *p = (icmLuMono *)pp;
	rv |= icmLuMonoBwd_curve(p, out, in);
	return rv;
}

/* -  -  -  -  -  -  -  -  -  -  -  -  -  - */

static void
icmLuMono_delete(
icmLuBase *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

static icmLuBase *
new_icmLuMono(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func,			/* Functionality requested */
	int dir								/* 0 = fwd, 1 = bwd */
) {
	icmLuMono *p;

	if ((p = (icmLuMono *) icp->al->calloc(icp->al,1,sizeof(icmLuMono))) == NULL)
		return NULL;
	p->icp      = icp;
	p->del      = icmLuMono_delete;
	p->lutspaces= icmLutSpaces;
	p->spaces   = icmLuSpaces;
	p->XYZ_Rel2Abs = icmLuXYZ_Rel2Abs;
	p->XYZ_Abs2Rel = icmLuXYZ_Abs2Rel;
	p->get_lutranges = icmLu_get_lutranges;
	p->get_ranges = icmLu_get_ranges;
	p->init_wh_bk = icmLuInit_Wh_bk;
	p->wh_bk_points = icmLuWh_bk_points;
	p->lu_wh_bk_points = icmLuLu_wh_bk_points;
	p->fwd_lookup = icmLuMonoFwd_lookup;
	p->fwd_curve  = icmLuMonoFwd_curve;
	p->fwd_map    = icmLuMonoFwd_map;
	p->fwd_abs    = icmLuMonoFwd_abs;
	p->bwd_lookup = icmLuMonoBwd_lookup;
	p->bwd_abs    = icmLuMonoFwd_abs;
	p->bwd_map    = icmLuMonoFwd_map;
	p->bwd_curve  = icmLuMonoFwd_curve;
	if (dir) {
		p->ttype         = icmMonoBwdType;
		p->lookup        = icmLuMonoBwd_lookup;
		p->lookup_in     = icmLuMonoBwd_lookup_in;
		p->lookup_core   = icmLuMonoBwd_lookup_core;
		p->lookup_out    = icmLuMonoBwd_lookup_out;
		p->lookup_inv_in = icmLuMonoFwd_lookup_out;		/* Opposite of Bwd_lookup_in */
	} else {
		p->ttype         = icmMonoFwdType;
		p->lookup        = icmLuMonoFwd_lookup;
		p->lookup_in     = icmLuMonoFwd_lookup_in;
		p->lookup_core   = icmLuMonoFwd_lookup_core;
		p->lookup_out    = icmLuMonoFwd_lookup_out;
		p->lookup_inv_in = icmLuMonoBwd_lookup_out;		/* Opposite of Fwd_lookup_in */
	}

	/* Lookup the white and black points */
	if (p->init_wh_bk((icmLuBase *)p)) {
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* See if the color spaces are appropriate for the mono type */
	if (number_ColorSpaceSignature(icp->header->colorSpace) != 1
	  || ( icp->header->pcs != icSigXYZData && icp->header->pcs != icSigLabData)) {
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Find the appropriate tags */
	if ((p->grayCurve = (icmCurve *)icp->read_tag(icp, icSigGrayTRCTag)) == NULL
         || p->grayCurve->ttype != icSigCurveType) {
		p->del((icmLuBase *)p);
		return NULL;
	}

	p->pcswht = icp->header->illuminant;
	p->intent   = intent;
	p->function = func;
	p->inSpace  = inSpace;
	p->outSpace = outSpace;
	p->pcs      = pcs;
	p->e_inSpace  = e_inSpace;
	p->e_outSpace = e_outSpace;
	p->e_pcs      = e_pcs;

	return (icmLuBase *)p;
}

static icmLuBase *
new_icmLuMonoFwd(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func			/* Functionality requested */
) {
	return new_icmLuMono(icp, inSpace, outSpace, pcs, e_inSpace, e_outSpace, e_pcs,
	                     intent, func, 0);
}


static icmLuBase *
new_icmLuMonoBwd(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func			/* Functionality requested */
) {
	return new_icmLuMono(icp, inSpace, outSpace, pcs, e_inSpace, e_outSpace, e_pcs,
	                     intent, func, 1);
}

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Forward and Backward Matrix type conversion */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* Individual components of Fwd conversion: */
static int
icmLuMatrixFwd_curve (
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;
	int rv = 0;

	/* Curve lookups */
	if ((rv |= p->redCurve->lookup_fwd(  p->redCurve,  &out[0],&in[0])) > 1
	 || (rv |= p->greenCurve->lookup_fwd(p->greenCurve,&out[1],&in[1])) > 1
	 || (rv |= p->blueCurve->lookup_fwd( p->blueCurve, &out[2],&in[2])) > 1) {
		sprintf(icp->err,"icc_lookup: Curve->lookup_fwd() failed");
		icp->errc = rv;
		return 2;
	}

	return rv;
}

static int
icmLuMatrixFwd_matrix (
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	double tt[3];

	/* Matrix */
	tt[0] = p->mx[0][0] * in[0] + p->mx[0][1] * in[1] + p->mx[0][2] * in[2];
	tt[1] = p->mx[1][0] * in[0] + p->mx[1][1] * in[1] + p->mx[1][2] * in[2];
	tt[2] = p->mx[2][0] * in[0] + p->mx[2][1] * in[1] + p->mx[2][2] * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];

	return rv;
}

static int
icmLuMatrixFwd_abs (/* Abs comes last in Fwd conversion */
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;

	if (out != in) {	/* Don't alter input values */
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	/* If required, convert from Relative to Absolute colorimetric */
	if (p->intent == icAbsoluteColorimetric
	 || p->intent == icmAbsolutePerceptual
	 || p->intent == icmAbsoluteSaturation) {
		icmMulBy3x3(out, p->toAbs, out);
	}

	/* If e_pcs is Lab, then convert XYZ to Lab */
	if (p->e_pcs == icSigLabData)
		icmXYZ2Lab(&p->pcswht, out, out);

	return rv;
}


/* Overall Fwd conversion (Dev->PCS)*/
static int
icmLuMatrixFwd_lookup (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMatrix *p = (icmLuMatrix *)pp;
	rv |= icmLuMatrixFwd_curve(p, out, in);
	rv |= icmLuMatrixFwd_matrix(p, out, out);
	rv |= icmLuMatrixFwd_abs(p, out, out);
	return rv;
}

/* Three stage conversion routines */
static int
icmLuMatrixFwd_lookup_in (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMatrix *p = (icmLuMatrix *)pp;
	rv |= icmLuMatrixFwd_curve(p, out, in);
	return rv;
}

static int
icmLuMatrixFwd_lookup_core (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMatrix *p = (icmLuMatrix *)pp;
	rv |= icmLuMatrixFwd_matrix(p, out, in);
	rv |= icmLuMatrixFwd_abs(p, out, out);
	return rv;
}

static int
icmLuMatrixFwd_lookup_out (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	return rv;
}

/* -  -  -  -  -  -  -  -  -  -  -  -  -  - */
/* Individual components of Bwd conversion: */

static int
icmLuMatrixBwd_abs (/* Abs comes first in Bwd conversion */
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;

	if (out != in) {	/* Don't alter input values */
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
	}

	/* If e_pcs is Lab, then convert Lab to XYZ */
	if (p->e_pcs == icSigLabData)
		icmLab2XYZ(&p->pcswht, out, out);

	/* If required, convert from Absolute to Relative colorimetric */
	if (p->intent == icAbsoluteColorimetric
	 || p->intent == icmAbsolutePerceptual
	 || p->intent == icmAbsoluteSaturation) {
		icmMulBy3x3(out, p->fromAbs, out);
	}

	return rv;
}

static int
icmLuMatrixBwd_matrix (
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	double tt[3];

	tt[0] = in[0];
	tt[1] = in[1];
	tt[2] = in[2];

	/* Matrix */
	out[0] = p->bmx[0][0] * tt[0] + p->bmx[0][1] * tt[1] + p->bmx[0][2] * tt[2];
	out[1] = p->bmx[1][0] * tt[0] + p->bmx[1][1] * tt[1] + p->bmx[1][2] * tt[2];
	out[2] = p->bmx[2][0] * tt[0] + p->bmx[2][1] * tt[1] + p->bmx[2][2] * tt[2];

	return rv;
}

static int
icmLuMatrixBwd_curve (
icmLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icc *icp = p->icp;
	int rv = 0;

	/* Curves */
	if ((rv |= p->redCurve->lookup_bwd(p->redCurve,&out[0],&in[0])) > 1
	 ||	(rv |= p->greenCurve->lookup_bwd(p->greenCurve,&out[1],&in[1])) > 1
	 || (rv |= p->blueCurve->lookup_bwd(p->blueCurve,&out[2],&in[2])) > 1) {
		sprintf(icp->err,"icc_lookup: Curve->lookup_bwd() failed");
		icp->errc = rv;
		return 2;
	}
	return rv;
}

/* Overall Bwd conversion (PCS->Dev) */
static int
icmLuMatrixBwd_lookup (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMatrix *p = (icmLuMatrix *)pp;
	rv |= icmLuMatrixBwd_abs(p, out, in);
	rv |= icmLuMatrixBwd_matrix(p, out, out);
	rv |= icmLuMatrixBwd_curve(p, out, out);
	return rv;
}

/* Three stage conversion routines */
static int
icmLuMatrixBwd_lookup_in (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	return rv;
}

static int
icmLuMatrixBwd_lookup_core (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMatrix *p = (icmLuMatrix *)pp;
	rv |= icmLuMatrixBwd_abs(p, out, in);
	rv |= icmLuMatrixBwd_matrix(p, out, out);
	return rv;
}

static int
icmLuMatrixBwd_lookup_out (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuMatrix *p = (icmLuMatrix *)pp;
	rv |= icmLuMatrixBwd_curve(p, out, in);
	return rv;
}

/* -  -  -  -  -  -  -  -  -  -  -  -  -  - */

static void
icmLuMatrix_delete(
icmLuBase *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

/* We setup valid fwd and bwd component conversions, */
/* but setup only the asked for overal conversion. */
static icmLuBase *
new_icmLuMatrix(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func,			/* Functionality requested */
	int dir								/* 0 = fwd, 1 = bwd */
) {
	icmLuMatrix *p;

	if ((p = (icmLuMatrix *) icp->al->calloc(icp->al,1,sizeof(icmLuMatrix))) == NULL)
		return NULL;
	p->icp      = icp;
	p->del      = icmLuMatrix_delete;
	p->lutspaces= icmLutSpaces;
	p->spaces   = icmLuSpaces;
	p->XYZ_Rel2Abs = icmLuXYZ_Rel2Abs;
	p->XYZ_Abs2Rel = icmLuXYZ_Abs2Rel;
	p->get_lutranges = icmLu_get_lutranges;
	p->get_ranges = icmLu_get_ranges;
	p->init_wh_bk = icmLuInit_Wh_bk;
	p->wh_bk_points = icmLuWh_bk_points;
	p->lu_wh_bk_points = icmLuLu_wh_bk_points;
	p->fwd_lookup = icmLuMatrixFwd_lookup;
	p->fwd_curve  = icmLuMatrixFwd_curve;
	p->fwd_matrix = icmLuMatrixFwd_matrix;
	p->fwd_abs    = icmLuMatrixFwd_abs;
	p->bwd_lookup = icmLuMatrixBwd_lookup;
	p->bwd_abs    = icmLuMatrixBwd_abs;
	p->bwd_matrix = icmLuMatrixBwd_matrix;
	p->bwd_curve  = icmLuMatrixBwd_curve;
	if (dir) {
		p->ttype         = icmMatrixBwdType;
		p->lookup        = icmLuMatrixBwd_lookup;
		p->lookup_in     = icmLuMatrixBwd_lookup_in;
		p->lookup_core   = icmLuMatrixBwd_lookup_core;
		p->lookup_out    = icmLuMatrixBwd_lookup_out;
		p->lookup_inv_in = icmLuMatrixFwd_lookup_out;		/* Opposite of Bwd_lookup_in */
	} else {
		p->ttype         = icmMatrixFwdType;
		p->lookup        = icmLuMatrixFwd_lookup;
		p->lookup_in     = icmLuMatrixFwd_lookup_in;
		p->lookup_core   = icmLuMatrixFwd_lookup_core;
		p->lookup_out    = icmLuMatrixFwd_lookup_out;
		p->lookup_inv_in = icmLuMatrixBwd_lookup_out;		/* Opposite of Fwd_lookup_in */
	}

	/* Lookup the white and black points */
	if (p->init_wh_bk((icmLuBase *)p)) {
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Note that we can use matrix type even if PCS is Lab, */
	/* by simply converting it. */

	/* Find the appropriate tags */
	if ((p->redCurve = (icmCurve *)icp->read_tag(icp, icSigRedTRCTag)) == NULL
     || p->redCurve->ttype != icSigCurveType
	 || (p->greenCurve = (icmCurve *)icp->read_tag(icp, icSigGreenTRCTag)) == NULL
     || p->greenCurve->ttype != icSigCurveType
	 || (p->blueCurve = (icmCurve *)icp->read_tag(icp, icSigBlueTRCTag)) == NULL
     || p->blueCurve->ttype != icSigCurveType
	 || (p->redColrnt = (icmXYZArray *)icp->read_tag(icp, icSigRedColorantTag)) == NULL
     || p->redColrnt->ttype != icSigXYZType || p->redColrnt->size < 1
	 || (p->greenColrnt = (icmXYZArray *)icp->read_tag(icp, icSigGreenColorantTag)) == NULL
     || p->greenColrnt->ttype != icSigXYZType || p->greenColrnt->size < 1
	 || (p->blueColrnt = (icmXYZArray *)icp->read_tag(icp, icSigBlueColorantTag)) == NULL
     || p->blueColrnt->ttype != icSigXYZType || p->blueColrnt->size < 1) {
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Setup the matrix */
	p->mx[0][0] = p->redColrnt->data[0].X;
	p->mx[0][1] = p->greenColrnt->data[0].X;
	p->mx[0][2] = p->blueColrnt->data[0].X;
	p->mx[1][1] = p->greenColrnt->data[0].Y;
	p->mx[1][0] = p->redColrnt->data[0].Y;
	p->mx[1][2] = p->blueColrnt->data[0].Y;
	p->mx[2][1] = p->greenColrnt->data[0].Z;
	p->mx[2][0] = p->redColrnt->data[0].Z;
	p->mx[2][2] = p->blueColrnt->data[0].Z;

#ifndef ICM_STRICT	
	/* Workaround for buggy Kodak RGB profiles. Their matrix values */
	/* may be scaled to 100 rather than 1.0, and the colorant curves */
	/* may be scaled by 0.5 */
	if (icp->header->cmmId == str2tag("KCMS")) {
		int i, j, oc = 0;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				if (p->mx[i][j] > 5.0)
					oc++;
		if (oc > 4) {		/* Looks like it */
			for (i = 0; i < 3; i++)
				for (j = 0; j < 3; j++)
					p->mx[i][j] /= 100.0;
		}
	}
#endif /* ICM_STRICT */

	if (icmInverse3x3(p->bmx, p->mx) != 0) {	/* Compute inverse */
		sprintf(icp->err,"icc_new_iccLuMatrix: Matrix wasn't invertable");
		icp->errc = 2;
		p->del((icmLuBase *)p);
		return NULL;
	}

	p->pcswht = icp->header->illuminant;
	p->intent   = intent;
	p->function = func;
	p->inSpace  = inSpace;
	p->outSpace = outSpace;
	p->pcs      = pcs;
	p->e_inSpace  = e_inSpace;
	p->e_outSpace = e_outSpace;
	p->e_pcs      = e_pcs;

	/* Lookup the white and black points */
	if (p->init_wh_bk((icmLuBase *)p)) {
		p->del((icmLuBase *)p);
		return NULL;
	}

	return (icmLuBase *)p;
}

static icmLuBase *
new_icmLuMatrixFwd(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func			/* Functionality requested */
) {
	return new_icmLuMatrix(icp, inSpace, outSpace, pcs, e_inSpace, e_outSpace, e_pcs,
	                       intent, func, 0);
}


static icmLuBase *
new_icmLuMatrixBwd(
	struct _icc          *icp,
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent */
	icmLookupFunc         func			/* Functionality requested */
) {
	return new_icmLuMatrix(icp, inSpace, outSpace, pcs, e_inSpace, e_outSpace, e_pcs,
	                       intent, func, 1);
}

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Forward and Backward Multi-Dimensional Interpolation type conversion */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* Components of overall lookup, in order */
static int icmLuLut_in_abs(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	DBLLL(("icm in_abs: input %s\n",icmPdv(lut->inputChan, in)));
	if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)		/* Don't alter input values */
			out[i] = in[i];
	}

	/* If Bwd Lut, take care of Absolute color space and effective input space */
	if ((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->e_inSpace == icSigLabData
		 || p->e_inSpace == icSigXYZData)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation)) {
	
		if (p->e_inSpace == icSigLabData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm in_abs: after Lab2XYZ %s\n",icmPdv(lut->inputChan, out)));
		}

		/* Convert from Absolute to Relative colorimetric */
		icmMulBy3x3(out, p->fromAbs, out);
		DBLLL(("icm in_abs: after fromAbs %s\n",icmPdv(lut->inputChan, out)));
		
		if (p->inSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm in_abs: after XYZ2Lab %s\n",icmPdv(lut->inputChan, out)));
		}

	} else {

		/* Convert from Effective to Native input space */
		if (p->e_inSpace == icSigLabData && p->inSpace == icSigXYZData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm in_abs: after Lab2XYZ %s\n",icmPdv(lut->inputChan, out)));
		} else if (p->e_inSpace == icSigXYZData && p->inSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm in_abs: after XYZ2Lab %s\n",icmPdv(lut->inputChan, out)));
		}
	}
	DBLLL(("icm in_abs: returning %s\n",icmPdv(lut->inputChan, out)));

	return rv;
}

/* Possible matrix lookup */
static int icmLuLut_matrix(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	if (p->usematrix)		
		rv |= lut->lookup_matrix(lut,out,in);
	else if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)
			out[i] = in[i];
	}
	return rv;
}

/* Do input -> input' lookup */
static int icmLuLut_input(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	p->in_normf(out, in); 						/* Normalize from input color space */
	rv |= lut->lookup_input(lut,out,out);		/* Lookup though input tables */
	p->in_denormf(out,out);						/* De-normalize to input color space */
	return rv;
}

/* Do input'->output' lookup */
static int icmLuLut_clut(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	double temp[MAX_CHAN];
	int rv = 0;

	p->in_normf(temp, in); 						/* Normalize from input color space */
	rv |= p->lookup_clut(lut,out,temp);			/* Lookup though clut tables */
	p->out_denormf(out,out);					/* De-normalize to output color space */
	return rv;
}

/* Do output'->output lookup */
static int icmLuLut_output(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	p->out_normf(out,in);						/* Normalize from output color space */
	rv |= lut->lookup_output(lut,out,out);		/* Lookup though output tables */
	p->out_denormf(out, out);					/* De-normalize to output color space */
	return rv;
}

static int icmLuLut_out_abs(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	DBLLL(("icm out_abs: input %s\n",icmPdv(lut->outputChan, in)));
	if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->outputChan; i++)		/* Don't alter input values */
			out[i] = in[i];
	}

	/* If Fwd Lut, take care of Absolute color space */
	/* and convert from native to effective out PCS */
	if ((p->function == icmFwd || p->function == icmPreview)
		&& (p->outSpace == icSigLabData
		 || p->outSpace == icSigXYZData)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation)) {

		if (p->outSpace == icSigLabData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm out_abs: after Lab2XYZ %s\n",icmPdv(lut->outputChan, out)));
		}
		
		/* Convert from Relative to Absolute colorimetric XYZ */
		icmMulBy3x3(out, p->toAbs, out);
		DBLLL(("icm out_abs: after toAbs %s\n",icmPdv(lut->outputChan, out)));

		if (p->e_outSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm out_abs: after XYZ2Lab %s\n",icmPdv(lut->outputChan, out)));
		}
	} else {

		/* Convert from Native to Effective output space */
		if (p->outSpace == icSigLabData && p->e_outSpace == icSigXYZData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm out_abs: after Lab2 %s\n",icmPdv(lut->outputChan, out)));
		} else if (p->outSpace == icSigXYZData && p->e_outSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm out_abs: after XYZ2Lab %s\n",icmPdv(lut->outputChan, out)));
		}
	}
	DBLLL(("icm out_abs: returning %s\n",icmPdv(lut->outputChan, out)));
	return rv;
}


/* Overall lookup */
static int
icmLuLut_lookup (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuLut *p = (icmLuLut *)pp;
	icmLut *lut = p->lut;
	double temp[MAX_CHAN];

	DBGLL(("icmLuLut_lookup: in = %s\n", icmPdv(p->lut->inputChan, in)));
	rv |= p->in_abs(p,temp,in);						/* Possible absolute conversion */
	DBGLL(("icmLuLut_lookup: in_abs = %s\n", icmPdv(p->lut->inputChan, temp)));
	if (p->usematrix) {
		rv |= lut->lookup_matrix(lut,temp,temp);/* If XYZ, multiply by non-unity matrix */
		DBGLL(("icmLuLut_lookup: matrix = %s\n", icmPdv(p->lut->inputChan, temp)));
	}
	p->in_normf(temp, temp);					/* Normalize for input color space */
	DBGLL(("icmLuLut_lookup: norm = %s\n", icmPdv(p->lut->inputChan, temp)));
	rv |= lut->lookup_input(lut,temp,temp);		/* Lookup though input tables */
	DBGLL(("icmLuLut_lookup: input = %s\n", icmPdv(p->lut->inputChan, temp)));
	rv |= p->lookup_clut(lut,out,temp);			/* Lookup though clut tables */
	DBGLL(("icmLuLut_lookup: clut = %s\n", icmPdv(p->lut->outputChan, out)));
	rv |= lut->lookup_output(lut,out,out);		/* Lookup though output tables */
	DBGLL(("icmLuLut_lookup: output = %s\n", icmPdv(p->lut->outputChan, out)));
	p->out_denormf(out,out);					/* Normalize for output color space */
	DBGLL(("icmLuLut_lookup: denorm = %s\n", icmPdv(p->lut->outputChan, out)));
	rv |= p->out_abs(p,out,out);				/* Possible absolute conversion */
	DBGLL(("icmLuLut_lookup: out_abse = %s\n", icmPdv(p->lut->outputChan, out)));

	return rv;
}

#ifdef NEVER	/* The following should be identical in effect to the above. */

/* Overall lookup */
static int
icmLuLut_lookup (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int i, rv = 0;
	icmLuLut *p = (icmLuLut *)pp;
	icmLut *lut = p->lut;
	double temp[MAX_CHAN];

	rv |= p->in_abs(p,temp,in);
	rv |= p->matrix(p,temp,temp);
	rv |= p->input(p,temp,temp);
	rv |= p->clut(p,out,temp);
	rv |= p->output(p,out,out);
	rv |= p->out_abs(p,out,out);

	return rv;
}

#endif	/* NEVER */

/* Three stage conversion */
static int
icmLuLut_lookup_in (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuLut *p = (icmLuLut *)pp;
	icmLut *lut = p->lut;

	/* If in_abs() or matrix() are active, then we can't have a per component input curve */
	if (((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->e_inSpace != p->inSpace)
	 || (p->usematrix)) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)
			out[i] = in[i];
	} else {
		rv |= p->input(p,out,in);
	}
	return rv;
}

static int
icmLuLut_lookup_core (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuLut *p = (icmLuLut *)pp;

	/* If in_abs() or matrix() are active, then we have to do the per component input curve here */
	if (((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->e_inSpace != p->inSpace)
	 || (p->usematrix)) {
		double temp[MAX_CHAN];
		rv |= p->in_abs(p,temp,in);
		rv |= p->matrix(p,temp,temp);
		rv |= p->input(p,temp,temp);
		rv |= p->clut(p,out,temp);
	} else {
		rv |= p->clut(p,out,in);
	}

	/* If out_abs() is active, then we can't have do per component out curve here */
	if (((p->function == icmFwd || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->outSpace != p->e_outSpace)) {
		rv |= p->output(p,out,out);
		rv |= p->out_abs(p,out,out);
	}

	return rv;
}

static int
icmLuLut_lookup_out (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuLut *p = (icmLuLut *)pp;
	icmLut *lut = p->lut;

	/* If out_abs() is active, then we can't have a per component out curve */
	if (((p->function == icmFwd || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->outSpace != p->e_outSpace)) {
		unsigned int i;
		for (i = 0; i < lut->outputChan; i++)
			out[i] = in[i];
	} else {
		rv |= p->output(p,out,in);
	}

	return rv;
}

/* Inverse three stage conversion (partly implemented) */
static int
icmLuLut_lookup_inv_in (
icmLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icmLuLut *p = (icmLuLut *)pp;
	icmLut *lut = p->lut;

	/* If in_abs() or matrix() are active, then we can't have a per component input curve */
	if (((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation))
	 || (p->e_inSpace != p->inSpace)
	 || (p->usematrix)) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)
			out[i] = in[i];
	} else {
		rv |= p->inv_input(p,out,in);
	}
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Some components of inverse lookup, in order */
/* ~~ should these be in icmLut (like all the fwd transforms)? */

static int icmLuLut_inv_out_abs(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	DBLLL(("icm inv_out_abs: input %s\n",icmPdv(lut->outputChan, in)));
	if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->outputChan; i++)		/* Don't alter input values */
			out[i] = in[i];
	}

	/* If Fwd Lut, take care of Absolute color space */
	/* and convert from effective to native inverse output PCS */
	/* OutSpace must be PCS: XYZ or Lab */
	if ((p->function == icmFwd || p->function == icmPreview)
		&& (p->e_outSpace == icSigLabData
		 || p->e_outSpace == icSigXYZData)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation)) {

		if (p->e_outSpace == icSigLabData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm inv_out_abs: after Lab2XYZ %s\n",icmPdv(lut->outputChan, out)));
		}
	
		/* Convert from Absolute to Relative colorimetric */
		icmMulBy3x3(out, p->fromAbs, out);
		DBLLL(("icm inv_out_abs: after fromAbs %s\n",icmPdv(lut->outputChan, out)));
		
		if (p->outSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm inv_out_abs: after XYZ2Lab %s\n",icmPdv(lut->outputChan, out)));
		}

	} else {

		/* Convert from Effective to Native output space */
		if (p->e_outSpace == icSigLabData && p->outSpace == icSigXYZData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm inv_out_abs: after Lab2XYZ %s\n",icmPdv(lut->outputChan, out)));
		} else if (p->e_outSpace == icSigXYZData && p->outSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm inv_out_abs: after XYZ2Lab %s\n",icmPdv(lut->outputChan, out)));
		}
	}
	return rv;
}

/* Do output->output' inverse lookup */
static int icmLuLut_inv_output(icmLuLut *p, double *out, double *in) {
	icc *icp = p->icp;
	icmLut *lut = p->lut;
	int i;
	int rv = 0;

	if (lut->rot[0].inited == 0) {	
		for (i = 0; i < lut->outputChan; i++) {
			rv = icmTable_setup_bwd(icp, &lut->rot[i], lut->outputEnt,
			                             lut->outputTable + i * lut->outputEnt);
			if (rv != 0) {
				sprintf(icp->err,"icc_Lut_inv_input: Malloc failure in inverse lookup init.");
				return icp->errc = rv;
			}
		}
	}

	p->out_normf(out,in);						/* Normalize from output color space */
	for (i = 0; i < lut->outputChan; i++) {
		/* Reverse lookup though output tables */
		rv |= icmTable_lookup_bwd(&lut->rot[i], &out[i], &out[i]);
	}
	p->out_denormf(out, out);					/* De-normalize to output color space */
	return rv;
}

/* No output' -> input inverse lookup. */
/* This is non-trivial ! */

/* Do input' -> input inverse lookup */
static int icmLuLut_inv_input(icmLuLut *p, double *out, double *in) {
	icc *icp = p->icp;
	icmLut *lut = p->lut;
	int i;
	int rv = 0;

	if (lut->rit[0].inited == 0) {	
		for (i = 0; i < lut->inputChan; i++) {
			rv = icmTable_setup_bwd(icp, &lut->rit[i], lut->inputEnt,
			                             lut->inputTable + i * lut->inputEnt);
			if (rv != 0) {
				sprintf(icp->err,"icc_Lut_inv_input: Malloc failure in inverse lookup init.");
				return icp->errc = rv;
			}
		}
	}

	p->in_normf(out, in); 						/* Normalize from input color space */
	for (i = 0; i < lut->inputChan; i++) {
		/* Reverse lookup though input tables */
		rv |= icmTable_lookup_bwd(&lut->rit[i], &out[i], &out[i]);
	}
	p->in_denormf(out,out);						/* De-normalize to input color space */
	return rv;
}

/* Possible inverse matrix lookup */
static int icmLuLut_inv_matrix(icmLuLut *p, double *out, double *in) {
	icc *icp = p->icp;
	icmLut *lut = p->lut;
	int rv = 0;

	if (p->usematrix) {
		double tt[3];
		if (p->imx_valid == 0) {
			if (icmInverse3x3(p->imx, lut->e) != 0) {	/* Compute inverse */
				sprintf(icp->err,"icc_new_iccLuMatrix: Matrix wasn't invertable");
				icp->errc = 2;
				return 2;
			}
			p->imx_valid = 1;
		}
		/* Matrix multiply */
		tt[0] = p->imx[0][0] * in[0] + p->imx[0][1] * in[1] + p->imx[0][2] * in[2];
		tt[1] = p->imx[1][0] * in[0] + p->imx[1][1] * in[1] + p->imx[1][2] * in[2];
		tt[2] = p->imx[2][0] * in[0] + p->imx[2][1] * in[1] + p->imx[2][2] * in[2];
		out[0] = tt[0], out[1] = tt[1], out[2] = tt[2];
	} else if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)
			out[i] = in[i];
	}
	return rv;
}

static int icmLuLut_inv_in_abs(icmLuLut *p, double *out, double *in) {
	icmLut *lut = p->lut;
	int rv = 0;

	DBLLL(("icm inv_in_abs: input %s\n",icmPdv(lut->inputChan, in)));
	if (out != in) {
		unsigned int i;
		for (i = 0; i < lut->inputChan; i++)		/* Don't alter input values */
			out[i] = in[i];
	}

	/* If Bwd Lut, take care of Absolute color space, and */
	/* convert from native to effective input space */
	if ((p->function == icmBwd || p->function == icmGamut || p->function == icmPreview)
		&& (p->inSpace == icSigLabData
		 || p->inSpace == icSigXYZData)
		&& (p->intent == icAbsoluteColorimetric
		 || p->intent == icmAbsolutePerceptual
		 || p->intent == icmAbsoluteSaturation)) {

		if (p->inSpace == icSigLabData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm inv_in_abs: after Lab2XYZ %s\n",icmPdv(lut->inputChan, out)));
		}

		/* Convert from Relative to Absolute colorimetric XYZ */
		icmMulBy3x3(out, p->toAbs, out);
		DBLLL(("icm inv_in_abs: after toAbs %s\n",icmPdv(lut->inputChan, out)));
		
		if (p->e_inSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm inv_in_abs: after XYZ2Lab %s\n",icmPdv(lut->inputChan, out)));
		}
	} else {

		/* Convert from Native to Effective input space */
		if (p->inSpace == icSigLabData && p->e_inSpace == icSigXYZData) {
			icmLab2XYZ(&p->pcswht, out, out);
			DBLLL(("icm inv_in_abs: after Lab2XYZ %s\n",icmPdv(lut->inputChan, out)));
		} else if (p->inSpace == icSigXYZData && p->e_inSpace == icSigLabData) {
			icmXYZ2Lab(&p->pcswht, out, out);
			DBLLL(("icm inv_in_abs: after XYZ2Lab %s\n",icmPdv(lut->inputChan, out)));
		}
	}
	DBLLL(("icm inv_in_abs: returning %s\n",icmPdv(lut->inputChan, out)));
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return LuLut information */
static void icmLuLut_get_info(
	icmLuLut     *p,		/* this */
	icmLut       **lutp,	/* Pointer to icc lut type */
	icmXYZNumber *pcswhtp,	/* Pointer to profile PCS white point */
	icmXYZNumber *whitep,	/* Pointer to profile absolute white point */
	icmXYZNumber *blackp	/* Pointer to profile absolute black point */
) {
	if (lutp != NULL)
		*lutp = p->lut;
	if (pcswhtp != NULL)
		*pcswhtp = p->pcswht;
	if (whitep != NULL)
		*whitep = p->whitePoint;
	if (blackp != NULL)
		*blackp = p->blackPoint;
}

/* Get the native ranges for the LuLut */
/* This is computed differently to the mono & matrix types, to */
/* accurately take into account the different range for 8 bit Lab */
/* lut type. The range returned for the effective PCS is not so accurate. */
static void
icmLuLut_get_lutranges (
	struct _icmLuBase *pp,
	double *inmin, double *inmax,		/* Return maximum range of inspace values */
	double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	icmLuLut *p = (icmLuLut *)pp;
	unsigned int i;

	for (i = 0; i < p->lut->inputChan; i++) {
		inmin[i] = 0.0;	/* Normalized range of input space values */
		inmax[i] = 1.0;
	}
	p->in_denormf(inmin,inmin);	/* Convert to real colorspace range */
	p->in_denormf(inmax,inmax);

	/* Make sure min and max are so. */
	for (i = 0; i < p->lut->inputChan; i++) {
		if (inmin[i] > inmax[i]) {
			double tt;
			tt = inmin[i];
			inmin[i] = inmax[i];
			inmax[i] = tt;
		}
	}

	for (i = 0; i < p->lut->outputChan; i++) {
		outmin[i] = 0.0;	/* Normalized range of output space values */
		outmax[i] = 1.0;
	}
	p->out_denormf(outmin,outmin);	/* Convert to real colorspace range */
	p->out_denormf(outmax,outmax);

	/* Make sure min and max are so. */
	for (i = 0; i < p->lut->outputChan; i++) {
		if (outmin[i] > outmax[i]) {
			double tt;
			tt = outmin[i];
			outmin[i] = outmax[i];
			outmax[i] = tt;
		}
	}
}

/* Get the effective (externaly visible) ranges for the LuLut */
/* This will be accurate if there is no override, but only */
/* aproximate if a PCS override is in place. */
static void
icmLuLut_get_ranges (
	struct _icmLuBase *pp,
	double *inmin, double *inmax,		/* Return maximum range of inspace values */
	double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	icmLuLut *p = (icmLuLut *)pp;

	/* Get the native ranges first */
	icmLuLut_get_lutranges(pp, inmin, inmax, outmin, outmax);

	/* And replace them if the effective space is different */
	if (p->e_inSpace != p->inSpace)
		getRange(p->icp, p->e_inSpace, p->lut->ttype, inmin, inmax);

	if (p->e_outSpace != p->outSpace)
		getRange(p->icp, p->e_outSpace, p->lut->ttype, outmin, outmax);
}

/* Return the underlying Lut matrix */
static void
icmLuLut_get_matrix (
	struct _icmLuLut *p,
	double m[3][3]
) {
	int i, j;
	icmLut *lut = p->lut;

	if (p->usematrix) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				m[i][j] = lut->e[i][j];	/* Copy from Lut */

	} else {							/* return unity matrix */
		icmSetUnity3x3(m);
	}
}


static void
icmLuLut_delete(
icmLuBase *p
) {
	icc *icp = p->icp;

	icp->al->free(icp->al, p);
}

icmLuBase *
icc_new_icmLuLut(
	icc                   *icp,
	icTagSignature        ttag,			/* Target Lut tag */
    icColorSpaceSignature inSpace,		/* Native Input color space */
    icColorSpaceSignature outSpace,		/* Native Output color space */
    icColorSpaceSignature pcs,			/* Native PCS (from header) */
    icColorSpaceSignature e_inSpace,	/* Effective Input color space */
    icColorSpaceSignature e_outSpace,	/* Effective Output color space */
    icColorSpaceSignature e_pcs,		/* Effective PCS */
	icRenderingIntent     intent,		/* Rendering intent (For absolute) */
	icmLookupFunc         func			/* Functionality requested (for icmLuSpaces()) */
) {
	icmLuLut *p;

	if ((p = (icmLuLut *) icp->al->calloc(icp->al,1,sizeof(icmLuLut))) == NULL)
		return NULL;
	p->ttype    = icmLutType;
	p->icp      = icp;
	p->del      = icmLuLut_delete;
	p->lutspaces= icmLutSpaces;
	p->spaces   = icmLuSpaces;
	p->XYZ_Rel2Abs = icmLuXYZ_Rel2Abs;
	p->XYZ_Abs2Rel = icmLuXYZ_Abs2Rel;
	p->init_wh_bk  = icmLuInit_Wh_bk;
	p->wh_bk_points = icmLuWh_bk_points;
	p->lu_wh_bk_points = icmLuLu_wh_bk_points;

	p->lookup        = icmLuLut_lookup;
	p->lookup_in     = icmLuLut_lookup_in;
	p->lookup_core   = icmLuLut_lookup_core;
	p->lookup_out    = icmLuLut_lookup_out;
	p->lookup_inv_in = icmLuLut_lookup_inv_in;

	p->in_abs   = icmLuLut_in_abs;
	p->matrix   = icmLuLut_matrix;
	p->input    = icmLuLut_input;
	p->clut     = icmLuLut_clut;
	p->output   = icmLuLut_output;
	p->out_abs  = icmLuLut_out_abs;

	p->inv_in_abs   = icmLuLut_inv_in_abs;
	p->inv_matrix   = icmLuLut_inv_matrix;
	p->inv_input    = icmLuLut_inv_input;
	p->inv_output   = icmLuLut_inv_output;
	p->inv_out_abs  = icmLuLut_inv_out_abs;

	p->pcswht   = icp->header->illuminant;
	p->intent   = intent;			/* used to trigger absolute processing */
	p->function = func;
	p->inSpace  = inSpace;
	p->outSpace = outSpace;
	p->pcs      = pcs;
	p->e_inSpace  = e_inSpace;
	p->e_outSpace = e_outSpace;
	p->e_pcs      = e_pcs;
	p->get_info = icmLuLut_get_info;
	p->get_lutranges = icmLuLut_get_lutranges;
	p->get_ranges = icmLuLut_get_ranges;
	p->get_matrix = icmLuLut_get_matrix;

	/* Lookup the white and black points */
	if (p->init_wh_bk((icmLuBase *)p)) {
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Get the Lut tag, & check that it is expected type */
	if ((p->lut = (icmLut *)icp->read_tag(icp, ttag)) == NULL
	 || (p->lut->ttype != icSigLut8Type && p->lut->ttype != icSigLut16Type)) {
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Check if matrix should be used */
	if (inSpace == icSigXYZData && p->lut->nu_matrix(p->lut))
		p->usematrix = 1;
	else
		p->usematrix = 0;

	/* Lookup input color space to normalized index function */
	if (getNormFunc(icp, inSpace, p->lut->ttype, icmToLuti, &p->in_normf)) {
		sprintf(icp->err,"icc_get_luobj: Unknown colorspace");
		icp->errc = 1;
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Lookup normalized index to input color space function */
	if (getNormFunc(icp, inSpace, p->lut->ttype, icmFromLuti, &p->in_denormf)) {
		sprintf(icp->err,"icc_get_luobj: Unknown colorspace");
		icp->errc = 1;
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Lookup output color space to normalized Lut entry value function */
	if (getNormFunc(icp, outSpace, p->lut->ttype, icmToLutv, &p->out_normf)) {
		sprintf(icp->err,"icc_get_luobj: Unknown colorspace");
		icp->errc = 1;
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Lookup normalized Lut entry value to output color space function */
	if (getNormFunc(icp, outSpace, p->lut->ttype, icmFromLutv, &p->out_denormf)) {
		sprintf(icp->err,"icc_get_luobj: Unknown colorspace");
		icp->errc = 1;
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Note that the following two are only used in computing the expected */
	/* value ranges of the effective PCS. This might not be the best way of */
	/* doing this. */
	/* Lookup normalized index to effective input color space function */
	if (getNormFunc(icp, e_inSpace, p->lut->ttype, icmFromLuti, &p->e_in_denormf)) {
		sprintf(icp->err,"icc_get_luobj: Unknown effective colorspace");
		icp->errc = 1;
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Lookup normalized Lut entry value to effective output color space function */
	if (getNormFunc(icp, e_outSpace, p->lut->ttype, icmFromLutv, &p->e_out_denormf)) {
		sprintf(icp->err,"icc_get_luobj: Unknown effective colorspace");
		icp->errc = 1;
		p->del((icmLuBase *)p);
		return NULL;
	}

	/* Determine appropriate clut lookup algorithm */
	{
		int use_sx;				/* -1 = undecided, 0 = N-linear, 1 = Simplex lookup */
		icColorSpaceSignature ins, outs;	/* In and out Lut color spaces */
		int inn, outn;		/* in and out number of Lut components */

		p->lutspaces((icmLuBase *)p, &ins, &inn, &outs, &outn, NULL);

		/* Determine if the input space is "Device" like, */
		/* ie. luminance will be expected to vary most strongly */
		/* with the diagonal change in input coordinates. */
		switch(ins) {

			/* Luminence is carried by the sum of all the output channels, */
			/* so output luminence will dominantly be in diagonal direction. */
			case icSigXYZData:		/* This seems to be appropriate ? */
			case icSigRgbData:
			case icSigGrayData:
			case icSigCmykData:
			case icSigCmyData:
			case icSigMch6Data:
				use_sx = 1;		/* Simplex interpolation is appropriate */
				break;

			/* A single channel carries the luminence information */
			case icSigLabData:
			case icSigLuvData:
			case icSigYCbCrData:
			case icSigYxyData:
			case icSigHlsData:
			case icSigHsvData:
				use_sx = 0;		/* N-linear interpolation is appropriate */
				break;
			default:
				use_sx = -1;		/* undecided */
			    	break;
		}

		/* If we couldn't figure it out from the input space, */
		/* check output luminance variation with a diagonal input */
		/* change. */
		if (use_sx == -1) {
			int lc;		/* Luminance channel */

			/* Determine where the luminence is carried in the output */
			switch(outs) {

				/* Luminence is carried by the sum of all the output channels */
				case icSigRgbData:
				case icSigGrayData:
				case icSigCmykData:
				case icSigCmyData:
				case icSigMch6Data:
					lc = -1;		/* Average all channels */
					break;

				/* A single channel carries the luminence information */
				case icSigLabData:
				case icSigLuvData:
				case icSigYCbCrData:
				case icSigYxyData:
					lc = 0;
					break;

				case icSigXYZData:
				case icSigHlsData:
					lc = 1;
					break;

				case icSigHsvData:
					lc = 2;
					break;
				
				/* default means give up and use N-linear type lookup */
				default:
					lc = -2;
					break;
			}

			/* If we know how luminance is represented in output space */
			if (lc != -2) {
				double tout1[MAX_CHAN];		/* Test output values */
				double tout2[MAX_CHAN];
				double tt, diag;
				int n;

				/* Determine input space location of min and max of */
				/* given output channel (chan = -1 means average of all) */
				p->lut->min_max(p->lut, tout1, tout2, lc);
				
				/* Convert to vector and then calculate normalized */
				/* dot product with diagonal vector (1,1,1...) */
				for (tt = 0.0, n = 0; n < inn; n++) {
					tout1[n] = tout2[n] - tout1[n];
					tt += tout1[n] * tout1[n];
				}
				if (tt > 0.0)
					tt = sqrt(tt);			/* normalizing factor for maximum delta */
				else
					tt = 1.0;				/* Hmm. */
				tt *= sqrt((double)inn);	/* Normalizing factor for diagonal vector */
				for (diag = 0.0, n = 0; n < outn; n++)
					diag += tout1[n] / tt;
				diag = fabs(diag);

				/* I'm not really convinced that this is a reliable */
				/* indicator of whether simplex interpolation should be used ... */
				/* It does seem to do the right thing with YCC space though. */
				if (diag > 0.8)	/* Diagonal is dominant ? */
					use_sx = 1;

				/* If we couldn't figure it out, use N-linear interpolation */
				if (use_sx == -1)
					use_sx = 0;
			}
		}

		if (use_sx) {
			p->lookup_clut = p->lut->lookup_clut_sx;
			p->lut->tune_value = icmLut_tune_value_sx;
		} else {
			p->lookup_clut = p->lut->lookup_clut_nl;
			p->lut->tune_value = icmLut_tune_value_nl;
		}
	}
	return (icmLuBase *)p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* Return an appropriate lookup object */
/* Return NULL on error, and detailed error in icc */
static icmLuBase* icc_get_luobj (
	icc *p,						/* ICC */
	icmLookupFunc func,			/* Conversion functionality */
	icRenderingIntent intent,	/* Rendering intent, including icmAbsoluteColorimetricXYZ */
	icColorSpaceSignature pcsor,/* PCS override (0 = def) */
	icmLookupOrder order		/* Conversion representation search Order */
) {
	icmLuBase *luobj = NULL;	/* Lookup object to return */
	icColorSpaceSignature pcs, e_pcs;	/* PCS and effective PCS */
	
#ifdef ICM_STRICT
	int rv;
	/* Check that the profile is legal, since we depend on it ? */
	if ((rv = check_icc_legal(p)) != 0)
		return NULL;
#endif /* ICM_STRICT */
	
	/* Figure out the native and effective PCS */
	e_pcs = pcs = p->header->pcs;
	if (pcsor != icmSigDefaultData)
		e_pcs = pcsor;			/* Override */

	/* How we expect to execute the request depends firstly on the type of profile */
	switch (p->header->deviceClass) {
    	case icSigInputClass:
    	case icSigDisplayClass:
    	case icSigColorSpaceClass:

			/* Look for Intent based AToBX profile + optional BToAX reverse */
			/* or for AToB0 based profile + optional BToA0 reverse */
			/* or three component matrix profile (reversable) */
			/* or momochrome table profile (reversable) */ 
			/* No Lut intent for ICC < V2.4, but possible for >= V2.4, */
			/* so fall back if we can't find the chosen Lut intent.. */
			/* Device <-> PCS */
			/* Determine the algorithm and set its parameters */

			switch (func) {
				icRenderingIntent fbintent;		/* Fallback intent */
				icTagSignature ttag, fbtag;

		    	case icmFwd:	/* Device to PCS */
					if (intent == icmDefaultIntent)
						intent = icPerceptual;	/* Make this the default */

					switch (intent) {
		    			case icAbsoluteColorimetric:
							ttag = icSigAToB1Tag;
							fbtag = icSigAToB0Tag;
							fbintent = intent;
							break;
		    			case icRelativeColorimetric:
							ttag = icSigAToB1Tag;
							fbtag = icSigAToB0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icPerceptual:
							ttag = icSigAToB0Tag;
							fbtag = icSigAToB0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icSaturation:
							ttag = icSigAToB2Tag;
							fbtag = icSigAToB0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
							ttag = icSigAToB0Tag;
							fbtag = icSigAToB0Tag;
							fbintent = intent;
							break;
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							ttag = icSigAToB2Tag;
							fbtag = icSigAToB0Tag;
							fbintent = intent;
							break;
						default:
							sprintf(p->err,"icc_get_luobj: Unknown intent");
							p->errc = 1;
							return NULL;
					}

					if (order != icmLuOrdRev) {
						/* Try Lut type lookup with the chosen intent first */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try the fallback tag */
						if ((luobj = icc_new_icmLuLut(p, fbtag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     fbintent, func)) != NULL)
							break;
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
					} else {
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;

						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try Lut type lookup last */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try the fallback tag */
						if ((luobj = icc_new_icmLuLut(p, fbtag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     fbintent, func)) != NULL)
							break;
					}
					break;

		    	case icmBwd:	/* PCS to Device */
					if (intent == icmDefaultIntent)
						intent = icPerceptual;	/* Make this the default */

					switch (intent) {
		    			case icAbsoluteColorimetric:
							ttag = icSigBToA1Tag;
							fbtag = icSigBToA0Tag;
							fbintent = intent;
							break;
		    			case icRelativeColorimetric:
							ttag = icSigBToA1Tag;
							fbtag = icSigBToA0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icPerceptual:
							ttag = icSigBToA0Tag;
							fbtag = icSigBToA0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icSaturation:
							ttag = icSigBToA2Tag;
							fbtag = icSigBToA0Tag;
							fbintent = icmDefaultIntent;
							break;
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
							ttag = icSigBToA0Tag;
							fbtag = icSigBToA0Tag;
							fbintent = intent;
							break;
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							ttag = icSigBToA2Tag;
							fbtag = icSigBToA0Tag;
							fbintent = intent;
							break;
						default:
							sprintf(p->err,"icc_get_luobj: Unknown intent");
							p->errc = 1;
							return NULL;
					}


					if (order != icmLuOrdRev) {
						/* Try Lut type lookup first */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;

						/* Try the fallback Lut */
						if ((luobj = icc_new_icmLuLut(p, fbtag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     fbintent, func)) != NULL)
							break;
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
					} else {
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try Lut type lookup last */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;

						/* Try the fallback Lut */
						if ((luobj = icc_new_icmLuLut(p, fbtag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     fbintent, func)) != NULL)
							break;
					}
					break;

				default:
					sprintf(p->err,"icc_get_luobj: Inaproptiate function requested");
					p->errc = 1;
					return NULL;
				}
			break;
			
    	case icSigOutputClass:
			/* Expect BToA Lut and optional AToB Lut, All intents, expect gamut */
			/* or momochrome table profile (reversable) */ 
			/* Device <-> PCS */
			/* Gamut Lut - no intent */
			/* Optional preview links PCS <-> PCS */
			
			/* Determine the algorithm and set its parameters */
			switch (func) {
				icTagSignature ttag;
				
		    	case icmFwd:	/* Device to PCS */

					if (intent == icmDefaultIntent)
						intent = icPerceptual;	/* Make this the default */

					switch (intent) {
		    			case icRelativeColorimetric:
		    			case icAbsoluteColorimetric:
								ttag = icSigAToB1Tag;
							break;
		    			case icPerceptual:
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
								ttag = icSigAToB0Tag;
							break;
		    			case icSaturation:
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
								ttag = icSigAToB2Tag;
							break;
						default:
							sprintf(p->err,"icc_get_luobj: Unknown intent");
							p->errc = 1;
							return NULL;
					}

					if (order != icmLuOrdRev) {
						/* Try Lut type lookup first */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL) {
							break;
						}
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
					} else {
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;

						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixFwd(p,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try Lut type lookup last */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     p->header->colorSpace, pcs, pcs,
						     p->header->colorSpace, e_pcs, e_pcs,
						     intent, func)) != NULL)
							break;
					}
					break;

		    	case icmBwd:	/* PCS to Device */

					if (intent == icmDefaultIntent)
						intent = icPerceptual;			/* Make this the default */

					switch (intent) {
		    			case icRelativeColorimetric:
		    			case icAbsoluteColorimetric:
								ttag = icSigBToA1Tag;
							break;
		    			case icPerceptual:
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
								ttag = icSigBToA0Tag;
							break;
		    			case icSaturation:
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
								ttag = icSigBToA2Tag;
							break;
						default:
							sprintf(p->err,"icc_get_luobj: Unknown intent");
							p->errc = 1;
							return NULL;
					}

					if (order != icmLuOrdRev) {
						/* Try Lut type lookup first */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
					} else {
						/* See if it could be a monochrome lookup */
						if ((luobj = new_icmLuMonoBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;

						/* See if it could be a matrix lookup */
						if ((luobj = new_icmLuMatrixBwd(p,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
	
						/* Try Lut type lookup last */
						if ((luobj = icc_new_icmLuLut(p, ttag,
						     pcs, p->header->colorSpace, pcs,
						     e_pcs, p->header->colorSpace, e_pcs,
						     intent, func)) != NULL)
							break;
					}
					break;

		    	case icmGamut:	/* PCS to 1D */

#ifdef ICM_STRICT	/* Allow only default and absolute */
					if (intent != icmDefaultIntent
					 && intent != icAbsoluteColorimetric) {
						sprintf(p->err,"icc_get_luobj: Intent is inappropriate for Gamut table");
						p->errc = 1;
						return NULL;
					}
#else				/* Be more forgiving */
					switch (intent) {
		    			case icAbsoluteColorimetric:
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							break;
						case icmDefaultIntent:
		    			case icRelativeColorimetric:
		    			case icPerceptual:
		    			case icSaturation:
							intent = icmDefaultIntent;	/* Make all other look like default */
							break;
						default:
							sprintf(p->err,"icc_get_luobj: Unknown intent (0x%x)",intent);
							p->errc = 1;
							return NULL;
					}

#endif
					/* If the target tag exists, and it is a Lut */
					luobj = icc_new_icmLuLut(p, icSigGamutTag,
					     pcs, icSigGrayData, pcs,
					     e_pcs, icSigGrayData, e_pcs,
					     intent, func);
					break;

		    	case icmPreview:	/* PCS to PCS */

					switch (intent)  {
		    			case icRelativeColorimetric:
								ttag = icSigPreview1Tag;
							break;
		    			case icPerceptual:
								ttag = icSigPreview0Tag;
							break;
		    			case icSaturation:
								ttag = icSigPreview2Tag;
							break;
		    			case icAbsoluteColorimetric:
		    			case icmAbsolutePerceptual:		/* Special icclib intent */
		    			case icmAbsoluteSaturation:		/* Special icclib intent */
							sprintf(p->err,"icc_get_luobj: Intent is inappropriate for preview table");
							p->errc = 1;
							return NULL;
						default:
							sprintf(p->err,"icc_get_luobj: Unknown intent");
							p->errc = 1;
							return NULL;
					}

					/* If the target tag exists, and it is a Lut */
					luobj = icc_new_icmLuLut(p, ttag,
					     pcs, pcs, pcs,
					     e_pcs, e_pcs, e_pcs,
					     intent, func);
					break;

				default:
					sprintf(p->err,"icc_get_luobj: Inaproptiate function requested");
					p->errc = 1;
					return NULL;
			}
			break;

    	case icSigLinkClass:
			/* Expect AToB0 Lut and optional BToA0 Lut, One intent in header */
			/* Device <-> Device */

			if (intent != p->header->renderingIntent
			 && intent != icmDefaultIntent) {
				sprintf(p->err,"icc_get_luobj: Intent is inappropriate for Link profile");
				p->errc = 1;
				return NULL;
			}
			intent = p->header->renderingIntent;

			/* Determine the algorithm and set its parameters */
			switch (func) {
		    	case icmFwd:	/* Device to PCS (== Device) */

					luobj = icc_new_icmLuLut(p, icSigAToB0Tag,
					     p->header->colorSpace, pcs, pcs,
					     p->header->colorSpace, pcs, pcs,
					     intent, func);
					break;

		    	case icmBwd:	/* PCS (== Device) to Device */

					luobj = icc_new_icmLuLut(p, icSigBToA0Tag,
					     pcs, p->header->colorSpace, pcs,
					     pcs, p->header->colorSpace, pcs,
					     intent, func);
					break;

				default:
					sprintf(p->err,"icc_get_luobj: Inaproptiate function requested");
					p->errc = 1;
					return NULL;
			}
			break;

    	case icSigAbstractClass:
			/* Expect AToB0 Lut and option BToA0 Lut, with either relative or absolute intent. */
			/* PCS <-> PCS */
			/* Determine the algorithm and set its parameters */

			if (intent != icmDefaultIntent
			 && intent != icRelativeColorimetric
			 && intent != icAbsoluteColorimetric) {
				sprintf(p->err,"icc_get_luobj: Intent is inappropriate for Abstract profile");
				p->errc = 1;
				return NULL;
			}

			switch (func) {
		    	case icmFwd:	/* PCS (== Device) to PCS */

					luobj = icc_new_icmLuLut(p, icSigAToB0Tag,
					     p->header->colorSpace, pcs, pcs,
					     e_pcs, e_pcs, e_pcs,
					     intent, func);
					break;

		    	case icmBwd:	/* PCS to PCS (== Device) */

					luobj = icc_new_icmLuLut(p, icSigBToA0Tag,
					     pcs, p->header->colorSpace, pcs,
					     e_pcs, e_pcs, e_pcs,
					     intent, func);
					break;

				default:
					sprintf(p->err,"icc_get_luobj: Inaproptiate function requested");
					p->errc = 1;
					return NULL;
			}
			break;

    	case icSigNamedColorClass:
			/* Expect Name -> Device, Optional PCS */
			/* and a reverse lookup would be useful */
			/* (ie. PCS or Device coords to closest named color) */
			/* ~~ to be implemented ~~ */

			/* ~~ Absolute intent is valid for processing of */
			/* PCS from named Colors. Also allow for e_pcs */
			if (intent != icmDefaultIntent
			 && intent != icRelativeColorimetric
			 && intent != icAbsoluteColorimetric) {
				sprintf(p->err,"icc_get_luobj: Intent is inappropriate for Named Color profile");
				p->errc = 1;
				return NULL;
			}

			sprintf(p->err,"icc_get_luobj: Named Colors not handled yet");
			p->errc = 1;
			return NULL;

    	default:
			sprintf(p->err,"icc_get_luobj: Unknown profile class");
			p->errc = 1;
			return NULL;
	}

	if (luobj == NULL) {
		sprintf(p->err,"icc_get_luobj: Unable to locate usable conversion");
		p->errc = 1;
	} else {
		luobj->order = order;
	}

	return luobj;
}

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* Returns total ink limit and channel maximums. */
/* Returns -1.0 if not applicable for this type of profile. */
/* Returns -1.0 for grey, additive, or any profiles < 4 channels. */
/* This is a place holder that uses a heuristic, */
/* until there is a private or standard tag for this information */
static double icm_get_tac(		/* return TAC */
icc *p,
double *chmax,					/* device return channel sums. May be NULL */
void (*calfunc)(void *cntx, double *out, double *in),	/* Optional calibration func. */
void *cntx
) {
	icmHeader *rh = p->header;
	icmLuBase *luo;
	icmLuLut *ll;
	icmLut *lut;
	icColorSpaceSignature outs;		/* Type of output space */
	int inn, outn;					/* Number of components */
	icmLuAlgType alg;				/* Type of lookup algorithm */
	double tac = 0.0;
	double max[MAX_CHAN];			/* Channel maximums */
	int i, f;
	unsigned int uf;
	int size;						/* Lut table size */
	double *gp;						/* Pointer to grid cube base */

	/* If not something that can really have a TAC */
	if (rh->deviceClass != icSigDisplayClass
	 && rh->deviceClass != icSigOutputClass
	 && rh->deviceClass != icSigLinkClass) {
		return -1.0;
	}

	/* If not a suitable color space */
	switch (rh->colorSpace) {
		/* Not applicable */
    	case icSigXYZData:
    	case icSigLabData:
    	case icSigLuvData:
    	case icSigYCbCrData:
    	case icSigYxyData:
    	case icSigHsvData:
    	case icSigHlsData:
			return -1.0;

		/* Assume no limit */
    	case icSigGrayData:
    	case icSig2colorData:
    	case icSig3colorData:
    	case icSigRgbData:
			return -1.0;

		default:
			break;
	}

	/* Get a PCS->device colorimetric lookup */
	if ((luo = p->get_luobj(p, icmBwd, icRelativeColorimetric, icmSigDefaultData, icmLuOrdNorm)) == NULL) {
		if ((luo = p->get_luobj(p, icmBwd, icmDefaultIntent, icmSigDefaultData, icmLuOrdNorm)) == NULL) {
			return -1.0;
		}
	}

	/* Get details of conversion (Arguments may be NULL if info not needed) */
	luo->spaces(luo, NULL, &inn, &outs, &outn, &alg, NULL, NULL, NULL, NULL);

	/* Assume any non-Lut type doesn't have a TAC */
	if (alg != icmLutType) {
		return -1.0;
	}

	ll = (icmLuLut *)luo;

	/* We have a Lut type. Search the lut for the largest values */
	for (f = 0; f < outn; f++)
		max[f] = 0.0;

	lut = ll->lut;
	gp = lut->clutTable;		/* Base of grid array */
	size = sat_pow(lut->clutPoints,lut->inputChan);
	for (i = 0; i < size; i++) {
		double tot, vv[MAX_CHAN];			
		
		lut->lookup_output(lut,vv,gp);		/* Lookup though output tables */
		ll->out_denormf(vv,vv);				/* Normalize for output color space */

		if (calfunc != NULL)
			calfunc(cntx, vv, vv);			/* Apply device calibration */

		for (tot = 0.0, uf = 0; uf < lut->outputChan; uf++) {
			tot += vv[uf];
			if (vv[uf] > max[uf])
				max[uf] = vv[uf];
		}
		if (tot > tac)
			tac = tot;
		gp += lut->outputChan;
	}

	if (chmax != NULL) {
		for (f = 0; f < outn; f++)
			chmax[f] = max[f];
	}

	luo->del(luo);

	return tac;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Create an empty object. Return NULL on error */
icc *new_icc_a(
icmAlloc *al			/* Memory allocator */
) {
	unsigned int i;
	icc *p;

	if ((p = (icc *) al->calloc(al, 1,sizeof(icc))) == NULL) {
		return NULL;
	}
	p->ver = icmVersionDefault;		/* default is V2.2.0 profile */

	p->al = al;			/* Heap allocator */

	p->get_rfp       = icc_get_rfp;
	p->set_version   = icc_set_version;
	p->get_size      = icc_get_size;
	p->read          = icc_read;
	p->read_x        = icc_read_x;
	p->write         = icc_write;
	p->write_x       = icc_write_x;
	p->dump          = icc_dump;
	p->del           = icc_delete;
	p->add_tag       = icc_add_tag;
	p->link_tag      = icc_link_tag;
	p->find_tag      = icc_find_tag;
	p->read_tag      = icc_read_tag;
	p->read_tag_any  = icc_read_tag_any;
	p->rename_tag    = icc_rename_tag;
	p->unread_tag    = icc_unread_tag;
	p->read_all_tags = icc_read_all_tags;
	p->delete_tag    = icc_delete_tag;
	p->check_id      = icc_check_id;
	p->get_tac       = icm_get_tac;
	p->get_luobj     = icc_get_luobj;
	p->new_clutluobj = icc_new_icmLuLut;
	p->chromAdaptMatrix = icc_chromAdaptMatrix;

#if defined(__IBMC__) && defined(_M_IX86)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
#endif

	/* Allocate a header object */
	if ((p->header = new_icmHeader(p)) == NULL) {
		al->free(al, p);
		return NULL;
	}

	/* Values that must be set before writing */
	p->header->deviceClass = icMaxEnumClass;/* Type of profile - must be set! */
    p->header->colorSpace = icMaxEnumData;	/* Clr space of data - must be set! */
    p->header->pcs = icMaxEnumData;			/* PCS: XYZ or Lab - must be set! */
    p->header->renderingIntent = icMaxEnumIntent;	/* Rendering intent - must be set ! */

	/* Values that should be set before writing */
	p->header->manufacturer = icmSigUnknownType;/* Dev manufacturer - should be set ! */
    p->header->model = icmSigUnknownType;		/* Dev model number - should be set ! */
    p->header->attributes.l = 0;				/* ICC Device attributes - should set ! */
    p->header->flags = 0;						/* Embedding flags - should be set ! */
	
	/* Values that may be set before writing */
    p->header->attributes.h = 0;			/* Dev Device attributes - may be set ! */
    p->header->creator = str2tag("argl");	/* Profile creator - Argyll - may be set ! */

	/* Init default values in header */
	p->header->cmmId = str2tag("argl");		/* CMM for profile - Argyll CMM */
    p->header->majv = 2;					/* Current version 2.2.0 */
	p->header->minv = 2;
	p->header->bfv  = 0;
	setcur_DateTimeNumber(&p->header->date);/* Creation Date */
#ifdef NT
    p->header->platform = icSigMicrosoft;	/* Primary Platform */
#endif
#ifdef __APPLE__
	p->header->platform = icSigMacintosh;
#endif
#if defined(UNIX) && !defined(__APPLE__)
	p->header->platform = icmSig_nix;
#endif
    p->header->illuminant = icmD50;			/* Profile illuminant - D50 */

	/* Values that will be created automatically */
	for (i = 0; i < 16; i++)
		p->header->id[i] = 0;

	p->autoWpchtmx = 1;						/* Auto on create */

	/* Should we use ICC standard Wrong Von Kries for */
	/* white point chromatic adapation for output class ? */
	if (getenv("ARGYLL_CREATE_WRONG_VON_KRIES_OUTPUT_CLASS_REL_WP") != NULL)
		p->useLinWpchtmx = 1;				/* Use Wrong Von Kries */
	else
		p->useLinWpchtmx = 0;				/* Use Bradford by default */
	p->wpchtmx_class = icMaxEnumClass;		/* Not set yet */

	/* Default to saving ArgyllCMS private 'arts' tag (if appropriate type of */
	/* profile) to make white point chromatic adapation explicit. */
	p->useArts = 1;

	/* Should we create a V4 style Display profile with D50 media white point */
	/* tag and 'chad' tag ? */
	if (getenv("ARGYLL_CREATE_DISPLAY_PROFILE_WITH_CHAD") != NULL)
		p->useChad = 1;				/* Mark Media WP as D50 and put absolute to relative */
									/* transform matrix in 'chad' tag. */
	else
		p->useChad = 0;				/* No by default - use Bradford and store real Media WP */

	/* Set a default wpchtmx in case the profile being read or written */
	/* doesn't use a white point (i.e., it's a device link) */
	if (!p->useLinWpchtmx) {
		icmCpy3x3(p->wpchtmx, icmBradford);
		icmInverse3x3(p->iwpchtmx, p->wpchtmx);
	} else {
		icmCpy3x3(p->wpchtmx, icmWrongVonKries);
		icmCpy3x3(p->iwpchtmx, icmWrongVonKries);
	}

	return p;
}


/* ---------------------------------------------------------- */
/* Print an int vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPiv(int di, int *p) {
	static char buf[5][MAX_CHAN * 16];
	static int ix = 0;
	int e;
	char *bp;

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	if (di > MAX_CHAN)
		di = MAX_CHAN;		/* Make sure that buf isn't overrun */

	for (e = 0; e < di; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, "%d", p[e]); bp += strlen(bp);
	}
	return buf[ix];
}

/* Print a double color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPdv(int di, double *p) {
	static char buf[5][MAX_CHAN * 16];
	static int ix = 0;
	int e;
	char *bp;

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	if (di > MAX_CHAN)
		di = MAX_CHAN;		/* Make sure that buf isn't overrun */

	for (e = 0; e < di; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, "%.8f", p[e]); bp += strlen(bp);
	}
	return buf[ix];
}

/* Print a float color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *icmPfv(int di, float *p) {
	static char buf[5][MAX_CHAN * 16];
	static int ix = 0;
	int e;
	char *bp;

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	if (di > MAX_CHAN)
		di = MAX_CHAN;		/* Make sure that buf isn't overrun */

	for (e = 0; e < di; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, "%.8f", p[e]); bp += strlen(bp);
	}
	return buf[ix];
}

/* Print an 0..1 range XYZ as a D50 Lab string */
/* Returned static buffer is re-used every 5 calls. */
char *icmPLab(double *p) {
	static char buf[5][MAX_CHAN * 16];
	static int ix = 0;
	int e;
	char *bp;
	double lab[3];

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	icmXYZ2Lab(&icmD50, lab, p);

	for (e = 0; e < 3; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, "%f", lab[e]); bp += strlen(bp);
	}
	return buf[ix];
}


/* ---------------------------------------------------------- */

