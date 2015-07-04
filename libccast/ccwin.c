
/* 
 * Argyll Color Correction System
 * ChromeCast Display target patch window
 *
 * Author: Graeme W. Gill
 * Date:   8/9/14
 *
 * Copyright 2013, 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */
#include <stdio.h>
#include <string.h>
#ifdef NT
# include <winsock2.h>
#endif
#ifdef UNIX
# include <sys/types.h>
# include <ifaddrs.h>
# include <netinet/in.h> 
# include <arpa/inet.h>
# ifdef __FreeBSD__
#  include <sys/socket.h>
# endif /* __FreeBSD__ */
#endif
#include "../h/aconfig.h"
#include "numsup.h"
#include "conv.h"
#include "mongoose.h"
#include "ccwin.h"
#include "../librender/render.h"

#undef WRITE_PNG		/* [und] Write each test patch to "ccwin.png" */
#undef CCTEST_PATTERN	/* [und] Create dev. spatial test pattern */
#undef SEND_TEST_FILE 	/* [und] Send this file name to ChromeCast instead of pattern */
#undef DO_TIMING       	/* [und] Print rendering timing */
#undef DEBUG			/* [und] */

#define DDITHER 1		/*       0 = no dither - quantize to PNG 8 bit RGB value */
						/* [def] 1 = use error diffusion dithering with ccast quant model */
						/*       2 = use crafted 4x4 dither cell */

#define VWIDTH  1920.0	/* Video stream and display size */
#define VHEIGHT 1080.0

#define IWIDTH  1280.0	/* This is the native still image framebuffer size for ChromeCasts */
#define IHEIGHT 720.0

//#define STANDALONE_TEST

#ifdef DEBUG
#define errout stderr
# define debug(xx)	fprintf(errout, xx )
# define debug2(xx)	fprintf xx
# define debugr(xx)	fprintf(errout, xx )
# define debugr2(xx)	fprintf xx
# define debugrr(xx)	fprintf(errout, xx )
# define debugrr2(xx)	fprintf xx
# define debugrr2l(lev, xx)	fprintf xx
#else
#define errout stderr
# define debug(xx) 
# define debug2(xx)
# define debugr(xx) if (p->ddebug) fprintf(errout, xx ) 
# define debugr2(xx) if (p->ddebug) fprintf xx
# define debugrr(xx) if (callback_ddebug) fprintf(errout, xx ) 
# define debugrr2(xx) if (callback_ddebug) fprintf xx
# define debugrr2l(lev, xx) if (callback_ddebug >= lev) fprintf xx
#endif


static void chws_del(chws *p) {

	if (p->mg != NULL)
		mg_stop(p->mg);

	if (p->cc != NULL)
		p->cc->del(p->cc);

	if (p->ibuf != NULL)
		free(p->ibuf);

	if (p->ws_url != NULL)
		free(p->ws_url);

	free(p);
}

/* Change the .png being served */
/* Return nz on error */
static int chws_update(chws *p, unsigned char *ibuf, size_t ilen, double *bg) {
	char url[200];

	debug("\nUpdate png\n");

	if (p->ibuf != NULL)
		free(p->ibuf);

	p->ibuf = ibuf;
	p->ilen = ilen;

	/* Send the PNG swatch direct */
	if (p->direct) {
		double x, y, w, h;
		/* Convert x,y,w,h to relative rather than pixel size */

		debugr2((errout,"Got x %f y %f w %f h %f\n", p->x, p->y, p->w, p->h));

		// Convert from quantized to direct loader parameters
		if (p->w < IWIDTH)
			x = p->x/(IWIDTH - p->w);
		else	
			x = 0.0;
		if (p->h < IHEIGHT)
			y = p->y/(IHEIGHT - p->h);
		else
			y = 0.0;
		w = p->w/(0.1 * IWIDTH);
		h = p->h/(0.1 * IWIDTH);

		debugr2((errout,"Sending direct x %f y %f w %f h %f\n", x, y, w, h));

		if (p->cc->load(p->cc, NULL, p->ibuf, p->ilen, bg, x, y, w, h)) {
			debugr2((errout,"ccwin_update direct load failed\n"));
			return 1;
		}

	/* Using web server */
	} else {

#ifdef SEND_TEST_FILE
		sprintf(url, "%s%s",p->ws_url, SEND_TEST_FILE);
#else
		sprintf(url, "%stpatch_%d.png",p->ws_url, ++p->pno); 
#endif

		if (p->cc->load(p->cc, url, NULL, 0, NULL,  0.0, 0.0, 0.0, 0.0)) {
			debugr2((errout,"ccwin_update server load failed\n"));
			return 1;
		}
	}
	return 0;
}

/* Web server event handler - return the current .png image */
static void *ccwin_ehandler(enum mg_event event,
                           struct mg_connection *conn) {
	const struct mg_request_info *request_info = mg_get_request_info(conn);
	chws *p = (chws *)mg_get_user_data(conn);
	char *cp;
	char sbuf[200];

	debugr2((errout,"ccwin_ehandler()\n"));

	if (event != MG_NEW_REQUEST) {
		return NULL;
	}

	debugr2((errout,"Event: uri = '%s'\n",request_info->uri));
	if (p->cc->forcedef == 1 && !p->cc->patgenrcv)
		return NULL;
#ifdef SEND_TEST_FILE
#pragma message("############################# ccwin.c SEND_TEST_FILE defined ! ##")
	return NULL;
#endif

	if (p->ibuf != NULL && p->ilen > 0
     && (cp = strrchr(request_info->uri, '.')) != NULL
	 && strcmp(cp, ".png") == 0) { 

		debugr2((errout,"Event: Loading %s\n",request_info->uri));

		debugr2((errout,"Returning current png size %d bytes\n",(int)p->ilen));
		sprintf(sbuf,
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: image/png\r\n"
		    "Content-Length: %d\r\n"
		    "\r\n"
		    ,(int)p->ilen);
		
	    mg_write(conn, sbuf, strlen(sbuf));
	    mg_write(conn, p->ibuf, p->ilen);

	} else {
		debugr2((errout,"Bad request or png - returning 404\n"));
		sprintf(sbuf,
			"HTTP/1.0 404 Not Found\r\n"
		    "\r\n"
			"<html><body><h1>Error 404 - Not Found</h1></body></html>");
		
	    mg_write(conn, sbuf, strlen(sbuf));
	}

	return "yes";
}

chws *new_chws(
ccast_id *cc_id,				/* ChromeCast to open */
double width, double height,	/* Width and height as % */
double hoff, double voff,		/* Offset from center in fraction of screen, range -1.0 .. 1.0 */
BOOL ws							/* Force webserver */
) {
	chws *p;
	const char *options[3];
	char port[50];
	int portno = 0;		/* Port number allocated */
	int forcedef = ws?1:0;	/* Force default reciever app. */

	if ((p = (chws *)calloc(sizeof(chws), 1)) == NULL) {
		error("new_chws: calloc failed");
		return NULL;
	}

	p->update = chws_update;
	p->del = chws_del;

	/* We make sure we round the test patch size and */
	/* location to integer resolution so that we can know */
	/* it's exact relationship to the upsampled pixel locations. */

	/* Setup window size and position */
	/* The default size is 10% of the width */
	p->w = floor(width/100.0 * 0.1 * IWIDTH + 0.5); 
	if (p->w > IWIDTH)
		p->w = IWIDTH;
	p->h = floor(height/100.0 * 0.1 * IWIDTH + 0.5); 
	if (p->h > IHEIGHT)
		p->h = IHEIGHT;

	// Make offset be on an even pixel boundary, so that we know
	// the up-filter phase.
	p->x = floor((hoff * 0.5 + 0.5) * (IWIDTH - p->w) + 0.5);
	p->y = floor((voff * 0.5 + 0.5) * (IHEIGHT - p->h) + 0.5);
	if (((int)p->x) & 1)
		p->x++;
	if (((int)p->y) & 1)
		p->y++;


#ifdef SEND_TEST_FILE
	forcedef = 1;
#endif

	/* Connect to the chrome cast */
	if ((p->cc = new_ccast(cc_id, forcedef)) == NULL) {
		error("new_ccast: failed");
		chws_del(p);
		return NULL;
	}

	p->direct = p->cc->get_direct_send(p->cc);

	if (!p->direct || ws) {
		/* Create a web server */
		options[0] = "listening_ports";
		sprintf(port,"%d", 8081);		/* Use fixed port for Linux firewall rule */
		options[1] = port;
		options[2] = NULL;

		p->mg = mg_start(&ccwin_ehandler, (void *)p, options);
	
		if ((p->ws_url = mg_get_url(p->mg)) == NULL) {
			debugr2((errout, "mg_get_url() failed\n"));
			chws_del(p);
			return NULL;
		}
		if (p->ddebug)
			printf("Created .png server at '%s'\n",p->ws_url);
	}

	return p;
}


/* ================================================================== */





/* ----------------------------------------------- */

/* Change the window color. */
/* Return 1 on error, 2 on window being closed */
static int ccwin_set_color(
dispwin *p,
double r, double g, double b	/* Color values 0.0 - 1.0 */
) {
	chws *ws = (chws *)p->pcntx;
	int j;
	double orgb[3];		/* Previous RGB value */
	int update_delay = 0;

	orgb[0] = p->rgb[0]; p->rgb[0] = r;
	orgb[1] = p->rgb[1]; p->rgb[1] = g;
	orgb[2] = p->rgb[2]; p->rgb[2] = b;

	for (j = 0; j < 3; j++) {
		if (p->rgb[j] < 0.0)
			p->rgb[j] = 0.0;
		else if (p->rgb[j] > 1.0)
			p->rgb[j] = 1.0;
		p->r_rgb[j] = p->s_rgb[j] = p->rgb[j];
		if (p->out_tvenc) {
			p->r_rgb[j] = p->s_rgb[j] = ((235.0 - 16.0) * p->s_rgb[j] + 16.0)/255.0;

			/* For video encoding the extra bits of precision are created by bit shifting */
			/* rather than scaling, so we need to scale the fp value to account for this. */
			if (p->pdepth > 8)
				p->r_rgb[j] = (p->s_rgb[j] * 255 * (1 << (p->pdepth - 8)))
				            /((1 << p->pdepth) - 1.0); 	
		}
	}

	/* This is probably not actually thread safe... */
//	p->ncix++;

#if DDITHER != 1
# pragma message("############################# ccwin.c DDITHER != 1 ##")
#endif

	/* Turn the color into a png file */
	{
		/* We want a raster of IWIDTH x IHEIGHT pixels for web server, */
		/* or p->w x p->h for PNG direct. */
		render2d *r;
		prim2d *rct;
		depth2d depth = bpc8_2d;
#if DDITHER == 1
		int dither = 0x8002;		/* 0x8002 = error diffuse FG only */
#elif DDITHER == 2
		int dither = 0x4000;		/* 0x4000 = no dither but don't average pixels */
									/* so as to allow pattern to come through. */
#else
		int dither = 0;				/* Don't dither in renderer */
#endif
		double hres = 1.0;					/* Resoltion in pix/mm */
		double vres = 1.0;					/* Resoltion in pix/mm */
		double iw, ih;						/* Size of page in mm (pixels) */
		double bg[3];						/* Background color */
		double area,r1l,r2l,r3l;
		color2d c;
		unsigned char *ibuf;		/* Memory image of .png file */
		size_t ilen;
#ifdef DO_TIMING
		int stime;
#endif

		if (ws->direct) {
			iw = ws->w;		/* Requested size */
			ih = ws->h;
		} else {
			iw = IWIDTH;
			ih = IHEIGHT;	/* Size of page in mm */
		}

		area = (iw / 1000. * ih / 1000.);
		r1l = (p->blackbg - area * p->rgb[0]) / (1 - area);
		r2l = (p->blackbg - area * p->rgb[1]) / (1 - area);
		r3l = (p->blackbg - area * p->rgb[2]) / (1 - area);
		if (r1l < 0) r1l = 0;
		if (r2l < 0) r2l = 0;
		if (r3l < 0) r3l = 0;
			bg[0] = r1l;
			bg[1] = r2l;
			bg[2] = r3l;

		if ((r = new_render2d(iw, ih, NULL, hres, vres, rgb_2d,
		     0, depth, dither,
#if DDITHER == 1
			 ccastQuant,
#else
			 NULL,
#endif
			 NULL)) == NULL) {
			error("ccwin: new_render2d() failed");
		}
	
		/* Set the background color */
		c[0] = bg[0];
		c[1] = bg[1];
		c[2] = bg[2];
		r->set_defc(r, c);
	
		c[0] = p->r_rgb[0];
		c[1] = p->r_rgb[1];
		c[2] = p->r_rgb[2];
		if (ws->direct)
			r->add(r, rct = new_rect2d(r, 0.0, 0.0, ws->w, ws->h, c)) ;
		else
			r->add(r, rct = new_rect2d(r, ws->x, ws->y, ws->w, ws->h, c)) ;

#if DDITHER == 2			/* Use dither pattern */
		{
			double rgb[3];
			double dpat[CCDITHSIZE][CCDITHSIZE][3];
			double (*cpat)[MXPATSIZE][MXPATSIZE][TOTC2D];
			int i, j;

			/* Get a chrome cast dither pattern to match target color */
			for (i = 0; i < 3; i++)
				rgb[i] = p->r_rgb[i] * 255.0;
			get_ccast_dith(dpat, rgb);

			if ((cpat = malloc(sizeof(double) * MXPATSIZE * MXPATSIZE * TOTC2D)) == NULL)
				error("ccwin: malloc of dither pattern failed");
			
			for (i = 0; i < CCDITHSIZE; i++) {
				for (j = 0; j < CCDITHSIZE; j++) {
					int k = (((int)IHEIGHT-2) - j) % CCDITHSIZE;	/* Flip to origin bot left */
					(*cpat)[i][k][0] = dpat[i][j][0]/255.0;			/* (HEIGHT-2 is correct!) */
					(*cpat)[i][k][1] = dpat[i][j][1]/255.0;
					(*cpat)[i][k][2] = dpat[i][j][2]/255.0;
				}
			}
			
			set_rect2d_dpat((rect2d *)rct, cpat, CCDITHSIZE, CCDITHSIZE);
		}
#endif /* DDITHER == 2 */

#ifdef CCTEST_PATTERN
#pragma message("############################# ccwin.c TEST_PATTERN defined ! ##")
		if (getenv("ARGYLL_CCAST_TEST_PATTERN") != NULL) {
			verbose(0, "Writing test pattern to '%s'\n","testpattern.png");
			if (r->write(r, "testpattern.png", 1, NULL, NULL, png_file))
				error("ccwin: render->write failed");
		}
#else	/* !CCTEST_PATTERN */
# ifdef WRITE_PNG		/* Write it to a file so that we can look at it */
#  pragma message("############################# spectro/ccwin.c WRITE_PNG is enabled ######")
		if (r->write(r, "ccwin.png", 1, NULL, NULL, png_file))
			error("ccwin: render->write failed");
# endif	/* WRITE_PNG */
#endif	/* !CCTEST_PATTERN */

#ifdef DO_TIMING
		stime = msec_time();
#endif
		if (r->write(r, "MemoryBuf", 1, &ibuf, &ilen, png_mem))
			error("ccwin: render->write failed");
#ifdef DO_TIMING
		stime = msec_time() - stime;
		printf("render->write took %d msec\n",stime);
#endif
		if (ws->update(ws, ibuf, ilen, bg))
			error("ccwin: color update failed");
//		p->ccix = p->ncix;
	}

	/* If update is notified asyncronously ... */
//	while(p->ncix != p->ccix) {
//		msec_sleep(50);
//	}
//printf("#################################################################\n");
//printf("#################     RGB update notified        ################\n");
//printf("#################################################################\n");

	/* Allow for display update & instrument delays */
//	update_delay = dispwin_compute_delay(p, orgb);
	msec_sleep(update_delay);

	return 0;
}

/* Set/unset the blackground color flag */
/* Return nz on error */

static int ccwin_set_bg(dispwin *p, double blackbg) {
	p->blackbg = blackbg;

	return 0;
}



/* Destroy ourselves */
static void ccwin_del(
dispwin *p
) {
	chws *ws;

	if (p == NULL)
		return;

	ws = (chws *)p->pcntx;

	if (ws != NULL)
		ws->del(ws);

	if (p->name != NULL)
		free(p->name);
	if (p->description != NULL)
		free(p->description);
//	if (p->callout != NULL)
//		free(p->callout);

	free(p);
}

/* ----------------------------------------------- */

/* Create a web display test window, default grey */
dispwin *new_ccwin(
ccast_id *cc_id,				/* ChromeCast to open */
double width, double height,	/* Width and height in mm. (TV width assumed to b 1000mm) */
double hoff, double voff,		/* Offset from center in fraction of screen, range -1.0 .. 1.0 */
int out_tvenc,					/* 1 = use RGB Video Level encoding */
double blackbg					/*background ratio */
) {
	dispwin *p = NULL;
	chws *ws = NULL;
	char *ws_url = NULL;

	if ((p = (dispwin *)calloc(sizeof(dispwin), 1)) == NULL) {
		return NULL;
	}

	/* !!!! Make changes in dispwin.c & madvrwin.c as well !!!! */
	p->name = strdup("CCast Window");
	p->width = width;
	p->height = height;
	p->out_tvenc = out_tvenc;
	p->blackbg = blackbg;
	p->set_color           = ccwin_set_color;
	p->set_bg              = ccwin_set_bg;
	p->del                 = ccwin_del;


	p->rgb[0] = p->rgb[1] = p->rgb[2] = 0.1;	/* Set Grey as the initial test color */
	

	p->pdepth = 8;		/* Assume this by API */
	
	/* Basic object is initialised, so create connection to ChromeCast */
	if ((ws = new_chws(cc_id, width, height, hoff, voff, FALSE)) == NULL) {
		return NULL;
	}

	p->ws_url = ws->ws_url;

	/* Extra delay ccast adds after confirming load */
//	p->extra_update_delay = ws->cc->get_load_delay(ws->cc) / 1000.0;

	p->pcntx = (void *)ws;

	/* Create a suitable description */
	{
		char buf[100];
		sprintf(buf,"ChromeCast '%s'",cc_id->name);
		p->description = strdup(buf);
	}

	return p;
}

