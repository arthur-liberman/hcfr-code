
#ifndef PACKET_H

/*
 * Class to deal with TLS connection to ChromCast,
 * and send and recieve packat format data.
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

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum {
	ccpacket_OK = 0,
	ccpacket_malloc,				/* malloc failed */
	ccpacket_context,				/* Getting a ssl context failed */
	ccpacket_connect,				/* Connecting to host failed */
	ccpacket_ssl,					/* Establishing SSL connection to host failed */

	ccpacket_timeout,				/* i/o timed out */
	ccpacket_send,					/* Message failed to send */
	ccpacket_recv					/* Messafe failed to read */
} ccpacket_err;

/* Error message from error number */
char *ccpacket_emes(ccpacket_err rv);

typedef struct _ccpacket {

/* Public: */

	/* Delete the ccpacket */
	void (*del)(struct _ccpacket *p);

	/* Establish an ccpacket connection */
	/* Return ccpacket_err on error */
	ccpacket_err (*connect)(struct _ccpacket *p,
		char *dip,			/* Destination IP address */
		int dport			/* Destination Port number */
	);

	/* Clear the connection and then re-stablish it */
	/* Return ccpacket_err on error */
	ccpacket_err (*reconnect)(struct _ccpacket *p);

	/* Send a message */
	/* Return ccpacket_err on error */
	ccpacket_err (*send)(struct _ccpacket *p,
	      ORD8 *buf, ORD32 len		/* Message body to send */
	);

	/* Receive a message */
	/* Return ccpacket_err on error */
	ccpacket_err (*receive)(struct _ccpacket *p,
	      ORD8 **pbuf, ORD32 *plen		/* ccpacket received, free after use */
	);

#ifdef CCPACKET_IMPL
/* Private */
	char *dip;          /* Destination IP address */
	int dport;          /* Destination Port number */
	SOCKET sock;
	SSL_CTX *ctx;
	SSL *ssl;
	amutex lock;		/* Lock to prevent simultanious send & receive */
#endif

} ccpacket;

/* Create a new ccpacket object */
/* Return NULL on error */
ccpacket *new_ccpacket();

#ifdef __cplusplus
	}
#endif

#define CCPACKET_H
#endif /* CCPACKET_H */
