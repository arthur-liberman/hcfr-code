#ifndef CCAST_H
#define CCAST_H

/* 
 * Argyll Color Correction System
 * ChromCast public API support
 *
 * Author: Graeme W. Gill
 * Date:   8/9/2014
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License2.txt file for licencing details.
 *
 */

/*
 * This class provides simple access to the Google ChromeCast
 * for the purposes of generating Video Test patches.
 */

#include "ccmdns.h"		/* Function to get a list of ChromeCasts */

/* A ChromCast instance */
struct _ccast {
	ccast_id id;		/* Id of CromeCast being accessed */
	struct _ccmessv *messv;
	int requestId;

	char *sessionId;
	char *transportId;

	/* Delete the ccast */
	void (*del)(struct _ccast *p);

	/* Load a URL */
	int (*load)(struct _ccast *p, char *url, unsigned char *data, size_t dlen,
		     double bg[3], double x, double y, double w, double h);

	/* Shut it down */
	void (*shutdown)(struct _ccast *p);

	/* Get the extra load delay */
	int (*get_load_delay)(struct _ccast *p);

	/* Return nz if we can send PNG directly as base64 + bg RGB, */
	/* else have to setup webserver and send URL */
	int (*get_direct_send)(struct _ccast *p);

	/* Thread context */
	athread *rmesth;				/* Receive message thread */
	struct _ccmes *rmes;			/* Linked list of received messages */
	amutex rlock;					/* Receive list lock protecting rmes */
	acond rcond;					/* Condition for client to wait on */
	int w_rq;						/* NZ if client is waiting on message */
	char *w_rqns;					/* Message namespac client is waiting for (if !NULL) */
	int w_rqid;						/* Message rqid client is waiting for (0 = any) */
	volatile int stop;				/* Thread stop flag */
	volatile int stopped;			/* Thread reply flag */

	/* Other */
	int mediaSessionId;
	int loaded1;					/* Set to nz if at least one load succeeded */
	int forcedef;					/* Force using default reciever rather than patgen */
	int patgenrcv;					/* nz if pattern generator receiver, else default receiver */
	int load_delay;					/* Delay needed after succesful LOAD */

	struct _ccast *next;			/* Next in static list for signal cleanup */
}; typedef struct _ccast ccast;

/* Create a new ChromeCast */
/* Return NULL on error */
/* nz forcedef to force using default reciever & web server rather than pattern generator app. */
/* Can also set environment variabl "ARGYLL_CCAST_DEFAULT_RECEIVER" to do this. */
ccast *new_ccast(ccast_id *id, int forcedef);

/* Quantization model of ChromCast and TV */
/* ctx is a placeholder, and can be NULL */

/* Input & output RGB 0.0 - 1.0 levels */
void ccastQuant(void *ctx, double out[3], double in[3]);

/* Input RGB 8 bit 0 - 255 levels, output YCbCr 8 bit 16 - 235 levels */
void ccast2YCbCr(void *ctx, double out[3], double in[3]);

/* Input RGB 8 bit 0 - 255 levels, output YCbCr 8 bit 16 - 235 levels */
/* non-quanized or clipped */
void ccast2YCbCr_nq(void *ctx, double out[3], double in[3]);

/* Input YCbCr 8 bit 16 - 235 levels output RGB 8 bit 0 - 255 levels. */
/* Return nz if the output was clipped */
int YCbCr2ccast(void *ctx, double out[3], double in[3]);

/* Input YCbCr 8 bit 16 - 235 levels output RGB 8 bit 0 - 255 levels. */
/* non-quanized or clipped */
void YCbCr2ccast_nq(void *ctx, double out[3], double in[3]);

#define CCDITHSIZE 4

/* Compute pattern [width][height] */
/* return the delta to the target */
double get_ccast_dith(double ipat[CCDITHSIZE][CCDITHSIZE][3], double val[3]);

#define CCAST_H
#endif /* CCAST_H */

