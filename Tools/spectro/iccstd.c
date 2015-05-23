
/* 
 * ICC library stdio and malloc utility classes.
 *
 * Author:  Graeme W. Gill
 * Date:    2002/10/24
 * Version: 2.15
 *
 * Copyright 1997 - 2012 Graeme W. Gill
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License.txt file in this directory for licensing details.
 *
 * These are kept in a separate file to allow them to be
 * selectively ommitted from the icc library.
 *
 */

#ifndef COMBINED_STD

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

#endif /* !COMBINED_STD */

#if defined(SEPARATE_STD) || defined(COMBINED_STD)

#ifdef _MSC_VER
#define fileno _fileno
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)(-1))
#endif

/* ------------------------------------------------- */
/* Standard Heap allocator icmAlloc compatible class */
/* Just call the standard system function */

#ifdef ICC_DEBUG_MALLOC

/* Make sure that inline malloc #defines are turned off for this file */
#undef was_debug_malloc
#ifdef malloc
#undef malloc
#undef calloc
#undef realloc
#undef free
#define was_debug_malloc
#endif	/* dmalloc */

static void *icmAllocStd_dmalloc(
struct _icmAlloc *pp,
size_t size,
char *name,
int line
) {
	return malloc(size);
}

static void *icmAllocStd_dcalloc(
struct _icmAlloc *pp,
size_t num,
size_t size,
char *name,
int line
) {
	if (size != 0 && num > (SIZE_MAX/size))
		return NULL;
	calloc(num, size);
}

static void *icmAllocStd_drealloc(
struct _icmAlloc *pp,
void *ptr,
size_t size,
char *name,
int line
) {
	return realloc(ptr, size);
}


static void icmAllocStd_dfree(
struct _icmAlloc *pp,
void *ptr,
char *name,
int line
) {
	free(ptr);
}

/* we're done with the AllocStd object */
static void icmAllocStd_delete(
icmAlloc *pp
) {
	icmAllocStd *p = (icmAllocStd *)pp;

	free(p);
}

/* Create icmAllocStd */
icmAlloc *new_icmAllocStd() {
	icmAllocStd *p;
	if ((p = (icmAllocStd *) calloc(1,sizeof(icmAllocStd))) == NULL)
		return NULL;
	p->dmalloc  = icmAllocStd_dmalloc;
	p->dcalloc  = icmAllocStd_dcalloc;
	p->drealloc = icmAllocStd_drealloc;
	p->dfree    = icmAllocStd_dfree;
	p->del     = icmAllocStd_delete;

	return (icmAlloc *)p;
}

#ifdef was_debug_malloc
#undef was_debug_malloc
#define malloc( p, size )	    dmalloc( p, size, __FILE__, __LINE__ )
#define calloc( p, num, size )	dcalloc( p, num, size, __FILE__, __LINE__ )
#define realloc( p, ptr, size )	drealloc( p, ptr, size, __FILE__, __LINE__ )
#define free( p, ptr )	        dfree( p, ptr , __FILE__, __LINE__ )
#endif	/* was_debug_malloc */

#else /* !DEBUG_MALLOC */

static void *icmAllocStd_malloc(
struct _icmAlloc *pp,
size_t size
) {
	return malloc(size);
}

static void *icmAllocStd_calloc(
struct _icmAlloc *pp,
size_t num,
size_t size
) {
	if (size != 0 && num > (SIZE_MAX/size))
		return NULL;
	return calloc(num, size);
}

static void *icmAllocStd_realloc(
struct _icmAlloc *pp,
void *ptr,
size_t size
) {
	return realloc(ptr, size);
}


static void icmAllocStd_free(
struct _icmAlloc *pp,
void *ptr
) {
	free(ptr);
}

/* we're done with the AllocStd object */
static void icmAllocStd_delete(
icmAlloc *pp
) {
	icmAllocStd *p = (icmAllocStd *)pp;

	free(p);
}

/* Create icmAllocStd */
icmAlloc *new_icmAllocStd() {
	icmAllocStd *p;
	if ((p = (icmAllocStd *) calloc(1,sizeof(icmAllocStd))) == NULL)
		return NULL;
	p->malloc  = icmAllocStd_malloc;
	p->calloc  = icmAllocStd_calloc;
	p->realloc = icmAllocStd_realloc;
	p->free    = icmAllocStd_free;
	p->del     = icmAllocStd_delete;

	return (icmAlloc *)p;
}

#endif	/* ICC_DEBUG_MALLOC */

/* ------------------------------------------------- */
/* Standard Stream file I/O icmFile compatible class */

/* Get the size of the file (Only valid for reading file. */
static size_t icmFileStd_get_size(icmFile *pp) {
	icmFileStd *p = (icmFileStd *)pp;

	return p->size;
}

/* Set current position to offset. Return 0 on success, nz on failure. */
static int icmFileStd_seek(
icmFile *pp,
unsigned int offset
) {
	icmFileStd *p = (icmFileStd *)pp;

	return fseek(p->fp, offset, SEEK_SET);
}

/* Read count items of size length. Return number of items successfully read. */
static size_t icmFileStd_read(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileStd *p = (icmFileStd *)pp;

	return fread(buffer, size, count, p->fp);
}

/* write count items of size length. Return number of items successfully written. */
static size_t icmFileStd_write(
icmFile *pp,
void *buffer,
size_t size,
size_t count
) {
	icmFileStd *p = (icmFileStd *)pp;

	return fwrite(buffer, size, count, p->fp);
}


/* do a printf */
static int icmFileStd_printf(
icmFile *pp,
const char *format,
...
) {
	int rv;
	va_list args;
	icmFileStd *p = (icmFileStd *)pp;

	va_start(args, format);
	rv = vfprintf(p->fp, format, args);
	va_end(args);
	return rv;
}

/* flush all write data out to secondary storage. Return nz on failure. */
static int icmFileStd_flush(
icmFile *pp
) {
	icmFileStd *p = (icmFileStd *)pp;

	return fflush(p->fp);
}

/* Return the memory buffer. Error if not icmFileMem */
static int icmFileStd_get_buf(
icmFile *pp,
unsigned char **buf,
size_t *len
) {
	return 1;
}

/* we're done with the file object, return nz on failure */
static int icmFileStd_delete(
icmFile *pp
) {
	int rv = 0;
	icmFileStd *p = (icmFileStd *)pp;
	icmAlloc *al = p->al;
	int del_al   = p->del_al;

	if (p->doclose != 0) {
		if (fclose(p->fp) != 0)
			rv = 2;
	}

	al->free(al, p);	/* Free object */
	if (del_al)			/* We are responsible for deleting allocator */
		al->del(al);

	return rv;
}

/* Create icmFile given a (binary) FILE* */
icmFile *new_icmFileStd_fp(
FILE *fp
) {
	return new_icmFileStd_fp_a(fp, NULL);
}

/* Create icmFile given a (binary) FILE* and allocator */
icmFile *new_icmFileStd_fp_a(
FILE *fp,
icmAlloc *al		/* heap allocator, NULL for default */
) {
	icmFileStd *p;
	int del_al = 0;

	if (al == NULL) {	/* None provided, create default */
		if ((al = new_icmAllocStd()) == NULL)
			return NULL;
		del_al = 1;		/* We need to delete the allocator we created */
	}

	if ((p = (icmFileStd *) al->calloc(al, 1, sizeof(icmFileStd))) == NULL) {
		if (del_al)
			al->del(al);
		return NULL;
	}
	p->al       = al;				/* Heap allocator */
	p->del_al   = del_al;			/* Flag noting whether we delete it */
	p->get_size = icmFileStd_get_size;
	p->seek     = icmFileStd_seek;
	p->read     = icmFileStd_read;
	p->write    = icmFileStd_write;
	p->gprintf  = icmFileStd_printf;
	p->flush    = icmFileStd_flush;
	p->get_buf  = icmFileStd_get_buf;
	p->del      = icmFileStd_delete;

	// fstat() is not so robust on MSWin.
	if (fseek(fp, 0L, SEEK_END) == 0) {
		p->size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
	} else {
		p->size = 0;
	}

	p->fp = fp;
	p->doclose = 0;

	return (icmFile *)p;
}

/* Create icmFile given a file name */
icmFile *new_icmFileStd_name(
char *name,
char *mode
) {
	return new_icmFileStd_name_a(name, mode, NULL);
}

/* Create given a file name and allocator */
icmFile *new_icmFileStd_name_a(
char *name,
char *mode,
icmAlloc *al			/* heap allocator, NULL for default */
) {
	FILE *fp;
	icmFile *p;
	char nmode[50];

	strcpy(nmode, mode);
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	if ((fp = fopen(name,nmode)) == NULL)
		return NULL;
	
	p = new_icmFileStd_fp_a(fp, al);

	if (p != NULL) {
		icmFileStd *pp = (icmFileStd *)p;
		pp->doclose = 1;
	}
	return p;
}

/* ------------------------------------------------- */

/* Create a memory image file access class with the std allocator */
icmFile *new_icmFileMem(
void *base,			/* Pointer to base of memory buffer */
size_t length		/* Number of bytes in buffer */
) {
	icmFile *p;
	icmAlloc *al;			/* memory allocator */

	if ((al = new_icmAllocStd()) == NULL)
		return NULL;

	if ((p = new_icmFileMem_a(base, length, al)) == NULL) {
		al->del(al);
		return NULL;
	}

	((icmFileMem *)p)->del_al = 1;		/* Get icmFileMem->del to cleanup allocator */
	return p;
}

/* Create a memory image file access class with the std allocator */
/* and delete buffer when icmFile is deleted. */
icmFile *new_icmFileMem_d(
void *base,			/* Pointer to base of memory buffer */
size_t length		/* Number of bytes in buffer */
) {
	icmFile *fp;

	if ((fp = new_icmFileMem(base, length)) != NULL) {	
		((icmFileMem *)fp)->del_buf = 1;
	}
	return fp;
}

/* ------------------------------------------------- */

/* Create an icc with the std allocator */
icc *new_icc(void) {
	icc *p;
	icmAlloc *al;			/* memory allocator */

	if ((al = new_icmAllocStd()) == NULL)
		return NULL;

	if ((p = new_icc_a(al)) == NULL) {
		al->del(al);
		return NULL;
	}

	p->del_al = 1;		/* Get icc->del to cleanup allocator */
	return p;
}


#endif /* defined(SEPARATE_STD) || defined(COMBINED_STD) */
