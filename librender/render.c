
/* 
 * render2d
 *
 * Simple 2D raster rendering support.
 *
 * Author:  Graeme W. Gill
 * Date:    28/12/2005
 *
 * Copyright 2005, 2008, 2012 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * TTBD: Should make this much more self contained in how
 * it deals with errors - return an error code & string,
 * and clean up resourcse.
 */

#undef DEBUG
#undef CCTEST_PATTERN		/* Create ccast upsampling test pattern if */
							/* "ARGYLL_CCAST_TEST_PATTERN" env variable is set */
#undef TEST_SCREENING		/* For testing by making screen visible */

#define OVERLAP 0.1			/* Stocastic screening overlap between levels */

#define verbo stdout

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <fcntl.h>
#include "../h/copyright.h"
#include "../h/aconfig.h"
#include "../h/sort.h"
#include "../libnum/numlib.h"
#include "../tiff/libtiff/tiffio.h"
#include "../Tools/png/png.h"
#include "render.h"
#include "thscreen.h"

/* ------------------------------------------------------------- */
/* Utilities */

/* Standard L*a*b* to TIFF 8 bit CIELAB */
static void cvt_Lab_to_CIELAB8(double *out, double *in) {
	out[0] = in[0];
	if (out[0] < 0.0)
		out[0] = 0.0;
	else if (out[0] > 100.0)
		out[0] = 100.0;
	out[0] = out[0] / 100.0 * 255.0;

	out[1] = in[1];
	if (out[1] < -128.0)
		out[1] = -128.0;
	else if (out[1] > 127.0)
		out[1] = 127.0;
	if (out[1] < 0.0)
		out[1] = 256.0 + out[1];

	out[2] = in[2];
	if (out[2] < -128.0)
		out[2] = -128.0;
	else if (out[2] > 127.0)
		out[2] = 127.0;
	if (out[2] < 0.0)
		out[2] = 256.0 + out[2];
}

/* Standard L*a*b* to TIFF 16 bit CIELAB */
static void cvt_Lab_to_CIELAB16(double *out, double *in) {
	out[0] = in[0];
	if (out[0] < 0.0)
		out[0] = 0.0;
	else if (out[0] > 100.0)
		out[0] = 100.0;
	out[0] = out[0] / 100.0 * 65535.0;

	out[1] = in[1];
	if (out[1] < -32768.0)
		out[1] = -32768.0;
	else if (out[1] > 32767.0)
		out[1] = 32767.0;
	if (out[1] < 0.0)
		out[1] = 65536.0 + out[1];

	out[2] = in[2];
	if (out[2] < -32768.0)
		out[2] = -32768.0;
	else if (out[2] > 32767.0)
		out[2] = 32767.0;
	if (out[2] < 0.0)
		out[2] = 65536.0 + out[2];
}

/* ------------------------------------------------------------- */
/* PNG memory write support */
typedef struct {
	unsigned char *buf;
	png_size_t len;			/* Current length of the buffer */
	png_size_t off;			/* Current offset of the buffer */
} png_mem_info;

static void mem_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	png_mem_info *s = (png_mem_info *)png_get_io_ptr(png_ptr);

	if ((s->off + length) > s->len) {			/* Need more space */
		png_size_t more = (s->off + length) - s->len;

		if (more < (1024 * 80))
			more = 1024 * 50 - 32;		/* Increase 50K at a time */
		s->len += more;

		if ((s->buf = realloc(s->buf, s->len)) == NULL) {
			png_error(png_ptr, "malloc failed in mem_write_data");
		}
	}
	memcpy(s->buf + s->off, data, length);
	s->off += length;
}

static void mem_flush_data(png_structp png_ptr) {
	return;
}

/* ------------------------------------------------------------- */
#ifdef CCTEST_PATTERN

#define SG 9		// Spacing is 9 pixels
//#define SG 64		// 10 lines per half screen

static void test_value(render2d *s, tdata_t *outbuf, int xx, int yy) {
	int x = xx, y = yy;
	int i, j, v;
	int oval[3];	/* 8 bit value */

	if (x < s->pw/2) {
		if (y < s->ph/2) {
			/* Generate vertical white stripes */
			/* Stripes are 5 pixels apart in groups of 2 of the same level, */
			/* declining by 1 each group */
			if ((x % SG) == 0) {
				i = x / (2 * SG);
				v = 255 - i;
				if (v < 0)
					v = 0;
				oval[0] = oval[1] = oval[2] = v;
			} else {
				oval[0] = oval[1] = oval[2] = 0;
			}
		} else {
			y -= s->ph/2;
			/* Generate white dots */
			if ((x % SG) == 0 && (y % SG) == 0) {
				i = x / (2 * SG);
				j = y / (2 * SG);

				i += j * s->pw/2/(2 * SG);
				oval[0] = oval[1] = oval[2] = 0;

				for (j = 5; j >= 0; j--) {
					oval[0] = (oval[0] << 1) | ((i >> (j * 3 + 0)) & 1);
					oval[1] = (oval[1] << 1) | ((i >> (j * 3 + 1)) & 1);
					oval[2] = (oval[2] << 1) | ((i >> (j * 3 + 2)) & 1);
				}
				oval[0] = 255 - oval[0];
				oval[1] = 255 - oval[1];
				oval[2] = 255 - oval[2];
			} else {
				oval[0] = oval[1] = oval[2] = 0;
			}
		}
	} else {
		x -= s->pw/2;
		if (y < s->ph/2) {
			/* Generate horizontal white stripes */
			/* Stripes are 5 pixels apart in groups of 2 of the same level, */
			/* declining by 1 each group */
			if ((y % SG) == 0) {
				j = y / (2 * SG);
				v = 255 - j;
				oval[0] = oval[1] = oval[2] = v;
			} else {
				oval[0] = oval[1] = oval[2] = 0;
			}
		} else {
			y -= s->ph/2;
			/* Generate black dots */
			if ((x % SG) == 0 && (y % SG) == 0) {
				i = x / (2 * SG);
				j = y / (2 * SG);
				i += j * s->pw/2/(2 * SG);
				oval[0] = oval[1] = oval[2] = 0;

				for (j = 5; j >= 0; j--) {
					oval[0] = (oval[0] << 1) | ((i >> (j * 3 + 0)) & 1);
					oval[1] = (oval[1] << 1) | ((i >> (j * 3 + 1)) & 1);
					oval[2] = (oval[2] << 1) | ((i >> (j * 3 + 2)) & 1);
				}
			} else {
				oval[0] = oval[1] = oval[2] = 255;
			}
		}
	}

	if (s->dpth == bpc8_2d) {
		unsigned char *p = ((unsigned char *)outbuf) + xx * s->ncc;
		p[0] = oval[0];
		p[1] = oval[1];
		p[2] = oval[2];
	} else {
		unsigned short *p = ((unsigned short *)outbuf) + xx * s->ncc;
		p[0] = oval[0] * 256;
		p[1] = oval[1] * 256;
		p[2] = oval[2] * 256;
	}
}
#undef SG
#endif

/* ------------------------------------------------------------- */
/* Main class implementation */

/* Free ourselves and all primitives */
static void render2d_del(render2d *s) {
	prim2d *th, *nx;

	/* Delete all the primitives */
	for (th = s->head; th != NULL; th = nx) {
		nx = th->next;
		th->del(th);
	}

	/* And then ourselves */
	free(s);
}

/* Add a primitive */
static void render2d_add(render2d *s, prim2d *p) {
	if (p == NULL)
		error("render2d: Adding NULL primitive");

	p->next = s->head;
	s->head = p;
	p->ix = s->ix;
	s->ix++;
}

/* Set the default color */
static void render2d_set_defc(render2d *s, color2d c) {
	int j;

	for (j = 0; j < s->ncc; j++)
		s->defc[j] = c[j];
	s->defc[PRIX2D] = c[PRIX2D];
}


/* Set background color function */
static void render2d_set_bg_func(render2d *s,
	void (*func)(void *cntx, color2d c, double x, double y),
	void *cntx
) {
	s->bgfunc = func;
	s->cntx = cntx;
}

/* Compute the length of a double nul terminated string, including */
/* the nuls. */
static int zzstrlen(char *s) {
	int i;
	for (i = 0;; i++) {
		if (s[i] == '\000' && s[i+1] == '\000')
			return i+2;
	}
	return 0;
}

/* Decide whether a color is different enough to need anti-aliasing */
static int colordiff(render2d *s, color2d c1, color2d c2) {
	int j;

	for (j = 0; j < s->ncc; j++) {
		if (fabs(c1[j] - c2[j]) > 1e-5)
			return 1;
	}
	return 0;
}

#define MIXPOW 1.3
#define OSAMLS 16

/* Render and write to a TIFF or PNG file or memory buffer */
/* Return NZ on error */
static int render2d_write(
	render2d *s,
	char *filename,			/* Name of file for file output */
	int comprn,				/* nz to use compression */
	unsigned char **obuf,	/* pointer to returned buffer for mem output. Free after use */
	size_t *olen,			/* pointer to returned length of data in buffer */
	rend_format fmt			/* Output format, tiff/png, file/memory */
) {
	TIFF *wh = NULL;
	uint16 samplesperpixel = 0, bitspersample = 0;
	uint16 extrasamples = 0;	/* Extra "alpha" samples */
	uint16 extrainfo[MXCH2D];	/* Info about extra samples */
	uint16 photometric = 0;
	uint16 inkset = 0xffff;
	char *inknames = NULL;
	tdata_t *outbuf = NULL;

	FILE *png_fp = NULL;
	png_mem_info png_minfo = { NULL, 0, 0 };
	png_structp png_ptr = NULL;
	png_infop png_info = NULL;
	png_uint_32 png_width = 0, png_height = 0;
	int png_bit_depth = 0, png_color_type = 0;
	int png_samplesperpixel = 0;

	unsigned char *dithbuf16 = NULL;	/* 16 bit buffer for dithering */
	thscreens *screen = NULL;			/* dithering object */
	int foundfg;						/* Found a forground object in this line */
	prim2d *th, **pthp;
	prim2d **xlist, **ylist;	/* X, Y sorted start lists */
	int xli, yli;				/* Indexes into X, Y list */
	int noix;					/* Number in x list */
	color2d *pixv0, *_pixv0;	/* Storage for pixel values around current */
	color2d *pixv1, *_pixv1;
	sobol *so;					/* Random sampler for anti-aliasing */
	int i, j;

	double rx0, rx1, ry0, ry1;	/* Box being processed, newest sample is rx1, ry1 */
	int x, y;					/* Pixel x & y index */

#ifdef CCTEST_PATTERN		// For testing by making screen visible
#pragma message("######### render.c TEST_PATTERN defined ! ##")
	int do_test_pattern = 0;

	if (getenv("ARGYLL_CCAST_TEST_PATTERN") != NULL) {
		verbose(0, "Substituting ChromeCast test pattern\n");
		do_test_pattern = 1;
	} else { 
		static int verbed = 0;
		if (!verbed) {
			verbose(0, "Set ARGYLL_CCAST_TEST_PATTERN to enable test pattern\n");
			verbed = 1;
		}
	}
#endif

	if ((so = new_sobol(2)) == NULL)
		return 1;

	if (fmt == tiff_file) {
		switch (s->csp) {
			case w_2d:			/* Video style grey */
				samplesperpixel = 1;
				photometric = PHOTOMETRIC_MINISBLACK;
				break;
			case k_2d:			/* Printing style grey */
				samplesperpixel = 1;
				photometric = PHOTOMETRIC_MINISWHITE;
				break;
			case lab_2d:		/* TIFF CIE L*a*b* */
				samplesperpixel = 3;
				photometric = PHOTOMETRIC_CIELAB;
				break;
			case rgb_2d:		/* RGB */
				samplesperpixel = 3;
				photometric = PHOTOMETRIC_RGB;
				break;
			case cmyk_2d:		/* CMYK */
				samplesperpixel = 4;
				photometric = PHOTOMETRIC_SEPARATED;
				inkset = INKSET_CMYK;
				inknames = "cyan\000magenta\000yellow\000\000";
				break;
			case ncol_2d:		/* N color */
				samplesperpixel = s->ncc;
				extrasamples = 0;
				photometric = PHOTOMETRIC_SEPARATED;
				inkset = 0;			// ~~99 should fix this
				inknames = NULL;	// ~~99 should fix this
				break;
			case ncol_a_2d:		/* N color with extras in alpha */
				samplesperpixel = s->ncc;
				extrasamples = 0;
				if (samplesperpixel > 4) {
					extrasamples = samplesperpixel - 4;	/* Call samples > 4 "alpha" samples */
					for (j = 0; j < extrasamples; j++)
						extrainfo[j] = EXTRASAMPLE_UNASSALPHA;
				}
				photometric = PHOTOMETRIC_SEPARATED;
				inkset = 0;			// ~~99 should fix this
				inknames = NULL;	// ~~99 should fix this
				break;
			default:
				error("render2d: Illegal colorspace for TIFF file '%s'",filename);
		}
		if (samplesperpixel != s->ncc)
			error("render2d: mismatched number of color components");

		switch (s->dpth) {
			case bpc8_2d:		/* 8 bits per component */
				bitspersample = 8;
				break;
			case bpc16_2d:		/* 16 bits per component */
				bitspersample = 16;
				break;
			default:
				error("render2d: Illegal bits per component for TIFF file '%s'",filename);
		}

		if ((wh = TIFFOpen(filename, "w")) == NULL)
			error("render2d: Can\'t create TIFF file '%s'!",filename);
		
		TIFFSetField(wh, TIFFTAG_IMAGEWIDTH,  s->pw);
		TIFFSetField(wh, TIFFTAG_IMAGELENGTH, s->ph);
		TIFFSetField(wh, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		TIFFSetField(wh, TIFFTAG_SAMPLESPERPIXEL, samplesperpixel);
		TIFFSetField(wh, TIFFTAG_BITSPERSAMPLE, bitspersample);
		TIFFSetField(wh, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(wh, TIFFTAG_PHOTOMETRIC, photometric);
		if (extrasamples > 0)
			TIFFSetField(wh, TIFFTAG_EXTRASAMPLES, extrasamples, extrainfo);

		if (inknames != NULL) {
			int inlen = zzstrlen(inknames);
			TIFFSetField(wh, TIFFTAG_INKSET, inkset);
			TIFFSetField(wh, TIFFTAG_INKNAMES, inlen, inknames);
		}
		TIFFSetField(wh, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
		TIFFSetField(wh, TIFFTAG_RESOLUTIONUNIT, RESUNIT_CENTIMETER);
		TIFFSetField(wh, TIFFTAG_XRESOLUTION, 10.0 * s->hres);	/* Cvt. to pixels/cm */
		TIFFSetField(wh, TIFFTAG_YRESOLUTION, 10.0 * s->vres);
		TIFFSetField(wh, TIFFTAG_XPOSITION, 0.1 * s->lm);		/* Cvt. to cm */
		TIFFSetField(wh, TIFFTAG_YPOSITION, 0.1 * s->tm);
		if (comprn) {
			TIFFSetField(wh, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
		}
		TIFFSetField(wh, TIFFTAG_IMAGEDESCRIPTION, "Test chart created with Argyll");

		/* Allocate one TIFF line buffer */
		outbuf = _TIFFmalloc(TIFFScanlineSize(wh));

	} else if (fmt == png_file
	        || fmt == png_mem) {
		char *nmode = "w";

#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
		nmode = "wb";
#endif
		png_width = s->pw;
		png_height = s->ph;

		switch (s->dpth) {
			case bpc8_2d:		/* 8 bits per component */
				png_bit_depth = 8;
				break;
			case bpc16_2d:		/* 16 bits per component */
				png_bit_depth = 16;
				break;
			default:
				error("render2d: Illegal bits per component for PNG file '%s'",filename);
		}

		switch (s->csp) {
			case w_2d:			/* Video style grey */
				png_color_type = PNG_COLOR_TYPE_GRAY;
				png_samplesperpixel = 1;
				break;
			case rgb_2d:		/* RGB */
				png_color_type = PNG_COLOR_TYPE_RGB;
				png_samplesperpixel = 3;
				break;
			default:
				error("render2d: Illegal colorspace for PNG file '%s'",filename);
		}

		if (fmt == png_file) {
			if ((png_fp = fopen(filename, nmode)) == NULL)
				error("render2d: Can\'t create PNG file '%s'!",filename);
		}

		if ((png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		                                    NULL, NULL, NULL)) == NULL)
			error("render2d: png_create_write_struct failed");

		if (fmt == png_file) {
			png_init_io(png_ptr, png_fp);
		}

		if ((png_info = png_create_info_struct(png_ptr)) == NULL) {
			png_destroy_write_struct(&png_ptr, &png_info);
			error("render2d: png_create_info_struct failed");
		}

	   if (setjmp(png_jmpbuf(png_ptr))) {
			a1loge(g_log, 1, "%s -> %s: render2d libpng write error\n", filename);
			png_destroy_info_struct(png_ptr, &png_info);
			png_destroy_write_struct(&png_ptr, &png_info);
			if (png_fp != NULL)
				fclose(png_fp);
			else {
				free(png_minfo.buf);
			}
			return (1);
		}

		if (fmt == png_mem) {
			png_set_write_fn(png_ptr, &png_minfo, mem_write_data, mem_flush_data);  
		}

		png_set_IHDR(png_ptr, png_info, png_width, png_height, png_bit_depth,
			png_color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);


		/* pix/mm to pix/meter */
		png_set_pHYs(png_ptr, png_info, (png_uint_32)(1000.0 * s->hres + 0.5),
		                                (png_uint_32)(1000.0 * s->vres + 0.5),
		                                PNG_RESOLUTION_METER);
		
		/* mm to um */
		png_set_oFFs(png_ptr, png_info, (png_uint_32)(1000.0 * 0.1 * s->lm),  
		            (png_uint_32)(1000.0 * s->tm), PNG_OFFSET_MICROMETER);

		{
			png_text txt;
			txt.compression = PNG_TEXT_COMPRESSION_NONE;
			txt.key = "Description";
			txt.text = "Test chart created with Argyll";
			txt.text_length = strlen(txt.text);
#ifdef PNG_iTXt_SUPPORTED
			txt.itxt_length = 0;
			txt.lang = NULL;
			txt.lang_key = NULL;
#endif
			png_set_text(png_ptr, png_info, &txt, 1);
		}

		/* Write the header */
		png_write_info(png_ptr, png_info);

		/* PNG expects network (BE) 16 bit values */
		if (png_bit_depth == 16) {
			png_uint_16 tt = 0x0001;
			if (*((unsigned char *)&tt) == 0x01) 	/* Little endian */
				png_set_swap(png_ptr);
		}

		/* Allocate one PNG line buffer */
		if ((outbuf = malloc((png_bit_depth >> 3) * png_samplesperpixel * s->pw) ) == NULL)
			error("malloc of PNG line buffer failed");

	} else {
		error("render2d: Illegal output format %d",fmt);
	}

	/* Allocate pixel value storage for aliasing detection */
	if ((_pixv0 = malloc(sizeof(color2d) * (s->pw+2))) == NULL)
		return 1;
	pixv0 = _pixv0+1;
	if ((_pixv1 = malloc(sizeof(color2d) * (s->pw+2))) == NULL)
		return 1;
	pixv1 = _pixv1+1;

	if (s->dpth == bpc8_2d && s->dither) {
#ifdef TEST_SCREENING		// For testing by making screen visible
#pragma message("######### render TEST_SCREENING defined! ##")
# define LEVELS 4
		int i, olevs[LEVELS];
		for (i = 0; i < LEVELS; i++) {
			olevs[i] = (int)(i/(LEVELS-1.0) * 255.0 + 0.5);
		}
		if ((screen = new_thscreens(0, s->ncc, 1.0, 79, scie_16, 8, LEVELS, olevs,
		                           scoo_l, OVERLAP, s->pw, NULL, NULL,
		                           s->dither == 2 ? 1 : 0, s->quant, s->qcntx)) == NULL)
#else
		if ((screen = new_thscreens(0, s->ncc, 1.0, 79, scie_16, 8, 256, NULL,
		                           scoo_l, OVERLAP, s->pw, NULL, NULL,
		                           s->dither == 2 ? 1 : 0, s->quant, s->qcntx)) == NULL)
#endif
			return 1;
		if ((dithbuf16 = malloc(s->pw * s->ncc * 2)) == NULL)
			return 1;
	}

	/* To accelerate rendering, we keep sorted Y and X lists, */
	/* and Y and X active linked lists derived from them. */
	/* Typically this means that we're calling render on */
	/* none, 1 or a handful of the primitives, greatly speeding up */
	/* rendering. */

	/* Allocate X and Y ordered lists */
	if ((ylist = malloc(sizeof(prim2d *) * s->ix)) == NULL)
		return 1;
	if ((xlist = malloc(sizeof(prim2d *) * s->ix)) == NULL)
		return 1;

	/* Initialise the Y list */
	for (th = s->head, i = 0; th != NULL; th = th->next, i++)
		ylist[i] = th;
	
	/* Sort the Y lists by y1 (because we rasterise top to bottom) */
#define HEAP_COMPARE(A,B) (A->y1 > B->y1)
	HEAPSORT(prim2d *,ylist,s->ix)
#undef HEAP_COMPARE
	yli = 0;
	s->yl = NULL;

	/* Render each line and write it, in raster order. */
	/* We sample +- half a pixel around the pixel we want. */
	/* We make the active element list encompass this region, */
	/* so that we can super sample it for anti-aliasing. */
	for (y = -1; y < s->ph; y++) {
		foundfg = 0;

		/* Convert to coordinate order */
		ry0 = (((s->ph-1) - y) - 0.5) / s->vres;
		ry1 = (((s->ph-1) - y) + 0.5) / s->vres;

		/* Remove any objects from the y list that are now out of range */
		for (pthp = &s->yl, th = s->yl; th != NULL;) {
			if (ry1 < th->y0) {
				*pthp = th->yl;
				th = th->yl;
			} else {
				pthp = &th->yl;
				th = th->yl;
			}
		}

		/* Add any objects that are now within this range to our y list */
		for(; yli < s->ix && ry0 < ylist[yli]->y1; yli++) {
			ylist[yli]->yl = s->yl;
			s->yl = ylist[yli];
		}

		/* Initialise the current X list */
		for (th = s->yl, noix = 0; th != NULL; th = th->yl, noix++)
			xlist[noix] = th;
		
		/* Sort the X lists by x0 */
#define HEAP_COMPARE(A,B) (A->x0 < B->x0)
		HEAPSORT(prim2d *,xlist,noix)
#undef HEAP_COMPARE
		xli = 0;
		s->xl = NULL;

		for (x = -1; x < s->pw; x++) {
			int j;
			color2d rv;

			rx0 = (x - 0.5) / s->hres;
			rx1 = (x + 0.5) / s->hres;

			/* Add any objects that are now within this range to our x list */
			for(; xli < noix && rx1 > xlist[xli]->x0; xli++) {
				xlist[xli]->xl = s->xl;
				s->xl = xlist[xli];
			}

			/* Set the default current color */
			for (j = 0; j < s->ncc; j++)
				pixv1[x][j] = s->defc[j];
			pixv1[x][PRIX2D] = -1;			/* Make sure all primitive ovewrite the default */

			/* Allow callback to set per pixel background color (or not) */
			if (s->bgfunc != NULL)
				s->bgfunc(s->cntx, pixv1[x], x, y);

			/* Overwrite it with any primitives, */
			/* and remove any that are out of range now */
			for (pthp = &s->xl, th = s->xl; th != NULL;) {
				if (rx0 > th->x1) {
					*pthp = th->xl;
					th = th->xl;
				} else {
//printf("x %d y %d, rx1 %f, ry0 %f\n",x,y,rx1, ry0);

					if (th->rend(th, rv, rx1, ry0) && th->ix > pixv1[x][PRIX2D]) {
						/* Overwrite the current color */
						/* (This is where we should handle depth and opacity */
						for (j = 0; j < s->ncc; j++)
							pixv1[x][j] = rv[j];
						pixv1[x][PRIX2D] = rv[PRIX2D];
					}
					pthp = &th->xl;
					th = th->xl;
				}
			}
			/* Check if anti-aliasing is needed for previous lines previous pixel */
			if (y >= 0 && x >= 0) {
				color2d cc;

				for (j = 0; j < s->ncc; j++)
					cc[j] = pixv1[x][j];
				cc[PRIX2D] = pixv1[x][PRIX2D];

				/* See if anti aliasing is needed */
				if (!s->noavg
				 && ((pixv0[x+0][PRIX2D] != cc[PRIX2D] && colordiff(s, pixv0[x+0], cc))
				  || (pixv0[x-1][PRIX2D] != cc[PRIX2D] && colordiff(s, pixv0[x-1], cc))
				  || (pixv1[x-1][PRIX2D] != cc[PRIX2D] && colordiff(s, pixv1[x-1], cc)))) {
					double nn = 0;

					so->reset(so);

					for (j = 0; j < s->ncc; j++)
						cc[j] = 0.0;
					cc[PRIX2D] = -1;

					/* Compute the sample value by re-sampling the region */
					/* around the pixel. */
					for (nn = 0; nn < OSAMLS; nn++) {
						double pos[2];
						double rx, ry;
						color2d ccc;

						so->next(so, pos);

						rx = (rx1 - rx0) * pos[0] + rx0;
						ry = (ry1 - ry0) * pos[1] + ry0;

						/* Set the default current color */
						for (j = 0; j < s->ncc; j++)
							ccc[j] = s->defc[j];
						ccc[PRIX2D] = -1;

						for (th = s->xl; th != NULL; th = th->xl) {
							if (th->rend(th, rv, rx, ry) && th->ix > ccc[PRIX2D]) {
								/* Overwrite the current color */
								/* (This is where we should handle depth and opacity */
								for (j = 0; j < s->ncc; j++)
									ccc[j] = rv[j];
								ccc[PRIX2D] = rv[PRIX2D];
							}
						}
						for (j = 0; j < s->ncc; j++)
							cc[j] += pow(ccc[j], MIXPOW);
						if (ccc[PRIX2D] > cc[PRIX2D])
							cc[PRIX2D] = ccc[PRIX2D];	/* Note if not BG */
					}
					for (j = 0; j < s->ncc; j++)
						cc[j] = pow(cc[j]/nn, 1.0/MIXPOW);

#ifdef NEVER	/* Mark aliased pixels */
					cc[0] = 0.5;
					cc[1] = 0.0;
					cc[2] = 1.0;
#endif
				} else if (s->noavg) {
					/* Compute output value directly from primitive */

					for (j = 0; j < s->ncc; j++)
						cc[j] = cc[j];

				} else {

					/* Compute output value as mean of surrounding samples */
					for (j = 0; j < s->ncc; j++) {
						cc[j] = cc[j]
						      + pixv0[x-1][j]
						      + pixv0[x][j]
						      + pixv1[x-1][j];
						cc[j] = cc[j] * 0.25;
					}
					/* Note if not BG */
					if (pixv0[x-1][PRIX2D] > cc[PRIX2D])
						cc[PRIX2D] = pixv0[x-1][PRIX2D];
					if (pixv0[x][PRIX2D] > cc[PRIX2D])
						cc[PRIX2D] = pixv0[x][PRIX2D];
					if (pixv1[x-1][PRIX2D] > cc[PRIX2D])
						cc[PRIX2D] = pixv1[x-1][PRIX2D];
				}
				if (cc[PRIX2D] != -1)		/* Line is no longer background */
					foundfg = 1;

				/* Translate from render value to output pixel value */
				if (s->dpth == bpc8_2d) {
					/* if dithering and dithering all or found FG in line */
					if (s->dither) {
						unsigned short *p = ((unsigned short *)dithbuf16) + x * s->ncc;

						if (s->csp == lab_2d) {
							cvt_Lab_to_CIELAB16(cc, cc);
							for (j = 0; j < s->ncc; j++)
								p[j] = (int)(cc[j] + 0.5);
						} else {
							for (j = 0; j < s->ncc; j++)
								p[j] = (int)(65535.0 * cc[j] + 0.5);
						}
					} else {
						unsigned char *p = ((unsigned char *)outbuf) + x * s->ncc;
						if (s->csp == lab_2d) {
							cvt_Lab_to_CIELAB8(cc, cc);
							for (j = 0; j < s->ncc; j++)
								p[j] = (int)(cc[j] + 0.5);
						} else {
							for (j = 0; j < s->ncc; j++)
								p[j] = (int)(255.0 * cc[j] + 0.5);
						}
					}
				} else {
					unsigned short *p = ((unsigned short *)outbuf) + x * s->ncc;
					if (s->csp == lab_2d) {
						cvt_Lab_to_CIELAB16(cc, cc);
						for (j = 0; j < s->ncc; j++)
							p[j] = (int)(cc[j] + 0.5);
					} else {
						for (j = 0; j < s->ncc; j++)
							p[j] = (int)(65535.0 * cc[j] + 0.5);
					}
				}
			}
		}

		if (y >= 0) {
			/* if dithering and dithering all or found FG in line */
			if (s->dpth == bpc8_2d && s->dither) {
				// If we need to screen this line
				if (!s->dithfgo || foundfg) {
					/* If we are dithering only the foreground colors, */
					/* Subsitute the quantized un-dithered color for any */
					/* pixels soley from the background */
					if (s->dithfgo) {
						int st, ed;
						unsigned short *ip = ((unsigned short *)dithbuf16);
						unsigned char *op = ((unsigned char *)outbuf);

						/* Copy pixels up to first non-BG */
						for (x = 0; x < s->pw; x++, ip += s->ncc, op += s->ncc) {
							if (pixv1[x][PRIX2D] == -1) {
								for (j = 0; j < s->ncc; j++)
									op[j] = (ip[j] * 255 + 128)/65535;
							} else {
								st = x;
								break;
							}
						}
						if (x < s->pw) {	/* If there are FG pixels */

							/* Copy down to first non-BG */
							ip = ((unsigned short *)dithbuf16) + (s->pw-1) * s->ncc;
							op = ((unsigned char *)outbuf) + (s->pw-1) * s->ncc;
							for (x = s->pw-1; x > st; x--, ip -= s->ncc, op -= s->ncc) {
								if (pixv1[x][PRIX2D] == -1) {
									for (j = 0; j < s->ncc; j++)
										op[j] = (ip[j] * 255 + 128)/65535;
								} else {
									ed = x;
									break;
								}
							}
							/* Screen just the FG pixels */
							ip = ((unsigned short *)dithbuf16) + st * s->ncc;
							op = ((unsigned char *)outbuf) + st * s->ncc;
							screen->screen(screen, ed-st+1, 1, st, y,
						                       op, s->pw * s->ncc,
						                       (unsigned char*)ip, s->pw * s->ncc);
						}

					/* Dither/screen the whole lot */
					} else {
						screen->screen(screen, s->pw, 1, 0, y,
						                       (unsigned char *)outbuf, s->pw * s->ncc,
						                       dithbuf16, s->pw * s->ncc);
					}
				// Don't need to screen this line - quantize from 16 bit
				} else {
					unsigned short *ip = ((unsigned short *)dithbuf16);
					unsigned char *op = ((unsigned char *)outbuf);
					for (x = 0; x < s->pw; x++, ip += s->ncc, op += s->ncc) {
						for (j = 0; j < s->ncc; j++)
							op[j] = (ip[j] * 255 + 128)/65535;
					}
				}
			}

#ifdef CCTEST_PATTERN		// Substitute the testing pattern
			if (do_test_pattern) {
				for (x = 0; x < s->pw; x++)
					test_value(s, outbuf, x, y);
			}
#endif
			if (fmt == tiff_file) {
				if (TIFFWriteScanline(wh, outbuf, y, 0) < 0)
					error ("Failed to write TIFF file '%s' line %d",filename,y);
			} else if (fmt == png_file
			        || fmt == png_mem) {
				png_bytep pixdata = (png_bytep)outbuf;
				png_write_rows(png_ptr, &pixdata, 1);
			}
		}

		/* Shuffle the pointers */
		{
			color2d *ttt;
			ttt = pixv0;
			pixv0 = pixv1;
			pixv1 = ttt;
		}
	}

	free(ylist);
	free(xlist);
	free(_pixv0);
	free(_pixv1);

	if (dithbuf16 != NULL)
		free(dithbuf16);
	if (screen != NULL)
		screen->del(screen);

	if (fmt == tiff_file) {
		_TIFFfree(outbuf);
		TIFFClose(wh);		/* Close Output file */

	} else if (fmt == png_file
	        || fmt == png_mem) {

		free(outbuf);
		png_write_end(png_ptr, NULL);
//		png_destroy_info_struct(png_ptr, &png_info);
		png_destroy_write_struct(&png_ptr, &png_info);
		if (fmt == png_file) {
			fclose(png_fp);
		} else if (fmt == png_mem) {
			*obuf = png_minfo.buf;
			*olen = png_minfo.off;
		}
	}

	so->del(so);

	return 0;
}

/* Constructor */
render2d *new_render2d(
double w,		/* width in mm */
double h,		/* height in mm */
double ma[4],	/* Margines, left, right, top, bottom, NULL for zero in mm */
double hres,	/* horizontal resolution in pixels/mm */
double vres,	/* horizontal resolution in pixels/mm */
colort2d csp,	/* Color type */
int nd,			/* Number of channels if c = ncol */
depth2d dpth,	/* Pixel depth */
int dither,		/* Dither flag, 1 = ordered, 2 = error diffusion, | 0x8000 to dither FG only  */
void (*quant)(void *qcntx, double *out, double *in), /* optional quantization func. for edith */
void *qcntx
) {
	render2d *s;

	if ((s = (render2d *)calloc(1, sizeof(render2d))) == NULL) {
		return NULL;
	}

	s->fw = w;
	s->fh = h;
	if (ma != NULL) {
		s->lm = ma[0];
		s->rm = ma[1];
		s->tm = ma[2];
		s->bm = ma[3];
	}
	w = s->fw - s->lm - s->rm;
	h = s->fh - s->tm - s->bm;
	if (w < 0.0)
		error("render2d: Left & Right margines %f %f exceed width %f",s->lm,s->rm,s->fw); 
	if (h < 0.0)
		error("render2d: Top & Bottom margines %f %f exceed height %f",s->tm,s->bm,s->fh); 
	s->hres = hres;
	s->vres = vres;
	s->csp = csp;
	s->dpth = dpth;
	s->dither  = 0x0fff & dither;
	s->noavg   = 0x4000 & dither;
	s->dithfgo = 0x8000 & dither;
	s->quant = quant;
	s->qcntx = qcntx;

	s->del = render2d_del;
	s->set_defc = render2d_set_defc;
	s->set_bg_func = render2d_set_bg_func;
	s->add = render2d_add;
	s->write = render2d_write;

	/* Figure the raster size and actuall size */
	s->pw = (int)(s->hres * w + 0.5);
	s->ph = (int)(s->vres * h + 0.5);
	s->w = s->pw * s->hres;
	s->h = s->ph * s->vres;

	switch (s->csp) {
		case w_2d:			/* Video style grey */
		case k_2d:			/* Printing style grey */
			s->ncc = 1;
			break;
		case lab_2d:		/* TIFF CIE L*a*b* */
			s->ncc = 3;
			break;
		case rgb_2d:		/* RGB */
			s->ncc = 3;
			break;
		case cmyk_2d:		/* CMYK */
			s->ncc = 4;
			break;
		case ncol_2d:
		case ncol_a_2d:
			if (nd > MXCH2D)
				error("render2d: Too many color chanels %d, max is %d",nd,MXCH2D); 
			s->ncc = nd;
			break;
		default:
			error("render2d: Illegal colorspace");
	}
	return s;
}

/* ------------------------------------------------------------- */
/* Primitive implementations */

/* Primitive destructor for no sub-allocation */
static void prim2d_del(prim2d *s) {
	free(s);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Flat shaded rectangle */

/* Render the rectangle object at location. Return nz if in primitive */
static int rect2d_rend(prim2d *ss, color2d rv, double x, double y) {
	rect2d *s = (rect2d *)ss;
	int j;

	if (y < s->ry0 || y > s->ry1
	 || x < s->rx0 || x > s->rx1)
		return 0;

	if (s->dpat == NULL) {
		for (j = 0; j < s->ncc; j++)
			rv[j] = s->c[j];

	/* We have a dither pattern */
	} else {
		int xi = ((int)floor(x)) % s->dp_w;
		int yi = ((int)floor(y)) % s->dp_h;
		double *val = (*s->dpat)[xi][yi];
		for (j = 0; j < s->ncc; j++)
			rv[j] = val[j];
	}

	rv[PRIX2D] = s->ix;

	return 1;
}

static void rect2d_del(prim2d *ss) {
	rect2d *s = (rect2d *)ss;
	if (s->dpat != NULL)
		free(s->dpat);
	prim2d_del(ss);
}

prim2d *new_rect2d(
render2d *ss,
double x,
double y,
double w,
double h,
color2d c
) {
	int j;
	rect2d *s;

	if ((s = (rect2d *)calloc(1, sizeof(rect2d))) == NULL) {
		return NULL;
	}

	/* Account for margines */
	x -= ss->lm;
	y -= ss->bm;

	s->ncc = ss->ncc;
	s->del = rect2d_del; 
	s->rend = rect2d_rend; 

	/* Set bounding box */
	s->x0 = x;
	s->y0 = y;
	s->x1 = x + w;
	s->y1 = y + h;

	/* Set rectangle extent */
	s->rx0 = s->x0;
	s->ry0 = s->y0;
	s->rx1 = s->x1;
	s->ry1 = s->y1;

	for (j = 0; j < s->ncc; j++)
		s->c[j] = c[j];

	return (prim2d *)s;
}

/* Allocate pat using malloc(sizeof(double) * MXPATSIZE * MXPATSIZE * TOTC2D) */
void set_rect2d_dpat(rect2d *s, double (*pat)[MXPATSIZE][MXPATSIZE][TOTC2D], int w, int h) {
	s->dpat = pat;
	s->dp_w = w;
	s->dp_h = h;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Vertex shaded rectangle */

/* Render the rectangle object at location. Return nz if in primitive */
static int rectvs2d_rend(prim2d *ss, color2d rv, double x, double y) {
	rectvs2d *s = (rectvs2d *)ss;
	double bx, by, b[4];
	int i, j;

	if (y < s->ry0 || y > s->ry1
	 || x < s->rx0 || x > s->rx1)
		return 0;

	/* Compute linear blend */
	bx = (x - s->x0)/(s->x1 - s->x0);
	by = (y - s->y0)/(s->y1 - s->y0);

	if (s->x_blend == 1) {
		bx = bx * bx * (3.0 - 2.0 * bx);	/* Cubic spline */
	} else if (s->x_blend == 2) {
		bx = bx - 0.5;						/* Sine */
		bx *= 3.141592654;
		bx = sin(bx);
		bx = 0.5 + (0.5 * bx);
	}
	if (s->y_blend == 1) {
		by = by * by * (3.0 - 2.0 * by);	/* Spline */
	} else if (s->y_blend == 2) {
		double ty;
		ty = by * by * (3.0 - 2.0 * by);	/* spline at y == 1, linear at y == 0 */
		by = by * ty + (1.0 - by) * by;
	} else if (s->y_blend == 3) {
		double ty;
		ty = by * by * (3.0 - 2.0 * by);	/* linear at y == 1, spline at y == 0 */
		by = (1.0 - by) * ty + by * by;
	}

	/* Compute 2d blend */
	b[0] = (1.0 - by) * (1.0 - bx);
	b[1] = (1.0 - by) * bx;
	b[2] = by * (1.0 - bx);
	b[3] = by * bx;

	/* Compute the resulting color */
	for (j = 0; j < s->ncc; j++) {
		rv[j] = 0.0;
		for (i = 0; i < 4; i++)
			rv[j] += b[i] * s->c[i][j];
		rv[j] = rv[j];
	}
	rv[PRIX2D] = s->ix;

	return 1;
}

prim2d *new_rectvs2d(
render2d *ss,
double x,
double y,
double w,
double h,
color2d c[4]
) {
	int i, j;
	rectvs2d *s;

	if ((s = (rectvs2d *)calloc(1, sizeof(rectvs2d))) == NULL) {
		return NULL;
	}

	/* Account for margines */
	x -= ss->lm;
	y -= ss->bm;

	s->ncc = ss->ncc;
	s->del = prim2d_del; 
	s->rend = rectvs2d_rend; 

	/* Set bounding box */
	s->x0 = x;
	s->y0 = y;
	s->x1 = x + w;
	s->y1 = y + h;

	/* Set rectangle extent */
	s->rx0 = s->x0;
	s->ry0 = s->y0;
	s->rx1 = s->x1;
	s->ry1 = s->y1;

	for (i = 0; i < 4; i++)
		for (j = 0; j < s->ncc; j++)
			s->c[i][j] = c[i][j];

	return (prim2d *)s;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Vertex shaded triangle */

/* Render the triangle object at location. Return nz if in primitive */
static int trivs2d_rend(prim2d *ss, color2d rv, double x, double y) {
	trivs2d *s = (trivs2d *)ss;
	double b[3];
	int i, j;

	/* Compute the baricentric values for the input point, */
	/* and reject this point if it is outside the triangle */
	for (i = 0; i < 3; i++) {
		b[i] = s->be[i][0] * x + s->be[i][1] * y + s->be[i][2]; 
		if (b[i] < 0.0 || b[i] > 1.0)
			return 0;
	}

#ifdef NEVER		/* Experiment with smoothing hue blending */
	{
		double ss = b[1] + b[2];
		if (ss > 1e-6) {
			b[1] /= ss;
			b[2] /= ss;
//			b[1] = b[1] * b[1] * (3.0 - 2.0 * b[1]);

			b[1] = b[1] - 0.5;
			b[1] *= 3.141592654;
			b[1] = sin(b[1]);
			b[1] = 0.5 + (0.5 * b[1]);

			b[2] = 1.0 - b[1];
			b[1] *= ss;
			b[2] *= ss;
		}
	}
#endif

	/* Compute the resulting color */
	for (j = 0; j < s->ncc; j++) {
		rv[j] = 0.0;
		for (i = 0; i < 3; i++)
			rv[j] += b[i] * s->c[i][j];
	}
	rv[PRIX2D] = s->ix;

	return 1;
}

static int inverse3x3(double out[3][3], double in[3][3]);

prim2d *new_trivs2d(
render2d *ss,
double v[3][2],			/* Vertex locations */
color2d c[3]			/* Corresponding colors */
) {
	int i, j;
	trivs2d *s;
	double vv[3][2];	/* Margin adjusted vertex locations */
	double tt[3][3];

	if ((s = (trivs2d *)calloc(1, sizeof(trivs2d))) == NULL) {
		return NULL;
	}

	/* Account for margines */
	for (i = 0; i < 3; i++) {
		vv[i][0] = v[i][0] - ss->lm;
		vv[i][1] = v[i][1] - ss->bm;
	}

	s->ncc = ss->ncc;
	s->del = prim2d_del; 
	s->rend = trivs2d_rend; 

	/* Set the bounding box values */
	s->x0 = s->y0 = 1e38;	
	s->x1 = s->y1 = -1e38;	
	for (i = 0; i < 3; i++) {
		if (vv[i][0] < s->x0)
			s->x0 = vv[i][0];
		if (vv[i][1] < s->y0)
			s->y0 = vv[i][1];
		if (vv[i][0] > s->x1)
			s->x1 = vv[i][0];
		if (vv[i][1] > s->y1)
			s->y1 = vv[i][1];
	}

	/* Compute baricentric equations */
	for (i = 0; i < 3; i++) {
		tt[0][i] = vv[i][0];
		tt[1][i] = vv[i][1];
		tt[2][i] = 1.0;
	}
	if (inverse3x3(s->be, tt))
		error("trivs2d: Matrix inversion failed");

	/* Copy vertex colors */
	for (i = 0; i < 3; i++) {
		for (j = 0; j < s->ncc; j++)
			s->c[i][j] = c[i][j];
	}

	return (prim2d *)s;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* A single Rounded end line. Mainly for supporting Hershey text output. */

/* Render the line object at this location */
static int line2d_rend(prim2d *ss, color2d rv, double x, double y) {
	line2d *s = (line2d *)ss;
	double rr, px, py;
	int j;

	px = x - s->lx0;		/* Input relative to x0 y0 */
	py = y - s->ly0;

	/* It's actually a point */
	if (s->t) {

		rr = (px * px) + (py * py) ;

		if (s->cap != 1)
			return 0;
		if (rr > s->ww)
			return 0;

	} else {
		/* Locate point on line between 0 and 1 closest to input point */
		double t, nx, ny;

		/* Closest point on line parametric value */
		t = (s->vx * px + s->vy * py)/(s->vx * s->vx + s->vy * s->vy);

		if (t < 0.0) {		/* 0 side end cap */
			if (s->cap != 1)
				return 0;
			rr = (px * px) + (py * py) ;
			if (rr > s->ww)
				return 0;	/* 1 side end cap */

		} else if (t > 1.0) {
			if (s->cap != 1)
				return 0;
			rr = (x - s->lx1) * (x - s->lx1)
			   + (y - s->ly1) * (y - s->ly1);
			if (rr > s->ww)
				return 0;

		} else {	/* Within rectangular part of line */
			double nx, ny;

			nx = t * s->vx;		/* Closest point on line relative to x0 y0 */
			ny = t * s->vy;

			rr = (px - nx) * (px - nx) + (py - ny) * (py - ny);
			if (rr > s->ww)
				return 0;
		}
	}

	for (j = 0; j < s->ncc; j++)
		rv[j] = s->c[j];
	rv[PRIX2D] = s->ix;

	return 1;
}

prim2d *new_line2d(
render2d *ss,
double x0, double y0, double x1, double y1,
double w,
int cap, 	/* 0 = butt, 1 = round, 2 = square */
color2d c
) {
	int i, j;
	line2d *s;
	double tt[3][3];

	if ((s = (line2d *)calloc(1, sizeof(line2d))) == NULL) {
		return NULL;
	}

	/* Account for margines */
	x0 -= ss->lm;
	y0 -= ss->bm;
	x1 -= ss->lm;
	y1 -= ss->bm;

	s->ncc = ss->ncc;
	s->del = prim2d_del; 
	s->rend = line2d_rend; 

	w *= 0.5;		/* half width */

	for (j = 0; j < s->ncc; j++)
		s->c[j] = c[j];

	/* Compute the line vector relative to x0 y0 */
	s->vx = x1 - x0;
	s->vy = y1 - y0;

	/* For square, extend the line by w/2 and render as butt */
	if (cap == 2) {
		double nvx, nvy;
		double ll = sqrt(s->vx * s->vx + s->vy * s->vy);

		/* Compute normalized vector */
		if (ll < 1e-6) {
			nvx = 1.0;
			nvy = 0.0;
		} else {
			nvx = s->vx/ll;
			nvy = s->vy/ll;
		}
		x0 -= w * nvx;
		y0 -= w * nvy;
		x1 += w * nvx;
		y1 += w * nvy;
		s->vx = x1 - x0;
		s->vy = y1 - y0;
	}

	s->lx0 = x0;
	s->ly0 = y0;
	s->lx1 = x1;
	s->ly1 = y1;
	s->ww = w * w;
	s->cap = cap;

	/* Set the bouding box values */
	if (x1 > x0) {
		s->x1 = x1 + w;
		s->x0 = x0 - w;
	} else {
		s->x1 = x0 + w;
		s->x0 = x1 - w;
	}
	if (y1 > y0) {
		s->y1 = y1 + w;
		s->y0 = y0 - w;
	} else {
		s->y1 = y0 + w;
		s->y0 = y1 - w;
	}

	/* Figure out if the line is degenerate */
	if (fabs(s->vx) < 1e-6 && fabs(s->vy) < 1e-6) {
		s->t = 1;
	}

	return (prim2d *)s;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Primitive Macros. These shapes are composed of */
/* underlying primitives */

/* add a dashed line */
void add_dashed_line2d(
struct _render2d *s,
double x0, double y0,
double x1, double y1,
double w,
double on, double off,
int cap,
color2d c) {
	double ll, rl;		/* Line length, rounded length */
	double vx, vy;
	double nvx, nvy;	/* Normalized vector */

	vx = x1 - x0;
	vy = y1 - y0;
	ll = sqrt(vx * vx + vy * vy);

	if (ll < 1e-6) {
		s->add(s, new_line2d(s, x0, y0, x1, y1, w, cap, c));
		return ;
	}
	nvx = vx/ll;
	nvy = vy/ll;
	
	for (;ll > 0.0;) {
		rl = ll;	
		if (rl > on) {
			rl = on;
		}
		x1 = x0 + rl * nvx;
		y1 = y0 + rl * nvy;
		s->add(s, new_line2d(s, x0, y0, x1, y1, w, cap, c));
		x0 = x1;
		y0 = y1;
		ll -= rl;
		if (ll <= 0.0)
			return;
		rl = ll;	
		if (rl > off) {
			rl = off;
		}
		x0 += rl * nvx;
		y0 += rl * nvy;
		ll -= rl;
	}
}


/* Text */
struct _hyfont {
	char *name;
	double sx, sy;		/* X and Y scaling */
	double sxi;			/* X spacing extra scale */
	double sw;			/* Line width scale */
	double ho;			/* horizontal alignment offset */
	double vo;			/* vertical alignment offset */
	char *enc[128];		/* Hershey encoded font */
}; typedef struct _hyfont hyfont;

hyfont fonts[];

double h2dbl(unsigned char c) { return (double)(c-'R'); }

/* Add a text character at the given location using lines */
static void add_char_imp(
render2d *s,
double *xinc,		/* Add increment to next character */
double *yinc,
font2d fo,			/* Font to use */
char ch,			/* Character code to be printed */
double x, double y,	/* Location of bottom left of normal orientation text */
double h,			/* Height of text in normal orientation */
int or,				/* Orintation, 0 = right, 1 = down, 2 = left, 3 = right */
color2d c,			/* Color of text */
int add				/* NZ if character is to be added */
) {
	hyfont *fp = &fonts[fo];
	char *cp = fp->enc[ch % 128];
	double lm, rm;
	double x0, y0, x1 = 0.0, y1 = 0.0;
	int got1 = 0;
	double w = fp->sw * h;
	double tx, ty;
	double mat[2][2];		/* Transformation matrix */

	if (or == 0) {
		mat[0][0] = 1.0; 
		mat[0][1] = 0.0; 
		mat[1][0] = 0.0; 
		mat[1][1] = 1.0; 
	} else if (or == 1) {
		mat[0][0] = 0.0; 
		mat[0][1] = 1.0; 
		mat[1][0] = -1.0; 
		mat[1][1] = 0.0; 
	} else if (or == 2) {
		mat[0][0] = -1.0; 
		mat[0][1] = 0.0; 
		mat[1][0] = 0.0; 
		mat[1][1] = -1.0; 
	} else {
		mat[0][0] = 0.0; 
		mat[0][1] = -1.0; 
		mat[1][0] = 1.0; 
		mat[1][1] = 0.0; 
	}
	if (cp[0] == '\000' || cp[1] == '\000') {
		if (xinc != NULL && yinc != NULL) {
			*xinc = 0.0;
			*yinc = 0.0;
		}
		return;
	}

	lm = h2dbl(cp[0]);
	rm = h2dbl(cp[1]);
	cp += 2;

	if (add) {
		for (;; cp += 2) {
			if (cp[0] == '\000' || cp[1] == '\000') {
				break;
			}
			/* End of polyline */
			if (cp[0] == ' ' && cp[1] == 'R' ) {
				got1 = 0;
			} else {
				x0 = x1;
				y0 = y1;
				tx = h * fp->sx * (h2dbl(cp[0]) -lm + fp->ho);
   				ty = -h * fp->sy * (h2dbl(cp[1]) -fp->vo);
				x1 = x + mat[0][0] * tx + mat[0][1] * ty;
				y1 = y + mat[1][0] * tx + mat[1][1] * ty;
				if (got1) {
					s->add(s, new_line2d(s, x0, y0, x1, y1, w, 1, c));
				}
				got1 = 1;
			}
		}
	}

	tx = fp->sxi * h * fp->sx * (rm - lm);
	ty = 0.0;
	if (xinc != NULL)
		*xinc += mat[0][0] * tx + mat[0][1] * ty;
	if (yinc != NULL)
		*yinc += mat[1][0] * tx + mat[1][1] * ty;
}

/* Add a text character at the given location using lines */
void add_char2d(
render2d *s,
double *xinc,		/* Add increment to next character */
double *yinc,
font2d fo,			/* Font to use */
char ch,			/* Character code to be printed */
double x, double y,	/* Location of bottom left of normal orientation text */
double h,			/* Height of text in normal orientation */
int or,				/* Orintation, 0 = right, 1 = down, 2 = left, 3 = right */
color2d c			/* Color of text */
) {
	add_char_imp(s, xinc, yinc, fo, ch, x, y, h, or, c, 1);
}

/* Return the total width of the character without adding it */
void meas_char2d(
render2d *s,
double *xinc,		/* Add increment to next character */
double *yinc,
font2d fo,			/* Font to use */
char ch,			/* Character code to be printed */
double h,			/* Height of text in normal orientation */
int or				/* Orintation, 0 = right, 1 = down, 2 = left, 3 = right */
) {
	color2d c;

	add_char_imp(s, xinc, yinc, fo, ch, 0.0, 0.0, h, or, c, 0);
}

/* Add a string from the given location using lines. */
/* Return the total width of the string  */
void add_string2d(
render2d *s,
double *xinc,		/* Add increment to next character */
double *yinc,
font2d fo,			/* Font to use */
char *string,		/* Character code to be printed */
double x, double y,	/* Location of bottom left of normal orientation text */
double h,			/* Height of text in normal orientation */
int or,				/* Orintation, 0 = right, 1 = down, 2 = left, 3 = right */
color2d c			/* Color of text */
) {
	char *ch;
	double xoff = 0.0, yoff = 0.0;

	for (ch = string; *ch != '\000'; ch++) {
		add_char2d(s, &xoff, &yoff, fo, *ch, x + xoff, y + yoff, h, or, c);
	}

	if (xinc != NULL)
		*xinc = xoff;
	if (yinc != NULL)
		*yinc = yoff;
}

/* Return the total width of the string without adding it */
void meas_string2d(
struct _render2d *s,
double *xinc,		/* Add increment to next character */
double *yinc,
font2d fo,			/* Font to use */
char *string,		/* Character code to be printed */
double h,			/* Height of text in normal orientation */
int or				/* Orintation, 0 = right, 1 = down, 2 = left, 3 = right */
) {
	char *ch;
	double xoff = 0.0, yoff = 0.0;

	for (ch = string; *ch != '\000'; ch++) {
		meas_char2d(s, &xoff, &yoff, fo, *ch, h, or);
	}

	if (xinc != NULL)
		*xinc = xoff;
	if (yinc != NULL)
		*yinc = yoff;
}

/* ==================================================== */
/* Misc. support functions.                      */

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
 * double = det3x3(  a1, a2, a3, b1, b2, b3, c1, c2, c3 )
 * 
 * calculate the determinant of a 3x3 matrix
 * in the form
 *
 *     | a1,  b1,  c1 |
 *     | a2,  b2,  c2 |
 *     | a3,  b3,  c3 |
 */

static double det3x3(double in[3][3]) {
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

#define SMALL_NUMBER	1.e-8
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
static int inverse3x3(
double out[3][3],
double in[3][3]
) {
    int i, j;
    double det;

    /*  calculate the 3x3 determinant
     *  if the determinant is zero, 
     *  then the inverse matrix is not unique.
     */
    det = det3x3(in);

    if ( fabs(det) < SMALL_NUMBER)
        return 1;

    /* calculate the adjoint matrix */
    adjoint(out, in);

    /* scale the adjoint matrix to get the inverse */
    for (i = 0; i < 3; i++)
        for(j = 0; j < 3; j++)
		    out[i][j] /= det;
	return 0;
}

/* ==================================================== */
//   Hershey.C
//   extracted from the hershey font
//   
//   Charles Schwieters 6/14/99
//   Various tweaks and updates by John Stone
//
//   font info:
//
//Peter Holzmann, Octopus Enterprises
//USPS: 19611 La Mar Court, Cupertino, CA 95014
//UUCP: {hplabs!hpdsd,pyramid}!octopus!pete
//Phone: 408/996-7746
//
//This distribution is made possible through the collective encouragement
//of the Usenet Font Consortium, a mailing list that sprang to life to get
//this accomplished and that will now most likely disappear into the mists
//of time... Thanks are especially due to Jim Hurt, who provided the packed
//font data for the distribution, along with a lot of other help.
//
//This file describes the Hershey Fonts in general, along with a description of
//the other files in this distribution and a simple re-distribution restriction.
//
//USE RESTRICTION:
//        This distribution of the Hershey Fonts may be used by anyone for
//        any purpose, commercial or otherwise, providing that:
//                1. The following acknowledgements must be distributed with
//                        the font data:
//                        - The Hershey Fonts were originally created by Dr.
//                                A. V. Hershey while working at the U. S.
//                                National Bureau of Standards.
//                        - The format of the Font data in this distribution
//                                was originally created by
//                                        James Hurt
//                                        Cognition, Inc.
//                                        900 Technology Park Drive
//                                        Billerica, MA 01821
//                                        (mit-eddie!ci-dandelion!hurt)
//                2. The font data in this distribution may be converted into
//                        any other format *EXCEPT* the format distributed by
//                        the U.S. NTIS (which organization holds the rights
//                        to the distribution and use of the font data in that
//                        particular format). Not that anybody would really
//                        *want* to use their format... each point is described
//                        in eight bytes as "xxx yyy:", where xxx and yyy are
//                        the coordinate values as ASCII numbers.
//

/*
 *  The Hershey romans font in ascii order (first 32 places held by space)
 *  NOTE: This font has been modified to yield fixed-width numeric 
 *        characters.  This makes it possible to produce justified numeric 
 *        text that correctly lines up in columns.
 *        The font was specifically changed for:
 *        ' ' (ascii 32), '+' (ascii 43), '-' (ascii 45), and '.' (ascii 46)
 */

hyfont fonts[] = {
{
	"Rowman Single",
	0.040,		/* X scale */
	0.040,		/* Y scale */
	0.95,		/* X spacing extra scale */
	0.08,		/* Line width scale */
	1.1,		/* horizontal offset */
	11.0,		/* vertical offset */
	{
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"MWRFRT RRYQZR[SZRY",
		"JZNFNM RVFVM",
		"H]SBLb RYBRb RLOZO RKUYU",
		"H\\PBP_ RTBT_ RYIWGTFPFMGKIKKLMMNOOUQWRXSYUYXWZT[P[MZKX",
		"F^[FI[ RNFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F RWTUUTWTYV[X[ZZ[X[VYTWT",
		"E_\\O\\N[MZMYNXPVUTXRZP[L[JZIYHWHUISJRQNRMSKSIRGPFNGMIMKNNPQUXWZY[",
		"MWRHQGRFSGSIRKQL",
		"KYVBTDRGPKOPOTPYR]T`Vb",
		"KYNBPDRGTKUPUTTYR]P`Nb",
		"JZRLRX RMOWU RWOMU",
		"E_RIR[ RIR[R",
		"NVSWRXQWRVSWSYQ[",
		"E_IR[R",
		"NVRVQWRXSWRV",
		"G][BIb",
		"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF",
		"H\\NJPISFS[",
		"H\\LKLJMHNGPFTFVGWHXJXLWNUQK[Y[",
		"H\\MFXFRNUNWOXPYSYUXXVZS[P[MZLYKW",
		"H\\UFKTZT RUFU[",
		"H\\WFMFLOMNPMSMVNXPYSYUXXVZS[P[MZLYKW",
		"H\\XIWGTFRFOGMJLOLTMXOZR[S[VZXXYUYTXQVOSNRNOOMQLT",
		"H\\YFO[ RKFYF",
		"H\\PFMGLILKMMONSOVPXRYTYWXYWZT[P[MZLYKWKTLRNPQOUNWMXKXIWGTFPF",
		"H\\XMWPURRSQSNRLPKMKLLINGQFRFUGWIXMXRWWUZR[P[MZLX",
		"NVROQPRQSPRO RRVQWRXSWRV",
		"NVROQPRQSPRO RSWRXQWRVSWSYQ[",
		"F^ZIJRZ[",
		"E_IO[O RIU[U",
		"F^JIZRJ[",
		"I[LKLJMHNGPFTFVGWHXJXLWNVORQRT RRYQZR[SZRY",
		"E`WNVLTKQKOLNMMPMSNUPVSVUUVS RQKOMNPNSOUPV RWKVSVUXVZV\\T]Q]O\\L[J",
		"I[RFJ[ RRFZ[ RMTWT",
		"G\\KFK[ RKFTFWGXHYJYLXNWOTP RKPTPWQXRYTYWXYWZT[K[",
		"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZV",
		"G\\KFK[ RKFRFUGWIXKYNYSXVWXUZR[K[",
		"H[LFL[ RLFYF RLPTP RL[Y[",
		"HZLFL[ RLFYF RLPTP",
		"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZVZS RUSZS",
		"G]KFK[ RYFY[ RKPYP",
		"NVRFR[",
		"JZVFVVUYTZR[P[NZMYLVLT",
		"G\\KFK[ RYFKT RPOY[",
		"HYLFL[ RL[X[",
		"F^JFJ[ RJFR[ RZFR[ RZFZ[",
		"G]KFK[ RKFY[ RYFY[",
		"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF",
		"G\\KFK[ RKFTFWGXHYJYMXOWPTQKQ",
		"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF RSWY]",
		"G\\KFK[ RKFTFWGXHYJYLXNWOTPKP RRPY[",
		"H\\YIWGTFPFMGKIKKLMMNOOUQWRXSYUYXWZT[P[MZKX",
		"JZRFR[ RKFYF",
		"G]KFKULXNZQ[S[VZXXYUYF",
		"I[JFR[ RZFR[",
		"F^HFM[ RRFM[ RRFW[ R\\FW[",
		"H\\KFY[ RYFK[",
		"I[JFRPR[ RZFRP",
		"H\\YFK[ RKFYF RK[Y[",
		"KYOBOb RPBPb ROBVB RObVb",
		"KYKFY^",
		"KYTBTb RUBUb RNBUB RNbUb",
		"JZRDJR RRDZR",
		"I[Ib[b",
		"NVSKQMQORPSORNQO",
		"I\\XMX[ RXPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"H[LFL[ RLPNNPMSMUNWPXSXUWXUZS[P[NZLX",
		"I[XPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"I\\XFX[ RXPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"I[LSXSXQWOVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"MYWFUFSGRJR[ ROMVM",
		"I\\XMX]W`VaTbQbOa RXPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"I\\MFM[ RMQPNRMUMWNXQX[",
		"NVQFRGSFREQF RRMR[",
		"MWRFSGTFSERF RSMS^RaPbNb",
		"IZMFM[ RWMMW RQSX[",
		"NVRFR[",
		"CaGMG[ RGQJNLMOMQNRQR[ RRQUNWMZM\\N]Q][",
		"I\\MMM[ RMQPNRMUMWNXQX[",
		"I\\QMONMPLSLUMXOZQ[T[VZXXYUYSXPVNTMQM",
		"H[LMLb RLPNNPMSMUNWPXSXUWXUZS[P[NZLX",
		"I\\XMXb RXPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"KXOMO[ ROSPPRNTMWM",
		"J[XPWNTMQMNNMPNRPSUTWUXWXXWZT[Q[NZMX",
		"MYRFRWSZU[W[ ROMVM",
		"I\\MMMWNZP[S[UZXW RXMX[",
		"JZLMR[ RXMR[",
		"G]JMN[ RRMN[ RRMV[ RZMV[",
		"J[MMX[ RXMM[",
		"JZLMR[ RXMR[P_NaLbKb",
		"J[XMM[ RMMXM RM[X[",
		"KYTBRCQDPFPHQJRKSMSOQQ RRCQEQGRISJTLTNSPORSTTVTXSZR[Q]Q_Ra RQSSU",
		"NVRBRb",
		"KYPBRCSDTFTHSJRKQMQOSQ RRCSESGRIQJPLPNQPURQTPVPXQZR[S]S_Ra RSSQU",
		"F^IUISJPLONOPPTSVTXTZS[Q RISJQLPNPPQTTVUXUZT[Q[O",
		"JZJFJ[K[KFLFL[M[MFNFN[O[OFPFP[Q[QFRFR[S[SFTFT[U[UFVFV[W[WFXFX[Y[YFZFZ["
	}
},
{
	"Rowman Double",
	0.040,		/* X scale */
	0.040,		/* Y scale */
	0.95,		/* X spacing extra scale */
	0.08,		/* Line width scale */
	0.0,		/* horizontal offset */
	11.0,		/* vertical offset */
	{
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"MXRFRTST RRFSFST RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"I[NFMGMM RNGMM RNFOGMM RWFVGVM RWGVM RWFXGVM",
		"H]SFLb RYFRb RLQZQ RKWYW",
		"I\\RBR_S_ RRBSBS_ RWIYIWGTFQFNGLILKMMNNVRWSXUXWWYTZQZOYNX RWIVHTGQGNHMIMKNMVQXSYUYWXYWZT[Q[NZLXNX RXXUZ",
		"F^[FI[ RNFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F RWTUUTWTYV[X[ZZ[X[VYTWT",
		"F_\\MZMXNWPUVTXSYQZMZKYJWJUKSLRQOSMTKTISGQFPFNGMIMKNNPQUWXZZ[\\[ R\\M\\NZNXO RYNXPVVUXSZQ[M[KZJYIWIUJSLQQNRMSKSIRG RSHQGPGNH ROGNINKONQQVWXYZZ\\Z\\[",
		"MXTHSIRIQHQGRFSFTGTJSLQM RRGRHSHSGRG RSITJ RTHSL",
		"KYUBSDQGOKNPNTOYQ]S`UbVb RUBVBTDRGPKOPOTPYR]T`Vb",
		"KYNBPDRGTKUPUTTYR]P`NbOb RNBOBQDSGUKVPVTUYS]Q`Ob",
		"JZRFQGSQRR RRFRR RRFSGQQRR RMINIVOWO RMIWO RMIMJWNWO RWIVINOMO RWIMO RWIWJMNMO",
		"F_RIRZSZ RRISISZ RJQ[Q[R RJQJR[R",
		"MXTZS[R[QZQYRXSXTYT\\S^Q_ RRYRZSZSYRY RS[T\\ RTZS^",
		"F_JQ[Q[R RJQJR[R",
		"MXRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"G^[BIbJb R[B\\BJb",
		"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF ROGMJLOLRMWOZ RNYQZSZVY RUZWWXRXOWJUG RVHSGQGNH",
		"H\\NJPISFS[ RNJNKPJRHR[S[",
		"H\\LKLJMHNGPFTFVGWHXJXLWNUQL[ RLKMKMJNHPGTGVHWJWLVNTQK[ RLZYZY[ RK[Y[",
		"H\\MFXFQO RMFMGWG RWFPO RQNSNVOXQYTYUXXVZS[P[MZLYKWLW RPOSOVPXS RTOWQXTXUWXTZ RXVVYSZPZMYLW ROZLX",
		"H\\UIU[V[ RVFV[ RVFKVZV RUILV RLUZUZV",
		"H\\MFLO RNGMN RMFWFWG RNGWG RMNPMSMVNXPYSYUXXVZS[P[MZLYKWLW RLOMOONSNVOXR RTNWPXSXUWXTZ RXVVYSZPZMYLW ROZLX",
		"H\\VGWIXIWGTFRFOGMJLOLTMXOZR[S[VZXXYUYTXQVOSNRNOOMQ RWHTGRGOH RPGNJMOMTNXQZ RMVOYRZSZVYXV RTZWXXUXTWQTO RXSVPSOROOPMS RQONQMT",
		"H\\KFYFO[ RKFKGXG RXFN[O[",
		"H\\PFMGLILKMMNNPOTPVQWRXTXWWYTZPZMYLWLTMRNQPPTOVNWMXKXIWGTFPF RNGMIMKNMPNTOVPXRYTYWXYWZT[P[MZLYKWKTLRNPPOTNVMWKWIVG RWHTGPGMH RLXOZ RUZXX",
		"H\\WPURRSQSNRLPKMKLLINGQFRFUGWIXMXRWWUZR[P[MZLXMXNZ RWMVPSR RWNUQRRQRNQLN RPRMPLMLLMIPG RLKNHQGRGUHWK RSGVIWMWRVWTZ RUYRZPZMY",
		"MXRMQNQORPSPTOTNSMRM RRNROSOSNRN RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"MXRMQNQORPSPTOTNSMRM RRNROSOSNRN RTZS[R[QZQYRXSXTYT\\S^Q_ RRYRZSZSYRY RS[T\\ RTZS^",
		"F^ZIJRZ[",
		"F_JM[M[N RJMJN[N RJU[U[V RJUJV[V",
		"F^JIZRJ[",
		"I\\LKLJMHNGQFTFWGXHYJYLXNWOUPRQ RLKMKMJNHQGTGWHXJXLWNUORP RMIPG RUGXI RXMTP RRPRTSTSP RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"E`WNVLTKQKOLNMMPMSNUPVSVUUVS RQKOMNPNSOUPV RWKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX RXKWSWUXV",
		"H\\RFJ[ RRIK[J[ RRIY[Z[ RRFZ[ RMUWU RLVXV",
		"H\\LFL[ RMGMZ RLFTFWGXHYJYMXOWPTQ RMGTGWHXJXMWOTP RMPTPWQXRYTYWXYWZT[L[ RMQTQWRXTXWWYTZMZ",
		"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZV RZKYKXIWHUGQGOHMKLNLSMVOYQZUZWYXXYVZV",
		"H]LFL[ RMGMZ RLFSFVGXIYKZNZSYVXXVZS[L[ RMGSGVHWIXKYNYSXVWXVYSZMZ",
		"I\\MFM[ RNGNZ RMFYF RNGYGYF RNPTPTQ RNQTQ RNZYZY[ RM[Y[",
		"I[MFM[ RNGN[M[ RMFYF RNGYGYF RNPTPTQ RNQTQ",
		"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZVZRUR RZKYKXIWHUGQGOHNIMKLNLSMVNXOYQZUZWYXXYVYSUSUR",
		"G]KFK[ RKFLFL[K[ RYFXFX[Y[ RYFY[ RLPXP RLQXQ",
		"NWRFR[S[ RRFSFS[",
		"J[VFVVUYSZQZOYNVMV RVFWFWVVYUZS[Q[OZNYMV",
		"H]LFL[M[ RLFMFM[ RZFYFMR RZFMS RPOY[Z[ RQOZ[",
		"IZMFM[ RMFNFNZ RNZYZY[ RM[Y[",
		"F^JFJ[ RKKK[J[ RKKR[ RJFRX RZFRX RYKR[ RYKY[Z[ RZFZ[",
		"G]KFK[ RLIL[K[ RLIY[ RKFXX RXFXX RXFYFY[",
		"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF RQGNHLKKNKSLVNYQZSZVYXVYSYNXKVHSGQG",
		"H\\LFL[ RMGM[L[ RLFUFWGXHYJYMXOWPUQMQ RMGUGWHXJXMWOUPMP",
		"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF RQGNHLKKNKSLVNYQZSZVYXVYSYNXKVHSGQG RSXX]Y] RSXTXY]",
		"H\\LFL[ RMGM[L[ RLFTFWGXHYJYMXOWPTQMQ RMGTGWHXJXMWOTPMP RRQX[Y[ RSQY[",
		"H\\YIWGTFPFMGKIKKLMMNOOTQVRWSXUXXWYTZPZNYMXKX RYIWIVHTGPGMHLILKMMONTPVQXSYUYXWZT[P[MZKX",
		"J[RGR[ RSGS[R[ RLFYFYG RLFLGYG",
		"G]KFKULXNZQ[S[VZXXYUYF RKFLFLUMXNYQZSZVYWXXUXFYF",
		"H\\JFR[ RJFKFRX RZFYFRX RZFR[",
		"E_GFM[ RGFHFMX RRFMX RRIM[ RRIW[ RRFWX R]F\\FWX R]FW[",
		"H\\KFX[Y[ RKFLFY[ RYFXFK[ RYFL[K[",
		"I\\KFRPR[S[ RKFLFSP RZFYFRP RZFSPS[",
		"H\\XFK[ RYFL[ RKFYF RKFKGXG RLZYZY[ RK[Y[",
		"KYOBOb RPBPb ROBVB RObVb",
		"KYKFY^",
		"KYTBTb RUBUb RNBUB RNbUb",
		"JZPLRITL RMORJWO RRJR[",
		"JZJ]Z]",
		"MXTFRGQIQLRMSMTLTKSJRJQK RRKRLSLSKRK RRGQK RQIRJ",
		"H\\WMW[X[ RWMXMX[ RWPUNSMPMNNLPKSKULXNZP[S[UZWX RWPSNPNNOMPLSLUMXNYPZSZWX",
		"H\\LFL[M[ RLFMFM[ RMPONQMTMVNXPYSYUXXVZT[Q[OZMX RMPQNTNVOWPXSXUWXVYTZQZMX",
		"I[XPVNTMQMONMPLSLUMXOZQ[T[VZXX RXPWQVOTNQNOONPMSMUNXOYQZTZVYWWXX",
		"H\\WFW[X[ RWFXFX[ RWPUNSMPMNNLPKSKULXNZP[S[UZWX RWPSNPNNOMPLSLUMXNYPZSZWX",
		"I[MTXTXQWOVNTMQMONMPLSLUMXOZQ[T[VZXX RMSWSWQVOTNQNOONPMSMUNXOYQZTZVYWWXX",
		"LZWFUFSGRJR[S[ RWFWGUGSH RTGSJS[ ROMVMVN ROMONVN",
		"H\\XMWMW\\V_U`SaQaO`N_L_ RXMX\\W_UaSbPbNaL_ RWPUNSMPMNNLPKSKULXNZP[S[UZWX RWPSNPNNOMPLSLUMXNYPZSZWX",
		"H\\LFL[M[ RLFMFM[ RMQPNRMUMWNXQX[ RMQPORNTNVOWQW[X[",
		"NWRFQGQHRISITHTGSFRF RRGRHSHSGRG RRMR[S[ RRMSMS[",
		"NWRFQGQHRISITHTGSFRF RRGRHSHSGRG RRMRbSb RRMSMSb",
		"H[LFL[M[ RLFMFM[ RXMWMMW RXMMX RPTV[X[ RQSX[",
		"NWRFR[S[ RRFSFS[",
		"CbGMG[H[ RGMHMH[ RHQKNMMPMRNSQS[ RHQKOMNONQORQR[S[ RSQVNXM[M]N^Q^[ RSQVOXNZN\\O]Q][^[",
		"H\\LML[M[ RLMMMM[ RMQPNRMUMWNXQX[ RMQPORNTNVOWQW[X[",
		"I\\QMONMPLSLUMXOZQ[T[VZXXYUYSXPVNTMQM RQNOONPMSMUNXOYQZTZVYWXXUXSWPVOTNQN",
		"H\\LMLbMb RLMMMMb RMPONQMTMVNXPYSYUXXVZT[Q[OZMX RMPQNTNVOWPXSXUWXVYTZQZMX",
		"H\\WMWbXb RWMXMXb RWPUNSMPMNNLPKSKULXNZP[S[UZWX RWPSNPNNOMPLSLUMXNYPZSZWX",
		"KYOMO[P[ ROMPMP[ RPSQPSNUMXM RPSQQSOUNXNXM",
		"J[XPWNTMQMNNMPNRPSUUWV RVUWWWXVZ RWYTZQZNY ROZNXMX RXPWPVN RWOTNQNNO RONNPOR RNQPRUTWUXWXXWZT[Q[NZMX",
		"MXRFR[S[ RRFSFS[ ROMVMVN ROMONVN",
		"H\\LMLWMZO[R[TZWW RLMMMMWNYPZRZTYWW RWMW[X[ RWMXMX[",
		"JZLMR[ RLMMMRY RXMWMRY RXMR[",
		"F^IMN[ RIMJMNX RRMNX RRPN[ RRPV[ RRMVX R[MZMVX R[MV[",
		"I[LMW[X[ RLMMMX[ RXMWML[ RXMM[L[",
		"JZLMR[ RLMMMRY RXMWMRYNb RXMR[ObNb",
		"I[VNL[ RXMNZ RLMXM RLMLNVN RNZXZX[ RL[X[",
		"KYTBRCQDPFPHQJRKSMSOQQ RRCQEQGRISJTLTNSPORSTTVTXSZR[Q]Q_Ra RQSSUSWRYQZP\\P^Q`RaTb",
		"NVRBRb",
		"KYPBRCSDTFTHSJRKQMQOSQ RRCSESGRIQJPLPNQPURQTPVPXQZR[S]S_Ra RSSQUQWRYSZT\\T^S`RaPb",
		"F^IUISJPLONOPPTSVTXTZS[Q RISJQLPNPPQTTVUXUZT[Q[O",
		"KYQFOGNINKOMQNSNUMVKVIUGSFQF RQFNIOMSNVKUGQF RSFOGNKQNUMVISF"
	}
},
{
	"Rowman Triple",
	0.040,		/* X scale */
	0.040,		/* Y scale */
	0.96,		/* X spacing extra scale */
	0.08,		/* Line width scale */
	0.3,		/* horizontal offset */
	11.0,		/* vertical offset */
	{
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"MXRFQGQIRQ RRFRTST RRFSFST RSFTGTISQ RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"I[NFMGMM RNGMM RNFOGMM RWFVGVM RWGVM RWFXGVM",
		"H]SFLb RYFRb RLQZQ RKWYW",
		"H\\PBP_ RTBT_ RXKXJWJWLYLYJXHWGTFPFMGKIKLLNOPURWSXUXXWZ RLLMNOOUQWRXT RMGLILKMMONUPXRYTYWXYWZT[P[MZLYKWKUMUMWLWLV",
		"F^[FI[ RNFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F RWTUUTWTYV[X[ZZ[X[VYTWT",
		"E_[O[NZNZP\\P\\N[MZMYNXPVUTXRZP[L[JZIXIUJSPORMSKSIRGPFNGMIMLNOPRTWWZY[[[\\Y\\X RKZJXJUKSLR RRMSI RSKRG RNGMK RNNPQTVWYYZ RN[LZKXKULSPO RMINMQQUVXYZZ[Z\\Y",
		"MXTHSIRIQHQGRFSFTGTJSLQM RRGRHSHSGRG RSITJ RTHSL",
		"KYUBSDQGOKNPNTOYQ]S`Ub RQHPKOOOUPYQ\\ RSDRFQIPOPUQ[R^S`",
		"KYOBQDSGUKVPVTUYS]Q`Ob RSHTKUOUUTYS\\ RQDRFSITOTUS[R^Q`",
		"JZRFQGSQRR RRFRR RRFSGQQRR RMINIVOWO RMIWO RMIMJWNWO RWIVINOMO RWIMO RWIWJMNMO",
		"F_RIRZSZ RRISISZ RJQ[Q[R RJQJR[R",
		"MXTZS[R[QZQYRXSXTYT\\S^Q_ RRYRZSZSYRY RS[T\\ RTZS^",
		"F_JQ[Q[R RJQJR[R",
		"MXRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"G^[BIbJb R[B\\BJb",
		"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF RNHMJLNLSMWNY RVYWWXSXNWJVH RQFOGNIMNMSNXOZQ[ RS[UZVXWSWNVIUGSF",
		"H\\QHQ[ RRHRZ RSFS[ RSFPINJ RM[W[ RQZO[ RQYP[ RSYT[ RSZU[",
		"H\\LJLKMKMJLJ RLIMINJNKMLLLKKKJLHMGPFTFWGXHYJYLXNUPPRNSLUKXK[ RWHXJXLWN RTFVGWJWLVNTPPR RKYLXNXSYWYYX RNXSZWZXY RNXS[W[XZYXYV",
		"H\\LJLKMKMJLJ RLIMINJNKMLLLKKKJLHMGPFTFWGXIXLWNTO RVGWIWLVN RSFUGVIVLUNSO RQOTOVPXRYTYWXYWZT[P[MZLYKWKVLUMUNVNWMXLX RWRXTXWWY RSOUPVQWTWWVZT[ RLVLWMWMVLV",
		"H\\SIS[ RTHTZ RUFU[ RUFJUZU RP[X[ RSZQ[ RSYR[ RUYV[ RUZW[",
		"H\\MFKPMNPMSMVNXPYSYUXXVZS[P[MZLYKWKVLUMUNVNWMXLX RWPXRXVWX RSMUNVOWRWVVYUZS[ RLVLWMWMVLV RMFWF RMGUG RMHQHUGWF",
		"H\\VIVJWJWIVI RWHVHUIUJVKWKXJXIWGUFRFOGMILKKOKULXNZQ[S[VZXXYUYTXQVOSNQNOONPMR RNIMKLOLUMXNY RWXXVXSWQ RRFPGOHNJMNMUNXOZQ[ RS[UZVYWVWSVPUOSN",
		"H\\KFKL RYFYIXLTQSSRWR[ RSRRTQWQ[ RXLSQQTPWP[R[ RKJLHNFPFUIWIXHYF RMHNGPGRH RKJLINHPHUI",
		"H\\PFMGLILLMNPOTOWNXLXIWGTFPF RNGMIMLNN RVNWLWIVG RPFOGNINLONPO RTOUNVLVIUGTF RPOMPLQKSKWLYMZP[T[WZXYYWYSXQWPTO RMQLSLWMY RWYXWXSWQ RPONPMSMWNZP[ RT[VZWWWSVPTO",
		"H\\MWMXNXNWMW RWOVQURSSQSNRLPKMKLLINGQFSFVGXIYLYRXVWXUZR[O[MZLXLWMVNVOWOXNYMY RMPLNLKMI RVHWIXLXRWVVX RQSORNQMNMKNHOGQF RSFUGVIWLWSVWUYTZR[",
		"MXRMQNQORPSPTOTNSMRM RRNROSOSNRN RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"MXRMQNQORPSPTOTNSMRM RRNROSOSNRN RTZS[R[QZQYRXSXTYT\\S^Q_ RRYRZSZSYRY RS[T\\ RTZS^",
		"F^ZIJRZ[",
		"F_JM[M[N RJMJN[N RJU[U[V RJUJV[V",
		"F^JIZRJ[",
		"I\\MKMJNJNLLLLJMHNGPFTFWGXHYJYLXNWOSQ RWHXIXMWN RTFVGWIWMVOUP RRQRTSTSQRQ RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"E`WNVLTKQKOLNMMPMSNUPVSVUUVS RQKOMNPNSOUPV RWKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX RXKWSWUXV",
		"H\\RFKZ RQIW[ RRIX[ RRFY[ RMUVU RI[O[ RT[[[ RKZJ[ RKZM[ RWZU[ RWYV[ RXYZ[",
		"G]LFL[ RMGMZ RNFN[ RIFUFXGYHZJZLYNXOUP RXHYJYLXN RUFWGXIXMWOUP RNPUPXQYRZTZWYYXZU[I[ RXRYTYWXY RUPWQXSXXWZU[ RJFLG RKFLH ROFNH RPFNG RLZJ[ RLYK[ RNYO[ RNZP[",
		"G\\XIYFYLXIVGTFQFNGLIKKJNJSKVLXNZQ[T[VZXXYV RMILKKNKSLVMX RQFOGMJLNLSMWOZQ[",
		"G]LFL[ RMGMZ RNFN[ RIFSFVGXIYKZNZSYVXXVZS[I[ RWIXKYNYSXVWX RSFUGWJXNXSWWUZS[ RJFLG RKFLH ROFNH RPFNG RLZJ[ RLYK[ RNYO[ RNZP[",
		"G\\LFL[ RMGMZ RNFN[ RIFYFYL RNPTP RTLTT RI[Y[YU RJFLG RKFLH ROFNH RPFNG RTFYG RVFYH RWFYI RXFYL RTLSPTT RTNRPTR RTOPPTQ RLZJ[ RLYK[ RNYO[ RNZP[ RT[YZ RV[YY RW[YX RX[YU",
		"G[LFL[ RMGMZ RNFN[ RIFYFYL RNPTP RTLTT RI[Q[ RJFLG RKFLH ROFNH RPFNG RTFYG RVFYH RWFYI RXFYL RTLSPTT RTNRPTR RTOPPTQ RLZJ[ RLYK[ RNYO[ RNZP[",
		"G^XIYFYLXIVGTFQFNGLIKKJNJSKVLXNZQ[T[VZXZY[YS RMILKKNKSLVMX RQFOGMJLNLSMWOZQ[ RXTXY RWSWYVZ RTS\\S RUSWT RVSWU RZSYU R[SYT",
		"F^KFK[ RLGLZ RMFM[ RWFW[ RXGXZ RYFY[ RHFPF RTF\\F RMPWP RH[P[ RT[\\[ RIFKG RJFKH RNFMH ROFMG RUFWG RVFWH RZFYH R[FYG RKZI[ RKYJ[ RMYN[ RMZO[ RWZU[ RWYV[ RYYZ[ RYZ[[",
		"LXQFQ[ RRGRZ RSFS[ RNFVF RN[V[ ROFQG RPFQH RTFSH RUFSG RQZO[ RQYP[ RSYT[ RSZU[",
		"JYSFSWRZQ[ RTGTWSZ RUFUWTZQ[O[MZLXLVMUNUOVOWNXMX RMVMWNWNVMV RPFXF RQFSG RRFSH RVFUH RWFUG",
		"F]KFK[ RLGLZ RMFM[ RXGMR RPPW[ RQPX[ RQNY[ RHFPF RUF[F RH[P[ RT[[[ RIFKG RJFKH RNFMH ROFMG RWFXG RZFXG RKZI[ RKYJ[ RMYN[ RMZO[ RWYU[ RWYZ[",
		"I[NFN[ ROGOZ RPFP[ RKFSF RK[Z[ZU RLFNG RMFNH RQFPH RRFPG RNZL[ RNYM[ RPYQ[ RPZR[ RU[ZZ RW[ZY RX[ZX RY[ZU",
		"E_JFJZ RJFQ[ RKFQX RLFRX RXFQ[ RXFX[ RYGYZ RZFZ[ RGFLF RXF]F RG[M[ RU[][ RHFJG R[FZH R\\FZG RJZH[ RJZL[ RXZV[ RXYW[ RZY[[ RZZ\\[",
		"F^KFKZ RKFY[ RLFXX RMFYX RYGY[ RHFMF RVF\\F RH[N[ RIFKG RWFYG R[FYG RKZI[ RKZM[",
		"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF RMILKKNKSLVMX RWXXVYSYNXKWI RQFOGMJLNLSMWOZQ[ RS[UZWWXSXNWJUGSF",
		"G]LFL[ RMGMZ RNFN[ RIFUFXGYHZJZMYOXPUQNQ RXHYJYMXO RUFWGXIXNWPUQ RI[Q[ RJFLG RKFLH ROFNH RPFNG RLZJ[ RLYK[ RNYO[ RNZP[",
		"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF RMILKKNKSLVMX RWXXVYSYNXKWI RQFOGMJLNLSMWOZQ[ RS[UZWWXSXNWJUGSF RNXOVQURUTVUXV^W`Y`Z^Z\\ RV\\W^X_Y_ RUXW]X^Y^Z]",
		"G]LFL[ RMGMZ RNFN[ RIFUFXGYHZJZLYNXOUPNP RXHYJYLXN RUFWGXIXMWOUP RRPTQUSWYX[Z[[Y[W RWWXYYZZZ RTQURXXYYZY[X RI[Q[ RJFLG RKFLH ROFNH RPFNG RLZJ[ RLYK[ RNYO[ RNZP[",
		"H\\XIYFYLXIVGSFPFMGKIKLLNOPURWSXUXXWZ RLLMNOOUQWRXT RMGLILKMMONUPXRYTYWXYWZT[Q[NZLXKUK[LX",
		"H\\JFJL RQFQ[ RRGRZ RSFS[ RZFZL RJFZF RN[V[ RKFJL RLFJI RMFJH ROFJG RUFZG RWFZH RXFZI RYFZL RQZO[ RQYP[ RSYT[ RSZU[",
		"F^KFKULXNZQ[S[VZXXYUYG RLGLVMX RMFMVNYOZQ[ RHFPF RVF\\F RIFKG RJFKH RNFMH ROFMG RWFYG R[FYG",
		"H\\KFR[ RLFRXR[ RMFSX RYGR[ RIFPF RUF[F RJFLH RNFMH ROFMG RWFYG RZFYG",
		"F^JFN[ RKFNVN[ RLFOV RRFOVN[ RRFV[ RSFVVV[ RTFWV RZGWVV[ RGFOF RRFTF RWF]F RHFKG RIFKH RMFLH RNFLG RXFZG R\\FZG",
		"H\\KFW[ RLFX[ RMFY[ RXGLZ RIFPF RUF[F RI[O[ RT[[[ RJFMH RNFMH ROFMG RVFXG RZFXG RLZJ[ RLZN[ RWZU[ RWYV[ RWYZ[",
		"G]JFQQQ[ RKFRQRZ RLFSQS[ RYGSQ RHFOF RVF\\F RN[V[ RIFKG RNFLG RWFYG R[FYG RQZO[ RQYP[ RSYT[ RSZU[",
		"H\\YFKFKL RWFK[ RXFL[ RYFM[ RK[Y[YU RLFKL RMFKI RNFKH RPFKG RT[YZ RV[YY RW[YX RX[YU",
		"KYOBOb RPBPb ROBVB RObVb",
		"KYKFY^",
		"KYTBTb RUBUb RNBUB RNbUb",
		"JZPLRITL RMORJWO RRJR[",
		"JZJ]Z]",
		"MXTFRGQIQLRMSMTLTKSJRJQK RRKRLSLSKRK RRGQK RQIRJ",
		"I]NPNOOOOQMQMONNPMTMVNWOXQXXYZZ[ RVOWQWXXZ RTMUNVPVXWZZ[[[ RVRUSPTMULWLXMZP[S[UZVX RNUMWMXNZ RUSQTOUNWNXOZP[",
		"G\\LFL[MZOZ RMGMY RIFNFNZ RNPONQMSMVNXPYSYUXXVZS[Q[OZNX RWPXRXVWX RSMUNVOWRWVVYUZS[ RJFLG RKFLH",
		"H[WQWPVPVRXRXPVNTMQMNNLPKSKULXNZQ[S[VZXX RMPLRLVMX RQMONNOMRMVNYOZQ[",
		"H]VFV[[[ RWGWZ RSFXFX[ RVPUNSMQMNNLPKSKULXNZQ[S[UZVX RMPLRLVMX RQMONNOMRMVNYOZQ[ RTFVG RUFVH RXYY[ RXZZ[",
		"H[MSXSXQWOVNSMQMNNLPKSKULXNZQ[S[VZXX RWRWQVO RMPLRLVMX RVSVPUNSM RQMONNOMRMVNYOZQ[",
		"KYWHWGVGVIXIXGWFTFRGQHPKP[ RRHQKQZ RTFSGRIR[ RMMVM RM[U[ RPZN[ RPYO[ RRYS[ RRZT[",
		"I\\XNYOZNYMXMVNUO RQMONNOMQMSNUOVQWSWUVVUWSWQVOUNSMQM ROONQNSOU RUUVSVQUO RQMPNOPOTPVQW RSWTVUTUPTNSM RNUMVLXLYM[N\\Q]U]X^Y_ RN[Q\\U\\X] RLYMZP[U[X\\Y^Y_XaUbObLaK_K^L\\O[ RObMaL_L^M\\O[",
		"G^LFL[ RMGMZ RIFNFN[ RNQOOPNRMUMWNXOYRY[ RWOXRXZ RUMVNWQW[ RI[Q[ RT[\\[ RJFLG RKFLH RLZJ[ RLYK[ RNYO[ RNZP[ RWZU[ RWYV[ RYYZ[ RYZ[[",
		"LXQFQHSHSFQF RRFRH RQGSG RQMQ[ RRNRZ RNMSMS[ RN[V[ ROMQN RPMQO RQZO[ RQYP[ RSYT[ RSZU[",
		"KXRFRHTHTFRF RSFSH RRGTG RRMR^QaPb RSNS]R` ROMTMT]S`RaPbMbLaL_N_NaMaM` RPMRN RQMRO",
		"G]LFL[ RMGMZ RIFNFN[ RWNNW RRSY[ RRTX[ RQTW[ RTM[M RI[Q[ RT[[[ RJFLG RKFLH RUMWN RZMWN RLZJ[ RLYK[ RNYO[ RNZP[ RWYU[ RVYZ[",
		"LXQFQ[ RRGRZ RNFSFS[ RN[V[ ROFQG RPFQH RQZO[ RQYP[ RSYT[ RSZU[",
		"AcFMF[ RGNGZ RCMHMH[ RHQIOJNLMOMQNROSRS[ RQORRRZ ROMPNQQQ[ RSQTOUNWMZM\\N]O^R^[ R\\O]R]Z RZM[N\\Q\\[ RC[K[ RN[V[ RY[a[ RDMFN REMFO RFZD[ RFYE[ RHYI[ RHZJ[ RQZO[ RQYP[ RSYT[ RSZU[ R\\ZZ[ R\\Y[[ R^Y_[ R^Z`[",
		"G^LML[ RMNMZ RIMNMN[ RNQOOPNRMUMWNXOYRY[ RWOXRXZ RUMVNWQW[ RI[Q[ RT[\\[ RJMLN RKMLO RLZJ[ RLYK[ RNYO[ RNZP[ RWZU[ RWYV[ RYYZ[ RYZ[[",
		"H\\QMNNLPKSKULXNZQ[S[VZXXYUYSXPVNSMQM RMPLRLVMX RWXXVXRWP RQMONNOMRMVNYOZQ[ RS[UZVYWVWRVOUNSM",
		"G\\LMLb RMNMa RIMNMNb RNPONQMSMVNXPYSYUXXVZS[Q[OZNX RWPXRXVWX RSMUNVOWRWVVYUZS[ RIbQb RJMLN RKMLO RLaJb RL`Kb RN`Ob RNaPb",
		"H\\VNVb RWOWa RUNWNXMXb RVPUNSMQMNNLPKSKULXNZQ[S[UZVX RMPLRLVMX RQMONNOMRMVNYOZQ[ RSb[b RVaTb RV`Ub RX`Yb RXaZb",
		"IZNMN[ RONOZ RKMPMP[ RWOWNVNVPXPXNWMUMSNQPPS RK[S[ RLMNN RMMNO RNZL[ RNYM[ RPYQ[ RPZR[",
		"J[WOXMXQWOVNTMPMNNMOMQNSPTUUWVXY RNNMQ RNRPSUTWU RXVWZ RMONQPRUSWTXVXYWZU[Q[OZNYMWM[NY",
		"KZPHPVQYRZT[V[XZYX RQHQWRY RPHRFRWSZT[ RMMVM",
		"G^LMLVMYNZP[S[UZVYWW RMNMWNY RIMNMNWOZP[ RWMW[\\[ RXNXZ RTMYMY[ RJMLN RKMLO RYYZ[ RYZ[[",
		"I[LMR[ RMMRY RNMSY RXNSYR[ RJMQM RTMZM RKMNO RPMNN RVMXN RYMXN",
		"F^JMN[ RKMNX RLMOX RRMOXN[ RRMV[ RSMVX RRMTMWX RZNWXV[ RGMOM RWM]M RHMKN RNMLN RXMZN R\\MZN",
		"H\\LMV[ RMMW[ RNMX[ RWNMZ RJMQM RTMZM RJ[P[ RS[Z[ RKMMN RPMNN RUMWN RYMWN RMZK[ RMZO[ RVZT[ RWZY[",
		"H[LMR[ RMMRY RNMSY RXNSYP_NaLbJbIaI_K_KaJaJ` RJMQM RTMZM RKMNO RPMNN RVMXN RYMXN",
		"I[VML[ RWMM[ RXMN[ RXMLMLQ RL[X[XW RMMLQ RNMLP ROMLO RQMLN RS[XZ RU[XY RV[XX RW[XW",
		"KYTBRCQDPFPHQJRKSMSOQQ RRCQEQGRISJTLTNSPORSTTVTXSZR[Q]Q_Ra RQSSUSWRYQZP\\P^Q`RaTb",
		"NVRBRb",
		"KYPBRCSDTFTHSJRKQMQOSQ RRCSESGRIQJPLPNQPURQTPVPXQZR[S]S_Ra RSSQUQWRYSZT\\T^S`RaPb",
		"F^IUISJPLONOPPTSVTXTZS[Q RISJQLPNPPQTTVUXUZT[Q[O",
		"KYQFOGNINKOMQNSNUMVKVIUGSFQF RQFNIOMSNVKUGQF RSFOGNKQNUMVISF"
	}
},
{
	"Times Roman",
	0.041,		/* X scale */
	0.041,		/* Y scale */
	0.95,		/* X spacing extra scale */
	0.055,		/* Line width scale */
	0.0,		/* horizontal offset */
	10.5,		/* vertical offset */
	{
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"MWRFQHRTSHRF RRHRN RRYQZR[SZRY",
		"I[NFMGMM RNGMM RNFOGMM RWFVGVM RWGVM RWFXGVM",
		"H]SBLb RYBRb RLOZO RKUYU",
		"H\\PBP_ RTBT_ RXIWJXKYJYIWGTFPFMGKIKKLMMNOOUQWRYT RKKMMONUPWQXRYTYXWZT[P[MZKXKWLVMWLX",
		"F^[FI[ RNFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F RWTUUTWTYV[X[ZZ[X[VYTWT",
		"F_[NZO[P\\O\\N[MZMYNXPVUTXRZP[M[JZIXIUJSPORMSKSIRGPFNGMIMKNNPQUXWZZ[[[\\Z\\Y RM[KZJXJUKSMQ RMKNMVXXZZ[",
		"NVRFQM RSFQM",
		"KYVBTDRGPKOPOTPYR]T`Vb RTDRHQKPPPTQYR\\T`",
		"KYNBPDRGTKUPUTTYR]P`Nb RPDRHSKTPTTSYR\\P`",
		"JZRLRX RMOWU RWOMU",
		"E_RIR[ RIR[R",
		"NVSWRXQWRVSWSYQ[",
		"E_IR[R",
		"NVRVQWRXSWRV",
		"G][BIb",
		"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF RQFOGNHMJLOLRMWNYOZQ[ RS[UZVYWWXRXOWJVHUGSF",
		"H\\NJPISFS[ RRGR[ RN[W[",
		"H\\LJMKLLKKKJLHMGPFTFWGXHYJYLXNUPPRNSLUKXK[ RTFVGWHXJXLWNTPPR RKYLXNXSZVZXYYX RNXS[W[XZYXYV",
		"H\\LJMKLLKKKJLHMGPFTFWGXIXLWNTOQO RTFVGWIWLVNTO RTOVPXRYTYWXYWZT[P[MZLYKWKVLUMVLW RWQXTXWWYVZT[",
		"H\\THT[ RUFU[ RUFJUZU RQ[X[",
		"H\\MFKP RKPMNPMSMVNXPYSYUXXVZS[P[MZLYKWKVLUMVLW RSMUNWPXSXUWXUZS[ RMFWF RMGRGWF",
		"H\\WIVJWKXJXIWGUFRFOGMILKKOKULXNZQ[S[VZXXYUYTXQVOSNRNOOMQLT RRFPGNIMKLOLUMXOZQ[ RS[UZWXXUXTWQUOSN",
		"H\\KFKL RKJLHNFPFUIWIXHYF RLHNGPGUI RYFYIXLTQSSRVR[ RXLSQRSQVQ[",
		"H\\PFMGLILLMNPOTOWNXLXIWGTFPF RPFNGMIMLNNPO RTOVNWLWIVGTF RPOMPLQKSKWLYMZP[T[WZXYYWYSXQWPTO RPONPMQLSLWMYNZP[ RT[VZWYXWXSWQVPTO",
		"H\\XMWPURRSQSNRLPKMKLLINGQFSFVGXIYLYRXVWXUZR[O[MZLXLWMVNWMX RQSORMPLMLLMIOGQF RSFUGWIXLXRWVVXTZR[",
		"NVROQPRQSPRO RRVQWRXSWRV",
		"NVROQPRQSPRO RSWRXQWRVSWSYQ[",
		"F^ZIJRZ[",
		"E_IO[O RIU[U",
		"F^JIZRJ[",
		"I[MJNKMLLKLJMHNGPFSFVGWHXJXLWNVORQRT RSFUGVHWJWLVNTP RRYQZR[SZRY",
		"E`WNVLTKQKOLNMMPMSNUPVSVUUVS RQKOMNPNSOUPV RWKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX RXKWSWUXV",
		"H\\RFK[ RRFY[ RRIX[ RMUVU RI[O[ RU[[[",
		"G]LFL[ RMFM[ RIFUFXGYHZJZLYNXOUP RUFWGXHYJYLXNWOUP RMPUPXQYRZTZWYYXZU[I[ RUPWQXRYTYWXYWZU[",
		"G\\XIYLYFXIVGSFQFNGLIKKJNJSKVLXNZQ[S[VZXXYV RQFOGMILKKNKSLVMXOZQ[",
		"G]LFL[ RMFM[ RIFSFVGXIYKZNZSYVXXVZS[I[ RSFUGWIXKYNYSXVWXUZS[",
		"G\\LFL[ RMFM[ RSLST RIFYFYLXF RMPSP RI[Y[YUX[",
		"G[LFL[ RMFM[ RSLST RIFYFYLXF RMPSP RI[P[",
		"G^XIYLYFXIVGSFQFNGLIKKJNJSKVLXNZQ[S[VZXX RQFOGMILKKNKSLVMXOZQ[ RXSX[ RYSY[ RUS\\S",
		"F^KFK[ RLFL[ RXFX[ RYFY[ RHFOF RUF\\F RLPXP RH[O[ RU[\\[",
		"MXRFR[ RSFS[ ROFVF RO[V[",
		"KZUFUWTZR[P[NZMXMVNUOVNW RTFTWSZR[ RQFXF",
		"F\\KFK[ RLFL[ RYFLS RQOY[ RPOX[ RHFOF RUF[F RH[O[ RU[[[",
		"I[NFN[ ROFO[ RKFRF RK[Z[ZUY[",
		"F_KFK[ RLFRX RKFR[ RYFR[ RYFY[ RZFZ[ RHFLF RYF]F RH[N[ RV[][",
		"G^LFL[ RMFYY RMHY[ RYFY[ RIFMF RVF\\F RI[O[",
		"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF RQFOGMILKKOKRLVMXOZQ[ RS[UZWXXVYRYOXKWIUGSF",
		"G]LFL[ RMFM[ RIFUFXGYHZJZMYOXPUQMQ RUFWGXHYJYMXOWPUQ RI[P[",
		"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF RQFOGMILKKOKRLVMXOZQ[ RS[UZWXXVYRYOXKWIUGSF RNYNXOVQURUTVUXV_W`Y`Z^Z] RUXV\\W^X_Y_Z^",
		"G]LFL[ RMFM[ RIFUFXGYHZJZLYNXOUPMP RUFWGXHYJYLXNWOUP RI[P[ RRPTQURXYYZZZ[Y RTQUSWZX[Z[[Y[X",
		"H\\XIYFYLXIVGSFPFMGKIKKLMMNOOUQWRYT RKKMMONUPWQXRYTYXWZT[Q[NZLXKUK[LX",
		"I\\RFR[ RSFS[ RLFKLKFZFZLYF RO[V[",
		"F^KFKULXNZQ[S[VZXXYUYF RLFLUMXOZQ[ RHFOF RVF\\F",
		"H\\KFR[ RLFRX RYFR[ RIFOF RUF[F",
		"F^JFN[ RKFNV RRFN[ RRFV[ RSFVV RZFV[ RGFNF RWF]F",
		"H\\KFX[ RLFY[ RYFK[ RIFOF RUF[F RI[O[ RU[[[",
		"H]KFRQR[ RLFSQS[ RZFSQ RIFOF RVF\\F RO[V[",
		"H\\XFK[ RYFL[ RLFKLKFYF RK[Y[YUX[",
		"KYOBOb RPBPb ROBVB RObVb",
		"KYKFY^",
		"KYTBTb RUBUb RNBUB RNbUb",
		"G]JTROZT RJTRPZT",
		"H\\Hb\\b",
		"LXPFUL RPFOGUL",
		"I]NONPMPMONNPMTMVNWOXQXXYZZ[ RWOWXXZZ[[[ RWQVRPSMTLVLXMZP[S[UZWX RPSNTMVMXNZP[",
		"G\\LFL[ RMFM[ RMPONQMSMVNXPYSYUXXVZS[Q[OZMX RSMUNWPXSXUWXUZS[ RIFMF",
		"H[WPVQWRXQXPVNTMQMNNLPKSKULXNZQ[S[VZXX RQMONMPLSLUMXOZQ[",
		"H]WFW[ RXFX[ RWPUNSMQMNNLPKSKULXNZQ[S[UZWX RQMONMPLSLUMXOZQ[ RTFXF RW[[[",
		"H[LSXSXQWOVNTMQMNNLPKSKULXNZQ[S[VZXX RWSWPVN RQMONMPLSLUMXOZQ[",
		"KXUGTHUIVHVGUFSFQGPIP[ RSFRGQIQ[ RMMUM RM[T[",
		"I\\QMONNOMQMSNUOVQWSWUVVUWSWQVOUNSMQM RONNPNTOV RUVVTVPUN RVOWNYMYNWN RNUMVLXLYM[P\\U\\X]Y^ RLYMZP[U[X\\Y^Y_XaUbObLaK_K^L\\O[",
		"G]LFL[ RMFM[ RMPONRMTMWNXPX[ RTMVNWPW[ RIFMF RI[P[ RT[[[",
		"MXRFQGRHSGRF RRMR[ RSMS[ ROMSM RO[V[",
		"MXSFRGSHTGSF RTMT_SaQbObNaN`O_P`Oa RSMS_RaQb RPMTM",
		"G\\LFL[ RMFM[ RWMMW RRSX[ RQSW[ RIFMF RTMZM RI[P[ RT[Z[",
		"MXRFR[ RSFS[ ROFSF RO[V[",
		"BcGMG[ RHMH[ RHPJNMMOMRNSPS[ ROMQNRPR[ RSPUNXMZM]N^P^[ RZM\\N]P][ RDMHM RD[K[ RO[V[ RZ[a[",
		"G]LML[ RMMM[ RMPONRMTMWNXPX[ RTMVNWPW[ RIMMM RI[P[ RT[[[",
		"H\\QMNNLPKSKULXNZQ[S[VZXXYUYSXPVNSMQM RQMONMPLSLUMXOZQ[ RS[UZWXXUXSWPUNSM",
		"G\\LMLb RMMMb RMPONQMSMVNXPYSYUXXVZS[Q[OZMX RSMUNWPXSXUWXUZS[ RIMMM RIbPb",
		"H\\WMWb RXMXb RWPUNSMQMNNLPKSKULXNZQ[S[UZWX RQMONMPLSLUMXOZQ[ RTb[b",
		"IZNMN[ ROMO[ ROSPPRNTMWMXNXOWPVOWN RKMOM RK[R[",
		"J[WOXMXQWOVNTMPMNNMOMQNRPSUUWVXW RMPNQPRUTWUXVXYWZU[Q[OZNYMWM[NY",
		"KZPFPWQZS[U[WZXX RQFQWRZS[ RMMUM",
		"G]LMLXMZP[R[UZWX RMMMXNZP[ RWMW[ RXMX[ RIMMM RTMXM RW[[[",
		"I[LMR[ RMMRY RXMR[ RJMPM RTMZM",
		"F^JMN[ RKMNX RRMN[ RRMV[ RSMVX RZMV[ RGMNM RWM]M",
		"H\\LMW[ RMMX[ RXML[ RJMPM RTMZM RJ[P[ RT[Z[",
		"H[LMR[ RMMRY RXMR[P_NaLbKbJaK`La RJMPM RTMZM",
		"I[WML[ RXMM[ RMMLQLMXM RL[X[XWW[",
		"KYTBRCQDPFPHQJRKSMSOQQ RRCQEQGRISJTLTNSPORSTTVTXSZR[Q]Q_Ra RQSSUSWRYQZP\\P^Q`RaTb",
		"NVRBRb",
		"KYPBRCSDTFTHSJRKQMQOSQ RRCSESGRIQJPLPNQPURQTPVPXQZR[S]S_Ra RSSQUQWRYSZT\\T^S`RaPb",
		"F^IUISJPLONOPPTSVTXTZS[Q RISJQLPNPPQTTVUXUZT[Q[O",
		"JZJFJ[K[KFLFL[M[MFNFN[O[OFPFP[Q[QFRFR[S[SFTFT[U[UFVFV[W[WFXFX[Y[YFZFZ["
	}
},
{
	"Times Roman Bold",
	0.041,		/* X scale */
	0.041,		/* Y scale */
	0.95,		/* X spacing extra scale */
	0.055,		/* Line width scale */
	0.0,		/* horizontal offset */
	10.5,		/* vertical offset */
	{
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"MXRFQGQIRQ RRFRTST RRFSFST RSFTGTISQ RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"I[NFMGMM RNGMM RNFOGMM RWFVGVM RWGVM RWFXGVM",
		"H]SBLb RYBRb RLOZO RKUYU",
		"H\\PBP_ RTBT_ RXKXJWJWLYLYJXHWGTFPFMGKIKLLNOPURWSXUXXWZ RLLMNOOUQWRXT RMGLILKMMONUPXRYTYWXYWZT[P[MZLYKWKUMUMWLWLV",
		"F^[FI[ RNFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F RWTUUTWTYV[X[ZZ[X[VYTWT",
		"E_[O[NZNZP\\P\\N[MZMYNXPVUTXRZP[L[JZIXIUJSPORMSKSIRGPFNGMIMLNOPRTWWZY[[[\\Y\\X RKZJXJUKSLR RRMSI RSKRG RNGMK RNNPQTVWYYZ RN[LZKXKULSPO RMINMQQUVXYZZ[Z\\Y",
		"NWSFRGRM RSGRM RSFTGRM",
		"KYUBSDQGOKNPNTOYQ]S`Ub RQHPKOOOUPYQ\\ RSDRFQIPOPUQ[R^S`",
		"KYOBQDSGUKVPVTUYS]Q`Ob RSHTKUOUUTYS\\ RQDRFSITOTUS[R^Q`",
		"JZRFQGSQRR RRFRR RRFSGQQRR RMINIVOWO RMIWO RMIMJWNWO RWIVINOMO RWIMO RWIWJMNMO",
		"F_RIRZSZ RRISISZ RJQ[Q[R RJQJR[R",
		"MXTZS[R[QZQYRXSXTYT\\S^Q_ RRYRZSZSYRY RS[T\\ RTZS^",
		"E_IR[R",
		"MXRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"G^[BIbJb R[B\\BJb",
		"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF RNHMJLNLSMWNY RVYWWXSXNWJVH RQFOGNIMNMSNXOZQ[ RS[UZVXWSWNVIUGSF",
		"H\\QHQ[ RRHRZ RSFS[ RSFPINJ RM[W[ RQZO[ RQYP[ RSYT[ RSZU[",
		"H\\LJLKMKMJLJ RLIMINJNKMLLLKKKJLHMGPFTFWGXHYJYLXNUPPRNSLUKXK[ RWHXJXLWN RTFVGWJWLVNTPPR RKYLXNXSYWYYX RNXSZWZXY RNXS[W[XZYXYV",
		"H\\LJLKMKMJLJ RLIMINJNKMLLLKKKJLHMGPFTFWGXIXLWNTO RVGWIWLVN RSFUGVIVLUNSO RQOTOVPXRYTYWXYWZT[P[MZLYKWKVLUMUNVNWMXLX RWRXTXWWY RSOUPVQWTWWVZT[ RLVLWMWMVLV",
		"H\\SIS[ RTHTZ RUFU[ RUFJUZU RP[X[ RSZQ[ RSYR[ RUYV[ RUZW[",
		"H\\MFKPMNPMSMVNXPYSYUXXVZS[P[MZLYKWKVLUMUNVNWMXLX RWPXRXVWX RSMUNVOWRWVVYUZS[ RLVLWMWMVLV RMFWF RMGUG RMHQHUGWF",
		"H\\VIVJWJWIVI RWHVHUIUJVKWKXJXIWGUFRFOGMILKKOKULXNZQ[S[VZXXYUYTXQVOSNQNOONPMR RNIMKLOLUMXNY RWXXVXSWQ RRFPGOHNJMNMUNXOZQ[ RS[UZVYWVWSVPUOSN",
		"H\\KFKL RYFYIXLTQSSRWR[ RSRRTQWQ[ RXLSQQTPWP[R[ RKJLHNFPFUIWIXHYF RMHNGPGRH RKJLINHPHUI",
		"H\\PFMGLILLMNPOTOWNXLXIWGTFPF RNGMIMLNN RVNWLWIVG RPFOGNINLONPO RTOUNVLVIUGTF RPOMPLQKSKWLYMZP[T[WZXYYWYSXQWPTO RMQLSLWMY RWYXWXSWQ RPONPMSMWNZP[ RT[VZWWWSVPTO",
		"H\\MWMXNXNWMW RWOVQURSSQSNRLPKMKLLINGQFSFVGXIYLYRXVWXUZR[O[MZLXLWMVNVOWOXNYMY RMPLNLKMI RVHWIXLXRWVVX RQSORNQMNMKNHOGQF RSFUGVIWLWSVWUYTZR[",
		"MXRMQNQORPSPTOTNSMRM RRNROSOSNRN RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"MXRMQNQORPSPTOTNSMRM RRNROSOSNRN RTZS[R[QZQYRXSXTYT\\S^Q_ RRYRZSZSYRY RS[T\\ RTZS^",
		"F^ZIJRZ[",
		"F_JM[M[N RJMJN[N RJU[U[V RJUJV[V",
		"F^JIZRJ[",
		"I\\MKMJNJNLLLLJMHNGPFTFWGXHYJYLXNWOSQ RWHXIXMWN RTFVGWIWMVOUP RRQRTSTSQRQ RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"E`WNVLTKQKOLNMMPMSNUPVSVUUVS RQKOMNPNSOUPV RWKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX RXKWSWUXV",
		"H\\RFKZ RQIW[ RRIX[ RRFY[ RMUVU RI[O[ RT[[[ RKZJ[ RKZM[ RWZU[ RWYV[ RXYZ[",
		"G]LFL[ RMGMZ RNFN[ RIFUFXGYHZJZLYNXOUP RXHYJYLXN RUFWGXIXMWOUP RNPUPXQYRZTZWYYXZU[I[ RXRYTYWXY RUPWQXSXXWZU[ RJFLG RKFLH ROFNH RPFNG RLZJ[ RLYK[ RNYO[ RNZP[",
		"G\\XIYFYLXIVGTFQFNGLIKKJNJSKVLXNZQ[T[VZXXYV RMILKKNKSLVMX RQFOGMJLNLSMWOZQ[",
		"G]LFL[ RMGMZ RNFN[ RIFSFVGXIYKZNZSYVXXVZS[I[ RWIXKYNYSXVWX RSFUGWJXNXSWWUZS[ RJFLG RKFLH ROFNH RPFNG RLZJ[ RLYK[ RNYO[ RNZP[",
		"G\\LFL[ RMGMZ RNFN[ RIFYFYL RNPTP RTLTT RI[Y[YU RJFLG RKFLH ROFNH RPFNG RTFYG RVFYH RWFYI RXFYL RTLSPTT RTNRPTR RTOPPTQ RLZJ[ RLYK[ RNYO[ RNZP[ RT[YZ RV[YY RW[YX RX[YU",
		"G[LFL[ RMGMZ RNFN[ RIFYFYL RNPTP RTLTT RI[Q[ RJFLG RKFLH ROFNH RPFNG RTFYG RVFYH RWFYI RXFYL RTLSPTT RTNRPTR RTOPPTQ RLZJ[ RLYK[ RNYO[ RNZP[",
		"G^XIYFYLXIVGTFQFNGLIKKJNJSKVLXNZQ[T[VZXZY[YS RMILKKNKSLVMX RQFOGMJLNLSMWOZQ[ RXTXY RWSWYVZ RTS\\S RUSWT RVSWU RZSYU R[SYT",
		"F^KFK[ RLGLZ RMFM[ RWFW[ RXGXZ RYFY[ RHFPF RTF\\F RMPWP RH[P[ RT[\\[ RIFKG RJFKH RNFMH ROFMG RUFWG RVFWH RZFYH R[FYG RKZI[ RKYJ[ RMYN[ RMZO[ RWZU[ RWYV[ RYYZ[ RYZ[[",
		"LXQFQ[ RRGRZ RSFS[ RNFVF RN[V[ ROFQG RPFQH RTFSH RUFSG RQZO[ RQYP[ RSYT[ RSZU[",
		"JZSFSWRZQ[ RTGTWSZ RUFUWTZQ[O[MZLXLVMUNUOVOWNXMX RMVMWNWNVMV RPFXF RQFSG RRFSH RVFUH RWFUG",
		"F\\KFK[ RLGLZ RMFM[ RXGMR RPPW[ RQPX[ RQNY[ RHFPF RUF[F RH[P[ RT[[[ RIFKG RJFKH RNFMH ROFMG RWFXG RZFXG RKZI[ RKYJ[ RMYN[ RMZO[ RWYU[ RWYZ[",
		"I[NFN[ ROGOZ RPFP[ RKFSF RK[Z[ZU RLFNG RMFNH RQFPH RRFPG RNZL[ RNYM[ RPYQ[ RPZR[ RU[ZZ RW[ZY RX[ZX RY[ZU",
		"E_JFJZ RJFQ[ RKFQX RLFRX RXFQ[ RXFX[ RYGYZ RZFZ[ RGFLF RXF]F RG[M[ RU[][ RHFJG R[FZH R\\FZG RJZH[ RJZL[ RXZV[ RXYW[ RZY[[ RZZ\\[",
		"F^KFKZ RKFY[ RLFXX RMFYX RYGY[ RHFMF RVF\\F RH[N[ RIFKG RWFYG R[FYG RKZI[ RKZM[",
		"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF RMILKKNKSLVMX RWXXVYSYNXKWI RQFOGMJLNLSMWOZQ[ RS[UZWWXSXNWJUGSF",
		"G]LFL[ RMGMZ RNFN[ RIFUFXGYHZJZMYOXPUQNQ RXHYJYMXO RUFWGXIXNWPUQ RI[Q[ RJFLG RKFLH ROFNH RPFNG RLZJ[ RLYK[ RNYO[ RNZP[",
		"G]QFNGLIKKJOJRKVLXNZQ[S[VZXXYVZRZOYKXIVGSFQF RMILKKNKSLVMX RWXXVYSYNXKWI RQFOGMJLNLSMWOZQ[ RS[UZWWXSXNWJUGSF RNXOVQURUTVUXV^W`Y`Z^Z\\ RV\\W^X_Y_ RUXW]X^Y^Z]",
		"G]LFL[ RMGMZ RNFN[ RIFUFXGYHZJZLYNXOUPNP RXHYJYLXN RUFWGXIXMWOUP RRPTQUSWYX[Z[[Y[W RWWXYYZZZ RTQURXXYYZY[X RI[Q[ RJFLG RKFLH ROFNH RPFNG RLZJ[ RLYK[ RNYO[ RNZP[",
		"H\\XIYFYLXIVGSFPFMGKIKLLNOPURWSXUXXWZ RLLMNOOUQWRXT RMGLILKMMONUPXRYTYWXYWZT[Q[NZLXKUK[LX",
		"H\\JFJL RQFQ[ RRGRZ RSFS[ RZFZL RJFZF RN[V[ RKFJL RLFJI RMFJH ROFJG RUFZG RWFZH RXFZI RYFZL RQZO[ RQYP[ RSYT[ RSZU[",
		"F^KFKULXNZQ[S[VZXXYUYG RLGLVMX RMFMVNYOZQ[ RHFPF RVF\\F RIFKG RJFKH RNFMH ROFMG RWFYG R[FYG",
		"H\\KFR[ RLFRXR[ RMFSX RYGR[ RIFPF RUF[F RJFLH RNFMH ROFMG RWFYG RZFYG",
		"F^JFN[ RKFNVN[ RLFOV RRFOVN[ RRFV[ RSFVVV[ RTFWV RZGWVV[ RGFOF RRFTF RWF]F RHFKG RIFKH RMFLH RNFLG RXFZG R\\FZG",
		"H\\KFW[ RLFX[ RMFY[ RXGLZ RIFPF RUF[F RI[O[ RT[[[ RJFMH RNFMH ROFMG RVFXG RZFXG RLZJ[ RLZN[ RWZU[ RWYV[ RWYZ[",
		"G]JFQQQ[ RKFRQRZ RLFSQS[ RYGSQ RHFOF RVF\\F RN[V[ RIFKG RNFLG RWFYG R[FYG RQZO[ RQYP[ RSYT[ RSZU[",
		"H\\YFKFKL RWFK[ RXFL[ RYFM[ RK[Y[YU RLFKL RMFKI RNFKH RPFKG RT[YZ RV[YY RW[YX RX[YU",
		"KYOBOb RPBPb ROBVB RObVb",
		"KYKFY^",
		"KYTBTb RUBUb RNBUB RNbUb",
		"G]JTROZT RJTRPZT",
		"H\\Hb\\b",
		"LXPFUL RPFOGUL",
		"I]NPNOOOOQMQMONNPMTMVNWOXQXXYZZ[ RVOWQWXXZ RTMUNVPVXWZZ[[[ RVRUSPTMULWLXMZP[S[UZVX RNUMWMXNZ RUSQTOUNWNXOZP[",
		"G\\LFL[MZOZ RMGMY RIFNFNZ RNPONQMSMVNXPYSYUXXVZS[Q[OZNX RWPXRXVWX RSMUNVOWRWVVYUZS[ RJFLG RKFLH",
		"H[WQWPVPVRXRXPVNTMQMNNLPKSKULXNZQ[S[VZXX RMPLRLVMX RQMONNOMRMVNYOZQ[",
		"H]VFV[[[ RWGWZ RSFXFX[ RVPUNSMQMNNLPKSKULXNZQ[S[UZVX RMPLRLVMX RQMONNOMRMVNYOZQ[ RTFVG RUFVH RXYY[ RXZZ[",
		"H[MSXSXQWOVNSMQMNNLPKSKULXNZQ[S[VZXX RWRWQVO RMPLRLVMX RVSVPUNSM RQMONNOMRMVNYOZQ[",
		"KYWHWGVGVIXIXGWFTFRGQHPKP[ RRHQKQZ RTFSGRIR[ RMMVM RM[U[ RPZN[ RPYO[ RRYS[ RRZT[",
		"I\\XNYOZNYMXMVNUO RQMONNOMQMSNUOVQWSWUVVUWSWQVOUNSMQM ROONQNSOU RUUVSVQUO RQMPNOPOTPVQW RSWTVUTUPTNSM RNUMVLXLYM[N\\Q]U]X^Y_ RN[Q\\U\\X] RLYMZP[U[X\\Y^Y_XaUbObLaK_K^L\\O[ RObMaL_L^M\\O[",
		"G^LFL[ RMGMZ RIFNFN[ RNQOOPNRMUMWNXOYRY[ RWOXRXZ RUMVNWQW[ RI[Q[ RT[\\[ RJFLG RKFLH RLZJ[ RLYK[ RNYO[ RNZP[ RWZU[ RWYV[ RYYZ[ RYZ[[",
		"LXQFQHSHSFQF RRFRH RQGSG RQMQ[ RRNRZ RNMSMS[ RN[V[ ROMQN RPMQO RQZO[ RQYP[ RSYT[ RSZU[",
		"KXRFRHTHTFRF RSFSH RRGTG RRMR^QaPb RSNS]R` ROMTMT]S`RaPbMbLaL_N_NaMaM` RPMRN RQMRO",
		"G]LFL[ RMGMZ RIFNFN[ RWNNW RRSY[ RRTX[ RQTW[ RTM[M RI[Q[ RT[[[ RJFLG RKFLH RUMWN RZMWN RLZJ[ RLYK[ RNYO[ RNZP[ RWYU[ RVYZ[",
		"LXQFQ[ RRGRZ RNFSFS[ RN[V[ ROFQG RPFQH RQZO[ RQYP[ RSYT[ RSZU[",
		"AcFMF[ RGNGZ RCMHMH[ RHQIOJNLMOMQNROSRS[ RQORRRZ ROMPNQQQ[ RSQTOUNWMZM\\N]O^R^[ R\\O]R]Z RZM[N\\Q\\[ RC[K[ RN[V[ RY[a[ RDMFN REMFO RFZD[ RFYE[ RHYI[ RHZJ[ RQZO[ RQYP[ RSYT[ RSZU[ R\\ZZ[ R\\Y[[ R^Y_[ R^Z`[",
		"G^LML[ RMNMZ RIMNMN[ RNQOOPNRMUMWNXOYRY[ RWOXRXZ RUMVNWQW[ RI[Q[ RT[\\[ RJMLN RKMLO RLZJ[ RLYK[ RNYO[ RNZP[ RWZU[ RWYV[ RYYZ[ RYZ[[",
		"H\\QMNNLPKSKULXNZQ[S[VZXXYUYSXPVNSMQM RMPLRLVMX RWXXVXRWP RQMONNOMRMVNYOZQ[ RS[UZVYWVWRVOUNSM",
		"G\\LMLb RMNMa RIMNMNb RNPONQMSMVNXPYSYUXXVZS[Q[OZNX RWPXRXVWX RSMUNVOWRWVVYUZS[ RIbQb RJMLN RKMLO RLaJb RL`Kb RN`Ob RNaPb",
		"H\\VNVb RWOWa RUNWNXMXb RVPUNSMQMNNLPKSKULXNZQ[S[UZVX RMPLRLVMX RQMONNOMRMVNYOZQ[ RSb[b RVaTb RV`Ub RX`Yb RXaZb",
		"IZNMN[ RONOZ RKMPMP[ RWOWNVNVPXPXNWMUMSNQPPS RK[S[ RLMNN RMMNO RNZL[ RNYM[ RPYQ[ RPZR[",
		"J[WOXMXQWOVNTMPMNNMOMQNSPTUUWVXY RNNMQ RNRPSUTWU RXVWZ RMONQPRUSWTXVXYWZU[Q[OZNYMWM[NY",
		"KZPHPVQYRZT[V[XZYX RQHQWRY RPHRFRWSZT[ RMMVM",
		"G^LMLVMYNZP[S[UZVYWW RMNMWNY RIMNMNWOZP[ RWMW[\\[ RXNXZ RTMYMY[ RJMLN RKMLO RYYZ[ RYZ[[",
		"I[LMR[ RMMRY RNMSY RXNSYR[ RJMQM RTMZM RKMNO RPMNN RVMXN RYMXN",
		"F^JMN[ RKMNX RLMOX RRMOXN[ RRMV[ RSMVX RRMTMWX RZNWXV[ RGMOM RWM]M RHMKN RNMLN RXMZN R\\MZN",
		"H\\LMV[ RMMW[ RNMX[ RWNMZ RJMQM RTMZM RJ[P[ RS[Z[ RKMMN RPMNN RUMWN RYMWN RMZK[ RMZO[ RVZT[ RWZY[",
		"H[LMR[ RMMRY RNMSY RXNSYP_NaLbJbIaI_K_KaJaJ` RJMQM RTMZM RKMNO RPMNN RVMXN RYMXN",
		"I[VML[ RWMM[ RXMN[ RXMLMLQ RL[X[XW RMMLQ RNMLP ROMLO RQMLN RS[XZ RU[XY RV[XX RW[XW",
		"KYTBRCQDPFPHQJRKSMSOQQ RRCQEQGRISJTLTNSPORSTTVTXSZR[Q]Q_Ra RQSSUSWRYQZP\\P^Q`RaTb",
		"NVRBRb",
		"KYPBRCSDTFTHSJRKQMQOSQ RRCSESGRIQJPLPNQPURQTPVPXQZR[S]S_Ra RSSQUQWRYSZT\\T^S`RaPb",
		"F^IUISJPLONOPPTSVTXTZS[Q RISJQLPNPPQTTVUXUZT[Q[O",
		"JZJFJ[K[KFLFL[M[MFNFN[O[OFPFP[Q[QFRFR[S[SFTFT[U[UFVFV[W[WFXFX[Y[YFZFZ["
	}
},
{
	"Futura Light",
	0.041,		/* X scale */
	0.041,		/* Y scale */
	0.95,		/* X spacing extra scale */
	0.08,		/* Line width scale */
	0.9,		/* horizontal offset */
	10.5,		/* vertical offset */
	{
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"MWRFRT RRYQZR[SZRY",
		"JZNFNM RVFVM",
		"H]SBLb RYBRb RLOZO RKUYU",
		"H\\PBP_ RTBT_ RYIWGTFPFMGKIKKLMMNOOUQWRXSYUYXWZT[P[MZKX",
		"F^[FI[ RNFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F RWTUUTWTYV[X[ZZ[X[VYTWT",
		"E_\\O\\N[MZMYNXPVUTXRZP[L[JZIYHWHUISJRQNRMSKSIRGPFNGMIMKNNPQUXWZY[[[\\Z\\Y",
		"MWRHQGRFSGSIRKQL",
		"KYVBTDRGPKOPOTPYR]T`Vb",
		"KYNBPDRGTKUPUTTYR]P`Nb",
		"JZRLRX RMOWU RWOMU",
		"E_RIR[ RIR[R",
		"NVSWRXQWRVSWSYQ[",
		"E_IR[R",
		"NVRVQWRXSWRV",
		"G][BIb",
		"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF",
		"H\\NJPISFS[",
		"H\\LKLJMHNGPFTFVGWHXJXLWNUQK[Y[",
		"H\\MFXFRNUNWOXPYSYUXXVZS[P[MZLYKW",
		"H\\UFKTZT RUFU[",
		"H\\WFMFLOMNPMSMVNXPYSYUXXVZS[P[MZLYKW",
		"H\\XIWGTFRFOGMJLOLTMXOZR[S[VZXXYUYTXQVOSNRNOOMQLT",
		"H\\YFO[ RKFYF",
		"H\\PFMGLILKMMONSOVPXRYTYWXYWZT[P[MZLYKWKTLRNPQOUNWMXKXIWGTFPF",
		"H\\XMWPURRSQSNRLPKMKLLINGQFRFUGWIXMXRWWUZR[P[MZLX",
		"NVROQPRQSPRO RRVQWRXSWRV",
		"NVROQPRQSPRO RSWRXQWRVSWSYQ[",
		"F^ZIJRZ[",
		"E_IO[O RIU[U",
		"F^JIZRJ[",
		"I[LKLJMHNGPFTFVGWHXJXLWNVORQRT RRYQZR[SZRY",
		"E`WNVLTKQKOLNMMPMSNUPVSVUUVS RQKOMNPNSOUPV RWKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX RXKWSWUXV",
		"I[RFJ[ RRFZ[ RMTWT",
		"G\\KFK[ RKFTFWGXHYJYLXNWOTP RKPTPWQXRYTYWXYWZT[K[",
		"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZV",
		"G\\KFK[ RKFRFUGWIXKYNYSXVWXUZR[K[",
		"H[LFL[ RLFYF RLPTP RL[Y[",
		"HZLFL[ RLFYF RLPTP",
		"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZVZS RUSZS",
		"G]KFK[ RYFY[ RKPYP",
		"NVRFR[",
		"JZVFVVUYTZR[P[NZMYLVLT",
		"G\\KFK[ RYFKT RPOY[",
		"HYLFL[ RL[X[",
		"F^JFJ[ RJFR[ RZFR[ RZFZ[",
		"G]KFK[ RKFY[ RYFY[",
		"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF",
		"G\\KFK[ RKFTFWGXHYJYMXOWPTQKQ",
		"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF RSWY]",
		"G\\KFK[ RKFTFWGXHYJYLXNWOTPKP RRPY[",
		"H\\YIWGTFPFMGKIKKLMMNOOUQWRXSYUYXWZT[P[MZKX",
		"JZRFR[ RKFYF",
		"G]KFKULXNZQ[S[VZXXYUYF",
		"I[JFR[ RZFR[",
		"F^HFM[ RRFM[ RRFW[ R\\FW[",
		"H\\KFY[ RYFK[",
		"I[JFRPR[ RZFRP",
		"H\\YFK[ RKFYF RK[Y[",
		"KYOBOb RPBPb ROBVB RObVb",
		"KYKFY^",
		"KYTBTb RUBUb RNBUB RNbUb",
		"JZRDJR RRDZR",
		"I[Ib[b",
		"NVSKQMQORPSORNQO",
		"I\\XMX[ RXPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"H[LFL[ RLPNNPMSMUNWPXSXUWXUZS[P[NZLX",
		"I[XPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"I\\XFX[ RXPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"I[LSXSXQWOVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"MYWFUFSGRJR[ ROMVM",
		"I\\XMX]W`VaTbQbOa RXPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"I\\MFM[ RMQPNRMUMWNXQX[",
		"NVQFRGSFREQF RRMR[",
		"MWRFSGTFSERF RSMS^RaPbNb",
		"IZMFM[ RWMMW RQSX[",
		"NVRFR[",
		"CaGMG[ RGQJNLMOMQNRQR[ RRQUNWMZM\\N]Q][",
		"I\\MMM[ RMQPNRMUMWNXQX[",
		"I\\QMONMPLSLUMXOZQ[T[VZXXYUYSXPVNTMQM",
		"H[LMLb RLPNNPMSMUNWPXSXUWXUZS[P[NZLX",
		"I\\XMXb RXPVNTMQMONMPLSLUMXOZQ[T[VZXX",
		"KXOMO[ ROSPPRNTMWM",
		"J[XPWNTMQMNNMPNRPSUTWUXWXXWZT[Q[NZMX",
		"MYRFRWSZU[W[ ROMVM",
		"I\\MMMWNZP[S[UZXW RXMX[",
		"JZLMR[ RXMR[",
		"G]JMN[ RRMN[ RRMV[ RZMV[",
		"J[MMX[ RXMM[",
		"JZLMR[ RXMR[P_NaLbKb",
		"J[XMM[ RMMXM RM[X[",
		"KYTBRCQDPFPHQJRKSMSOQQ RRCQEQGRISJTLTNSPORSTTVTXSZR[Q]Q_Ra RQSSUSWRYQZP\\P^Q`RaTb",
		"NVRBRb",
		"KYPBRCSDTFTHSJRKQMQOSQ RRCSESGRIQJPLPNQPURQTPVPXQZR[S]S_Ra RSSQUQWRYSZT\\T^S`RaPb",
		"F^IUISJPLONOPPTSVTXTZS[Q RISJQLPNPPQTTVUXUZT[Q[O",
		"JZJFJ[K[KFLFL[M[MFNFN[O[OFPFP[Q[QFRFR[S[SFTFT[U[UFVFV[W[WFXFX[Y[YFZFZ[",
	}
},
{
	"Futura Medium",
	0.041,		/* X scale */
	0.041,		/* Y scale */
	0.95,		/* X spacing extra scale */
	0.065,		/* Line width scale */
	0.1,		/* horizontal offset */
	10.5,		/* vertical offset */
	{
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"JZ",
		"MXRFRTST RRFSFST RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"I[NFMGMM RNGMM RNFOGMM RWFVGVM RWGVM RWFXGVM",
		"H]SBLb RYBRb RLOZO RKUYU",
		"I\\RBR_S_ RRBSBS_ RWIYIWGTFQFNGLILKMMNNVRWSXUXWWYTZQZOYNX RWIVHTGQGNHMIMKNMVQXSYUYWXYWZT[Q[NZLXNX RXXUZ",
		"F^[FI[ RNFPHPJOLMMKMIKIIJGLFNFPGSHVHYG[F RWTUUTWTYV[X[ZZ[X[VYTWT",
		"F_[NZO[P\\O\\N[MZMYNXPVUTXRZP[M[JZIXIUJSPORMSKSIRGPFNGMIMKNNPQUXWZZ[[[\\Z\\Y RM[KZJXJUKSMQ RMKNMVXXZZ[",
		"NWSFRGRM RSGRM RSFTGRM",
		"KYVBTDRGPKOPOTPYR]T`Vb RTDRHQKPPPTQYR\\T`",
		"KYNBPDRGTKUPUTTYR]P`Nb RPDRHSKTPTTSYR\\P`",
		"JZRFQGSQRR RRFRR RRFSGQQRR RMINIVOWO RMIWO RMIMJWNWO RWIVINOMO RWIMO RWIWJMNMO",
		"F_RIRZSZ RRISISZ RJQ[Q[R RJQJR[R",
		"MXTZS[R[QZQYRXSXTYT\\S^Q_ RRYRZSZSYRY RS[T\\ RTZS^",
		"E_IR[R",
		"MXRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"G^[BIbJb R[B\\BJb",
		"H\\QFNGLJKOKRLWNZQ[S[VZXWYRYOXJVGSFQF ROGMJLOLRMWOZ RNYQZSZVY RUZWWXRXOWJUG RVHSGQGNH",
		"H\\NJPISFS[ RNJNKPJRHR[S[",
		"H\\LKLJMHNGPFTFVGWHXJXLWNUQL[ RLKMKMJNHPGTGVHWJWLVNTQK[ RLZYZY[ RK[Y[",
		"H\\MFXFQO RMFMGWG RWFPO RQNSNVOXQYTYUXXVZS[P[MZLYKWLW RPOSOVPXS RTOWQXTXUWXTZ RXVVYSZPZMYLW ROZLX",
		"H\\UIU[V[ RVFV[ RVFKVZV RUILV RLUZUZV",
		"H\\MFLO RNGMN RMFWFWG RNGWG RMNPMSMVNXPYSYUXXVZS[P[MZLYKWLW RLOMOONSNVOXR RTNWPXSXUWXTZ RXVVYSZPZMYLW ROZLX",
		"H\\VGWIXIWGTFRFOGMJLOLTMXOZR[S[VZXXYUYTXQVOSNRNOOMQ RWHTGRGOH RPGNJMOMTNXQZ RMVOYRZSZVYXV RTZWXXUXTWQTO RXSVPSOROOPMS RQONQMT",
		"H\\KFYFO[ RKFKGXG RXFN[O[",
		"H\\PFMGLILKMMNNPOTPVQWRXTXWWYTZPZMYLWLTMRNQPPTOVNWMXKXIWGTFPF RNGMIMKNMPNTOVPXRYTYWXYWZT[P[MZLYKWKTLRNPPOTNVMWKWIVG RWHTGPGMH RLXOZ RUZXX",
		"H\\WPURRSQSNRLPKMKLLINGQFRFUGWIXMXRWWUZR[P[MZLXMXNZ RWMVPSR RWNUQRRQRNQLN RPRMPLMLLMIPG RLKNHQGRGUHWK RSGVIWMWRVWTZ RUYRZPZMY",
		"MXRMQNQORPSPTOTNSMRM RRNROSOSNRN RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"MXRMQNQORPSPTOTNSMRM RRNROSOSNRN RTZS[R[QZQYRXSXTYT\\S^Q_ RRYRZSZSYRY RS[T\\ RTZS^",
		"F^ZIJRZ[",
		"F_JM[M[N RJMJN[N RJU[U[V RJUJV[V",
		"F^JIZRJ[",
		"I\\LKLJMHNGQFTFWGXHYJYLXNWOUPRQ RLKMKMJNHQGTGWHXJXLWNUORP RMIPG RUGXI RXMTP RRPRTSTSP RRXQYQZR[S[TZTYSXRX RRYRZSZSYRY",
		"E`WNVLTKQKOLNMMPMSNUPVSVUUVS RQKOMNPNSOUPV RWKVSVUXVZV\\T]Q]O\\L[JYHWGTFQFNGLHJJILHOHRIUJWLYNZQ[T[WZYYZX RXKWSWUXV",
		"H\\RFJ[ RRIK[J[ RRIY[Z[ RRFZ[ RMUWU RLVXV",
		"H\\LFL[ RMGMZ RLFTFWGXHYJYMXOWPTQ RMGTGWHXJXMWOTP RMPTPWQXRYTYWXYWZT[L[ RMQTQWRXTXWWYTZMZ",
		"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZV RZKYKXIWHUGQGOHMKLNLSMVOYQZUZWYXXYVZV",
		"H]LFL[ RMGMZ RLFSFVGXIYKZNZSYVXXVZS[L[ RMGSGVHWIXKYNYSXVWXVYSZMZ",
		"I\\MFM[ RNGNZ RMFYF RNGYGYF RNPTPTQ RNQTQ RNZYZY[ RM[Y[",
		"I[MFM[ RNGN[M[ RMFYF RNGYGYF RNPTPTQ RNQTQ",
		"H]ZKYIWGUFQFOGMILKKNKSLVMXOZQ[U[WZYXZVZRUR RZKYKXIWHUGQGOHNIMKLNLSMVNXOYQZUZWYXXYVYSUSUR",
		"G]KFK[ RKFLFL[K[ RYFXFX[Y[ RYFY[ RLPXP RLQXQ",
		"NWRFR[S[ RRFSFS[",
		"J[VFVVUYSZQZOYNVMV RVFWFWVVYUZS[Q[OZNYMV",
		"H]LFL[M[ RLFMFM[ RZFYFMR RZFMS RPOY[Z[ RQOZ[",
		"IZMFM[ RMFNFNZ RNZYZY[ RM[Y[",
		"F^JFJ[ RKKK[J[ RKKR[ RJFRX RZFRX RYKR[ RYKY[Z[ RZFZ[",
		"G]KFK[ RLIL[K[ RLIY[ RKFXX RXFXX RXFYFY[",
		"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF RQGNHLKKNKSLVNYQZSZVYXVYSYNXKVHSGQG",
		"H\\LFL[ RMGM[L[ RLFUFWGXHYJYMXOWPUQMQ RMGUGWHXJXMWOUPMP",
		"G]PFNGLIKKJNJSKVLXNZP[T[VZXXYVZSZNYKXIVGTFPF RQGNHLKKNKSLVNYQZSZVYXVYSYNXKVHSGQG RSXX]Y] RSXTXY]",
		"H\\LFL[ RMGM[L[ RLFTFWGXHYJYMXOWPTQMQ RMGTGWHXJXMWOTPMP RRQX[Y[ RSQY[",
		"H\\YIWGTFPFMGKIKKLMMNOOTQVRWSXUXXWYTZPZNYMXKX RYIWIVHTGPGMHLILKMMONTPVQXSYUYXWZT[P[MZKX",
		"J[RGR[ RSGS[R[ RLFYFYG RLFLGYG",
		"G]KFKULXNZQ[S[VZXXYUYF RKFLFLUMXNYQZSZVYWXXUXFYF",
		"H\\JFR[ RJFKFRX RZFYFRX RZFR[",
		"E_GFM[ RGFHFMX RRFMX RRIM[ RRIW[ RRFWX R]F\\FWX R]FW[",
		"H\\KFX[Y[ RKFLFY[ RYFXFK[ RYFL[K[",
		"I\\KFRPR[S[ RKFLFSP RZFYFRP RZFSPS[",
		"H\\XFK[ RYFL[ RKFYF RKFKGXG RLZYZY[ RK[Y[",
		"KYOBOb RPBPb ROBVB RObVb",
		"KYKFY^",
		"KYTBTb RUBUb RNBUB RNbUb",
		"G]JTROZT RJTRPZT",
		"H\\Hb\\b",
		"LXPFUL RPFOGUL",
		"H\\WMW[X[ RWMXMX[ RWPUNSMPMNNLPKSKULXNZP[S[UZWX RWPSNPNNOMPLSLUMXNYPZSZWX",
		"H\\LFL[M[ RLFMFM[ RMPONQMTMVNXPYSYUXXVZT[Q[OZMX RMPQNTNVOWPXSXUWXVYTZQZMX",
		"I[XPVNTMQMONMPLSLUMXOZQ[T[VZXX RXPWQVOTNQNOONPMSMUNXOYQZTZVYWWXX",
		"H\\WFW[X[ RWFXFX[ RWPUNSMPMNNLPKSKULXNZP[S[UZWX RWPSNPNNOMPLSLUMXNYPZSZWX",
		"I[MTXTXQWOVNTMQMONMPLSLUMXOZQ[T[VZXX RMSWSWQVOTNQNOONPMSMUNXOYQZTZVYWWXX",
		"LZWFUFSGRJR[S[ RWFWGUGSH RTGSJS[ ROMVMVN ROMONVN",
		"H\\XMWMW\\V_U`SaQaO`N_L_ RXMX\\W_UaSbPbNaL_ RWPUNSMPMNNLPKSKULXNZP[S[UZWX RWPSNPNNOMPLSLUMXNYPZSZWX",
		"H\\LFL[M[ RLFMFM[ RMQPNRMUMWNXQX[ RMQPORNTNVOWQW[X[",
		"NWRFQGQHRISITHTGSFRF RRGRHSHSGRG RRMR[S[ RRMSMS[",
		"NWRFQGQHRISITHTGSFRF RRGRHSHSGRG RRMRbSb RRMSMSb",
		"H[LFL[M[ RLFMFM[ RXMWMMW RXMMX RPTV[X[ RQSX[",
		"NWRFR[S[ RRFSFS[",
		"CbGMG[H[ RGMHMH[ RHQKNMMPMRNSQS[ RHQKOMNONQORQR[S[ RSQVNXM[M]N^Q^[ RSQVOXNZN\\O]Q][^[",
		"H\\LML[M[ RLMMMM[ RMQPNRMUMWNXQX[ RMQPORNTNVOWQW[X[",
		"I\\QMONMPLSLUMXOZQ[T[VZXXYUYSXPVNTMQM RQNOONPMSMUNXOYQZTZVYWXXUXSWPVOTNQN",
		"H\\LMLbMb RLMMMMb RMPONQMTMVNXPYSYUXXVZT[Q[OZMX RMPQNTNVOWPXSXUWXVYTZQZMX",
		"H\\WMWbXb RWMXMXb RWPUNSMPMNNLPKSKULXNZP[S[UZWX RWPSNPNNOMPLSLUMXNYPZSZWX",
		"KYOMO[P[ ROMPMP[ RPSQPSNUMXM RPSQQSOUNXNXM",
		"J[XPWNTMQMNNMPNRPSUUWV RVUWWWXVZ RWYTZQZNY ROZNXMX RXPWPVN RWOTNQNNO RONNPOR RNQPRUTWUXWXXWZT[Q[NZMX",
		"MXRFR[S[ RRFSFS[ ROMVMVN ROMONVN",
		"H\\LMLWMZO[R[TZWW RLMMMMWNYPZRZTYWW RWMW[X[ RWMXMX[",
		"JZLMR[ RLMMMRY RXMWMRY RXMR[",
		"F^IMN[ RIMJMNX RRMNX RRPN[ RRPV[ RRMVX R[MZMVX R[MV[",
		"I[LMW[X[ RLMMMX[ RXMWML[ RXMM[L[",
		"JZLMR[ RLMMMRY RXMWMRYNb RXMR[ObNb",
		"I[VNL[ RXMNZ RLMXM RLMLNVN RNZXZX[ RL[X[",
		"KYUBNRUb",
		"NVRBRb",
		"KYOBVROb",
		"F^IUISJPLONOPPTSVTXTZS[Q RISJQLPNPPQTTVUXUZT[Q[O",
		"JZJFJ[K[KFLFL[M[MFNFN[O[OFPFP[Q[QFRFR[S[SFTFT[U[UFVFV[W[WFXFX[Y[YFZFZ[",
	}
}
};


