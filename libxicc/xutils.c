
/* 
 * xicc standalone utilities
 *
 * Author:  Graeme W. Gill
 * Date:    2/7/00
 * Version: 1.00
 *
 * Copyright 2000 - 2006 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/*
 * This module provides expanded capabilities,
 * but is independent of other modules.
 */

#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#ifdef __sun
#include <unistd.h>
#endif
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif
#include "copyright.h"
#include "aconfig.h"
#include "icc.h"
#include "tiffio.h"
#include "jpeglib.h"
#include "iccjpeg.h"
#include "xutils.h"		/* definitions for this library */


#undef DEBUG

#ifdef DEBUG
# define errout stderr
# define debug(xx)	fprintf(errout, xx )
# define debug2(xx)	fprintf xx
#else
# define debug(xx)
# define debug2(xx)
#endif

#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif

/* ------------------------------------------------------ */
/* Common clut table code */

/* Default table of clut resolutions */
/* See discussion in imdi/imdi_gen.c for ideal numbers */
static int lut_resolutions[9][4] = {
	/* low, med, high, vhigh */
	{ 0,     0,    0,    0 },		/* 0 */
	{ 256, 772, 4370, 4370 },		/* 1 */
	{  86, 256,  256,  256 },		/* 2 */
	{   9,  17,   33,   52 },		/* 3 */
	{   6,   9,   18,   33 },		/* 4 */
	{   6,   9,   16,   18 },		/* 5 */
	{   6,   6,    9,   12 },		/* 6 */
	{   6,   7,    7,    9 },		/* 7 */
	{   3,   5,    5,    7 }		/* 8 */
};


/* return a lut resolution given the input dimesion and quality */
/* Input dimension [0-8], quality: low 0, medium 1, high 2, very high 3 . */
/* A returned value of 0 indicates illegal.  */
int dim_to_clutres(int dim, int quality) {
	if (dim < 0)
		dim = 0;
	else if (dim > 8)
		dim = 8;
	if (quality < 0)
		quality = 0;
	if (quality > 3)
		quality = 3;
	return lut_resolutions[dim][quality];
}

/* ------------------------------------------------------ */

/* JPEG error information */
typedef struct {
	jmp_buf env;		/* setjmp/longjmp environment */
	char message[JMSG_LENGTH_MAX];
} jpegerrorinfo;

/* JPEG error handler */
static void jpeg_error(j_common_ptr cinfo) {  
	jpegerrorinfo *p = (jpegerrorinfo *)cinfo->client_data;
	(*cinfo->err->format_message) (cinfo, p->message);
	longjmp(p->env, 1);
}

/* ------------------------------------------------------ */

/* Open an ICC file or a TIFF or JPEG  file with an embedded ICC profile for reading. */
/* Return NULL on error */
icc *read_embedded_icc(char *file_name) {
	TIFF *rh = NULL;
	int  size;
	void *tag, *buf;
	icmAlloc *al;
	icmFile *fp;
	icc *icco;
	TIFFErrorHandler olderrh, oldwarnh;
	TIFFErrorHandlerExt olderrhx, oldwarnhx;
	int rv;

	/* First see if the file can be opened as an ICC profile */
	if ((fp = new_icmFileStd_name(file_name,"r")) == NULL) {
		debug2((errout,"Can't open file '%s'\n",file_name));
		return NULL;
	}

	if ((icco = new_icc()) == NULL) {
		debug("Creation of ICC object failed\n");
		fp->del(fp);
		return NULL;
	}

	if ((rv = icco->read_x(icco,fp,0,1)) == 0) {
		debug2((errout,"Opened '%s' as an icc profile\n",file_name));
		return icco;
	}

	debug2((errout,"icc read failed with %d, %s\n",rv,icco->err));
	icco->del(icco);		/* icc wil fp->del() */

	/* Not an ICC profile, see if it's a TIFF file */
	olderrh = TIFFSetErrorHandler(NULL);
	oldwarnh = TIFFSetWarningHandler(NULL);
	olderrhx = TIFFSetErrorHandlerExt(NULL);
	oldwarnhx = TIFFSetWarningHandlerExt(NULL);

	if ((rh = TIFFOpen(file_name, "r")) != NULL) {
		TIFFSetErrorHandler(olderrh);
		TIFFSetWarningHandler(oldwarnh);
		TIFFSetErrorHandlerExt(olderrhx);
		TIFFSetWarningHandlerExt(oldwarnhx);
		debug("TIFFOpen suceeded\n");

		if (TIFFGetField(rh, TIFFTAG_ICCPROFILE, &size, &tag) == 0 || size == 0) {
			debug2((errout,"no ICC profile found in '%s'\n",file_name));
			TIFFClose(rh);
			return NULL;
		}

		/* Make a copy of the profile to a memory buffer */
		if ((al = new_icmAllocStd()) == NULL) {
			debug("new_icmAllocStd failed\n");
			TIFFClose(rh);
		    return NULL;
		}
		if ((buf = al->malloc(al, size)) == NULL) {
			debug("malloc of profile buffer failed\n");
			al->del(al);
			TIFFClose(rh);
		    return NULL;
		}

		memmove(buf, tag, size);
		TIFFClose(rh);

	} else {
		jpegerrorinfo jpeg_rerr;
		FILE *rf = NULL;
		struct jpeg_decompress_struct rj;
		struct jpeg_error_mgr jerr;
		unsigned char *pdata;
		unsigned int plen;

		debug2((errout,"TIFFOpen failed for '%s'\n",file_name));
		TIFFSetErrorHandler(olderrh);
		TIFFSetWarningHandler(oldwarnh);
		TIFFSetErrorHandlerExt(olderrhx);
		TIFFSetWarningHandlerExt(oldwarnhx);

		/* We cope with the horrible ijg jpeg library error handling */
		/* by using a setjmp/longjmp. */
		jpeg_std_error(&jerr);
		jerr.error_exit = jpeg_error;
		if (setjmp(jpeg_rerr.env)) {
			debug2((errout,"jpeg_read_header failed for '%s'\n",file_name));
			jpeg_destroy_decompress(&rj);
			fclose(rf);
			return NULL;
		}

		rj.err = &jerr;
		rj.client_data = &jpeg_rerr;
		jpeg_create_decompress(&rj);

#if defined(O_BINARY) || defined(_O_BINARY)
	    if ((rf = fopen(file_name,"rb")) == NULL)
#else
	    if ((rf = fopen(file_name,"r")) == NULL)
#endif
		{
			debug2((errout,"fopen failed for '%s'\n",file_name));
			jpeg_destroy_decompress(&rj);
			return NULL;
		}
		
		jpeg_stdio_src(&rj, rf);
		setup_read_icc_profile(&rj);

		/* we'll longjmp on error */
		jpeg_read_header(&rj, TRUE);

		if (!read_icc_profile(&rj, &pdata, &plen)) {
			debug2((errout,"no ICC profile found in '%s'\n",file_name));
			jpeg_destroy_decompress(&rj);
			fclose(rf);
			return NULL;
		}
		jpeg_destroy_decompress(&rj);
		fclose(rf);

		/* Make a copy of the profile to a memory buffer */
		/* (icmAllocStd may not be the same as malloc ?) */
		if ((al = new_icmAllocStd()) == NULL) {
			debug("new_icmAllocStd failed\n");
		    return NULL;
		}
		if ((buf = al->malloc(al, plen)) == NULL) {
			debug("malloc of profile buffer failed\n");
			al->del(al);
			TIFFClose(rh);
		    return NULL;
		}
		memmove(buf, pdata, plen);
		size = (int)plen;
		free(pdata);
	}

	/* Memory File fp that will free the buffer when deleted: */
	if ((fp = new_icmFileMem_ad(buf, size, al)) == NULL) {
		debug("Creating memory file from CMProfileLocation failed");
		al->free(al, buf);
		al->del(al);
		return NULL;
	}

	if ((icco = new_icc()) == NULL) {
		debug("Creation of ICC object failed\n");
		fp->del(fp);	/* fp will delete al */
		return NULL;
	}

	if ((rv = icco->read_x(icco,fp,0,1)) == 0) {
		debug2((errout,"Opened '%s' embedded icc profile\n",file_name));
		return icco;
	}

	debug2((errout,"Failed to read '%s' embedded icc profile\n",file_name));
	icco->del(icco);	/* icco will delete fp and al */
	return NULL;
}

/* ------------------------------------------------------ */
























