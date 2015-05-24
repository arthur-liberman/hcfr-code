

/*
 * Class to deal with protobuf messages
 * to/from ChromCast.
 *
 * Author: Graeme W. Gill
 * Date:   3/9/2014
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License2.txt file for licencing details.
 *
 */

#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "../h/aconfig.h"
#include <sys/types.h>
#ifndef SALONEINSTLIB
#include "numlib.h"
#else
#include "numsup.h"
#endif

#include "ssl.h"		/* axTLS header */
#include "yajl.h"
#include "conv.h"
#include "ccpacket.h"
#include "cast_channel.pb-c.h"
#include "ccmes.h"

#undef LOWVERBTRACE		/* Low verboseness message trace */
#undef DEBUG			/* Full message trace + debug */

/* ------------------------------------------------------------------- */

#ifdef DEBUG
# define DBG(xxx) fprintf xxx ;
#else
# define DBG(xxx) ;
#endif  /* DEBUG */
# define dbgo stdout

/* Error message from error number */
char *ccmessv_emes(ccmessv_err rv) {
#ifndef SERVER_ONLY
	if (rv & 0x10000000) {
		return ccmessv_emes(rv & 0x0fffffff);
	} 
#endif

	switch(rv) {
		case ccmessv_OK:
			return "ccmes: OK";
		case ccmessv_context:
			return "ccmes: getting a ssl contextfailed";
		case ccmessv_malloc:
			return "ccmes: malloc failed";
		case ccmessv_connect:
			return "ccmes: connecting to host failed";
		case ccmessv_ssl:
			return "ccmes: ssl connect to host failed";

		case ccmessv_send:
			return "ccmes: message failed to send";
		case ccmessv_recv:
			return "ccmes: failed to receive";
		case ccmessv_unpack:
			return "ccmes: failed to unpack";
		case ccmessv_timeout:
			return "ccmes: i/o has timed out";
		case ccmessv_closed:
			return "ccmes: connection has been closed";
	}

	return "Uknown ccmessv error";
}

#if defined(LOWVERBTRACE) || defined(DEBUG)
static void mes_dump(ccmes *mes, char *pfx) {
#ifdef DEBUG
	fprintf(dbgo,"%s message:\n",pfx);
	fprintf(dbgo,"  source_id = '%s'\n",mes->source_id);
	fprintf(dbgo,"  destination_id = '%s'\n",mes->destination_id);
	fprintf(dbgo,"  namespace = '%s'\n",mes->namespace);
	if (mes->binary) {
		fprintf(dbgo,"  payload =\n");
		cc_dump_bytes(dbgo,"  ", mes->data, mes->bin_len);
	} else {
		fprintf(dbgo,"  payload = '%s'\n",mes->data);
	}
#else
	fprintf(dbgo,"%s '%s'",pfx,mes->namespace); 
#endif
	if (mes->binary) {
#ifdef DEBUG
		fprintf(dbgo,"  %d bytes of binary data:\n",mes->bin_len);
		cc_dump_bytes(dbgo,"  ", mes->data, mes->bin_len);
#else
		fprintf(dbgo,", %d bytes of bin data:\n",mes->bin_len);
#endif
	} else {
		/* Would like to pretty print the JSON data */
		/* ie. convert json_reformat.c to a function */
#ifdef DEBUG
		fprintf(dbgo,"  %d bytes of text data:\n",strlen(mes->data));
		fprintf(dbgo,"  '%s'\n",mes->data);
#else
		yajl_val tnode, v, i;
		int len = strlen((char *)mes->data);
		tnode = yajl_tree_parse((char *)mes->data, NULL, 0);
		if (tnode == NULL) {
			fprintf(dbgo,", %d bytes of text data\n",len);
		} else {
			if ((v = yajl_tree_get_first(tnode, "type", yajl_t_string)) != NULL) {
				char *mtype = YAJL_GET_STRING(v);
				if ((i = yajl_tree_get_first(tnode, "requestId", yajl_t_number)) != NULL) {
					int rqid = YAJL_GET_INTEGER(i);
					fprintf(dbgo,", %d bytes of JSON data, type '%s' id %d\n",
					                                 len,mtype,rqid);
				} else { 
					fprintf(dbgo,", %d bytes of JSON data, type '%s'\n",len,mtype);
				}
			} else {
				fprintf(dbgo,", %d bytes of JSON data\n",len);
			}
		}
#endif
	}
}
#endif

/* Transfer the data from one message to another */
void ccmes_transfer(ccmes *dst, ccmes *src) {
	*dst = *src;	/* Struct copy */
	memset((void *)src, 0, sizeof(*src));
}

/* Initialise a message */
void ccmes_init(ccmes *mes) {
	memset((void *)mes, 0, sizeof(*mes));
}

/* Free up just the contents */
void ccmes_empty(ccmes *mes) {
	if (mes->tnode != NULL)
		yajl_tree_free(mes->tnode);
		
	if (mes->data != NULL)
		free(mes->data);

	memset((void *)mes, 0, sizeof(*mes));
}

/* Free up the message and its contents */
void ccmes_del(ccmes *mes) {
	if (mes != NULL) {
		ccmes_empty(mes);
		free(mes);
	}
}

/* Send a normal raw message */
/* Return ccmessv_err on error */
ccmessv_err send_ccmessv(ccmessv *p, ccmes *mes) {
	ccpacket_err perr;
	ORD8 *buf;
    unsigned int len;
	Extensions__Api__CastChannel__CastMessage msg
	    = EXTENSIONS__API__CAST_CHANNEL__CAST_MESSAGE__INIT;

	if (p->pk == NULL)
		return ccmessv_closed;

#if defined(LOWVERBTRACE) || defined(DEBUG)
	mes_dump(mes, "Send");
#endif

	msg.protocol_version
	         = EXTENSIONS__API__CAST_CHANNEL__CAST_MESSAGE__PROTOCOL_VERSION__CASTV2_1_0;
	msg.source_id      = mes->source_id;
	msg.destination_id = mes->destination_id;
	msg.namespace_     = mes->namespace;

	if (mes->binary) {
		msg.payload_type = EXTENSIONS__API__CAST_CHANNEL__CAST_MESSAGE__PAYLOAD_TYPE__BINARY;
		msg.has_payload_binary = 1;
		msg.payload_binary.len = mes->bin_len;
		msg.payload_binary.data = (uint8_t *)mes->data;
		msg.payload_utf8 = NULL;
	} else {
		msg.payload_type = EXTENSIONS__API__CAST_CHANNEL__CAST_MESSAGE__PAYLOAD_TYPE__STRING;
		msg.payload_utf8 = (char *)mes->data;
		msg.has_payload_binary = 0;
		msg.payload_binary.len = 0;
		msg.payload_binary.data = NULL;
	}
    len = extensions__api__cast_channel__cast_message__get_packed_size(&msg);
        
    if ((buf = malloc(len)) == NULL)
		return ccmessv_malloc;

    extensions__api__cast_channel__cast_message__pack(&msg, buf);

	amutex_lock(p->slock);
	if ((perr = p->pk->send(p->pk, buf, len)) != ccpacket_OK) {
		amutex_unlock(p->slock);
		free(buf);
		if (perr == ccpacket_timeout)
			return ccmessv_timeout;
		return ccmessv_send; 
	}
	amutex_unlock(p->slock);
	free(buf);

	return ccmessv_OK;
}

/* Receive a raw message. ccmes_clear() or ccmes_del() afterwards */
/* Fills in tnode, mtype, rqid */
/* Return ccmessv_err on error */
ccmessv_err receive_ccmessv(ccmessv *p, ccmes *mes) {
	ccpacket_err perr;
	ORD8 *buf;
	unsigned int len;
	Extensions__Api__CastChannel__CastMessage *msg;

	if (p->pk == NULL)
		return ccmessv_closed;

	if ((perr = p->pk->receive(p->pk, &buf, &len)) != ccpacket_OK) {
		if (perr == ccpacket_timeout)
			return ccmessv_timeout;
		return ccmessv_recv; 
	}

	msg = extensions__api__cast_channel__cast_message__unpack(NULL, len, buf);   
	if (msg == NULL)
		return ccmessv_unpack; 

	ccmes_init(mes);

	if ((mes->source_id = strdup(msg->source_id)) == NULL)
		return ccmessv_malloc;
	if ((mes->destination_id = strdup(msg->destination_id)) == NULL)
		return ccmessv_malloc;
	if ((mes->namespace = strdup(msg->namespace_)) == NULL)
		return ccmessv_malloc;

	if (msg->payload_type == EXTENSIONS__API__CAST_CHANNEL__CAST_MESSAGE__PAYLOAD_TYPE__BINARY) {
		mes->binary = 1;
		if ((mes->data = malloc(msg->payload_binary.len)) == NULL)
			return ccmessv_malloc;
		memcpy(mes->data, msg->payload_binary.data, msg->payload_binary.len);
		mes->bin_len = msg->payload_binary.len;
	} else {
		mes->binary = 0;
		if ((mes->data = (ORD8 *)strdup(msg->payload_utf8)) == NULL)
			return ccmessv_malloc;
	}
	extensions__api__cast_channel__cast_message__free_unpacked(msg, NULL);

#if defined(LOWVERBTRACE) || defined(DEBUG)
	mes_dump(mes, "Recv");
#endif

	/* Parse out some information if we can */
	mes->mtype = NULL;
	mes->rqid = 0;
	if (!mes->binary && mes->tnode == NULL) {
		yajl_val tyn, idn;
		char errbuf[1024];

		if ((mes->tnode = yajl_tree_parse((char *)mes->data, errbuf, sizeof(errbuf))) == NULL) {
			DBG((dbgo,"ccthread: yajl_tree_parse failed with '%s'\n",errbuf))
		} else if ((tyn = yajl_tree_get_first(mes->tnode, "type", yajl_t_string)) == NULL) {
			DBG((dbgo,"ccthread: no type\n"))
		} else {
			mes->mtype = YAJL_GET_STRING(tyn);
			if ((idn = yajl_tree_get_first(mes->tnode, "requestId", yajl_t_number)) != NULL)
				mes->rqid = YAJL_GET_INTEGER(idn);
			else {
				DBG((dbgo,"ccthread: no id\n"))
			}
		}
	}
	
	return ccmessv_OK;
}

/* Delete the ccmessv */
void del_ccmessv(ccmessv *p) {
	if (p != NULL) {
		amutex_del(p->slock);
		if (p->pk != NULL)
			p->pk->del(p->pk);
		free(p);
	}
}

/* Create an ccmessv. We take ownership of pk */
/* Return NULL on error */
ccmessv *new_ccmessv(ccpacket *pk) {
	ccmessv *p = NULL;

	if ((p = (ccmessv *)calloc(1, sizeof(ccmessv))) == NULL) {
		DBG((dbgo, "calloc failed\n"))
		return NULL;
	}

	amutex_init(p->slock);

	p->pk = pk;

	/* Init method pointers */
	p->del     = del_ccmessv;
	p->send    = send_ccmessv;
	p->receive = receive_ccmessv;

	return p;
}

