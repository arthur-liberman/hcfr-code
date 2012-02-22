
/* 
 * parse library stdio and malloc utility classes.
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

#define _PARSSTD_C_

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

#include "pars.h"

#endif /* !COMBINED_STD */

#if defined(SEPARATE_STD) || defined(COMBINED_STD)

/* ----------------------------------------------- */
/* Standard Heap allocator cgatsAlloc compatible class */
/* Just call the standard system function */

#ifdef CGATS_DEBUG_MALLOC

/* Make sure that inline malloc #defines are turned off for this file */
#undef was_debug_malloc
#ifdef malloc
#undef malloc
#undef calloc
#undef realloc
#undef free
#define was_debug_malloc
#endif	/* dmalloc */

#ifdef NT
#define fileno _fileno
#endif

static void *cgatsAllocStd_dmalloc(
struct _cgatsAlloc *pp,
size_t size,
char *file,
int line
) {
	void *rv = malloc(size);
	return rv;
}

static void *cgatsAllocStd_dcalloc(
struct _cgatsAlloc *pp,
size_t num,
size_t size,
char *file,
int line
) {
	void *rv = calloc(num, size);
	return rv;
}

static void *cgatsAllocStd_drealloc(
struct _cgatsAlloc *pp,
void *ptr,
size_t size,
char *file,
int line
) {
	void *rv = realloc(ptr, size);
	return rv;
}


static void cgatsAllocStd_dfree(
struct _cgatsAlloc *pp,
void *ptr,
char *file,
int line
) {
	free(ptr);
}

/* we're done with the AllocStd object */
static void cgatsAllocStd_delete(
cgatsAlloc *pp
) {
	cgatsAllocStd *p = (cgatsAllocStd *)pp;

	free(p);
}

/* Create cgatsAllocStd */
cgatsAlloc *new_cgatsAllocStd() {
	cgatsAllocStd *p;
	if ((p = (cgatsAllocStd *) calloc(1,sizeof(cgatsAllocStd))) == NULL)
		return NULL;
	p->dmalloc  = cgatsAllocStd_dmalloc;
	p->dcalloc  = cgatsAllocStd_dcalloc;
	p->drealloc = cgatsAllocStd_drealloc;
	p->dfree    = cgatsAllocStd_dfree;
	p->del      = cgatsAllocStd_delete;

	return (cgatsAlloc *)p;
}

#ifdef was_debug_malloc
#undef was_debug_malloc
#define malloc( p, size )	    dmalloc( p, size, __FILE__, __LINE__ )
#define calloc( p, num, size )	dcalloc( p, num, size, __FILE__, __LINE__ )
#define realloc( p, ptr, size )	drealloc( p, ptr, size, __FILE__, __LINE__ )
#define free( p, ptr )	        dfree( p, ptr , __FILE__, __LINE__ )
#endif	/* was_debug_malloc */

#else /* !CGATS_DEBUG_MALLOC */

static void *cgatsAllocStd_malloc(
struct _cgatsAlloc *pp,
size_t size
) {
	void *rv = malloc(size);
	return rv;
}

static void *cgatsAllocStd_calloc(
struct _cgatsAlloc *pp,
size_t num,
size_t size
) {
	void *rv = calloc(num, size);
	return rv;
}

static void *cgatsAllocStd_realloc(
struct _cgatsAlloc *pp,
void *ptr,
size_t size
) {
	void *rv = realloc(ptr, size);
	return rv;
}


static void cgatsAllocStd_free(
struct _cgatsAlloc *pp,
void *ptr
) {
	free(ptr);
}

/* we're done with the AllocStd object */
static void cgatsAllocStd_delete(
cgatsAlloc *pp
) {
	cgatsAllocStd *p = (cgatsAllocStd *)pp;

	free(p);
}

/* Create cgatsAllocStd */
cgatsAlloc *new_cgatsAllocStd() {
	cgatsAllocStd *p;
	if ((p = (cgatsAllocStd *) calloc(1,sizeof(cgatsAllocStd))) == NULL)
		return NULL;
	p->malloc  = cgatsAllocStd_malloc;
	p->calloc  = cgatsAllocStd_calloc;
	p->realloc = cgatsAllocStd_realloc;
	p->free    = cgatsAllocStd_free;
	p->del     = cgatsAllocStd_delete;

	return (cgatsAlloc *)p;
}

#endif /* !CGATS_DEBUG_MALLOC */

/* ------------------------------------------------- */
/* Standard Stream file I/O cgatsFile compatible class */

/* Get the size of the file (Only valid for reading file. */
static size_t cgatsFileStd_get_size(cgatsFile *pp) {
	return pp->size;
}

/* Set current position to offset. Return 0 on success, nz on failure. */
static int cgatsFileStd_seek(
cgatsFile *pp,
unsigned int offset
) {
	cgatsFileStd *p = (cgatsFileStd *)pp;

	return fseek(p->fp, offset, SEEK_SET);
}

/* Read count items of size length. Return number of items successfully read. */
static size_t cgatsFileStd_read(
cgatsFile *pp,
void *buffer,
size_t size,
size_t count
) {
	cgatsFileStd *p = (cgatsFileStd *)pp;

	return fread(buffer, size, count, p->fp);
}

/* Read a character */
static int cgatsFileStd_getch(
cgatsFile *pp
) {

	cgatsFileStd *p = (cgatsFileStd *)pp;

	return fgetc(p->fp);
}

/* write count items of size length. Return number of items successfully written. */
static size_t cgatsFileStd_write(
cgatsFile *pp,
void *buffer,
size_t size,
size_t count
) {
	cgatsFileStd *p = (cgatsFileStd *)pp;

	return fwrite(buffer, size, count, p->fp);
}

/* do a printf */
static int cgatsFileStd_printf(
cgatsFile *pp,
const char *format,
...
) {
	int rv;
	va_list args;
	cgatsFileStd *p = (cgatsFileStd *)pp;

	va_start(args, format);
	rv = vfprintf(p->fp, format, args);
	va_end(args);
	return rv;
}

/* flush all write data out to secondary storage. Return nz on failure. */
static int cgatsFileStd_flush(
cgatsFile *pp
) {
	cgatsFileStd *p = (cgatsFileStd *)pp;

	return fflush(p->fp);
}

/* return the filename */
static char *cgatsFileStd_fname(
cgatsFile *pp
) {
	cgatsFileStd *p = (cgatsFileStd *)pp;

	if (p->filename != NULL)
		return p->filename;
	else
		return "**Unknown**";
}


/* we're done with the file object, return nz on failure */
static int cgatsFileStd_delete(
cgatsFile *pp
) {
	int rv = 0;
	cgatsFileStd *p = (cgatsFileStd *)pp;
	cgatsAlloc *al = p->al;
	int del_al   = p->del_al;

	if (p->doclose != 0) {
		if (fclose(p->fp) != 0)
			rv = 2;
	}

	if (p->filename != NULL)
		al->free(al, p->filename);

	al->free(al, p);	/* Free object */
	if (del_al)			/* We are responsible for deleting allocator */
		al->del(al);

	return rv;
}

/* Create cgatsFile given a (binary) FILE* */
cgatsFile *new_cgatsFileStd_fp(
FILE *fp
) {
	return new_cgatsFileStd_fp_a(fp, NULL);
}

/* Create cgatsFile given a (binary) FILE* and allocator */
cgatsFile *new_cgatsFileStd_fp_a(
FILE *fp,
cgatsAlloc *al		/* heap allocator, NULL for default */
) {
	cgatsFileStd *p;
	int del_al = 0;
	struct stat sbuf;

	if (al == NULL) {	/* None provided, create default */
		if ((al = new_cgatsAllocStd()) == NULL)
			return NULL;
		del_al = 1;		/* We need to delete it */
	}

	if ((p = (cgatsFileStd *) al->calloc(al, 1, sizeof(cgatsFileStd))) == NULL) {
		if (del_al)
			al->del(al);
		return NULL;
	}
	p->al       = al;				/* Heap allocator */
	p->del_al   = del_al;			/* Flag noting whether we delete it */
	p->get_size = cgatsFileStd_get_size;
	p->seek     = cgatsFileStd_seek;
	p->read     = cgatsFileStd_read;
	p->getch    = cgatsFileStd_getch;
	p->write    = cgatsFileStd_write;
	p->gprintf  = cgatsFileStd_printf;
	p->flush    = cgatsFileStd_flush;
	p->fname    = cgatsFileStd_fname;
	p->del      = cgatsFileStd_delete;

	if (fstat(fileno(fp), &sbuf) == 0) {
		p->size = sbuf.st_size;
	} else {
		p->size = 0;
	}

	p->fp = fp;
	p->doclose = 0;

	return (cgatsFile *)p;
}

/* Create cgatsFile given a file name */
cgatsFile *new_cgatsFileStd_name(
const char *name,
const char *mode
) {
	return new_cgatsFileStd_name_a(name, mode, NULL);
}

/* Create given a file name and allocator */
cgatsFile *new_cgatsFileStd_name_a(
const char *name,
const char *mode,
cgatsAlloc *al			/* heap allocator, NULL for default */
) {
	FILE *fp;
	cgatsFile *p;
	char nmode[50];

	strcpy(nmode, mode);
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	if ((fp = fopen(name, nmode)) == NULL)
		return NULL;
	
	p = new_cgatsFileStd_fp_a(fp, al);

	if (p != NULL) {
		cgatsFileStd *pp = (cgatsFileStd *)p;
		pp->doclose = 1;

		pp->filename = pp->al->malloc(pp->al, strlen(name) + 1);
		strcpy(pp->filename, name);
	}
	return p;
}

/* Create a memory image file access class with the std allocator */
cgatsFile *new_cgatsFileMem(
void *base,			/* Pointer to base of memory buffer */
size_t length		/* Number of bytes in buffer */
) {
	cgatsFile *p;
	cgatsAlloc *al;			/* memory allocator */

	if ((al = new_cgatsAllocStd()) == NULL)
		return NULL;

	if ((p = new_cgatsFileMem_a(base, length, al)) == NULL) {
		al->del(al);
		return NULL;
	}

	((cgatsFileMem *)p)->del_al = 1;		/* Get cgatsFileMem->del to cleanup allocator */
	return p;
}

/* ------------------------------------------------- */

/* Create an empty parse object, default standard allocator */
parse *
new_parse(cgatsFile *fp) {
	parse *p;
	cgatsAlloc *al;			/* memory allocator */

	if ((al = new_cgatsAllocStd()) == NULL)
		return NULL;

	if ((p = new_parse_al(al, fp)) == NULL) {
		al->del(al);
		return NULL;
	}

	p->del_al = 1;			/* Get parse->del to cleanup allocator */
	return p;
}


#endif /* defined(SEPARATE_STD) || defined(COMBINED_STD) */
