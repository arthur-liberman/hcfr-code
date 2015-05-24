

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
#include "conv.h"
#include "ssl.h"		/* axTLS header */

#undef DEBUG
#undef DUMPSDATA		/* Send data */
#undef DUMPRDATA		/* Receive data */

#if defined(NT) // Windows specific
# if _WIN32_WINNT < 0x0400
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0400 // To make it link in VS2005
# endif
# define WIN32_LEAN_AND_MEAN
# include <winsock2.h>
# include <windows.h>
# include <ws2tcpip.h>

# include <process.h>
# include <direct.h>
# include <io.h>

# define ERRNO GetLastError()
# ifndef ETIMEDOUT
#  define ETIMEDOUT WSAETIMEDOUT
# endif

#else /* !NT = Unix, OS X */

# include <errno.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <sys/time.h>
# include <stdint.h>
# include <inttypes.h>
# include <netdb.h>

# include <pwd.h>
# include <unistd.h>
# include <dirent.h>

#undef SYNC_SSL				/* Use lock to make sure ssl_read & write don't clash. */

typedef int SOCKET;
# define ERRNO errno

# define INVALID_SOCKET -1
# define SOCKET_ERROR -1

#define closesocket(xxx) close(xxx);

#endif

#define CCPACKET_IMPL
#include "ccpacket.h"

/* ------------------------------------------------------------------- */

#ifdef DEBUG
# define dbgo stdout
# define DBG(xxx) fprintf xxx ;
void cc_dump_bytes(FILE *fp, char *pfx, unsigned char *buf, int len);
#else
# define DBG(xxx) ;
#endif  /* DEBUG */

/* Error message from error number */
char *ccpacket_emes(ccpacket_err rv) {
#ifndef SERVER_ONLY
	if (rv & 0x10000000) {
		return ccpacket_emes(rv & 0x0fffffff);
	} 
#endif

	switch(rv) {
		case ccpacket_OK:
			return "Packet: OK";
		case ccpacket_context:
			return "Packet: getting a ssl contextfailed";
		case ccpacket_malloc:
			return "Packet: malloc failed";
		case ccpacket_connect:
			return "Packet: connecting to host failed";
		case ccpacket_ssl:
			return "Packet: ssl connect to host failed";

		case ccpacket_timeout:
			return "Packet:: i/o timed out";
		case ccpacket_send:
			return "Packet: message failed to send";
		case ccpacket_recv:
			return "Packet: failed to read message";
	}

	return "Uknown ccpacket error";
}

/* Establish an ccpacket connection - implementation */
static ccpacket_err connect_ccpacket_imp(
	ccpacket *p
) {
	struct sockaddr_in server;		/* Server address */
	uint8_t sesid[32] = { 0 };
	int rv;

#ifdef NT
    WSADATA data;
    WSAStartup(MAKEWORD(2,2), &data);
#endif

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(p->dip);
	server.sin_port = htons((short)p->dport);

	if ((p->ctx = ssl_ctx_new(SSL_SERVER_VERIFY_LATER, 1)) == NULL) {
		DBG((dbgo, "connect ssl_ctx_new failed\n"))
		return ccpacket_context;
	}

	/* Open socket */
	if ((p->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		DBG((dbgo, "socket() failed with errno %d\n",ERRNO))
		return ccpacket_connect;
	}

	/* Make recieve timeout after 100 msec, and send timeout after 2 seconds */
	{
		struct linger;
#ifdef NT
		DWORD tv;
#ifdef SYNC_SSL
		tv = 100;
#else
		tv = 2000;
#endif
		if ((rv = setsockopt(p->sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,
			                                                   sizeof(tv))) < 0) {
			DBG((dbgo,"setsockopt timout failed with %d, errno %d",rv,ERRNO))
			return ccpacket_connect;
		}
		tv = 2000;
		if ((rv = setsockopt(p->sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv,
			                                                    sizeof(tv))) < 0) {
			DBG((dbgo,"setsockopt timout failed with %d, errno %d",rv,ERRNO))
			return ccpacket_connect;
		}
#else
		struct timeval tv;
#ifdef SYNC_SSL
		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;
#else
		tv.tv_sec = 2;
		tv.tv_usec = 0;
#endif
		if ((rv = setsockopt(p->sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,
			                                                    sizeof(tv))) < 0) {
			DBG((dbgo,"setsockopt timout failed with %d, errno %d",rv,ERRNO))
			return ccpacket_connect;
		}
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		if ((rv = setsockopt(p->sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv,
			                                                    sizeof(tv))) < 0) {
			DBG((dbgo,"setsockopt timout failed with %d, errno %d",rv,ERRNO))
			return ccpacket_connect;
		}
#endif

#ifdef NEVER		/* This stops send timeout working - why ? */
		ling.l_onoff = 1;
		ling.l_linger = 2;	/* Two seconds */
		if ((rv = setsockopt(p->sock, SOL_SOCKET, SO_LINGER, (const char*)&ling,
			                                                    sizeof(ling))) < 0) {
			DBG((dbgo,"setsockopt timout failed with %d, errno %d",rv,ERRNO))
			return ccpacket_connect;
		}
#endif /* NEVER */
	}

	/* Connect */
	if (rv = (connect(p->sock, (struct sockaddr *)&server, sizeof(server))) != 0) {
		DBG((dbgo, "TCP connect IP '%s' port %d failed with %d, errno %d\n",p->dip, p->dport,rv,ERRNO))
		return ccpacket_connect;
	}
	DBG((dbgo, "socket connect IP '%s' port %d success\n",p->dip, p->dport))
	
	/* Establish TLS */
	/* Return nz if we can send PNG directly as base64 + bg RGB, */
	/* else have to setup webserver and send URL */
	if ((p->ssl = ssl_client_new(p->ctx, p->sock, sesid, 32)) == NULL) {
		DBG((dbgo, "connect IP '%s' port %d ssl_ctx_new failed\n",p->dip, p->dport))
		return ccpacket_ssl;
	} 	
	DBG((dbgo, "TLS connect IP '%s' port %d success\n",p->dip, p->dport))
	return ccpacket_OK;
}

/* Establish an ccpacket connection */
static ccpacket_err connect_ccpacket(
	ccpacket *p,
	char *dip,			/* Destination IP address */
	int dport			/* Destination Port number */
) {

	if ((p->dip = strdup(dip)) == NULL)
		return ccpacket_malloc; 
	p->dport = dport;

	return connect_ccpacket_imp(p);
}

static void clear_ccpacket(ccpacket *p);

/* Re-establish communication */
static ccpacket_err re_connect_ccpacket(
	ccpacket *p
) {
	clear_ccpacket(p);
	return connect_ccpacket_imp(p);
}

/* It appears that the axTLS library can't deal with simultanous */
/* send and recieve, due to the sharing of a single buffer and */
/* intricate dependence on this in dealing with handshaking, so */
/* rather than try and fix this limitation, we avoid the problem */
/* with a lock. Note that we need to make sure that a receive */
/* doesn't wait for too long, or it will block sends. */

/* Send a message */
/* Return ccpacket_err on error */
static ccpacket_err send_ccpacket(ccpacket *p,
	ORD8 *buf, ORD32 len		/* Message body to send */
) {
	int lens, rv;
	ORD8 *sbuf;
	ORD32 slen;

	if (p->ssl == NULL)
		return ccpacket_ssl; 

	if ((sbuf = malloc(4 + len)) == NULL) {
		return ccpacket_malloc; 
	}

	write_ORD32_be(len, sbuf);
	memcpy(sbuf+4, buf, len);
	slen = len + 4;

#if defined(DEBUG) && defined(DUMPSDATA)
	printf("send_ccpacket sending packet:\n");
	cc_dump_bytes(dbgo,"  ", sbuf, slen);
#endif

	for (lens = 0; lens < slen; lens += rv) {
		DBG((dbgo, "Sending packet %d bytes\n",slen - lens))
		if (p->ssl == NULL) {
			return ccpacket_ssl; 
		}
#ifdef SYNC_SSL
		amutex_lock(p->lock);
printf("~1 send locked\n");
#endif
		if ((rv = ssl_write(p->ssl, sbuf + lens, slen - lens)) < 0) {
			DBG((dbgo, "TLS send body failed with '%s'\n",ssl_error_string(rv)))
#ifdef SYNC_SSL
printf("~1 send unlocked\n");
			amutex_unlock(p->lock);
#endif
			free(sbuf);
			if (rv == SSL_TIMEDOUT)
				return ccpacket_timeout;
			return ccpacket_send;
		}
#ifdef SYNC_SSL
printf("~1 send unlocked\n");
		amutex_unlock(p->lock);
#endif
	} 
	free(sbuf);

	return ccpacket_OK;
}

/* Receive a message */
/* Return ccpacket_err on error */
static ccpacket_err receive_ccpacket(ccpacket *p,
	ORD8 **pbuf, ORD32 *plen		/* ccpacket received, free after use */
) {
	ORD8 *ibuf;					/* Buffer from ssl_read() */
	int ioff, ilen;				/* Remaining data at ioff lenght ilen in ibuf */
	ORD8 hbuf[4], *rbuf = hbuf;	/* Header buffer, returned buffer */
	int tlen, clen, rlen;		/* Target length, copy length, return length */

	if (p->ssl == NULL)
		return ccpacket_ssl; 

#ifdef SYNC_SSL
	amutex_lock(p->lock);
printf("~1 receive locked\n");
#endif

	/* Until we have 4 bytes for the header */
	for (tlen = 4, rlen = 0; rlen < tlen;) {
		ioff = 0;
		if ((ilen = ssl_read(p->ssl, &ibuf)) < 0) {
			DBG((dbgo, "header recv failed with '%s'\n",ssl_error_string(ilen)))
			if (ilen == SSL_OK)		/* Hmm. */
				continue;
#ifdef SYNC_SSL
printf("~1 receive unlocked\n");
			amutex_unlock(p->lock);
#endif
			if (ilen == SSL_TIMEDOUT)
				return ccpacket_timeout;
			return ccpacket_recv;
		}
		DBG((dbgo, "receive_ccpacket read %d bytes\n",ilen))
		if (ilen == 0)
			continue;
		if ((clen = ilen) > (tlen - rlen))		/* More than we need */
			clen = tlen - rlen;
		memcpy(rbuf + rlen, ibuf + ioff, clen);
		rlen += clen;
		ioff += clen;
		ilen -= clen;
	}
	/* We have ilen left in ibuf at offset ioff */
	DBG((dbgo, "receive_ccpacket %d bytes left over\n",ilen))

	tlen = read_ORD32_be(ibuf);
	DBG((dbgo, "receive_ccpacket expecting %d more bytes\n",tlen))

	if (tlen < 0 || tlen > 64 * 2014) {
#ifdef SYNC_SSL
printf("~1 receive unlocked\n");
		amutex_unlock(p->lock);
#endif
		DBG((dbgo, "receive_ccpacket got bad data length - returning error\n"))
		return ccpacket_recv;
	}

	if ((rbuf = malloc(tlen)) == NULL) {
#ifdef SYNC_SSL
printf("~1 receive unlocked\n");
		amutex_unlock(p->lock);
#endif
		DBG((dbgo, "receive_ccpacket malloc failed\n"))
		return ccpacket_malloc; 
	}
	rlen = 0;

	/* Use any data we have left over from first read */
	if (ilen > 0) {
		if ((clen = ilen) > (tlen - rlen))
			clen = tlen - rlen;
		DBG((dbgo, "receive_ccpacket using %d spair bytesr\n",clen))
		memcpy(rbuf + rlen, ibuf + ioff, clen);
		rlen += clen;
		ioff += clen;
		ilen -= clen;
	}

	/* Get the remainder of the body if we need to */
	for (; rlen < tlen;) {
		ioff = 0;
		if ((ilen = ssl_read(p->ssl, &ibuf)) < 0) {
			DBG((dbgo, "body recv failed with '%s'\n",ssl_error_string(ilen)))
			if (ilen == SSL_OK)
				continue;
#ifdef SYNC_SSL
printf("~1 receive unlocked\n");
			amutex_unlock(p->lock);
#endif
			if (ilen == SSL_TIMEDOUT)
				return ccpacket_timeout;
			return ccpacket_recv;
		}
		DBG((dbgo, "receive_ccpacket read %d bytes\n",ilen))
		if (ilen == 0)
			continue;
		if ((clen = ilen) > (tlen - rlen))
			clen = tlen - rlen;
		memcpy(rbuf + rlen, ibuf + ioff, clen);
		rlen += clen;
		ioff += clen;
		ilen -= clen;
	}
#ifdef SYNC_SSL
printf("~1 receive unlocked\n");
	amutex_unlock(p->lock);
#endif
	if (ilen > 0) {		/* Hmm. We should keep this for the next read ?*/
		DBG((dbgo, " ################## got %d byts left over ###########\n", ilen))
	}
#if defined(DEBUG) && defined(DUMPRDATA)
	printf("receive_ccpacket got:\n");
	cc_dump_bytes(dbgo,"  ", rbuf, rlen);
#endif
	*pbuf = rbuf;
	*plen = rlen;
	
	return ccpacket_OK;
}

/* Clear the ccpacket */
static void clear_ccpacket(ccpacket *p) {
	if (p != NULL) {

		if (p->ssl != NULL) {
			ssl_free(p->ssl);
			p->ssl = NULL;
		}
		if (p->ctx != NULL) {
			ssl_ctx_free(p->ctx);
			p->ctx = NULL;
		}
		if (p->sock != INVALID_SOCKET) {
	        closesocket(p->sock);
			p->sock = 0;
		}
	}
}

/* Delete the ccpacket */
static void del_ccpacket(ccpacket *p) {
	if (p != NULL) {
		clear_ccpacket(p);
		if (p->dip != NULL) {
			free(p->dip);
			p->dip = NULL;
		}
#ifdef SYNC_SSL
		amutex_del(p->lock);
#endif
		free(p);
	}
}

/* Create an ccpacket. */
/* Return NULL on error */
ccpacket *new_ccpacket() {
	ccpacket *p = NULL;

	if ((p = (ccpacket *)calloc(1, sizeof(ccpacket))) == NULL) {
		DBG((dbgo, "calloc failed\n"))
		return NULL;
	}

#ifdef SYNC_SSL
	amutex_init(p->lock);
#endif

	/* Init method pointers */
	p->del       = del_ccpacket;
	p->connect   = connect_ccpacket;
	p->reconnect = re_connect_ccpacket;
	p->send      = send_ccpacket;
	p->receive   = receive_ccpacket;

	return p;
}

