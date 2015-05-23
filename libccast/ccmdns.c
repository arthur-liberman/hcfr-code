
/* 
 * Argyll Color Correction System
 * ChromCast support
 *
 * Author: Graeme W. Gill
 * Date:   28/8/2014
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

/*
	TTBD:	Ideally we should use the system mDNS resource if it is available,
	and fall back to our mDNS code if not.

	ie. Linux may have nothing, be using avahi-daemon,
	            possibly Zeroconf (== Bonjour ?)
	  Avahi has Bonjour compatibility library ??

	ie. OS X is using mDNSRespo (Bonjour ?)

	MSWin may have Bonjour installed on it ?
	 Has dns-sd command - what's that using (SSDP/UPnP ?)

	  From <http://stackoverflow.com/questions/8659638/how-does-windows-know-how-to-resolve-mdns-queries>

		Bonjour for Windows allows any software using the standard name resolution APIs
		to resolve mDNS names; it does so by registering a DLL (mdnsnsp.dll) as a
		namespace provider using WSCInstallNameSpace.

		The corresponding code is included in the mDNSResponder source (in particular,
		look at the mdnsNSP and NSPTool components).

	To test on Linux:

	  To check what's listening: sudo netstat -anp | grep 5353

	  /etc/init.d/avahi-daemon stop
	  /etc/init.d/avahi-daemon start

	To test on OS X:

	  To check what's listening: sudo netstat -an | grep 5353

	  launchctl unload -w /System/Library/LaunchDaemons/com.apple.mDNSResponder.plist
	  launchctl load -w /System/Library/LaunchDaemons/com.apple.mDNSResponder.plist
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
#include "copyright.h"
#include "aconfig.h"
#include <sys/types.h>
#ifndef SALONEINSTLIB
#include "numlib.h"
#else
#include "numsup.h"
#endif
#include "ccmdns.h"
#include "conv.h"
#undef DEBUG

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
# define UDP_SOCKET_TIMEOUT WSAETIMEDOUT

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

typedef int SOCKET;
# define ERRNO errno

# define INVALID_SOCKET -1
# define SOCKET_ERROR -1
# define UDP_SOCKET_TIMEOUT EAGAIN

#define closesocket(xxx) close(xxx);

#endif

#ifdef DEBUG
# define dbgo stdout
# define DBG(xxx) fprintf xxx ;
void cc_dump_bytes(FILE *fp, char *pfx, unsigned char *buf, int len);
#else
# define DBG(xxx) ;
#endif  /* DEBUG */

/* ================================================================ */

/* mDNS details */
#define DESTINATION_MCAST "224.0.0.251"
#define DESTINATION_MCASTV6 "FF02::FB"
#define SOURCE_PORT 5353 
#define DESTINATION_PORT 5353 

#define BUFSIZE 1024

/* Various DNS defines */
#define DNS_CLASS_IN 0x0001

// Used by ChromeCast
#define DNS_TYPE_PTR  12	/* Domain name pointer				*/
#define DNS_TYPE_TXT  16	/* Text String						*/
#define DNS_TYPE_SRV  33	/* Server selection (SRV) record	*/
#define DNS_TYPE_A	   1	/* Host address (A) record			*/
#define DNS_TYPE_AAAA 28	/* IPv6 address ???					*/
#define DNS_TYPE_NSEC 47	/* DNS Next Secure Name				*/


#ifdef NEVER
/* Print out a V6 address in zero compresed form */
/* (It would be nice to add zero compression) */
static char *print_IPV6_Address(ORD8 *buf) {
	static char sbuf[40], *p = sbuf;
	int i;
	for (i = 0; i < 8; i++) {
		p += sprintf(p, "%x", buf[i * 2 + 1] * 256 + buf[i * 2 + 0]);
		if (i < 7)
		*p++ = ':';
	}
	*p++ = '\000';
	return sbuf;
}
#endif

/* Write a DNS string to a buffer. */
/* return the offset of the next byte after the string */
static int write_string(ORD8 *buf, int off, char *s) {
	int len = strlen(s);
	if (len >= 0xc0)
		len = 0xc0;	// Hmm.
	buf[off] = len;
	off++;
	memcpy(buf + off, s, len);
	off += len;
	return off;
}

/* Return NZ on error */
static int init_mDNS() {

#ifdef NT
	WSADATA data;
	if (WSAStartup(MAKEWORD(2,2), &data)) {
		DBG((dbgo,"WSAStartup failed\n"))
		return 1;
	}
#endif

	return 0;
}

/* Setup the send socket */
/* Return nz on error */
static int init_send_mDNS(SOCKET *psock) {
	int nRet, nOptVal; 
//	int off;
	SOCKET sock; 
	struct sockaddr_in stSourceAddr; 

	DBG((dbgo,"init_send_mDNS() called\n")) 

	/* get a datagram (UDP) socket */ 
	sock = socket(PF_INET, SOCK_DGRAM, 0); 
	if (sock == INVALID_SOCKET) { 
		DBG((dbgo,"opening UDP socked failed with %d\n",ERRNO)) 
		return 1;
	} 

	/*----------------------- to send --------------------------- */

	/* Theoretically, you do not need any special preparation to 
	 * send to a multicast address. However, you may want a few
	 * things to overcome the limits of the default behavior 
	 */

	// If we're doing a one-shot, we shouldn't transmit from port 5353, */
	// but ChromCast won't see the packet if we don't. */

	/* We cant send from port 5353 if someone else is using it, */
	/* so set the SO_REUSEADDR option (which is enough for MSWin), */
	/* and SO_REUSEPORT for OS X and Linux */
	{
		int on = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) {
			DBG((dbgo,"setsockopt(SO_REUSEADDR)  failed with %d\n",ERRNO))
			closesocket(sock);
			return 1;
		}

		/* Need this to be able to open port on Unix like systems */
#ifndef NT
# ifndef SO_REUSEPORT
#  ifdef __APPLE__
#   define SO_REUSEPORT 0x0200 
#  else	/* Linux */
#   define SO_REUSEPORT 15 
#  endif
# endif
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char *)&on, sizeof(on))) {
			DBG((dbgo,"setsockopt(SO_REUSEPORT)  failed with %d\n",ERRNO))
		}
#endif
	}

	/* init source address structure */ 
	stSourceAddr.sin_family = PF_INET; 
	stSourceAddr.sin_port = htons(SOURCE_PORT); 
	stSourceAddr.sin_addr.s_addr = INADDR_ANY; 

	/* 
	 * Calling bind() is not required, but some implementations need it 
	 * before you can reference any multicast socket options
	 * and in this case we must be on port 5353 for ChromeCast to see the packet
	 */ 
	nRet = bind(sock, (struct sockaddr *)&stSourceAddr, 
				 sizeof(struct sockaddr)); 
	if (nRet == SOCKET_ERROR) { 
		DBG((dbgo,"bind failed with %d\n",ERRNO))
		closesocket(sock);
		return 1;
	} 

	/* disable loopback of multicast datagrams we send, since the 
	 * default--according to Steve Deering--is to loopback all
	 * datagrams sent on any interface which is a member of the
	 * destination group address of that datagram.
	 */ 
	nOptVal = 0; 
	nRet = setsockopt (sock, IPPROTO_IP, IP_MULTICAST_LOOP, 
				(char *)&nOptVal, sizeof(int)); 
	if (nRet == SOCKET_ERROR) { 
		/* rather than notifying the user, we make note that this option 
		 * failed. Some WinSocks don't support this option, and default
		 * with loopback disabled), so this failure is of no consequence.
		 * However, if we *do* get loop-backed data, we'll know why
		 */ 
		DBG((dbgo,"[disabling loopback failed with %d]\n",ERRNO)) 
	} 

#ifdef NEVER		// We only want this to be local
	/* increase the IP TTL from the default of one to 64, so our
	 * multicast datagrams can get off of the local network 
	 */
	nOptVal = 64; 
	nRet = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
				 (char *)&nOptVal, sizeof(int)); 
	if (nRet == SOCKET_ERROR) { 
		DBG((dbgo,"increasing TTL failed with %d\n",ERRNO))
		closesocket(sock);
		return 1;
	} 
#endif

	if (psock != NULL)
		*psock = sock;

	DBG((dbgo,"init sending mDNS succeed\n",ERRNO))

	return 0;
}

/* Send an mDNS quesry */
/* on some platforms if we try to transmit & recieve at the same time */
/* Return nz on error */
static int send_mDNS(SOCKET sock) {
	int nRet; 
	int off;
	ORD8 achOutBuf[BUFSIZE]; 
	struct sockaddr_in stDestAddr; 

	DBG((dbgo,"send_mDNS() called\n")) 

	/* Initialize the Destination Address structure and send */ 
	stDestAddr.sin_family = PF_INET; 
	stDestAddr.sin_addr.s_addr = inet_addr(DESTINATION_MCAST);
	stDestAddr.sin_port = htons(DESTINATION_PORT); 

	/* Setup the FQDN data packet to send */
	write_ORD16_be(0, achOutBuf + 0);	/* ID */
	write_ORD16_be(0, achOutBuf + 2);	/* Flags */
	write_ORD16_be(1, achOutBuf + 4);	/* QDCOUNT - one question */
	write_ORD16_be(0, achOutBuf + 6);	/* ANCOUNT */
	write_ORD16_be(0, achOutBuf + 8);	/* NSCOUNT */
	write_ORD16_be(0, achOutBuf + 10);	/* ARCOUNT */
	off = 12;
	off = write_string(achOutBuf, off, "_googlecast");
	off = write_string(achOutBuf, off, "_tcp");
	off = write_string(achOutBuf, off, "local");
	write_ORD8(0, achOutBuf + off);			/* null string */
	off += 1;
	write_ORD16_be(0x000c, achOutBuf + off);	/* QCOUNT */
	off += 2;
	/* Set top bit to get a unicast response (not for ChromeCast though) */
	write_ORD16_be(0x0001, achOutBuf + off);	/* QCLASS */
	off += 2;

	nRet = sendto(sock, (char *)achOutBuf, off,
					0, 
					(struct sockaddr *) &stDestAddr, 
					sizeof(struct sockaddr)); 
	if (nRet == SOCKET_ERROR) { 
		DBG((dbgo,"sending mDNS query failed with %d\n",ERRNO))
		return 1;
	} 

	DBG((dbgo,"sending mDNS query succeed\n",ERRNO))

	return 0;
}

static int parse_dns(char **name, char **ip, ORD8 *buf, int size);

/* Free up what get_ccids returned */
void free_ccids(ccast_id **ids) {
	if (ids != NULL) {
		int i;

		for (i = 0; ids[i] != NULL; i++) {
			free(ids[i]->name);
			free(ids[i]->ip);
			free(ids[i]);
		}
		free(ids);
	}
}

/* Spend the given time waiting for replies. */
/* (Note than on MSWin this will be a minimum of 500msec) */
/* Add any ChromeCast replies to the list. */
/* return nz on error */
static int init_receive_mDNS(SOCKET *psock) {
	int nRet; 
//	int off;
	struct sockaddr_in stSourceAddr; 
	struct ip_mreq stIpMreq; 
	SOCKET sock;

	DBG((dbgo,"init_receive_mDNS() called\n"))

	/* get a datagram (UDP) socket */ 
	sock = socket(PF_INET, SOCK_DGRAM, 0); 
	if (sock == INVALID_SOCKET) { 
		DBG((dbgo,"opening UDP socked failed with %d\n",ERRNO)) 
		return 1;
	} 

	/* We can't receive from port 5353 if someone else is using it, */
	/* so set the SO_REUSEADDR option (which is enough for MSWin), */
	/* and SO_REUSEPORT for OS X and Linux */
	{
		int on = 1, off = 0;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on))) {
			DBG((dbgo,"setsockopt(SO_REUSEADDR)  failed with %d\n",ERRNO))
			closesocket(sock);
			return 1;
		}

		/* Need this to be able to open port on Unix like systems */
#ifndef NT
# ifndef SO_REUSEPORT
#  ifdef __APPLE__
#   define SO_REUSEPORT 0x0200 
#  else	/* Linux */
#   define SO_REUSEPORT 15 
#  endif
# endif
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char *)&on, sizeof(on))) {
			DBG((dbgo,"setsockopt(SO_REUSEPORT)  failed with %d\n",ERRNO))
		}
#endif
	}

	/* init source address structure */ 
	stSourceAddr.sin_family = PF_INET; 
	stSourceAddr.sin_port = htons(SOURCE_PORT); 
	stSourceAddr.sin_addr.s_addr = INADDR_ANY; 

	/* 
	 * Calling bind() is not required, but some implementations need it 
	 * before you can reference any multicast socket options
	 * and in this case we must be on port 5353.
	 */ 
	nRet = bind(sock, (struct sockaddr *)&stSourceAddr, 
				sizeof(struct sockaddr)); 
	if (nRet == SOCKET_ERROR) { 
		DBG((dbgo,"bind failed with %d\n",ERRNO))
		closesocket(sock);
		return 1;
	} 

	/* join the multicast group we want to receive datagrams from */ 
	stIpMreq.imr_multiaddr.s_addr = inet_addr(DESTINATION_MCAST); /* group addr */ 
	stIpMreq.imr_interface.s_addr = INADDR_ANY; /* use default */ 
	nRet = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
					(char *)&stIpMreq, sizeof (struct ip_mreq));

	if (nRet == SOCKET_ERROR) { 
		DBG((dbgo,"registering for read events failed with %d\n",ERRNO))
		closesocket(sock);
		return 1;
	} 

	/* Make this timeout after 100 msec second */
#ifdef NT
	{
		DWORD tv;
		tv = 100;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
			DBG((dbgo,"setsockopt timout failed with %d\n",ERRNO))
			closesocket(sock);
			return 1;
		}
	}
#else
	{
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
			DBG((dbgo,"setsockopt timout failed with %d\n",ERRNO))
			closesocket(sock);
			return 1;
		}
	}
#endif

	if (psock != NULL)
		*psock = sock;

	return 0;
}

/* Spend the given time waiting for replies. */
/* (Note than on MSWin this will be a minimum of 500msec) */
/* Add any ChromeCast replies to the list. */
/* return nz on error & free *ids */
static int receive_mDNS(SOCKET sock, ccast_id ***ids, int emsec) {
	int nids = 0;
	unsigned int smsec;
	int nSize;
	int size;
	ORD8 achInBuf[BUFSIZE]; 
//	struct sockaddr_in stSourceAddr; 

	/* Count the number of current id's */
	if (*ids != NULL) {
		for (nids = 0; (*ids)[nids] != NULL; nids++)
			;
	}

	DBG((dbgo,"receive_mDNS() called with %d ids\n",nids))

	for (smsec = msec_time(), emsec += smsec;msec_time() <= emsec;) {
		int i;
		char *name, *ip;
		struct sockaddr stSockAddr; 

		/* Recv the available data */ 
		nSize = sizeof(struct sockaddr); 
		size = recvfrom(sock, (char *)achInBuf, 
						BUFSIZE, 0, (struct sockaddr *) &stSockAddr, &nSize); 
		if (size == SOCKET_ERROR) { 
			if (ERRNO == UDP_SOCKET_TIMEOUT)
				continue;			/* Timeout */
			DBG((dbgo,"recvfrom failed with %d\n",ERRNO))
			free_ccids(*ids);
			*ids = NULL;
			return 1;
		}
		DBG((dbgo,"Got mDNS message length %d bytes\n",size))
#ifdef DEBUG
		cc_dump_bytes(dbgo, "    ", achInBuf, size);
#endif

		if (parse_dns(&name, &ip, achInBuf, size) != 0) {
			DBG((dbgo,"Failed to parse the reply\n"))
		} else {
			DBG((dbgo,"Parsed reply OK\n"))

//if (*pnids > 0) {
//	name = strdup("Argyll1234");
//	ip = strdup("10.0.0.129");
//}
			/* If we found an entry */
			if (name != NULL && ip != NULL) {
				DBG((dbgo,"Got a name '%s' & IP '%s'\n",name,ip))
				/* Check if it is a duplcate */
				for (i = 0; i < nids; i++) {
					if (strcmp((*ids)[i]->name, name) == 0
					 && strcmp((*ids)[i]->ip, ip) == 0)
						break;	/* yes */
				} 
				if (i < nids) {
					DBG((dbgo,"Duplicate\n"))
					free(name);
					free(ip);
				} else {
					ccast_id **tids;
					if ((tids = realloc(*ids, (nids + 2) * sizeof(ccast_id *))) == NULL
					 || (*ids = tids, (*ids)[nids] = malloc(sizeof(ccast_id)), (*ids)[nids]) == NULL) {
						DBG((dbgo,"realloc/malloc fail\n"))
						free(name);
						free(ip);
						free_ccids(*ids);
						*ids = NULL;
						return 1;
					} else {
						DBG((dbgo,"Adding entry\n"))
						(*ids)[nids]->name = name;
						(*ids)[nids]->ip = ip;
						(*ids)[++nids] = NULL;		/* End marker */
					}
				}
			}
		}
	}

	DBG((dbgo,"receive_mDNS() returning %d in list\n",nids))

	return 0;
}

/* ==================================================================== */

/* Get a list of Chromecasts. Return NULL on error */
/* Last pointer in array is NULL */
/* Takes 0.5 second to return */
ccast_id **get_ccids() {
	ccast_id **ids = NULL;
	int i, j;
	unsigned int smsec;
	SOCKET ssock, rsock;

	if (init_mDNS()) {
		DBG((dbgo,"init_mDNS() failed\n"))
		return NULL;
	}

	if (init_send_mDNS(&ssock)) {
		DBG((dbgo,"init_send_mDNS() failed\n"))
		return NULL;
	}

	if (init_receive_mDNS(&rsock)) {
		DBG((dbgo,"init_receive_mDNS() failed\n"))
		closesocket(ssock);
		return NULL;
	}

	smsec = msec_time();

	DBG((dbgo,"Sending mDNS query:\n"))
	if (send_mDNS(ssock)) {
		DBG((dbgo,"send_mDNS() #1 failed\n"))
		closesocket(ssock);
		closesocket(rsock);
		return NULL;
	}

	if (receive_mDNS(rsock, &ids, 100)) {
		DBG((dbgo,"receive_mDNS() #1 failed\n"))
		closesocket(ssock);
		closesocket(rsock);
		return NULL;
	}

	if (ids == NULL && (msec_time() - smsec) < 200) {

		DBG((dbgo,"Sending another mDNS query:\n"))
		if (send_mDNS(ssock)) {
			DBG((dbgo,"send_mDNS() #2 failed\n"))
			closesocket(ssock);
			closesocket(rsock);
			return NULL;
		}

		if (receive_mDNS(rsock, &ids, 500)) {
			DBG((dbgo,"receive_mDNS() #2 failed\n"))
			closesocket(ssock);
			closesocket(rsock);
			return NULL;
		}
	}

	if (ids == NULL) {

		DBG((dbgo,"Sending a final mDNS query:\n"))
		if (send_mDNS(ssock)) {
			DBG((dbgo,"send_mDNS() #3 failed\n"))
			closesocket(ssock);
			closesocket(rsock);
			return NULL;
		}

		if (receive_mDNS(rsock, &ids, 500)) {
			DBG((dbgo,"receive_mDNS() #3 failed\n"))
			closesocket(ssock);
			closesocket(rsock);
			return NULL;
		}
	}

	closesocket(ssock);
	closesocket(rsock);

	/* If no ChromCasts found, return an empty list */
	if (ids == NULL) {
		if ((ids = calloc(sizeof(ccast_id *), 1)) == NULL) {
			DBG((dbgo,"calloc fail\n"))
			return NULL;
		}
	}
	/* Sort the results so that it is stable */
	for (i = 0; ids[i] != NULL && ids[i+1] != NULL; i++) {
		for (j = i+1; ids[j] != NULL; j++) {
			if (strcmp(ids[i]->name, ids[j]->name) > 0) {
				ccast_id *tmp;
				tmp = ids[i];
				ids[i] = ids[j];
				ids[j] = tmp;
			}
		}
	}

	return ids;
}

void ccast_id_copy(ccast_id *dst, ccast_id *src) {
	dst->name = strdup(src->name);
	dst->ip = strdup(src->ip);
}

ccast_id *ccast_id_clone(ccast_id *src) {
	ccast_id *dst;

	if ((dst = calloc(sizeof(ccast_id), 1)) == NULL)
		return dst;

	if (src->name != NULL && (dst->name = strdup(src->name)) == NULL) {
		free(dst);
		return NULL;
	}
	if (src->ip != NULL && (dst->ip = strdup(src->ip)) == NULL) {
		free(dst->name);
		free(dst);
		return NULL;
	}
	return dst;
}

void ccast_id_del(ccast_id *id) {
	free(id->name);
	free(id->ip);
	free(id);
}

/* ==================================================================== */

/* Read a string with a trailing '.' added. */
/* Return the next offset or -1 and *rv = NULL on error */
static int read_string_imp(char **rv, int *slen, ORD8 *buf, int off, int size, int rec) {
	int len;
	int d1 = 0;

//printf("~1 read_string_imp called for off 0x%x rec %d\n",off,rec);

	if (rec > 10)		/* Too many recursions */
		return -1;		/* Error */

	if (off >= size)
		return -1;		/* Error */

	for (;;) {
//printf("~1 top of loop at off 0x%x\n",off);
		len = buf[off];
		if (len == 0xc0) {		/* Is it an offset marker */
			int poff;
//printf("~1 got pointer\n");
			if ((size - off) < 2)
				return -1;

			poff = read_ORD16_be(buf + off); off += 2;
			poff -= 0xc000;

			if (poff < 0 || poff >= size)
				return -1;

			read_string_imp(rv, slen, buf, poff, size, rec+1);
//if (slen != NULL) printf("~1 after recurse, slen = %d, off = 0x%x\n",*slen,off);

			break;				/* we're done */

		} else {
//printf("~1 got string length %d\n",len);
			off++;
			if ((off + len) >= size)
				return -1;

			if (len == 0)
				break;				/* we're done */

			if (rv != NULL) {
				memcpy(*rv, buf + off, len);
//(*rv)[len] = '\000'; printf("Copied string %p = '%s'\n",*rv,*rv);

				*rv += len;
			}
			off += len;

			if (slen != NULL) {
				(*slen) += len;
//printf("~1 slen now = %d\n",*slen);
			}

		}
		d1 = 1;

		if (slen != NULL)
			(*slen)++;

		if (rv != NULL) {
			(*rv)[0] = '.';
			(*rv)++;
		}
	}
//if (slen != NULL) printf("~1 returning slen = %d\n",*slen);
	return off;
}

/* Read a string */
/* Return the next offset or -1 and *rv = NULL on error */
static int read_string(char **rv, ORD8 *buf, int off, int size) {
	int len = 0, toff = off;
	char *trv;

//printf("~1 read_string called for off 0x%x\n",off);

	/* See how long it will be */
	if ((toff = read_string_imp(NULL, &len, buf, off, size, 0)) < 0) {
//printf("~1 read_string_imp length returned error\n");
		return toff;
	}
//printf("~1 read_string_imp got length %d\n",len);

	if (len == 0) {
//printf("~1 Got zero length string\n");
		len++;			/* Room for null string */
	}
	if ((*rv = trv = malloc(len)) == NULL) {
		return -1;
	}
//printf("Malloced %p\n",*rv);
	off = read_string_imp(&trv, NULL, buf, off, size, 0);
	if (off >= 0) {
		(*rv)[len-1] = '\000';
//printf("~1 read string ok: %p = '%s'\n",*rv, *rv);
	}
//else printf("~1 reading string failed\n");
	return off;
}
 
/* Parse an mDNS query */
/* Return updated off value or -1 on error */
int parse_query(ORD8 *buf, int off, int size) {
	char *sv;
	int qtype, qclass;

	DBG((dbgo,"Parsing query at 0x%x:\n",off))
	if ((off = read_string(&sv, buf, off, size)) < 0)
		return -1;
	DBG((dbgo," Got string '%s'\n",sv))
	free(sv);

	if ((size - off) < 2)
		return -1;
	qtype = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," QTYPE = 0x%04x\n",qtype))

	if ((size - off) < 2)
		return -1;
	qclass = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," QCLASS = 0x%04x\n",qtype))

	return off;
}

/* Parse an mDNS reply */
/* Return updated off value or -1 on error */
int parse_reply(char **pname, char **pip, ORD8 *buf, int off, int size) {
	char *sv;
	int rtype, rclass, rdlength;
	unsigned int ttl;

	DBG((dbgo,"Parsing reply at 0x%x:\n",off))
	if ((off = read_string(&sv, buf, off, size)) < 0)
		return -1;

	DBG((dbgo," Got string '%s'\n",sv))

	if ((size - off) < 2) {
		free(sv);
		return -1;
	}
	rtype = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," RTYPE = %d\n",rtype))

	if ((size - off) < 2) {
		free(sv);
		return -1;
	}
	rclass = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," RCLASS = 0x%04x\n",rclass))
	/* rclass top bit is cache flush bit */

	if ((rclass & 0x7fff) != DNS_CLASS_IN) {
		DBG((dbgo," response has RCLASS != 0x%04x\n",DNS_CLASS_IN))
		free(sv);
		return -1;
	}

	if ((size - off) < 4)
		return -1;
	ttl = read_ORD32_be(buf + off); off += 4;
	DBG((dbgo," TTL = %u\n",ttl))

	if ((size - off) < 2)
		return -1;
	rdlength = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," RDLENGTH = %d\n",rdlength))

	if ((off + rdlength) > size) {
		DBG((dbgo," response RDLENGTH is longer than remaining buffer (%d)\n",size - off))
		free(sv);
		return -1;
	}
	
	/* Just decode the replies we need */
	if (rtype == DNS_TYPE_TXT) {	/* Check it's a ChromeCast & get its name */
		char *cp;
		if ((cp = strchr(sv, '.')) == NULL) {
			free(sv);
			return -1;
		}
		*cp = '\000';
		if (strcmp(cp+1, "_googlecast._tcp.local") != 0) {
			DBG((dbgo,"Not a chromecast (got '%s')\n",cp+1))
			free(sv);
			return -1;
		}

		DBG((dbgo," Chromacast '%s'\n", sv))
		if ((*pname = strdup(sv)) == NULL) {
			DBG((dbgo,"strdup failed\n"))
			free(sv);
			return -1;
		}
	} else if (rtype == DNS_TYPE_A) {
		/* Should we check name matches ? */
		if ((*pip = malloc(3 * 4 + 3 + 1)) == NULL) {
			DBG((dbgo,"malloc failed\n"))
			free(*pname);
			free(sv);
		}
		sprintf(*pip, "%d.%d.%d.%d", buf[off], buf[off+1], buf[off+2], buf[off+3]);
		DBG((dbgo," V4 address = %s\n",*pip))
	
	} else if (rtype == DNS_TYPE_AAAA) {		/* The IPV6 address */
		/* Should we check name matches ? */
		if ((*pip = malloc(8 * 4 + 7 + 1)) == NULL) {
			DBG((dbgo,"malloc failed\n"))
			free(*pname);
			free(sv);
		}
		sprintf(*pip, "%x:%x:%x:%x:%x:%x:%x:%x",
		buf[off+0] * 245 + buf[off+1],
		buf[off+2] * 245 + buf[off+3],
		buf[off+4] * 245 + buf[off+5],
		buf[off+6] * 245 + buf[off+7],
		buf[off+8] * 245 + buf[off+9],
		buf[off+10] * 245 + buf[off+11],
		buf[off+12] * 245 + buf[off+13],
		buf[off+14] * 245 + buf[off+15]);
		DBG((dbgo," V6 address = %s\n",*pip))

	} else {
		DBG((dbgo," Skipping reply at 0x%x\n",off))
	}
	off += rdlength;
	free(sv);

	return off;
}

/* Parse an mDNS reply into a ChromCast name & IP address */
/* Allocate and return name and IP on finding ChromeCast reply, NULL otherwise */
/* Return nz on failure */
static int parse_dns(char **pname, char **pip, ORD8 *buf, int size) {
	int i, off = 0;
	int id, flags, qdcount, ancount, nscount, arcount;

	DBG((dbgo,"Parsing mDNS reply:\n"))

	*pname = NULL;
	*pip = NULL;

	// Parse reply header
	if ((size - off) < 2) return 1;
	id = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," ID = %d\n",id))

	if ((size - off) < 2) return 1;
	flags = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," FLAGS = 0x%04x\n",flags))

	if ((size - off) < 2) return 1;
	qdcount = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," QDCOUNT = %d\n",qdcount))

	if ((size - off) < 2) return 1;
	ancount = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," ANCOUNT = %d\n",ancount))

	if ((size - off) < 2) return 1;
	nscount = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," NSCOUNT = %d\n",nscount))

	if ((size - off) < 2) return 1;
	arcount = read_ORD16_be(buf + off); off += 2;
	DBG((dbgo," ARCOUNT = %d\n",arcount))

//printf("~1 after strings, off = 0x%x\n",off);

	// Parse all the queries (QDCOUNT)
	for (i = 0; i < qdcount; i++) {
		if ((off = parse_query(buf, off, size)) < 0) {
			DBG((dbgo," ### Parsing query failed ###\n"))
			return 1;
		}
	}

	// Parse all the answers (ANCOUNT)
	for (i = 0; i < ancount; i++) {
		if ((off = parse_reply(pname, pip, buf, off, size)) < 0) {
			DBG((dbgo," ### Parsing answer failed ###\n"))
			return 1;
		}
	}

	// Parse all the NS records (NSCOUNT)
	for (i = 0; i < nscount; i++) {
		if ((off = parse_reply(pname, pip, buf, off, size)) < 0) {
			DBG((dbgo," ### Parsing NS record failed ###\n"))
			return 1;
		}
	}

	// Parse all the addition RR answers (ARCOUNT)
	for (i = 0; i < arcount; i++) {
		if ((off = parse_reply(pname, pip, buf, off, size)) < 0) {
			DBG((dbgo," ### Parsing additional records failed ###\n"))
			return 1;
		}
	}
	return 0;
}

/*
	Analysis of ChromeCast reply

    0000: 00 00  84 00  00 00  00 01  00 00  00 05  0b 5f 67 6f  ............._go

	ID 		0x0000		// ID 0
	Flags	0x8400		// Standard query response, No error
	QDCOUNT 0x0000		// No queries
	ANCOUNT 0x0001		// 1 Answer
	NSCOUNT 0x0000
	ARCOUNT 0x0005		// 5 Additional records

	// Answer
                                              0b 5f 67 6f  _go
    0010: 6f 67 6c 65 63 61 73 74                          oglecast

    0010:                         04 5f 74 63 70           _tcp

    0010: 05 6c 6f  lo
    0020: 63 61 6c 00                                       cal.

	"_googlecast" "_tcp" "cal" ""		//TLD

    0020: 00 0c  00 01  00 00 11 94  00 11  0e 43  ...........C

	RTYPE    0x000c
	RCLASS   0x0001
	TTL      0x00001194 
	RDLENGTH 0x0011

	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    0000: 00 00 84 00 00 00 00 01 00 00 00 05 0b 5f 67 6f  ............._go
    0010: 6f 67 6c 65 63 61 73 74 04 5f 74 63 70 05 6c 6f  oglecast._tcp.lo
    0020: 63 61 6c 00 00 0c 00 01 00 00 11 94 00 11 0e 43  cal............C
    0030: 68 72 6f 6d 65 63 61 73 74 36 38 39 32 c0 0c c0  hromecast6892...
    0040: 2e 00 10 80 01 00 00 11 94 00 67 23 69 64 3d 63  ..........g#id=c
    0050: 31 37 61 65 38 38 32 65 37 65 31 38 37 35 64 30  17ae882e7e1875d0
    0060: 31 33 65 64 32 31 32 63 31 36 62 62 64 34 30 05  13ed212c16bbd40.
    0070: 76 65 3d 30 32 0d 6d 64 3d 43 68 72 6f 6d 65 63  ve=02.md=Chromec
    0080: 61 73 74 12 69 63 3d 2f 73 65 74 75 70 2f 69 63  ast.ic=/setup/ic
    0090: 6f 6e 2e 70 6e 67 11 66 6e 3d 43 68 72 6f 6d 65  on.png.fn=Chrome
    00a0: 63 61 73 74 36 38 39 32 04 63 61 3d 35 04 73 74  cast6892.ca=5.st
    00b0: 3d 30 c0 2e 00 21 80 01 00 00 00 78 00 17 00 00  =0...!.....x....
    00c0: 00 00 1f 49 0e 43 68 72 6f 6d 65 63 61 73 74 36  ...I.Chromecast6
    00d0: 38 39 32 c0 1d c0 c4 00 01 80 01 00 00 00 78 00  892...........x.
    00e0: 04 0a 00 00 80 c0 2e 00 2f 80 01 00 00 11 94 00  ......../.......
    00f0: 09 c0 2e 00 05 00 00 80 00 40 c0 c4 00 2f 80 01  .........@.../..
    0100: 00 00 00 78 00 05 c0 c4 00 01 40                 ...x......@


*/

/* ================================================================== */
/* Debug */

// Print bytes as hex to stdout
void cc_dump_bytes(FILE *fp, char *pfx, unsigned char *buf, int len) {
	int i, j, ii;
	char oline[200] = { '\000' }, *bp = oline;
	for (i = j = 0; i < len; i++) {
		if ((i % 16) == 0)
			bp += sprintf(bp,"%s%04x:",pfx,i);
		bp += sprintf(bp," %02x",buf[i]);
		if ((i+1) >= len || ((i+1) % 16) == 0) {
			for (ii = i; ((ii+1) % 16) != 0; ii++)
				bp += sprintf(bp,"   ");
			bp += sprintf(bp,"  ");
			for (; j <= i; j++) {
				if (!(buf[j] & 0x80) && isprint(buf[j]))
					bp += sprintf(bp,"%c",buf[j]);
				else
					bp += sprintf(bp,".");
			}
			bp += sprintf(bp,"\n");
			fprintf(fp, "%s", oline);
			bp = oline;
		}
	}
}

