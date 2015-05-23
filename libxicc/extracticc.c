
/* 
 * Extract an ICC profile from a TIFF file.
 *
 * Author:  Graeme W. Gill
 * Date:    2008/5/18
 * Version: 1.00
 *
 * Copyright 2008 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * TTBD:
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include "numlib.h"
#include "tiffio.h"
#include "jpeglib.h"
#include "iccjpeg.h"
#include "copyright.h"
#include "aconfig.h"
#include "icc.h"


void usage(char *diag, ...) {
	int i;
	fprintf(stderr,"Extract an ICC profile from a TIFF or JPEG file, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: extracticc  [-v] infile.tif/jpg outfile%s\n",ICC_FILE_EXT);
	fprintf(stderr," -v            Verbose\n");
	exit(1);
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
int
main(int argc, char *argv[]) {
	int fa,nfa;					/* argument we're looking at */
	char in_name[MAXNAMEL+1];	/* TIFF input name */
	char out_name[MAXNAMEL+1];	/* ICC output name */
	int verb = 0;
	TIFFErrorHandler olderrh, oldwarnh;
	TIFFErrorHandlerExt olderrhx, oldwarnhx;
	TIFF *rh;
	int  size = 0;
	void *buf = NULL;
	icmFile *fp;
	int rv = 0;
	
	error_program = argv[0];

	if (argc < 3)
		usage("Too few parameters");

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage(NULL);

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			else 
				usage("Unknown flag '%c'",argv[fa][1]);
		} else
			break;
	}

    if (fa >= argc || argv[fa][0] == '-') usage("Missing input TIFF file");
    strncpy(in_name,argv[fa++],MAXNAMEL); in_name[MAXNAMEL] = '\000';

    if (fa >= argc || argv[fa][0] == '-') usage("Missing output ICC profile");
    strncpy(out_name,argv[fa++],MAXNAMEL); out_name[MAXNAMEL] = '\000';

	/* - - - - - - - - - - - - - - - */
	/* Open up input tiff file ready for reading */
	/* Got arguments, so setup to process the file */
	olderrh = TIFFSetErrorHandler(NULL);
	oldwarnh = TIFFSetWarningHandler(NULL);
	olderrhx = TIFFSetErrorHandlerExt(NULL);
	oldwarnhx = TIFFSetWarningHandlerExt(NULL);
	if ((rh = TIFFOpen(in_name, "r")) != NULL) {
		TIFFSetErrorHandler(olderrh);
		TIFFSetWarningHandler(oldwarnh);
		TIFFSetErrorHandlerExt(olderrhx);
		TIFFSetWarningHandlerExt(oldwarnhx);

		if (TIFFGetField(rh, TIFFTAG_ICCPROFILE, &size, &buf) == 0 || size == 0) {
			error("unable to find ICC profile tag in TIFF file '%s'",in_name);
		}
	
		if ((fp = new_icmFileStd_name(out_name, "w")) == NULL) {
			error("unable to open output ICC profile '%s'",out_name);
		}
	
		if (fp->write(fp, buf, 1, size) != size) {
			error("error writing file '%s'",out_name);
		}
	
		if (fp->del(fp) != 0) {
			error("error closing file '%s'",out_name);
		}
	
		TIFFClose(rh);		/* Close Input file and free field buffer */

	} else {
		jpegerrorinfo jpeg_rerr;
		FILE *rf = NULL;
		struct jpeg_decompress_struct rj;
		struct jpeg_error_mgr jerr;

		TIFFSetErrorHandler(olderrh);
		TIFFSetWarningHandler(oldwarnh);
		TIFFSetErrorHandlerExt(olderrhx);
		TIFFSetWarningHandlerExt(oldwarnhx);

		/* We cope with the horrible ijg jpeg library error handling */
		/* by using a setjmp/longjmp. */
		jpeg_std_error(&jerr);
		jerr.error_exit = jpeg_error;
		if (setjmp(jpeg_rerr.env)) {
			jpeg_destroy_decompress(&rj);
			fclose(rf);
			error("error opening read file '%s'",in_name);
		}

		rj.err = &jerr;
		rj.client_data = &jpeg_rerr;
		jpeg_create_decompress(&rj);

#if defined(O_BINARY) || defined(_O_BINARY)
	    if ((rf = fopen(in_name,"rb")) == NULL)
#else
	    if ((rf = fopen(in_name,"r")) == NULL)
#endif
		{
			jpeg_destroy_decompress(&rj);
			error("error opening read file '%s'",in_name);
		}
		
		jpeg_stdio_src(&rj, rf);
		setup_read_icc_profile(&rj);

		/* we'll longjmp on error */
		jpeg_read_header(&rj, TRUE);

		if (!read_icc_profile(&rj, (unsigned char **)&buf, (unsigned int *)&size)) {
			jpeg_destroy_decompress(&rj);
			fclose(rf);
			error("unable to find ICC profile marker in JPEG file '%s'",in_name);
		}
		jpeg_destroy_decompress(&rj);
		fclose(rf);

		if ((fp = new_icmFileStd_name(out_name, "w")) == NULL) {
			error("unable to open output ICC profile '%s'",out_name);
		}
	
		if (fp->write(fp, buf, 1, size) != size) {
			error("error writing file '%s'",out_name);
		}
	
		if (fp->del(fp) != 0) {
			error("error closing file '%s'",out_name);
		}
		free(buf);
	}
	return 0;
}
