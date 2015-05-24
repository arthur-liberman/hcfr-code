
/* 
 * Argyll Color Correction System
 * ChromCast support.
 *
 * Author: Graeme W. Gill
 * Date:   10/9/2014
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License2.txt file for licencing details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include "../h/aconfig.h"
#ifndef SALONEINSTLIB
#include "numlib.h"
#else
#include "numsup.h"
#endif
#include "yajl.h"
#include "conv.h"
#include "base64.h"
#include "ccpacket.h"
#include "ccmes.h"
#include "ccast.h"

#undef DEBUG
#undef CHECK_JSON

#ifdef DEBUG
# define dbgo stdout
# define DBG(xxx) fprintf xxx ;
void cc_dump_bytes(FILE *fp, char *pfx, unsigned char *buf, int len);
#else
# define DBG(xxx) ;
#endif  /* DEBUG */

#define START_TRIES 6
#define LOAD_TRIES 4

/* - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef CHECK_JSON

/* Check if JSON is invalid */
/* Exits if invalid */
static void check_json(char *mesbuf) {
	yajl_val node;
	char errbuf[1024];

	if ((node = yajl_tree_parse(mesbuf, errbuf, sizeof(errbuf))) == NULL) {
		fprintf(dbgo,"yajl_tree_parse of send message failed with '%s'\n",errbuf);
		fprintf(dbgo,"JSON = '%s'\n",mesbuf);
		exit(1);
	}
	yajl_tree_free(node);
}
#endif

/* ============================================================ */
/* Receive thread */

/* Read messages. If they are ones we deal with, send a reply */
/* If they are anonomous (sessionId == 0), then ignore them */
/* (Could save last known anonomous message if they prove useful) */
/* and if they are numbered, keep then in a sorted list. */

static int cc_rec_thread(void *context) {
	ccast *p = (ccast *)context;
	ccmessv *sv = p->messv;
	ccmessv_err merr;
	ccmes mes, smes;
	int errc = 0;
//	char errbuf[1024];
//	yajl_val idn;
	int rv = 0;

	DBG((dbgo,"ccthread starting\n"))

	ccmes_init(&mes);
	ccmes_init(&smes);

	/* Preset PONG message */
	smes.source_id      = "sender-0";
	smes.destination_id = "receiver-0";
	smes.namespace       = "urn:x-cast:com.google.cast.tp.heartbeat";
	smes.binary         = 0;
	smes.data   = (ORD8 *)"{ \"type\": \"PONG\" }";

	for(;!p->stop;) {

		if ((merr = sv->receive(sv, &mes)) != ccmessv_OK) {
			if (merr == ccmessv_timeout) {
				DBG((dbgo,"ccthread: got receive timeout (OK)\n"))
				msec_sleep(100);
			} else {
				DBG((dbgo,"ccthread: messv->receive failed with '%s'\n",ccmessv_emes(merr)))
				msec_sleep(100);
#ifdef NEVER
//This won't work - we need to re-join the session etc.,
				if (p->messv->pk->reconnect(p->messv->pk)) {
					DBG((dbgo,"ccthread: reconnect after error failed\n"))
					rv = 1;
					break;
				}
#endif
				if (errc++ > 20) {		/* Too many failures */
					DBG((dbgo,"ccthread: too many errors - giving up\n"))
					/* Hmm. The connection seems to have gone down ? */
					rv = 1;
					break;
				}
			}
			continue;
		}

		errc = 0;

		/* Got status query */
		if (mes.mtype != NULL && strcmp(mes.mtype, "CLOSE") == 0) {
			/* Hmm. That indicates an error */
		
			DBG((dbgo,"ccthread: got CLOSE message - giving up\n"))
			rv = 1;
			break;

		} else if (mes.mtype != NULL && strcmp(mes.mtype, "PING") == 0) {
			if ((merr = sv->send(sv, &smes)) != ccmessv_OK) {
				DBG((dbgo,"ccthread: send PONG failed with '%s'\n",ccmessv_emes(merr)))
			}
		
		/* Got reply - add to linked list */
		} else {
			int found;
#ifdef DEBUG
			if (p->w_rq) {
				DBG((dbgo,"ccthread: waiting for ns '%s' and mes->ns '%s'\n",
				                                 p->w_rqns, mes.namespace))
				DBG((dbgo,"ccthread: waiting for id %d and mes->id %d\n",
				                                 p->w_rqid, mes.rqid))
			} else {
				DBG((dbgo,"ccthread: has no client waiting\n"))
			}
#endif

			/* Is it the one the client is waiting for ? */
			found = (p->w_rq != 0
				 && (p->w_rqns == NULL || strcmp(p->w_rqns, mes.namespace) == 0)
				 && (p->w_rqid == 0 || p->w_rqid == mes.rqid));

			if (found || mes.rqid != 0) {
				ccmes *nmes;
				if ((nmes = (ccmes *)calloc(1, sizeof(ccmes))) == NULL) {
					DBG((dbgo,"ccthread: calloc failed\n"))
				} else {
					ccmes_transfer(nmes, &mes);
	
					DBG((dbgo,"ccthread: adding message type '%s' id %d to list (found %d)\n",nmes->mtype,nmes->rqid,found))
					amutex_lock(p->rlock);	/* We're modifying p->rmes list */
					nmes->next = p->rmes;	/* Put at start of list */
					p->rmes = nmes; 
	
					/* Client is waiting for this */
					if (found) {
						DBG((dbgo,"ccthread: client was waiting for this message\n"))
						acond_signal(p->rcond);
					}
					amutex_unlock(p->rlock);		/* We've finished modifying p->rmes list */
				}
			}
		}

		/* Got anonomous status message */
		ccmes_empty(&mes);
	}
	DBG((dbgo, "ccthread: about to exit - stop = %d\n",p->stop))

	/* We're bailing out or stopping */
	p->stopped = 1;

	/* Release client if it was waiting */
	amutex_lock(p->rlock);
	if (p->w_rq != 0) { 
		DBG((dbgo,"ccthread: client was waiting for message - abort it\n"))
		acond_signal(p->rcond);
	}
	amutex_unlock(p->rlock);

	DBG((dbgo,"ccthread returning %d\n",rv))

	return rv;
}

/* Wait for a specific message rqid on a specific channel. */
/* Use rqid = 0 to get first message rather than specific one. */
/* Use namespace = NULL to ignore channel */
/* Return 1 on an error */
/* Return 2 on a timeout */
static int get_a_reply_id(ccast *p, char *namespace, int rqid, ccmes *rmes, int to) {
	ccmes *nlist = NULL;
	ccmes *mes, *xmes, *fmes = NULL;
	int rv = 0;

	DBG((dbgo," get_a_reply_id getting namespace '%s' id %d\n",
	            namespace == NULL ? "(none)" : namespace, rqid))

	amutex_lock(p->rlock);		/* We're modifying p->rmes list */

	if (p->stop || p->stopped) {
		amutex_unlock(p->rlock);		/* Allow thread to modify p->rmes list */
		DBG((dbgo," get_a_reply_id: thread is stopping or stopped\n"))
		return 1;
	}

	/* Setup request to thread */
	p->w_rq   = 1;
	p->w_rqns = namespace;
	p->w_rqid = rqid;
	
	/* Until we've got our message, we time out, or the thread is being stopped */
	for (;!p->stop && !p->stopped;) {
	
		/* Check if the message has already been received */
		for (mes = p->rmes; mes != NULL; mes = xmes) {
			int ins = (namespace == NULL || strcmp(namespace, mes->namespace) == 0);
			xmes = mes->next;
			if (ins && rqid != 0 && mes->rqid < rqid) {
				ccmes_del(mes);			/* Too old - throw away */
			} else if (ins && (rqid == 0 || mes->rqid == rqid)) {
				fmes = mes;				/* The one we want */
			} else {
				mes->next = nlist;		/* Keep in list */
				nlist = mes; 
			}
		}
		p->rmes = nlist;

		if (fmes != NULL)
			break;					/* Got it */


#ifndef NEVER
		/* We need to wait until it turns up */
		/* Allow thread to modify p->rmes list and signal us */
		if (acond_timedwait(p->rcond, p->rlock, to) != 0) {
			DBG((dbgo," get_a_reply_id timed out after %f secs\n",to/1000.0))
			rv = 2;
			break;
		}
#else
		acond_wait(p->rcond, p->rlock);
#endif
		DBG((dbgo," get_a_reply_id got released\n"))
	}
	p->w_rq = 0;					/* We're not looking for anything now */
	amutex_unlock(p->rlock);		/* Allow thread to modify p->rmes list */

	if (p->stop || p->stopped) {
		DBG((dbgo," get_a_reply_id failed because thread is stopped or stopping\n"))
		ccmes_init(rmes);
		return 1;
	}

	if (rv != 0) {
		DBG((dbgo," get_a_reply_id returning error %d\n",rv))
	} else {
		ccmes_transfer(rmes, fmes);
		DBG((dbgo," get_a_reply_id returning type '%s' id %d\n",rmes->mtype,rmes->rqid))
	}

	return rv;
}

/* ============================================================ */

void ccast_delete_from_cleanup_list(ccast *p);

/* Cleanup any created objects */
static void cleanup_ccast(ccast *p) {

	DBG((dbgo," cleanup_ccast() called\n"))

	p->stop = 1;		/* Tell the thread to exit */

	/* Wait for thread (could use semaphore) */
	/* and then delete it */
	if (p->rmesth != NULL) {

		while (!p->stopped) {
			msec_sleep(10);
		}
		p->rmesth->del(p->rmesth);
		p->rmesth = NULL;
	}

	if (p->sessionId != NULL) {
		free(p->sessionId);
		p->sessionId = NULL;
	}

	if (p->transportId != NULL) {
		free(p->transportId);
		p->transportId = NULL;
	}

	p->mediaSessionId = 0;

	if (p->messv != NULL) {
		p->messv->del(p->messv);
		p->messv = NULL;
	}

	/* Clean up linked list */
	{
		ccmes *mes, *xmes;
		for (mes = p->rmes; mes != NULL; mes = xmes) {
			xmes = mes->next;
			ccmes_del(mes);
		}
		p->rmes = NULL;
	}
}

/* Shut down the connection, in such a way that we can */
/* try and re-connect. */
static void shutdown_ccast(ccast *p) {
	ccmes mes;

	DBG((dbgo," shutdown_ccast() called\n"))

	ccmes_init(&mes);

	p->stop = 1;		/* Tell the thread to exit */

	/* Close the media channel */
	if (p->transportId != NULL && p->messv != NULL) {
		mes.source_id      = "sender-0";
		mes.destination_id = p->transportId;
		mes.namespace      = "urn:x-cast:com.google.cast.tp.connection";
		mes.binary         = 0;
		mes.data   = (ORD8 *)"{ \"type\": \"CLOSE\" }";
		p->messv->send(p->messv, &mes);
	}

	/* Stop the application */
	if (p->sessionId != NULL && p->messv != NULL) {
		int reqid = ++p->requestId;
		char mesbuf[1024];
		sprintf(mesbuf, "{ \"requestId\": %d, \"type\": \"STOP\", \"sessionId\": \"%s\" }",
			reqid, p->sessionId);
		mes.source_id      = "sender-0";
		mes.destination_id = "receiver-0";
		mes.namespace      = "urn:x-cast:com.google.cast.receiver";
		mes.binary         = 0;
		mes.data   = (ORD8 *)mesbuf;
		p->messv->send(p->messv, &mes);
	}
	
	/* Close the platform channel */
	if (p->messv != NULL) {
		mes.source_id      = "sender-0";
		mes.destination_id = "receiver-0";
		mes.namespace      = "urn:x-cast:com.google.cast.receiver";
		mes.binary         = 0;
		mes.data   = (ORD8 *)"{ \"type\": \"CLOSE\" }";
		p->messv->send(p->messv, &mes);
	}

	cleanup_ccast(p);
}

static void del_ccast(ccast *p) {
	if (p != NULL) {

		shutdown_ccast(p);

		amutex_del(p->rlock);
		acond_del(p->rcond);

		free(p);
	}
}

static int load_ccast(ccast *p, char *url, unsigned char *ibuf, size_t ilen,
                      double bg[3], double x, double y, double w, double h);
void ccast_install_signal_handlers(ccast *p);

/* Startup a ChromCast session */
/* Return nz on error */
static int start_ccast(ccast *p) {
	ccpacket *pk = NULL;
	ccpacket_err perr;
	ccmessv_err merr;
	ccmes mes, rmes;
	char mesbuf[1024];
	int reqid, tries, maxtries = START_TRIES;
	char *connection_chan = "urn:x-cast:com.google.cast.tp.connection";
	char *heartbeat_chan = "urn:x-cast:com.google.cast.tp.heartbeat";
	char *receiver_chan = "urn:x-cast:com.google.cast.receiver";

	/* Try this a few times if we fail in some way */
	for (tries = 0; tries < maxtries; tries++) {
		int app = 0, naps = 2;

		/* Use the default receiver rather than pattern generator */
		if (p->forcedef || getenv("ARGYLL_CCAST_DEFAULT_RECEIVER") != NULL)
			app = 1;

		ccmes_init(&mes);
		ccmes_init(&rmes);

		p->stop = 0;
		p->stopped = 0;
//		p->requestId = 0;

		amutex_init(p->rlock);
		acond_init(p->rcond);

		/* Hmm. Could put creation of pk inside new_ccmessv() ? */
		if ((pk = new_ccpacket()) == NULL) {
			DBG((dbgo,"start_ccast: new_ccpacket() failed\n"))
			goto retry;
		}
		
		if ((perr = pk->connect(pk, p->id.ip, 8009)) != ccpacket_OK) {
			DBG((dbgo,"start_ccast: ccpacket connect failed with '%s'\n",ccpacket_emes(perr)))
			goto retry;
		} 

		DBG((dbgo,"Got TLS connection to '%s\n'",p->id.name))

		if ((p->messv = new_ccmessv(pk)) == NULL) {
			DBG((dbgo,"start_ccast: new_ccmessv() failed\n"))
			goto retry;
		}
		pk = NULL;		/* Will get deleted with messv now */

		/* Attempt a connection */
		mes.source_id      = "sender-0";
		mes.destination_id = "receiver-0";
		mes.namespace      = connection_chan;
		mes.binary         = 0;
		mes.data   = (ORD8 *)"{ \"type\": \"CONNECT\" }";
		if ((merr = p->messv->send(p->messv, &mes)) != ccmessv_OK) {
			DBG((dbgo,"start_ccast: CONNECT failed with '%s'\n",ccmessv_emes(merr)))
			goto retry;
		}

		/* Start the thread. */
		/* We don't want to start this until the TLS negotiations and */
		/* the synchronous ssl_readi()'s it uses are complete. */ 
		if ((p->rmesth = new_athread(cc_rec_thread, (void *)p)) == NULL) {
			DBG((dbgo,"start_ccast: creating message thread failed\n"))
			goto retry;
		}

#ifdef NEVER
		/* Send a ping */
		mes.namespace      = heartbeat_chan;
		mes.data   = (ORD8 *)"{ \"type\": \"PING\" }";
		if ((merr = p->messv->send(p->messv, &mes)) != ccmessv_OK) {
			DBG((dbgo,"start_ccast: PING failed with '%s'\n",ccmessv_emes(merr)))
			return 1;
		}
		
		/* Wait for a PONG */
//	get_a_reply(p->messv, NULL);
#endif

		/* Try and find an app we can work with */
		for (; app < naps; app++) {
			char *appid;

			reqid = ++p->requestId;

			if (app == 0) {
				appid = "B5C2CBFC";		/* Pattern generator reciever */
				p->patgenrcv = 1;
				p->load_delay = 350.0;
			} else {
				appid = "CC1AD845";		/* Default Receiver */
				p->patgenrcv = 0;
				p->load_delay = 1500.0;	/* Actually about 600msec, but fade produces a soft */
					/* transition that instrument meas_delay() doesn't cope with accurately. */
			}

			/* Attempt to launch the Default receiver */
			sprintf(mesbuf, "{ \"requestId\": %d, \"type\": \"LAUNCH\", \"appId\": \"%s\" }",
			              reqid, appid);

			DBG((dbgo,"start_ccast: about to do LAUNCH\n"))
			/* Launch the default application */
			/* (Presumably we would use the com.google.cast.receiver channel */
			/*  for monitoring and controlling the reciever) */
			mes.namespace      = receiver_chan;
			mes.data   = (ORD8 *)mesbuf;
			if ((merr = p->messv->send(p->messv, &mes)) != ccmessv_OK) {
				DBG((dbgo,"start_ccast: LAUNCH failed with '%s'\n",ccmessv_emes(merr)))
				goto retry;
			}

			/* Receive the RECEIVER_STATUS status messages until it is ready to cast */
			/* and get the sessionId and transportId */
			/* We get periodic notification messages (requestId=0) as well as */
			/* a response messages to our requestId */

			/* Wait for a reply to the LAUNCH (15 sec) */
			if (get_a_reply_id(p, receiver_chan, reqid, &rmes, 15000) != 0) {
				DBG((dbgo,"start_ccast: LAUNCH failed to get a reply\n"))
				goto retry;
			}

			if (rmes.mtype != NULL
			 && strcmp(rmes.mtype, "RECEIVER_STATUS") == 0
			 && rmes.tnode != NULL) {
				break;			/* Launched OK */
			}

			if (rmes.mtype == NULL
			 || strcmp(rmes.mtype, "LAUNCH_ERROR") != 0
			 || rmes.tnode == NULL
			 || (app+1) >= naps) {
				DBG((dbgo,"start_ccast: LAUNCH failed to get a RECEIVER_STATUS or LAUNCH ERROR reply\n"))
				ccmes_empty(&rmes);
				goto retry;
			}

			/* Try the next application */
		}

		DBG((dbgo,"start_ccast: LAUNCH soceeded, load delay = %d msec\n",p->load_delay))
		{
			yajl_val idn, tpn;
			if ((idn = yajl_tree_get_first(rmes.tnode, "sessionId", yajl_t_string)) == NULL
			 || (tpn = yajl_tree_get_first(rmes.tnode, "transportId", yajl_t_string)) == NULL) {
				DBG((dbgo,"start_ccast: LAUNCH failed to get sessionId & transportId\n"))
				ccmes_empty(&rmes);
				goto retry;
			}
			p->sessionId = strdup(YAJL_GET_STRING(idn));
			p->transportId = strdup(YAJL_GET_STRING(tpn));
			if (p->sessionId == NULL || p->transportId == NULL) {
				DBG((dbgo,"start_ccast: strdup failed\n"))
				ccmes_empty(&rmes);
				goto retry;
			}
		}
		ccmes_empty(&rmes);

		DBG((dbgo,"### Got sessionId = '%s', transportId = '%s'\n",p->sessionId, p->transportId))

		/* Connect up to the reciever media channels */
		mes.destination_id = p->transportId;
		mes.namespace      = connection_chan;
		mes.data   = (ORD8 *)"{ \"type\": \"CONNECT\" }";
		if ((merr = p->messv->send(p->messv, &mes)) != ccmessv_OK) {
			DBG((dbgo,"messv->send CONNECT failed with '%s'\n",ccmessv_emes(merr)))
			goto retry;
		}

		// Hmm. Should we wait for a RECEIVER_STATUS message with id 0 here ?

		/* If we get here, we assume that we've suceeded */
		break;

      retry:;

		/* If we get here, we're going around again, so start again from scratch */
		if (pk != NULL) {
			pk->del(pk);
			pk = NULL;
		}

		cleanup_ccast(p);

	}

	if (tries >= maxtries) {
		DBG((dbgo,"Failed to start ChromeCast\n"))
		return 1;
	}

	DBG((dbgo,"Succeeded in starting ChromeCast\n"))

	ccast_install_signal_handlers(p);

	return 0;
}



/* Get the extra load delay */
static int get_load_delay(ccast *p) {
	return p->load_delay;
}

/* Return nz if we can send PNG directly as base64 + bg RGB, */
/* else have to setup webserver and send URL */
static int get_direct_send(ccast *p) {
	return p->patgenrcv;
}

/* Create a new ChromeCast */
/* Return NULL on error */
ccast *new_ccast(ccast_id *id,
int forcedef) {
	ccast *p = NULL;

	if ((p = (ccast *)calloc(1, sizeof(ccast))) == NULL) {
		DBG((dbgo, "new_ccast: calloc failed\n"))
		return NULL;
	}

	/* Init method pointers */
	p->del             = del_ccast;
	p->load            = load_ccast;
	p->shutdown        = shutdown_ccast;
	p->get_load_delay  = get_load_delay;
	p->get_direct_send = get_direct_send;

	p->forcedef = forcedef;

	ccast_id_copy(&p->id, id);

	/* Establish communications */
	if (start_ccast(p)) {
		del_ccast(p);
		return NULL;
	}

	return p;
}

/* Load up a URL */
/* Returns nz on error: */
/*	1 send error */
/*	2 receieve error */
/*	3 invalid player state/load failed/load cancelled */
static int load_ccast(
	ccast *p,
	char *url,						/* URL to load, NULL if sent in-line */
	unsigned char *ibuf, size_t ilen,/* PNG image to load, NULL if url */
	double bg[3],					/* Background color RGB */
	double x, double y,				/* Window location and size as prop. of display */
	double w, double h				/* Size as multiplier of default 10% width */
) {
	ccmessv_err merr;
	int reqid, firstid, lastid;
	ccmes mes;
	char *media_chan = "urn:x-cast:com.google.cast.media";
	char *direct_chan = "urn:x-cast:net.hoech.cast.patterngenerator";
	char *receiver_chan = "urn:x-cast:com.google.cast.receiver";
//	char *player_message_chan = "urn:x-cast:com.google.cast.player.message";
	int dchan = 0;		/* Using direct channel */
	int i, maxtries = LOAD_TRIES, rv = 0;

	ccmes_init(&mes);

	/* Retry loop */
	for (i = 0; ; i++) {
		unsigned char *iibuf = ibuf;
		size_t iilen = ilen;
		firstid = lastid = reqid = ++p->requestId;

		DBG((dbgo,"##### load_ccast try %d/%d\n",i+1,maxtries))

		if (p->messv == NULL) {
			DBG((dbgo,"mes->send LOAD failed due to lost connection\n"))

		} else {

			if (url == NULL && !p->patgenrcv) {
				DBG((dbgo,"mes->send not given URL\n"))
				return 1;
			}

			/* Send the LOAD URL command */
			if (url != NULL) {
				char *xl, mesbuf[1024];

				xl = strrchr(url, '.');		// Get extension location 
	
				if (xl != NULL && stricmp(xl, ".webm") == 0) { 
					sprintf(mesbuf, "{ \"requestId\": %d, \"type\": \"LOAD\", \"media\": "
					                "{ \"contentId\": \"%s\",", reqid, url);
					strcat(mesbuf,
					  "\"contentType\": \"video/webm\" },"
					  "\"autplay\": \"true\" }");

				} else if (xl != NULL && stricmp(xl, ".mp4") == 0) { 
					sprintf(mesbuf, "{ \"requestId\": %d, \"type\": \"LOAD\", \"media\": "
					                "{ \"contentId\": \"%s\",", reqid, url);
					strcat(mesbuf,
					  "\"contentType\": \"video/mp4\" },"
					  "\"autplay\": \"true\" }");

				} else {	/* Assume PNG */
					sprintf(mesbuf, "{ \"requestId\": %d, \"type\": \"LOAD\", \"media\": "
					                "{ \"contentId\": \"%s\",", reqid, url);
					strcat(mesbuf,
					"\"contentType\": \"image/png\" } }");
				}

				mes.source_id      = "sender-0";
				mes.destination_id = p->transportId;
				mes.namespace      = media_chan;
				mes.binary         = 0;
				mes.data   = (ORD8 *)mesbuf;
#ifdef CHECK_JSON
				check_json((char *)mes.data);
#endif
				if ((merr = p->messv->send(p->messv, &mes)) != ccmessv_OK) {
					DBG((dbgo,"mes->send LOAD failed with '%s'\n",ccmessv_emes(merr)))
					rv = 1;
					goto retry;		/* Failed */
				}

#ifdef NEVER
			/* Send base64 PNG image & background color definition in one message */
			/* This will fail if the message is > 64K */
			} else if (iibuf != NULL) {
				char *mesbuf, *cp;
				int dlen;

				if ((mesbuf = malloc(1024 + EBASE64LEN(iilen))) == NULL) {
					DBG((dbgo,"mes->send malloc failed\n"))
					return 1;
				}

				cp = mesbuf;
				cp += sprintf(cp, "{ \"requestId\": %d, \"type\": \"LOAD\", \"media\": "
				"{ \"contentId\": \"data:image/png;base64,",reqid);
				ebase64(&dlen, cp, iibuf, iilen);
				cp += dlen;
				DBG((dbgo,"base64 encoded PNG = %d bytes\n",dlen))
				sprintf(cp, "|rgb(%d, %d, %d)|%f|%f|%f|%f\","
				"\"streamType\": \"LIVE\",\"contentType\": \"text/plain\" } }",
				(int)(bg[0] * 255.0 + 0.5), (int)(bg[1] * 255.0 + 0.5), (int)(bg[2] * 255.0 + 0.5),
				x, y, w, h);
		
				mes.source_id      = "sender-0";
				mes.destination_id = p->transportId;
				mes.namespace      = media_chan;
				mes.binary         = 0;
				mes.data   = (ORD8 *)mesbuf;
#ifdef CHECK_JSON
				check_json((char *)mes.data);
#endif
				if ((merr = p->messv->send(p->messv, &mes)) != ccmessv_OK) {
					DBG((dbgo,"mes->send LOAD failed with '%s'\n",ccmessv_emes(merr)))
					free(mesbuf);
					rv = 1;
					goto retry;		/* Failed */
				}
				free(mesbuf);

#else	/* !NEVER */
			/* Send base64 PNG image & background color definition in multiple packets */
			} else if (iibuf != NULL) {
				char *mesbuf, *cp;
				size_t maxlen, meslen;
				size_t enclen, senclen = 0;
				int dlen;			/* Encoded length to send */

				if ((mesbuf = malloc(61 * 1024)) == NULL) {
					DBG((dbgo,"mes->send malloc failed\n"))
					return 1;
				}

				dchan = 1;

				enclen = EBASE64LEN(ilen);			/* Encoded length of whole image */
				maxlen = DBASE64LEN(60 * 1024);		/* Maximum image bytes to send per message */
				meslen = iilen;
				if (meslen > maxlen)	
					meslen = maxlen;
 
				cp = mesbuf;
				cp += sprintf(cp, "{ ");
				cp += sprintf(cp, "\"requestId\": %d,",reqid); 
				cp += sprintf(cp, "\"foreground\": { ");
				cp += sprintf(cp, "\"contentType\": \"image/png\",");
				cp += sprintf(cp, "\"encoding\": \"base64\",");
				cp += sprintf(cp, "\"data\": \"");
				ebase64(&dlen, cp, iibuf, meslen);
				DBG((dbgo,"part base64 encoded PNG = %d bytes\n",dlen))
				iibuf += meslen;
				iilen -= meslen;
				senclen += dlen;
				cp += dlen;
				cp += sprintf(cp, "\",");
				cp += sprintf(cp, "\"size\": %lu",(unsigned long)EBASE64LEN(ilen));
				cp += sprintf(cp, " },");

				cp += sprintf(cp, "\"background\": \"rgb(%d, %d, %d)\",",
				                               (int)(bg[0] * 255.0 + 0.5),
				                               (int)(bg[1] * 255.0 + 0.5),
				                               (int)(bg[2] * 255.0 + 0.5));

				cp += sprintf(cp, "\"offset\": [%f, %f],",x,y);
				cp += sprintf(cp, "\"scale\": [%f, %f]",w,h);
				cp += sprintf(cp, " }");

				mes.source_id      = "sender-0";
				mes.destination_id = p->transportId;
				mes.namespace      = direct_chan;
				mes.binary         = 0;
				mes.data   = (ORD8 *)mesbuf;
#ifdef CHECK_JSON
				check_json((char *)mes.data);
#endif

				if ((merr = p->messv->send(p->messv, &mes)) != ccmessv_OK) {
					DBG((dbgo,"mes->send LOAD failed with '%s'\n",ccmessv_emes(merr)))
					free(mesbuf);
					rv = 1;
					goto retry;		/* Failed */
				}

				/* If we didn't send the whole image in one go */
				while (iilen >  0) {

					/* Send the next chunk */
					meslen = iilen;
					if (meslen > maxlen)	
						meslen = maxlen;

					DBG((dbgo,"Sending %d bytes of %d remaining in image\n",meslen,iilen))

					lastid = reqid = ++p->requestId;

					cp = mesbuf;
					cp += sprintf(cp, "{ ");
					cp += sprintf(cp, "\"requestId\": %d,",reqid); 
					cp += sprintf(cp, "\"foreground\": \"");
					ebase64(&dlen, cp, iibuf, meslen);
					DBG((dbgo,"part base64 encoded PNG = %d bytes\n",dlen))
					iibuf += meslen;
					iilen -= meslen;
					senclen += dlen;


					cp += dlen;
					cp += sprintf(cp, "\" }");
	
					mes.source_id      = "sender-0";
					mes.destination_id = p->transportId;
					mes.namespace      = direct_chan;
					mes.binary         = 0;
					mes.data   = (ORD8 *)mesbuf;
#ifdef CHECK_JSON
					check_json((char *)mes.data);
#endif
					if ((merr = p->messv->send(p->messv, &mes)) != ccmessv_OK) {
						DBG((dbgo,"mes->send LOAD failed with '%s'\n",ccmessv_emes(merr)))
						free(mesbuf);
						rv = 1;
						goto retry;		/* Failed */
					}

					if ((lastid - firstid) > 0) {

						/* Wait for an ACK, to make sure the channel doesn't get choked up */
						if (get_a_reply_id(p, direct_chan, firstid, &mes, 10000) != 0) {
							DBG((dbgo,"load_ccast: failed to get reply\n"))
							rv = 2;
							goto retry;		/* Failed */
						}
						if (mes.mtype == NULL) {
							DBG((dbgo,"load_ccast: mtype == NULL\n"))
							rv = 2;
							goto retry;		/* Failed */
					
						} else if (dchan && strcmp(mes.mtype, "ACK") == 0) {
							DBG((dbgo,"load_ccast: got ACK\n"))
					
						} else if (dchan && strcmp(mes.mtype, "NACK") == 0) {
							/* Failed. Get error status */
							yajl_val errors;
							if ((errors = yajl_tree_get_first(mes.tnode, "errors", yajl_t_array)) != NULL) {
								DBG((dbgo,"NACK returned errors:\n"))
								if (YAJL_IS_ARRAY(errors)) {
									for (i = 0; i < errors->u.array.len; i++) {
										yajl_val error = errors->u.array.values[i];
										DBG((dbgo,"%s\n",error->u.string))
									}
					
								} else {
									DBG((dbgo,"NACK errors is not an array!\n"))
								}
							} else {
								DBG((dbgo,"NACK failed to return errors\n"))
							}
							rv = 2;
							goto retry;		/* Failed */
			
						} else {
							rv = 3;
							DBG((dbgo,"load_ccast: got mtype '%s'\n",mes.mtype))
							goto retry;		/* Failed */
						}
						ccmes_empty(&mes);
						firstid++;
					}
				};
				free(mesbuf);

				/* This would be bad... */
				if (iilen == 0 && senclen != enclen)
					fprintf(stderr,"ccast load finished but senclen %lu != enclen %lu\n",
					                        (unsigned long)senclen,(unsigned long)enclen);
#endif /* !NEVER */

			} else {
				DBG((dbgo,"mes->send not given URL or png data\n"))
				return 1;
			}
		}

		/* Wait for a reply to each load message */
		for (reqid = firstid; reqid <= lastid; reqid++) {
	
			if (get_a_reply_id(p, dchan ? direct_chan : media_chan, reqid, &mes, 5000) != 0) {
				DBG((dbgo,"load_ccast: failed to get reply\n"))
				rv = 2;
				goto retry;		/* Failed */
			}
			/* Reply could be:
				MEDIA_STATUS
				INVALID_PLAYER_STATE,
				LOAD_FAILED,
				LOAD_CANCELLED
		
				For net.hoech.cast.patterngenerator
				ACK,
				NACK
		 	*/
			if (mes.mtype == NULL) {
				DBG((dbgo,"load_ccast: mtype == NULL\n"))
				rv = 2;
				goto retry;		/* Failed */
		
			} else if (dchan && strcmp(mes.mtype, "ACK") == 0) {
				DBG((dbgo,"load_ccast: got ACK\n"))
		
			} else if (dchan && strcmp(mes.mtype, "NACK") == 0) {
				/* Failed. Get error status */
				yajl_val errors;
				if ((errors = yajl_tree_get_first(mes.tnode, "errors", yajl_t_array)) != NULL) {
					DBG((dbgo,"NACK returned errors:\n"))
					if (YAJL_IS_ARRAY(errors)) {
						for (i = 0; i < errors->u.array.len; i++) {
							yajl_val error = errors->u.array.values[i];
							DBG((dbgo,"%s\n",error->u.string))
						}
		
					} else {
						DBG((dbgo,"NACK errors is not an array!\n"))
					}
				} else {
					DBG((dbgo,"NACK failed to return errors\n"))
				}
				rv = 2;
				goto retry;		/* Failed */

			} else if (strcmp(mes.mtype, "MEDIA_STATUS") == 0) {
				yajl_val i;
				if ((i = yajl_tree_get_first(mes.tnode, "mediaSessionId", yajl_t_number)) != NULL) {
					p->mediaSessionId = YAJL_GET_INTEGER(i);
					DBG((dbgo,"MEDIA_STATUS returned mediaSessionId %d\n",p->mediaSessionId))
				} else {
					DBG((dbgo,"MEDIA_STATUS failed to return mediaSessionId\n"))
				}
				/* Suceeded */

			} else {
				rv = 3;
				DBG((dbgo,"load_ccast: got mtype '%s'\n",mes.mtype))
				goto retry;		/* Failed */
			}
			ccmes_empty(&mes);
		}
		/* If we got here, we succeeded */
		break;

	  retry:;

//		if (!p->loaded1 || (i+1) >= maxtries)
		if ((i+1) >= maxtries) {
			return rv;		/* Too many tries - give up */
		}
	
		DBG((dbgo,"load_ccast: failed on try %d/%d - re-connecting to chrome cast\n",i+1,maxtries))
		shutdown_ccast(p);	/* Tear connection down */
		if (start_ccast(p)) {	/* Set it up again */
			DBG((dbgo,"load_ccast: re-connecting failed\n"))
			return 1;
		}
		/* And retry */
		rv = 0;
	}	/* Retry loop */

	/* Success */
	p->loaded1 = 1;		/* Loaded at least once */

	/* Currently there is a 1.5 second fade up delay imposed by */
	/* the base ChromeCast software or default receiver. */
	if (p->load_delay > 0.0)
		msec_sleep(p->load_delay);

	return rv;
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Static list so that all open ChromCast connections can be closed on a SIGKILL */
static ccast *ccast_list = NULL;

/* Clean up any open ChromeCast connections */
static void ccast_cleanup() {
	ccast *pp, *np;

	for (pp = ccast_list; pp != NULL; pp = np) { 
		np = pp->next;
		a1logd(g_log, 2, "ccast_cleanup: closing 0x%x\n",pp);
		pp->shutdown(pp);
	}
}

#ifdef NT
static void (__cdecl *ccast_int)(int sig) = SIG_DFL;
static void (__cdecl *ccast_term)(int sig) = SIG_DFL;
#endif
#ifdef UNIX
static void (*ccast_hup)(int sig) = SIG_DFL;
static void (*ccast_int)(int sig) = SIG_DFL;
static void (*ccast_term)(int sig) = SIG_DFL;
#endif

/* On something killing our process, deal with USB cleanup */
static void ccast_sighandler(int arg) {
	static amutex_static(lock);

	a1logd(g_log, 2, "ccast_sighandler: invoked with arg = %d\n",arg);

	/* Make sure we don't re-enter */
	if (amutex_trylock(lock)) {
		return;
	}

	ccast_cleanup();
	a1logd(g_log, 2, "ccast_sighandler: done ccast_sighandler()\n");

	/* Call the existing handlers */
#ifdef UNIX
	if (arg == SIGHUP && ccast_hup != SIG_DFL && ccast_hup != SIG_IGN)
		ccast_hup(arg);
#endif /* UNIX */
	if (arg == SIGINT && ccast_int != SIG_DFL && ccast_int != SIG_IGN)
		ccast_int(arg);
	if (arg == SIGTERM && ccast_term != SIG_DFL && ccast_term != SIG_IGN) 
		ccast_term(arg);

	a1logd(g_log, 2, "ccast_sighandler: calling exit()\n");

	amutex_unlock(lock);
	exit(0);
}


/* Install the cleanup signal handlers */
void ccast_install_signal_handlers(ccast *p) {

	if (ccast_list == NULL) {
		a1logd(g_log, 2, "ccast_install_signal_handlers: called\n");
#if defined(UNIX)
		ccast_hup = signal(SIGHUP, ccast_sighandler);
#endif /* UNIX */
		ccast_int = signal(SIGINT, ccast_sighandler);
		ccast_term = signal(SIGTERM, ccast_sighandler);
	}

	/* Add it to our static list, to allow automatic cleanup on signal */
	p->next = ccast_list;
	ccast_list = p;
	a1logd(g_log, 6, "ccast_install_signal_handlers: done\n");
}

/* Delete an ccast from our static signal cleanup list */
void ccast_delete_from_cleanup_list(ccast *p) {

	/* Find it and delete it from our static cleanup list */
	if (ccast_list != NULL) {
		a1logd(g_log, 6, "ccast_install_signal_handlers: called\n");
		if (ccast_list == p) {
			ccast_list = p->next;
			if (ccast_list == NULL) {
#if defined(UNIX)
				signal(SIGHUP, ccast_hup);
#endif /* UNIX */
				signal(SIGINT, ccast_int);
				signal(SIGTERM, ccast_term);
			}
		} else {
			ccast *pp;
			for (pp = ccast_list; pp != NULL; pp = pp->next) { 
				if (pp->next == p) {
					pp->next = p->next;
					break;
				}
			}
		}
		a1logd(g_log, 6, "ccast_install_signal_handlers: done\n");
	}
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Quantization model of ChromCast and TV */
/* ctx is a placeholder, and can be NULL */

#define QUANT(xx, qq) (floor((xx) * (qq) + 0.5)/(qq))

/* Input & output RGB 0.0 - 1.0 levels */
void ccastQuant(void *ctx, double out[3], double in[3]) {
	double r = in[0], g = in[1], b = in[2];
	double Y, Cb, Cr;

//	printf("ccastQuant: %f %f %f",r,g,b);

	/* Scale RGB to 8 bit then quantize, since that's the limit of the frame buffer */
	r = floor(r * 255.0 + 0.5);
	g = floor(g * 255.0 + 0.5);
	b = floor(b * 255.0 + 0.5);

	/* ChromCast hardware seems to use 9 bit coeficients */
	Y = r * QUANT((235.0 - 16.0) * 0.2126/255.0, 512.0)
	  + g * QUANT((235.0 - 16.0) * 0.7152/255.0, 512.0)
	  + b * QUANT((235.0 - 16.0) * 0.0722/255.0, 512.0);

	Cb = r * -QUANT((240.0 - 16.0) *        0.2126 /(255.0 * 1.8556), 512.0)
	   + g * -QUANT((240.0 - 16.0) *        0.7152 /(255.0 * 1.8556), 512.0)
	   + b *  QUANT((240.0 - 16.0) * (1.0 - 0.0722)/(255.0 * 1.8556), 512.0);

	Cr = r *  QUANT((240.0 - 16.0) * (1.0 - 0.2126)/(255.0 * 1.5748), 512.0)
	   + g * -QUANT((240.0 - 16.0) *        0.7152 /(255.0 * 1.5748), 512.0)
	   + b * -QUANT((240.0 - 16.0) *        0.0722 /(255.0 * 1.5748), 512.0);

	/* (Don't bother with offsets, since they don't affect quantization) */

	/* Quantize YCbCr to 8 bit, since that's what ChromCast HDMI delivers */
	Y  = floor(Y  + 0.5);
	Cb = floor(Cb + 0.5);
	Cr = floor(Cr + 0.5);

	/* We simply assume that the TV decoding is perfect, */
	/* but is limited to 8 bit full range RGB output */
    r = Y  * 255.0/(235.0 - 16.0)
	  + Cr * (1.574800000 * 255.0)/(240.0 - 16.0);

    g = Y  * 255.0/(235.0 - 16.0)
	  + Cb * (-0.187324273 * 255.0)/(240.0 - 16.0)
	  + Cr * (-0.468124273 * 255.0)/(240.0 - 16.0);

    b = Y  * 255.0/(235.0 - 16.0)
	  + Cb * (1.855600000 * 255.0)/(240.0 - 16.0);

	/* Clip */
	if (r > 255.0)
		r = 255.0;
	else if (r < 0.0)
		r = 0.0;
	if (g > 255.0)
		g = 255.0;
	else if (g < 0.0)
		g = 0.0;
	if (b > 255.0)
		b = 255.0;
	else if (b < 0.0)
		b = 0.0;

	/* We assume that the TV is 8 bit, so */
	/* quantize to 8 bits and return to 0.0 - 1.0 range. */
	r = floor(r + 0.5)/255.0;
	g = floor(g + 0.5)/255.0;
	b = floor(b + 0.5)/255.0;

	out[0] = r;
	out[1] = g;
	out[2] = b;

//	printf(" -> %f %f %f\n",r,g,b);
}

/* Input RGB 8 bit 0 - 255 levels, output YCbCr 8 bit 16 - 235 levels */
/* Non-clipping, non-8 bit quantizing */
void ccast2YCbCr_nq(void *ctx, double out[3], double in[3]) {
	double r = in[0], g = in[1], b = in[2];
	double Y, Cb, Cr;

//	printf("ccast2YCbCr_nq: %f %f %f",r,g,b);

	/* ChromCast hardware seems to use 9 bit coeficients */
	Y = r * QUANT((235.0 - 16.0) * 0.2126/255.0, 512.0)
	  + g * QUANT((235.0 - 16.0) * 0.7152/255.0, 512.0)
	  + b * QUANT((235.0 - 16.0) * 0.0722/255.0, 512.0)
	  + 16.0;

	Cb = r * -QUANT((240.0 - 16.0) *        0.2126 /(255.0 * 1.8556), 512.0)
	   + g * -QUANT((240.0 - 16.0) *        0.7152 /(255.0 * 1.8556), 512.0)
	   + b *  QUANT((240.0 - 16.0) * (1.0 - 0.0722)/(255.0 * 1.8556), 512.0)
	   + 16.0;

	Cr = r *  QUANT((240.0 - 16.0) * (1.0 - 0.2126)/(255.0 * 1.5748), 512.0)
	   + g * -QUANT((240.0 - 16.0) *        0.7152 /(255.0 * 1.5748), 512.0)
	   + b * -QUANT((240.0 - 16.0) *        0.0722 /(255.0 * 1.5748), 512.0)
	   + 16.0;

	out[0] = Y;
	out[1] = Cb;
	out[2] = Cr;

//	printf(" -> %f %f %f\n",out[0],out[1],out[2]);
}

/* Input RGB 8 bit 0 - 255 levels, output YCbCr 8 bit 16 - 235 levels */
void ccast2YCbCr(void *ctx, double out[3], double in[3]) {
	double rgb[3];

//	printf("ccast2YCbCr: %f %f %f",r,g,b);

	/* Quantize RGB to 8 bit, since that's the limit of the frame buffer */
	rgb[0] = floor(in[0] + 0.5);
	rgb[1] = floor(in[1] + 0.5);
	rgb[2] = floor(in[2] + 0.5);

	ccast2YCbCr_nq(ctx, out, rgb);

	/* Quantize YCbCr to 8 bit, since that's what ChromCast HDMI delivers */
	out[0] = floor(out[0] + 0.5);
	out[1] = floor(out[1] + 0.5);
	out[2] = floor(out[2] + 0.5);
//	printf(" -> %f %f %f\n",out[0],out[1],out[2]);
}

/* Input YCbCr 8 bit 16 - 235 levels output RGB 8 bit 0 - 255 levels. */
/* Non-clipping, non-8 bit quantizing */
void YCbCr2ccast_nq(void *ctx, double out[3], double in[3]) {
	double Y = in[0], Cb = in[1], Cr = in[2];
	double r, g, b;

//	printf("YCbCr2ccast_nq: %f %f %f",Y,Cb,Cr);

	Y  -= 16.0;
	Cb -= 16.0;
	Cr -= 16.0;

	/* We simply assume that the TV decoding is perfect, */
	/* but is limited to 8 bit full range RGB output */
    r = Y  * 255.0/(235.0 - 16.0)
	  + Cr * (1.574800000 * 255.0)/(240.0 - 16.0);

    g = Y  * 255.0/(235.0 - 16.0)
	  + Cb * (-0.187324273 * 255.0)/(240.0 - 16.0)
	  + Cr * (-0.468124273 * 255.0)/(240.0 - 16.0);

    b = Y  * 255.0/(235.0 - 16.0)
	  + Cb * (1.855600000 * 255.0)/(240.0 - 16.0);

	out[0] = r;
	out[1] = g;
	out[2] = b;

//	printf(" -> %f %f %f\n",out[0],out[1],out[2]);
}

/* Input YCbCr 8 bit 16 - 235 levels output RGB 8 bit 0 - 255 levels. */
/* Quantize input, and quantize and clip output. */
/* Return nz if the output was clipped */
int YCbCr2ccast(void *ctx, double out[3], double in[3]) {
	double YCbCr[3];
	int k, rv = 0;

//	printf("YCbCr2ccast: %f %f %f",Y,Cb,Cr);

	/* Quantize YCbCr to 8 bit, since that's what ChromCast HDMI delivers */
	YCbCr[0] = floor(in[0] + 0.5);
	YCbCr[1] = floor(in[1] + 0.5);
	YCbCr[2] = floor(in[2] + 0.5);

	YCbCr2ccast_nq(ctx, out, YCbCr);

	/* Quantize and clip to RGB, since we assume the TV does this */
	for (k = 0; k < 3; k++) {
		out[k] = floor(out[k] + 0.5);

		if (out[k] > 255.0) {
			out[k] = 255.0;
			rv = 1;
		} else if (out[k] < 0.0) {
			out[k] = 0.0;
			rv = 1;
		}
	}

//	printf(" -> %f %f %f, clip %d\n",out[0],out[1],out[2],rv);

	return rv;
}

