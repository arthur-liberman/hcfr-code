
/* 
 * Simple ASCII file parsing object.
 * Used as a base for the CGATS.5 and IT8.7 family file I/O class
 * Version 2.05
 *
 * Author: Graeme W. Gill
 * Date:   20/12/95
 *
 * Copyright 1996, 2002 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License.txt file in this directory for licensing details.
 */

#define _PARS_C_				/* Turn on implimentation code */

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

#ifdef _MSC_VER
#define vsnprintf _vsnprintf
#define snprintf _snprintf
#endif

#ifdef STANDALONE_TEST
extern void error(const char *fmt, ...), warning(const char *fmt, ...);
#endif

#include "pars.h"

#undef DEBUG			/* Print each token returned */

static void del_parse(parse *p);
static int read_line(parse *p);
static void reset_del(parse *p);
static void add_del(struct _parse *p, char *t,
                    char *nr, char *c, char *q);
static char *get_token(parse *p);

/* Open the file, allocate and initialize the parse structure */
/* Return pointer to parse structure. Return NULL on error */
parse *
new_parse_al(
cgatsAlloc *al,		/* Allocator object */
cgatsFile *fp		/* File to read from */
) {
	parse *p;

	if ((p = (parse *) al->calloc(al, sizeof(parse), 1)) == NULL) {
		return NULL;
	}
	p->al = al;				/* Heap allocator */

	p->fp = fp;
	p->b = NULL;	/* Init line buffer */
	p->bs = 0;
	p->bo = 0;
	p->tb = NULL;	/* Init token buffer */
	p->tbs = 0;
	p->to = 0;
	p->line = 0;
	p->token = 0;
	p->ltflag = 0;
	p->q = 0;
	p->errc = 0;
	p->err[0] = '\000';

	/* Reset the parsing delimiters */
	reset_del(p);

	/* Set default pointers to methods */
	p->del       = del_parse;
	p->read_line = read_line;
	p->reset_del = reset_del;
	p->add_del   = add_del;
	p->get_token = get_token;

	return p;
}

/* new_parse() with default malloc allocator is in parsstd.c */

#ifndef SEPARATE_STD
#define COMBINED_STD

#include "parsstd.c"

#undef COMBINED_STD
#endif /* SEPARATE_STD */

/* --------------------------------------------- */

/* size_t versions of saturating arithmatic */

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)(-1))
#endif

/* a * b */
static size_t ssat_mul(size_t a, size_t b) {

	if (a == 0 || b == 0)
		return 0;

	if (a > (SIZE_MAX/b))
		return SIZE_MAX;
	else
		return a * b;
}

/* --------------------------------------------- */
/* Memory image cgatsFile compatible class */
/* Buffer is assumed to have been allocated by the given allocator, */
/* and will be expanded on write. */

/* Get the size of the file (Only valid for memory file). */
static size_t cgatsFileMem_get_size(cgatsFile *pp) {
	cgatsFileMem *p = (cgatsFileMem *)pp;

	return p->end - p->start;
}

/* Set current position to offset. Return 0 on success, nz on failure. */
static int cgatsFileMem_seek(
cgatsFile *pp,
unsigned int offset
) {
	cgatsFileMem *p = (cgatsFileMem *)pp;
	unsigned char *np;

	np = p->start + offset;
	if (np < p->start || np >= p->end)
		return 1;
	p->cur = np;
	return 0;
}

/* Read count items of size length. Return number of items successfully read. */
static size_t cgatsFileMem_read(
cgatsFile *pp,
void *buffer,
size_t size,
size_t count
) {
	cgatsFileMem *p = (cgatsFileMem *)pp;
	size_t len;

	len = ssat_mul(size, count);
	if (len > (size_t)(p->end - p->cur)) { /* Too much */
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

/* Read a character */
static int cgatsFileMem_getch(
cgatsFile *pp
) {
	cgatsFileMem *p = (cgatsFileMem *)pp;
	int c;

	if (p->cur < p->start || p->cur >= p->end)
		return EOF;

	c = (int)*p->cur;
	p->cur++;

	return c;
}

/* Expand the memory buffer file to hold up to pointer ep */
/* Don't expand if realloc fails */
static void cgatsFileMem_filemem_resize(cgatsFileMem *p, unsigned char *ep) {
	size_t na, co, ce;
	unsigned char *nstart;
	
	/* No need to realloc */
	if (ep <= p->aend) {
		return;
	}

	co = p->cur - p->start;		/* Current offset */
	ce = p->end - p->start;		/* Current end */
	na = ep - p->start;			/* new allocatd size */

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
static size_t cgatsFileMem_write(
cgatsFile *pp,
void *buffer,
size_t size,
size_t count
) {
	cgatsFileMem *p = (cgatsFileMem *)pp;
	size_t len;

	len = ssat_mul(size, count);
	if (len > (size_t)(p->end - p->cur))  /* Try and expand buffer */
		cgatsFileMem_filemem_resize(p, p->start + len);

	if (len > (size_t)(p->end - p->cur)) {
		if (size > 0)
			count = (p->end - p->cur)/size;
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
static int cgatsFileMem_printf(
cgatsFile *pp,
const char *format,
...
) {
	int rv;
	va_list args;
	cgatsFileMem *p = (cgatsFileMem *)pp;
	int len;

	va_start(args, format);

	rv = 1;
	len = 100;					/* Initial allocation for printf */
	cgatsFileMem_filemem_resize(p, p->cur + len);

	/* We have to use the available printf functions to resize the buffer if needed. */
	for (;rv != 0;) {
		/* vsnprintf() either returns -1 if it doesn't fit, or */
		/* returns the size-1 needed in order to fit. */
		len = vsnprintf((char *)p->cur, (p->aend - p->cur), format, args);

		if (len > -1 && ((p->cur + len +1) <= p->aend))	/* Fitted in current allocation */
			break;

		if (len > -1)				/* vsnprintf returned needed size-1 */
			len = len+2;			/* (In case vsnprintf returned 1 less than it needs) */
		else
			len *= 2;				/* We just have to guess */

		/* Attempt to resize */
		cgatsFileMem_filemem_resize(p, p->cur + len);

		/* If resize failed */
		if ((p->aend - p->cur) < len) {
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
static int cgatsFileMem_flush(
cgatsFile *pp
) {
	return 0;
}

/* Return the memory buffer. Error if not cgatsFileMem */
static int cgatsFileMem_get_buf(
cgatsFile *pp,
unsigned char **buf,
size_t *len
) {
	cgatsFileMem *p = (cgatsFileMem *)pp;
	if (buf != NULL)
		*buf = p->start;
	if (len != NULL)
		*len = p->end - p->start;
	return 0;
}

/* return the filename */
static char *cgatsFileMem_fname(
cgatsFile *pp
) {
//	cgatsFileMem *p = (cgatsFileMem *)pp;

	/* Memory doesn't have a name */
	return "**Mem**";
}

/* we're done with the file object, return nz on failure */
static int cgatsFileMem_delete(
cgatsFile *pp
) {
	cgatsFileMem *p = (cgatsFileMem *)pp;
	cgatsAlloc *al = p->al;
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
cgatsFile *new_cgatsFileMem_a(
void *base,			/* Pointer to base of memory buffer */
size_t length,		/* Number of bytes in buffer */
cgatsAlloc *al		/* heap allocator */
) {
	cgatsFileMem *p;

	if ((p = (cgatsFileMem *) al->calloc(al, 1, sizeof(cgatsFileMem))) == NULL) {
		return NULL;
	}
	p->al       = al;				/* Heap allocator */
	p->get_size = cgatsFileMem_get_size;
	p->seek     = cgatsFileMem_seek;
	p->read     = cgatsFileMem_read;
	p->getch    = cgatsFileMem_getch;
	p->write    = cgatsFileMem_write;
	p->gprintf  = cgatsFileMem_printf;
	p->flush    = cgatsFileMem_flush;
	p->get_buf  = cgatsFileMem_get_buf;
	p->fname    = cgatsFileMem_fname;
	p->del      = cgatsFileMem_delete;

	p->start = (unsigned char *)base;
	p->cur = p->start;
	p->aend = p->end = p->start + length;

	return (cgatsFile *)p;
}

/* Create a memory image file access class with given allocator */
/* and delete base when cgatsFile is deleted. */
cgatsFile *new_cgatsFileMem_ad(void *base, size_t length, cgatsAlloc *al) {
	cgatsFile *fp;

	if ((fp = new_cgatsFileMem_a(base, length, al)) != NULL) {	
		((cgatsFileMem *)fp)->del_buf = 1;
	}

	return fp;
}

/* --------------------------------------------- */
/* Free up the structure (doesn't close the file) */
static void
del_parse(parse *p) {
	cgatsAlloc *al = p->al;
	int del_al     = p->del_al;

	if (p->b != NULL)
		al->free(al, p->b);
	if (p->tb != NULL)
		al->free(al, p->tb);
	al->free(al, p);

	if (del_al)			/* We are responsible for deleting allocator */
		al->del(al);
}


/* Read the next line from the file into the line buffer. */
/* Return 0 if the read fails due to reaching EOF before */
/* putting anything in the buffer. */
/* Return -1 if there was some other sort of failure, */
/* and the error message in parse will be valid. */
static int
read_line(parse *p) {
	int c;
	p->bo = 0;			/* Reset pointer to the start of the line buffer */
	p->q = 0;			/* Reset quoted flag */
	p->errc = 0;		/* Reset error status */
	p->err[0] = '\000';
	do {
		if ((c = p->fp->getch(p->fp)) == EOF) {
			if (p->bo == 0) {	/* If there is nothing in the buffer */
				p->line = 0;
#ifdef DEBUG
				printf("pars: read_line() got EOF\n");
#endif
				return 0;
			}
			c = 0;			/* Finish the line */
		}
		if (p->ltflag == 1) {		/* Finished last line on '\r' */
			p->ltflag = 0;
			if (c == '\n') {
				if (p->q == 0)
					continue;		/* Ignore the following '\n' */
				else
					p->line--;		/* Undo double increment due to \n after \r */
			}
			/* else fall through and use character */
		} else if (p->ltflag == 2) {	/* Finished last line on comment character */
									/* Suck up chars till the start of the next line */
			if (c == '\r')
				p->ltflag = 1;
			else if (c == '\n')
				p->ltflag = 0;
			continue;				/* Ignore characters untill we get to the end of the line */
		}

		if (c == '\r') {
			p->line++;			/* Increment line number */
			p->ltflag = 1;		/* Remember to allow 1 of '\n' before next line */
			if (p->q == 0)
				c = 0;			/* Finish the line */
		} else if (p->q == 0 && (p->delf[c] & PARS_COMM) != 0) {	/* Hit a comment */
			p->line++;			/* Increment line number */
			p->ltflag = 2;		/* Remember to flush all chars up to end of line */
			c = 0;				/* Finish the line */
		} else if (c == '\n') {
			p->line++;			/* Increment line number */
			if (p->q == 0)
				c = 0;			/* Finish the the line */
		}

		/* Deal with starting/stopping a quoted section */
		if ((p->delf[c] & PARS_QUOTE) != 0) {
			if (p->q == 0)		/* We weren't in a quoted section */
				p->q = c;		/* Start of quoted section */
			else if (c == p->q)	/* If matching quote */
				p->q = 0;		/* End quoted section */
		}

		/* Can put the character in the buffer now */
		if (p->bo == p->bs) {				/* Run out of buffer space */
			p->bs = (p->bs + 100) * 2;	/* Expand line buffer size */
			if ((p->b = (char *) p->al->realloc(p->al, p->b, p->bs)) == NULL) {
				sprintf(p->err,"parse.read_line(), realloc failed!");
				return (p->errc = -1);
			}
		}
		p->b[p->bo++] = c;		/* Stash character away */
	} while (c != 0);	/* Null means we've done the end of the line */
	p->to = 0;			/* Reset token pointer to the start of the line buffer */
	p->q = 0;			/* Reset quoted flag */
#ifdef DEBUG
	printf("pars: read_line() got buffer '%s'\n",p->b);
#endif
	return 1;
}

/* Reset the delimiter character set */
static void
reset_del(parse *p) {
	int i;
	for (i = 0; i < 256; i++)
		p->delf[i] = 0;
	p->delf[0] = PARS_TERM;
}
	
/* Add to the parsing characters */
static void
add_del(
	parse *p,		/* Parse structure */
	char *t,		/* Terminators */
	char *nr,		/* Not Read */
	char *c,		/* Comment start */
	char *q)		/* Quote characters */
	{
	int i;
	if (t != NULL)
		for (i = 0; t[i] != '\000'; i++)
			p->delf[(int)t[i]] |= PARS_TERM;
	if (nr != NULL)
		for (i = 0; nr[i] != '\000'; i++)
			p->delf[(int)nr[i]] |= PARS_SKIP;
	if (c != NULL)
		for (i = 0; c[i] != '\000'; i++)
			p->delf[(int)c[i]] |= PARS_COMM;
	if (q != NULL)
		for (i = 0; q[i] != '\000'; i++)
			p->delf[(int)q[i]] |= PARS_QUOTE;
	}

/* Using the current token delimiter table and the current line, */
/* parse it from the current location and return a pointer to the */
/* null terminated token. Return NULL if there is no token found */
/* set the parse err and errc to non-zero if there was some other error */
static char *
get_token(parse *p) {
	int tbo = 0;	/* Token buffer offset */
	int term = 0;	/* flag to trigger token termination */
	char c;

	p->errc = 0;		/* Reset error status */
	p->err[0] = '\000';
	if (p->b == NULL) {
#ifdef DEBUG
		printf("pars: read_token() NULL buffe\n");
#endif
		return NULL;
	}
	p->token++;		/* Increment token number */
	p->q = 0;
	do {
		if (term)
			c = '\000';		/* end token */
		else if ((c = p->b[p->to++]) == '\000')	/* Fetch next token */
			p->to--;							/* Safety - don't pass end */

		/* Deal with starting/stopping a quoted section */
		if ((p->delf[c] & PARS_QUOTE) != 0) {
			if (p->q == 0)		/* We weren't in a quoted section */
				p->q = c;		/* Start of quoted section */
			else if (c == p->q)	/* If matching quote */
				p->q = 0;		/* End quoted section */
		}

		if (tbo == p->tbs) {				/* Run out of buffer space */
			p->tbs = (p->tbs + 100) * 2;	/* Expand token buffer size */
			if ((p->tb = (char *) p->al->realloc(p->al, p->tb, p->tbs)) == NULL) {
				sprintf(p->err,"parse.get_token(), realloc failed!");
				p->errc = -1;
				return NULL;
			}
		}

		if ((p->q != 0 && (p->q != c || (p->delf[c] & PARS_SKIP) == 0))
				/* If quoted, store if trigger quite is not being skipped */
		 || (!(tbo == 0 && (p->delf[c] & PARS_TERM) != 0 && (p->delf[c] & PARS_SKIP) != 0)
											/* Skip initial non-reader terminators */
		   && (p->delf[c] & PARS_SKIP) == 0))	/* Skip non-readers */
			p->tb[tbo++] = c;		/* Stash character away in token */

		if (p->q == 0	/* If not quoted and if token is non-empty and we have a terminator */
		 && tbo != 0 && (p->delf[c] & PARS_TERM) != 0)
			term = 1;									/* Finish token off next time around */
	} while (c != '\000');	/* Null means we've done the end of the token */
	p->q = 0;
	if (tbo <= 1) {
		p->token = 0;
#ifdef DEBUG
		printf("pars: read_token() got nothing useful\n");
#endif
		return NULL;		/* Haven't read anything useful */
	}
#ifdef DEBUG
	printf("pars: read_token() returning '%s'\n",p->tb);
#endif
	return p->tb;
}

/* ========================================================== */
/* Test code */

#ifdef STANDALONE_TEST
int
main() {
	int rc;
	parse *pp;
	cgatsFile *fp;

	if ((fp = new_cgatsFileStd_name("test.txt", "r")) == NULL)
		error("Failed to open file 'test.txt'");

	if ((pp = new_parse(fp)) == NULL)
		error("Failed to create parse object with file 'test.txt'");

	/* Setup our token parsing charaters */
	pp->add_del(pp, " ,\t"," ,\t", "#", "\"");

	for (;;) {
		char *tp;
		if ((rc = pp->read_line(pp)) == -1)
			error("%s",pp->err);
		if (rc == 0)
			break;
		printf("Line %d = '%s'\n",pp->line,pp->b);
		do {
			tp = pp->get_token(pp);
			if (pp->errc != 0)
				error("%s",pp->err);
			if (tp != NULL)
				printf("Token %d = '%s'\n",pp->token,tp);
			} while (tp != NULL);
		}
	printf("End of File\n");

	pp->del(pp);		/* Clean up */
	fp->del(fp);		/* Close the file */

	return 0;
}


/* Basic printf type error() and warning() routines for standalone test */
void
error(const char *fmt, ...) {
	va_list args;

	fprintf(stderr,"chart: Error - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit (-1);
}

void
warning(const char *fmt, ...) {
	va_list args;

	fprintf(stderr,"chart: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

#endif /* STANDALONE_TEST */
/* ---------------------------------------------------------- */
