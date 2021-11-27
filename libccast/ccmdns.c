
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
#include "conv.h"
#include "ccmdns.h"

#undef DEBUG		/* [und] */

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
# define DBG(xxx) a1logd xxx ;
# define DBG2(xxx) a1logd xxx ;
# define DLEV 0
# define DLEVP1 0
#else
# define DBG(xxx) ;
# define DBG2(xxx) a1logd xxx ;
# define DLEV 2
# define DLEVP1 3
#endif  /* DEBUG */

/* ================================================================ */

/* mDNS details */
#define DESTINATION_MCAST "224.0.0.251"
#define DESTINATION_MCASTV6 "FF02::FB"
#define SOURCE_PORT 5353 
#define DESTINATION_PORT 5353 

#define BUFSIZE 2048

/* Various DNS defines */
#define DNS_CLASS_IN 0x0001

// Used by ChromeCast
#define DNS_TYPE_PTR  12	/* Domain name pointer				*/
#define DNS_TYPE_TXT  16	/* Text String						*/
#define DNS_TYPE_SRV  33	/* Server selection (SRV) record	*/
#define DNS_TYPE_A	   1	/* Host address (A) record			*/
#define DNS_TYPE_AAAA 28	/* IPv6 address ???					*/
#define DNS_TYPE_NSEC 47	/* DNS Next Secure Name				*/


char *cctype2str(cctype typ) {
	switch (typ) {
		case cctyp_unkn:
			return "Unknown";
		case cctyp_1:
			return "One";
		case cctyp_2:
			return "Two";
		case cctyp_Audio:
			return "Audio";
		case cctyp_Ultra:
			return "Ultra";
		case cctyp_Other:
			return "Other";
		default:
			return "Unexpected";
	}
}

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
		DBG((g_log,0,"WSAStartup failed\n"))
		return 1;
	}
#endif

	return 0;
}

/* Setup the send and recieve socket */
/* Return nz on error */
static int init_socket_mDNS(SOCKET *psock) {
	int nRet, nOptVal; 
	int off;
	SOCKET sock; 
	struct sockaddr_in stSourceAddr; 
	struct ip_mreq stIpMreq;

	DBG((g_log,0,"init_socket_mDNS() called\n")) 

	/* get a datagram (UDP) socket */ 
	sock = socket(PF_INET, SOCK_DGRAM, 0); 
	if (sock == INVALID_SOCKET) { 
		DBG((g_log,0,"opening UDP socked failed with %d\n",ERRNO)) 
		return 1;
	} 

	/*----------------------- to send --------------------------- */

	/* Theoretically, you do not need any special preparation to 
	 * send to a multicast address. However, you may want a few
	 * things to overcome the limits of the default behavior 
	 */

	// If we're doing a one-shot, we shouldn't transmit from port 5353, */
	// but ChromCast won't see the packet if we don't. */

	/* We can't send from port 5353 if someone else is using it, */
	/* so set the SO_REUSEADDR option (which is enough for MSWin), */
	/* and SO_REUSEPORT for OS X and Linux */
	{
		int on = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on))) {
			DBG((g_log,0,"setsockopt(SO_REUSEADDR) failed with %d\n",ERRNO))
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
			DBG((g_log,0,"setsockopt(SO_REUSEPORT)  failed with %d\n",ERRNO))
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
		DBG((g_log,0,"bind failed with %d\n",ERRNO))
		closesocket(sock);
		return 1;
	} 

	/* - - - - - - - */
	/*   Send setup  */

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
		DBG((g_log,0,"[disabling loopback failed with %d]\n",ERRNO)) 
	} 

	/* Is this desirable ? */

	/* increase the IP TTL from the default of one to 64, so our
	 * multicast datagrams can get off of the local network 
	 */
	nOptVal = 64; 
	nRet = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
				 (char *)&nOptVal, sizeof(int)); 
	if (nRet == SOCKET_ERROR) { 
		DBG((g_log,0,"increasing TTL failed with %d\n",ERRNO))
		closesocket(sock);
		return 1;
	} 

	if (psock != NULL)
		*psock = sock;

	/* - - - - - - - */
	/* Recieve setup */

	/* join the multicast group we want to receive datagrams from */ 
	stIpMreq.imr_multiaddr.s_addr = inet_addr(DESTINATION_MCAST); /* group addr */ 
	stIpMreq.imr_interface.s_addr = INADDR_ANY; /* use default */ 
	nRet = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
					(char *)&stIpMreq, sizeof (struct ip_mreq));

	if (nRet == SOCKET_ERROR) { 
		DBG((g_log,0,"registering for read events failed with %d\n",ERRNO))
		closesocket(sock);
		return 1;
	} 

	/* Make this timeout after 100 msec second */
#ifdef NT
	{
		DWORD tv;
		tv = 100;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
			DBG((g_log,0,"setsockopt timout failed with %d\n",ERRNO))
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
			DBG((g_log,0,"setsockopt timout failed with %d\n",ERRNO))
			closesocket(sock);
			return 1;
		}
	}
#endif

	if (psock != NULL)
		*psock = sock;

	DBG((g_log,0,"init mDNS socket succeed\n",ERRNO))

	return 0;
}

/* Send an mDNS query */
/* On some platforms if we try to transmit & recieve at the same time (what ?) */
/* Return nz on error */
static int send_mDNS(SOCKET sock) {
	int nRet; 
	int off;
	ORD8 achOutBuf[BUFSIZE]; 
	struct sockaddr_in stDestAddr; 

	DBG((g_log,0,"send_mDNS() called\n")) 

	/* Initialize the Destination Address structure and send */ 
	stDestAddr.sin_family = PF_INET; 
	stDestAddr.sin_addr.s_addr = inet_addr(DESTINATION_MCAST);
	stDestAddr.sin_port = htons(DESTINATION_PORT); 

	/* Setup the FQDN data packet to send */
	write_ORD16_be(achOutBuf + 0, 0);	/* ID */
	write_ORD16_be(achOutBuf + 2, 0);	/* Flags */
	write_ORD16_be(achOutBuf + 4, 1);	/* QDCOUNT - one question */
	write_ORD16_be(achOutBuf + 6, 0);	/* ANCOUNT */
	write_ORD16_be(achOutBuf + 8, 0);	/* NSCOUNT */
	write_ORD16_be(achOutBuf + 10, 0);	/* ARCOUNT */
	off = 12;
	off = write_string(achOutBuf, off, "_googlecast");
	off = write_string(achOutBuf, off, "_tcp");
	off = write_string(achOutBuf, off, "local");
	write_ORD8(achOutBuf + off, 0);			/* null string */
	off += 1;
	write_ORD16_be(achOutBuf + off, 0x000c);	/* QCOUNT */
	off += 2;
	/* Set top bit to get a unicast response (not for ChromeCast though) */
	write_ORD16_be(achOutBuf + off, 0x0001);	/* QCLASS */
	off += 2;

	nRet = sendto(sock, (char *)achOutBuf, off,
					0, 
					(struct sockaddr *) &stDestAddr, 
					sizeof(struct sockaddr)); 
	if (nRet == SOCKET_ERROR) { 
		DBG((g_log,0,"sending mDNS query failed with %d\n",ERRNO))
		return 1;
	} 

	DBG((g_log,0,"sending mDNS query succeed\n",ERRNO))

	return 0;
}

static int parse_dns(char **name, char **ip, cctype *ptyp, int *pcaflags, ORD8 *buf, int size);

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
/* return nz on error & free *ids */
static int receive_mDNS(SOCKET sock, ccast_id ***ids, int *nids, int emsec) {
	unsigned int smsec;
	unsigned int nSize;
	int off, size;
	ORD8 achInBuf[BUFSIZE]; 
	struct sockaddr_in stSourceAddr; 

	DBG((g_log,0,"receive_mDNS() called with %d ids\n",*nids))

	/* While there is still wait time */
	for (smsec = msec_time(), emsec += smsec;msec_time() <= emsec;) {
		int i;
		char *name, *ip;
		cctype typ;
		int caflags;					/* (We're not currently saving this) */
		struct sockaddr stSockAddr; 

		/* Recv the available data */ 
		nSize = sizeof(struct sockaddr); 
		size = recvfrom(sock, (char *)achInBuf, 
						BUFSIZE, 0, (struct sockaddr *) &stSockAddr, &nSize); 
		if (size == SOCKET_ERROR) { 
			if (ERRNO == UDP_SOCKET_TIMEOUT)
				continue;			/* Timeout */
			DBG2((g_log,DLEV,"recvfrom failed with %d\n",ERRNO))
			free_ccids(*ids);
			*ids = NULL;
			return 1;
		}

		DBG2((g_log,DLEVP1,"Got mDNS message length %d bytes\n",size))
#ifdef DEBUG
		adump_bytes(g_log, "    ", achInBuf, 0, size);
#endif

		if (parse_dns(&name, &ip, &typ, &caflags, achInBuf, size) != 0) {
			DBG((g_log,0,"Failed to parse the reply\n"))
		} else {
			DBG((g_log,0,"Parsed reply OK\n"))

			/* If we found an entry */
			if (name != NULL && ip != NULL) {
				DBG((g_log,0,"Got a name '%s', IP '%s', type %s\n",name,ip, cctype2str(typ)))

				/* Check if it is a Chromecast-Audio or Other */
				if (typ == cctyp_Audio
				 || typ == cctyp_Other) {
					DBG((g_log,0,"Ignoring Chromecast-Audio/Other\n"))
					free(name);
					free(ip);

				} else {
			
					/* Check if it is a duplcate */
					for (i = 0; i < *nids; i++) {
						if (strcmp((*ids)[i]->name, name) == 0
						 && strcmp((*ids)[i]->ip, ip) == 0)
							break;	/* yes */
					} 
					if (i < *nids) {
						DBG((g_log,0,"Duplicate\n"))
						free(name);
						free(ip);
					} else {
						ccast_id **tids;
						if ((tids = realloc(*ids, (*nids + 2) * sizeof(ccast_id *))) == NULL
						 || (*ids = tids, (*ids)[*nids] = malloc(sizeof(ccast_id)), (*ids)[*nids]) == NULL) {
							DBG((g_log,0,"realloc/malloc fail\n"))
							free(name);
							free(ip);
							free_ccids(*ids);
							*ids = NULL;
							return 1;
						} else {
							DBG((g_log,0,"Adding entry\n"))
							(*ids)[*nids]->name = name;
							(*ids)[*nids]->ip = ip;
							(*ids)[*nids]->typ = typ;
							(*nids)++;
							(*ids)[*nids] = NULL;		/* End marker */
						}
					}
				}
			}
		}
	}

	DBG2((g_log,DLEVP1,"receive_mDNS() returning %d in list\n",*nids))

	return 0;
}

/* ==================================================================== */

/* Get a list of Video output capable Chromecasts. Return NULL on error */
/* Last pointer in array is NULL */
/* Takes 1.5 second to return */
ccast_id **get_ccids() {
	ccast_id **ids = NULL;
	int nids = 0;
	int i, j, k;
	unsigned int smsec;
	int waittime = 200;
	SOCKET sock;

	DBG2((g_log,DLEV,"get_ccids: called\n"))

	if (init_mDNS()) {
		DBG2((g_log,0,"get_ccids: init_mDNS() failed\n"))
		return NULL;
	}

	if (init_socket_mDNS(&sock)) {
		DBG2((g_log,0,"get_ccids: init_socket_mDNS() failed\n"))
		return NULL;
	}

	smsec = msec_time();

	/* Try a few times, with increasing response wait time */
	for (k = 1;
		    ((msec_time() - smsec) < 700)
		 || (nids == 0 && (msec_time() - smsec) < 1600);
	     k++) {

		DBG2((g_log,DLEV,"get_ccids: Sending mDNS query #%d:\n",k))
		if (send_mDNS(sock)) {
			DBG2((g_log,0,"get_ccids: send_mDNS() #1 failed\n"))
			closesocket(sock);
			return NULL;
		}
	
		DBG2((g_log,DLEV,"get_ccids: Waiting for mDNS reply #%d:\n",k))
		if (receive_mDNS(sock, &ids, &nids, waittime)) {
			DBG2((g_log,0,"get_ccids: receive_mDNS() #%d failed\n",k))
			closesocket(sock);
			return NULL;
		}
		DBG2((g_log,DLEV,"get_ccids: have %d %s\n",nids, nids == 1 ? "reply" : "replies"))

		if (waittime < 500)
			waittime = 500;
	}
	closesocket(sock);

	/* If no ChromCasts found, return an empty list */
	if (ids == NULL) {
		DBG2((g_log,DLEV,"get_ccids: no devices found\n"))
		if ((ids = calloc(sizeof(ccast_id *), 1)) == NULL) {
			DBG2((g_log,0,"get_ccids: calloc fail\n"))
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

	for (i = 0; ids[i] != NULL; i++) {
		DBG2((g_log,DLEV,"  Entry %d:\n",i))
		DBG2((g_log,DLEV,"   Name: %s\n",ids[i]->name))
		DBG2((g_log,DLEV,"   IP:   %s\n",ids[i]->ip))
		DBG2((g_log,DLEV,"   Type: %s\n",cctype2str(ids[i]->typ)))
	}
	DBG2((g_log,DLEV,"get_ccids: Returning %d devices\n",i))

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

	DBG((g_log,8,"read_string_imp called for off %d (0x%x) rec %d\n",off,off,rec));

	if (rec > 10) {		/* Too many recursions */
		DBG((g_log,8,"read_string_imp too many recursions\n"));
		return -1;		/* Error */
	}

	if (off >= size) {
		DBG((g_log,8,"read_string_imp off %d >= size %d\n",off,size));
		return -1;		/* Error */
	}

	for (;;) {
		DBG((g_log,8,"top of loop at off %d\n",off));
		len = buf[off];
		if ((len & 0xc0) == 0xc0) {	/* Is it an offset marker */
			int poff;

			DBG((g_log,8,"got pointer\n"));
			if ((size - off) < 2) {
				DBG((g_log,8,"read_string_imp size %d - off %d < 2\n",size,off));
				return -1;
			}

			poff = read_ORD16_be(buf + off); off += 2;
			poff -= 0xc000;

			if (poff < 0 || poff >= size) {
				return -1;
			}

			DBG((g_log,8,"read_string_imp recursing\n"));
			read_string_imp(rv, slen, buf, poff, size, rec+1);

			if (slen != NULL) {
				DBG((g_log,8,"after recurse, slen = %d, off = %d (0x%x)\n",*slen,off,off));
			}
			break;				/* we're done */

		} else {
			DBG((g_log,8,"got string length %d\n",len));

			off++;
			if ((off + len) >= size) {
				DBG((g_log,8,"read_string_imp size off %d + len %d >= size %d\n",off,len,size));
				return -1;
			}

			if (len == 0) {
				DBG((g_log,8,"read_string_imp len == 0 - done\n"));
				break;				/* we're done */
			}

			if (rv != NULL) {
				memcpy(*rv, buf + off, len);

#ifdef DEBUG
				(*rv)[len] = '\000';
				DBG((g_log,8,"Copied string %p = '%s'\n",*rv,*rv));
#endif

				*rv += len;
			}
			off += len;

			if (slen != NULL) {
				(*slen) += len;
				DBG((g_log,8,"slen now = %d\n",*slen));
			}

		}
		d1 = 1;

		if (slen != NULL) {
			(*slen)++;
		}

		if (rv != NULL) {
			(*rv)[0] = '.';
			(*rv)++;
		}
	}
	if (slen != NULL) {
		DBG((g_log,8,"returning slen = %d\n",*slen));
	}

	return off;
}

/* Read a string */
/* Return the next offset or -1 and *rv = NULL on error */
static int read_string(char **rv, ORD8 *buf, int off, int size) {
	int len = 0, toff = off;
	char *trv;

	DBG((g_log,7,"read_string called for off 0x%x\n",off));

	/* See how long it will be */
	if ((toff = read_string_imp(NULL, &len, buf, off, size, 0)) < 0) {
		DBG((g_log,7,"read_string_imp length returned error\n"));
		return toff;
	}
	DBG((g_log,7,"read_string_imp got length %d\n",len));

	if (len == 0) {
		DBG((g_log,7,"Got zero length string\n"));
		len++;			/* Room for null string */
	}
	if ((*rv = trv = malloc(len)) == NULL) {
		DBG((g_log,7,"malloc for string failed\n"));
		return -1;
	}
	DBG((g_log,7,"loced %p\n",*rv));
	off = read_string_imp(&trv, NULL, buf, off, size, 0);
	if (off >= 0) {
		(*rv)[len-1] = '\000';
		DBG((g_log,7,"read string ok: %p = '%s'\n",*rv, *rv));
	} else {
		DBG((g_log,7,"reading string failed\n"));
	}
	return off;
}
 
/* Parse an mDNS query */
/* Return updated off value or -1 on error */
int parse_query(ORD8 *buf, int off, int size) {
	char *sv;
	int qtype, qclass;

	DBG((g_log,0,"Parsing query at 0x%x:\n",off))
	if ((off = read_string(&sv, buf, off, size)) < 0)
		return -1;
	DBG((g_log,0," Got string '%s'\n",sv))
	free(sv);

	if ((size - off) < 2)
		return -1;
	qtype = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," QTYPE = 0x%04x\n",qtype))

	if ((size - off) < 2)
		return -1;
	qclass = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," QCLASS = 0x%04x\n",qtype))

	return off;
}

/* Parse an mDNS reply, and set Friendly name (if known) + formal name + IP */
/* Return updated off value or -1 on error */
int parse_reply(char **pname, char **pip, cctype *ptyp, int *pcaflags, ORD8 *buf, int off, int size) {
	char *sv;
	int rtype, rclass, rdlength;
	unsigned int ttl;

	DBG((g_log,0,"Parsing reply at 0x%x:\n",off))
	if ((off = read_string(&sv, buf, off, size)) < 0)
		return -1;

	DBG((g_log,0," Got string '%s', now off = 0x%x\n",sv,off))

	if ((size - off) < 2) {
		free(sv);
		return -1;
	}
	rtype = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," RTYPE = %d, now off = 0x%x\n",rtype,off))

	if ((size - off) < 2) {
		free(sv);
		return -1;
	}
	rclass = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," RCLASS = 0x%04x, now off = 0x%x\n",rclass,off))
	/* rclass top bit is cache flush bit */

	if ((rclass & 0x7fff) != DNS_CLASS_IN) {
		DBG((g_log,0," response has RCLASS != 0x%04x\n",DNS_CLASS_IN))
		free(sv);
		return -1;
	}

	if ((size - off) < 4)
		return -1;
	ttl = read_ORD32_be(buf + off); off += 4;
	DBG((g_log,0," TTL = %u, now off = 0x%x\n",ttl,off))

	if ((size - off) < 2)
		return -1;
	rdlength = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," RDLENGTH = %d, now off = 0x%x\n",rdlength,off))

	if ((off + rdlength) > size) {
		DBG((g_log,0," response RDLENGTH is longer than remaining buffer (%d)\n",size - off))
		free(sv);
		return -1;
	}
	
	/* Just decode the replies we need */
	if (rtype == DNS_TYPE_TXT) {	/* Check it's a ChromeCast & get its name */
		char *cp, *fn = NULL;
		int rdsize = off + rdlength;
		int caflags = 0;

		if ((cp = strchr(sv, '.')) == NULL) {
			free(sv);
			return -1;
		}
		*cp = '\000';
		if (strcmp(cp+1, "_googlecast._tcp.local") != 0) {
			DBG((g_log,0,"Not a chromecast (got '%s')\n",cp+1))
			free(sv);
			return -1;
		}

#ifdef NEVER
		DBG((g_log,0," TXT data:\n"))
		adump_bytes(g_log, "    ", buf + off, 0, rdlength);
#endif

		/*	Chromecast TXT DATA format:

			Buffer is series of strings, each beginning with a byte length.
			Each string is id=value, with the following known values:

			id=c17a8e82ee7187d5013e2d12c61bbd40		uuidNoHyphens
			rm=669B9448366A01CB
			ve=05									SW version ????
			md=Chromecast							Model ?
			   										i.e. Chromecast-Audio
			   										i.e. Chromecast-Ultra
			   										i.e. Group
			ic=/setup/icon.png						Icon file ??
			fn=Chromecast6892						Friendly name
			ca=4101									Capabilities bits ???
			st=1									Application running ??
			bs=FA8FCA566645							Application ID ??
			rs=Pattern generator ready				Application state ??

		 */

		/* Parse the Chromacast TX Data */
		for (; off < rdsize; ) {
			int slen;
			char *ss;

			/* Read the string length */
			if ((rdsize - off) < 1)
				goto done_tx;

			slen = read_ORD8(buf + off); off += 1;

			if ((rdsize - off) < slen)
				goto done_tx;
			
			if ((ss = malloc(slen + 1)) == NULL) {
				DBG((g_log,0,"malloc for sub-string failed\n"));
				return -1;
			}

			memcpy(ss, buf + off, slen);
			ss[slen] = '\000';
			off += slen;

			DBG((g_log,0," TX Sub-string '%s'\n", ss))

			/* Record info we want: */
			if (strncmp(ss, "fn=", 3) == 0) {	/* Friendly name */
				if ((fn = malloc(slen -3 +1)) == NULL) {
					DBG((g_log,0,"malloc for fn-string failed\n"));
					return -1;
				}
				strcpy(fn, ss + 3);
			}

			if (strncmp(ss, "ca=", 3) == 0) {	/* Capability bits ? */
				caflags = atoi(ss + 3);
			}
			free(ss);
		}

	  done_tx:

		if (fn != NULL) {
			DBG((g_log,0," Chromacast '%s', fn '%s'\n", sv, fn))
		} else {
			DBG((g_log,0," Chromacast '%s'\n", sv))
		}

		DBG2((g_log,DLEV,"ca bits 0x%x\n", caflags))

		if (strncmp(sv, "Chromecast-Ultra", 16) == 0) {
			*ptyp = cctyp_Ultra;
		} else if (strncmp(sv, "Chromecast-Audio", 16) == 0) {
			*ptyp = cctyp_Audio;
		} else {
			// Hmm. A CC1 or non-Google device.

			/* We're guessing that bit 0 of the ca bits == Video capable */
			/* and bit 1 of the ca bits == Audio capable */
			if (caflags & 1)
				*ptyp = cctyp_1;
			else if (caflags & 4)
				*ptyp = cctyp_Audio;
			else
				*ptyp = cctyp_Other;
		}

		if (fn != NULL) {
			*pname = fn;
		} else {
			free(fn);
			if ((*pname = strdup(sv)) == NULL) {
				DBG((g_log,0,"strdup failed\n"))
				free(sv);
				return -1;
			}
		}

		off = rdsize;

	} else if (rtype == DNS_TYPE_A) {
		/* Should we check name matches ? */
		if ((*pip = malloc(3 * 4 + 3 + 1)) == NULL) {
			DBG((g_log,0,"malloc failed\n"))
			free(*pname);
			free(sv);
		}
		sprintf(*pip, "%d.%d.%d.%d", buf[off], buf[off+1], buf[off+2], buf[off+3]);
		DBG((g_log,0," V4 address = %s\n",*pip))
	
		off += rdlength;
	} else if (rtype == DNS_TYPE_AAAA) {		/* The IPV6 address */
		/* Should we check name matches ? */
		if ((*pip = malloc(8 * 4 + 7 + 1)) == NULL) {
			DBG((g_log,0,"malloc failed\n"))
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
		DBG((g_log,0," V6 address = %s\n",*pip))

		off += rdlength;
	} else {
		DBG((g_log,0," Skipping reply at 0x%x\n",off))
		off += rdlength;
	}
	free(sv);

	return off;
}

/* Parse an mDNS reply into a ChromCast name & IP address */
/* Allocate and return name and IP on finding ChromeCast reply, NULL otherwise */
/* Return nz on failure */
static int parse_dns(char **pname, char **pip, cctype *ptyp, int *pcaflags, ORD8 *buf, int size) {
	int i, off = 0;
	int id, flags, qdcount, ancount, nscount, arcount;

	DBG((g_log,0,"Parsing mDNS reply:\n"))

	*pname = NULL;
	*pip = NULL;

	// Parse reply header
	if ((size - off) < 2) return 1;
	id = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," ID = %d\n",id))

	if ((size - off) < 2) return 1;
	flags = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," FLAGS = 0x%04x\n",flags))

	if ((size - off) < 2) return 1;
	qdcount = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," QDCOUNT = %d\n",qdcount))

	if ((size - off) < 2) return 1;
	ancount = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," ANCOUNT = %d\n",ancount))

	if ((size - off) < 2) return 1;
	nscount = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," NSCOUNT = %d\n",nscount))

	if ((size - off) < 2) return 1;
	arcount = read_ORD16_be(buf + off); off += 2;
	DBG((g_log,0," ARCOUNT = %d\n",arcount))

//printf("~1 after strings, off = 0x%x\n",off);

	// Parse all the queries (QDCOUNT)
	for (i = 0; i < qdcount; i++) {
		if ((off = parse_query(buf, off, size)) < 0) {
			DBG((g_log,0," ### Parsing query failed ###\n"))
			return 1;
		}
	}

	// Parse all the answers (ANCOUNT)
	for (i = 0; i < ancount; i++) {
		if ((off = parse_reply(pname, pip, ptyp, pcaflags, buf, off, size)) < 0) {
			DBG((g_log,0," ### Parsing answer failed ###\n"))
			return 1;
		}
	}

	// Parse all the NS records (NSCOUNT)
	for (i = 0; i < nscount; i++) {
		if ((off = parse_reply(pname, pip, ptyp, pcaflags, buf, off, size)) < 0) {
			DBG((g_log,0," ### Parsing NS record failed ###\n"))
			return 1;
		}
	}

	// Parse all the addition RR answers (ARCOUNT)
	for (i = 0; i < arcount; i++) {
		if ((off = parse_reply(pname, pip, ptyp, pcaflags, buf, off, size)) < 0) {
			DBG((g_log,0," ### Parsing additional records failed ###\n"))
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

*/

/*
	.ca values:

	Original CC		   5 = 0x0005
	Latest CC1		4101 = 0x1005
	Latest CC2		4101 = 0x1005
	Latest CC3		4101 = 0x1005
	Vizio TV        2053 = 0x0805
	Toshiba TV		4101 = 0x1005
	Audio CC        4100 = 0x1004
	Sound bar		2052 = 0x0804

	Google documents the following Capability strings:

	VIDEO_OUT The receiver supports video output.
	VIDEO_IN The receiver supports video input (camera).
	AUDIO_OUT The receiver supports audio output.
	AUDIO_IN The receiver supports audio input (microphone).
	MULTIZONE_GROUP The receiver represents a multi-zone group.

	Which may correspond to ca bits 0, 1, 2, 3 respectively ?

*/

/*
	Original CC 1 dump ?

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

/*

	ChromeCast 1 dump

0000: 00 00 84 00 00 00 00 01 00 00 00 03 0b 5f 67 6f  ............._go
0010: 6f 67 6c 65 63 61 73 74 04 5f 74 63 70 05 6c 6f  oglecast._tcp.lo
0020: 63 61 6c 00 00 0c 00 01 00 00 00 78 00 44 2b 43  cal........x.D+C
0030: 68 72 6f 6d 65 63 61 73 74 2d 63 31 37 61 65 38  hromecast-c17ae8
0040: 38 32 65 37 65 31 38 37 35 64 30 31 33 65 64 32  82e7e1875d013ed2
0050: 31 32 63 31 36 62 62 64 34 30 0b 5f 67 6f 6f 67  12c16bbd40._goog
0060: 6c 65 63 61 73 74 04 5f 74 63 70 05 6c 6f 63 61  lecast._tcp.loca
0070: 6c 00 2b 43 68 72 6f 6d 65 63 61 73 74 2d 63 31  l.+Chromecast-c1
0080: 37 61 65 38 38 32 65 37 65 31 38 37 35 64 30 31  7ae882e7e1875d01
0090: 33 65 64 32 31 32 63 31 36 62 62 64 34 30 0b 5f  3ed212c16bbd40._
00a0: 67 6f 6f 67 6c 65 63 61 73 74 04 5f 74 63 70 05  googlecast._tcp.
00b0: 6c 6f 63 61 6c 00 00 10 80 01 00 00 11 94 00 82  local...........
00c0: 23 69 64 3d 63 31 37 61 65 38 38 32 65 37 65 31  #id=c17ae882e7e1
00d0: 38 37 35 64 30 31 33 65 64 32 31 32 63 31 36 62  875d013ed212c16b
00e0: 62 64 34 30 03 72 6d 3d 05 76 65 3d 30 35 0d 6d  bd40.rm=.ve=05.m
00f0: 64 3d 43 68 72 6f 6d 65 63 61 73 74 12 69 63 3d  d=Chromecast.ic=
0100: 2f 73 65 74 75 70 2f 69 63 6f 6e 2e 70 6e 67 11  /setup/icon.png.
0110: 66 6e 3d 43 68 72 6f 6d 65 63 61 73 74 36 38 39  fn=Chromecast689
0120: 32 07 63 61 3d 34 31 30 31 04 73 74 3d 30 0f 62  2.ca=4101.st=0.b
0130: 73 3d 46 41 38 46 43 41 35 36 36 36 34 35 03 72  s=FA8FCA566645.r
0140: 73 3d 2b 43 68 72 6f 6d 65 63 61 73 74 2d 63 31  s=+Chromecast-c1
0150: 37 61 65 38 38 32 65 37 65 31 38 37 35 64 30 31  7ae882e7e1875d01
0160: 33 65 64 32 31 32 63 31 36 62 62 64 34 30 0b 5f  3ed212c16bbd40._
0170: 67 6f 6f 67 6c 65 63 61 73 74 04 5f 74 63 70 05  googlecast._tcp.
0180: 6c 6f 63 61 6c 00 00 21 80 01 00 00 00 78 00 32  local..!.....x.2
0190: 00 00 00 00 1f 49 24 63 31 37 61 65 38 38 32 2d  .....I$c17ae882-
01a0: 65 37 65 31 2d 38 37 35 64 2d 30 31 33 65 2d 64  e7e1-875d-013e-d
01b0: 32 31 32 63 31 36 62 62 64 34 30 05 6c 6f 63 61  212c16bbd40.loca
01c0: 6c 00 24 63 31 37 61 65 38 38 32 2d 65 37 65 31  l.$c17ae882-e7e1
01d0: 2d 38 37 35 64 2d 30 31 33 65 2d 64 32 31 32 63  -875d-013e-d212c
01e0: 31 36 62 62 64 34 30 05 6c 6f 63 61 6c 00 00 01  16bbd40.local...
01f0: 80 01 00 00 00 78 00 04 c0 a8 01 68              .....x.....h


ChromeCast 2 dump:

0000: 00 00 84 00 00 00 00 01 00 00 00 03 0b 5f 67 6f  ............._go
0010: 6f 67 6c 65 63 61 73 74 04 5f 74 63 70 05 6c 6f  oglecast._tcp.lo
0020: 63 61 6c 00 00 0c 00 01 00 00 00 78 00 44 2b 43  cal........x.D+C
0030: 68 72 6f 6d 65 63 61 73 74 2d 38 32 38 30 30 66  hromecast-82800f
0040: 65 63 33 37 37 36 35 39 38 66 35 38 64 34 61 61  ec3776598f58d4aa
0050: 63 39 65 36 35 64 30 30 30 61 0b 5f 67 6f 6f 67  c9e65d000a._goog
0060: 6c 65 63 61 73 74 04 5f 74 63 70 05 6c 6f 63 61  lecast._tcp.loca
0070: 6c 00 2b 43 68 72 6f 6d 65 63 61 73 74 2d 38 32  l.+Chromecast-82
0080: 38 30 30 66 65 63 33 37 37 36 35 39 38 66 35 38  800fec3776598f58
0090: 64 34 61 61 63 39 65 36 35 64 30 30 30 61 0b 5f  d4aac9e65d000a._
00a0: 67 6f 6f 67 6c 65 63 61 73 74 04 5f 74 63 70 05  googlecast._tcp.
00b0: 6c 6f 63 61 6c 00 00 10 80 01 00 00 11 94 00 a9  local...........
00c0: 23 69 64 3d 38 32 38 30 30 66 65 63 33 37 37 36  #id=82800fec3776
00d0: 35 39 38 66 35 38 64 34 61 61 63 39 65 36 35 64  598f58d4aac9e65d
00e0: 30 30 30 61 13 72 6d 3d 36 36 39 42 39 34 34 38  000a.rm=669B9448
00f0: 36 33 36 30 41 31 43 42 05 76 65 3d 30 35 0d 6d  6360A1CB.ve=05.m
0100: 64 3d 43 68 72 6f 6d 65 63 61 73 74 12 69 63 3d  d=Chromecast.ic=
0110: 2f 73 65 74 75 70 2f 69 63 6f 6e 2e 70 6e 67 11  /setup/icon.png.
0120: 66 6e 3d 43 68 72 6f 6d 65 63 61 73 74 36 35 30  fn=Chromecast650
0130: 32 07 63 61 3d 34 31 30 31 04 73 74 3d 31 0f 62  2.ca=4101.st=1.b
0140: 73 3d 46 41 38 46 43 41 39 45 33 45 30 31 1a 72  s=FA8FCA9E3E01.r
0150: 73 3d 50 61 74 74 65 72 6e 20 67 65 6e 65 72 61  s=Pattern genera
0160: 74 6f 72 20 72 65 61 64 79 2b 43 68 72 6f 6d 65  tor ready+Chrome
0170: 63 61 73 74 2d 38 32 38 30 30 66 65 63 33 37 37  cast-82800fec377
0180: 36 35 39 38 66 35 38 64 34 61 61 63 39 65 36 35  6598f58d4aac9e65
0190: 64 30 30 30 61 0b 5f 67 6f 6f 67 6c 65 63 61 73  d000a._googlecas
01a0: 74 04 5f 74 63 70 05 6c 6f 63 61 6c 00 00 21 80  t._tcp.local..!.
01b0: 01 00 00 00 78 00 32 00 00 00 00 1f 49 24 38 32  ....x.2.....I$82
01c0: 38 30 30 66 65 63 2d 33 37 37 36 2d 35 39 38 66  800fec-3776-598f
01d0: 2d 35 38 64 34 2d 61 61 63 39 65 36 35 64 30 30  -58d4-aac9e65d00
01e0: 30 61 05 6c 6f 63 61 6c 00 24 38 32 38 30 30 66  0a.local.$82800f
01f0: 65 63 2d 33 37 37 36 2d 35 39 38 66 2d 35 38 64  ec-3776-598f-58d
0200: 34 2d 61 61 63 39 65 36 35 64 30 30 30 61 05 6c  4-aac9e65d000a.l
0210: 6f 63 61 6c 00 00 01 80 01 00 00 00 78 00 04 0a  ocal........x...
0220: 00 00 80                                         ...

ChromeCast 3 (Ultra)

0000: 00 00 84 00 00 00 00 01 00 00 00 03 0b 5f 67 6f  ............._go
0010: 6f 67 6c 65 63 61 73 74 04 5f 74 63 70 05 6c 6f  oglecast._tcp.lo
0020: 63 61 6c 00 00 0c 00 01 00 00 00 78 00 34 31 43  cal........x.41C
0030: 68 72 6f 6d 65 63 61 73 74 2d 55 6c 74 72 61 2d  hromecast-Ultra-
0040: 32 38 36 65 36 63 63 65 38 34 65 32 38 65 61 33  286e6cce84e28ea3
0050: 36 63 37 63 33 31 39 32 66 33 35 62 65 30 33 65  6c7c3192f35be03e
0060: c0 0c c0 2e 00 10 80 01 00 00 11 94 00 8d 23 69  ..............#i
0070: 64 3d 32 38 36 65 36 63 63 65 38 34 65 32 38 65  d=286e6cce84e28e
0080: 61 33 36 63 37 63 33 31 39 32 66 33 35 62 65 30  a36c7c3192f35be0
0090: 33 65 03 72 6d 3d 05 76 65 3d 30 35 13 6d 64 3d  3e.rm=.ve=05.md=
00a0: 43 68 72 6f 6d 65 63 61 73 74 20 55 6c 74 72 61  Chromecast Ultra
00b0: 12 69 63 3d 2f 73 65 74 75 70 2f 69 63 6f 6e 2e  .ic=/setup/icon.
00c0: 70 6e 67 16 66 6e 3d 43 68 72 6f 6d 65 63 61 73  png.fn=Chromecas
00d0: 74 55 6c 74 72 61 36 32 35 30 07 63 61 3d 34 31  tUltra6250.ca=41
00e0: 30 31 04 73 74 3d 30 0f 62 73 3d 46 41 38 46 43  01.st=0.bs=FA8FC
00f0: 41 37 30 38 35 42 35 03 72 73 3d c0 2e 00 21 80  A7085B5.rs=...!.
0100: 01 00 00 00 78 00 2d 00 00 00 00 1f 49 24 32 38  ....x.-.....I$28
0110: 36 65 36 63 63 65 2d 38 34 65 32 2d 38 65 61 33  6e6cce-84e2-8ea3
0120: 2d 36 63 37 63 2d 33 31 39 32 66 33 35 62 65 30  -6c7c-3192f35be0
0130: 33 65 c0 1d c1 0d 00 01 80 01 00 00 00 78 00 04  3e...........x..
0140: 0a 00 01 1d                                      ....

*/


/* Other non-Google devices:

Vizio M60-D1 :- Smart TV

00000000  00 00 84 00 00 00 00 01  00 00 00 03 0b 5f 67 6f  |............._go|
00000010  6f 67 6c 65 63 61 73 74  04 5f 74 63 70 05 6c 6f  |oglecast._tcp.lo|
00000020  63 61 6c 00 00 0c 00 01  00 00 00 78 00 40 27 4d  |cal........x.@'M|
00000030  36 30 2d 44 31 2d 37 63  35 35 36 37 39 31 35 38  |60-D1-7c55679158|
00000040  38 64 63 64 33 37 63 37  37 32 33 31 61 30 64 64  |8dcd37c77231a0dd|
00000050  31 35 61 35 30 36 0b 5f  67 6f 6f 67 6c 65 63 61  |15a506._googleca|
00000060  73 74 04 5f 74 63 70 05  6c 6f 63 61 6c 00 27 4d  |st._tcp.local.'M|
00000070  36 30 2d 44 31 2d 37 63  35 35 36 37 39 31 35 38  |60-D1-7c55679158|
00000080  38 64 63 64 33 37 63 37  37 32 33 31 61 30 64 64  |8dcd37c77231a0dd|
00000090  31 35 61 35 30 36 0b 5f  67 6f 6f 67 6c 65 63 61  |15a506._googleca|
000000a0  73 74 04 5f 74 63 70 05  6c 6f 63 61 6c 00 00 10  |st._tcp.local...|
000000b0  80 01 00 00 11 94 00 80  23 69 64 3d 37 63 35 35  |........#id=7c55|
000000c0  36 37 39 31 35 38 38 64  63 64 33 37 63 37 37 32  |6791588dcd37c772|
000000d0  33 31 61 30 64 64 31 35  61 35 30 36 13 72 6d 3d  |31a0dd15a506.rm=|
000000e0  31 33 39 46 33 32 34 34  38 34 44 46 41 39 32 34  |139F324484DFA924|
000000f0  05 76 65 3d 30 35 09 6d  64 3d 4d 36 30 2d 44 31  |.ve=05.md=M60-D1|
00000100  12 69 63 3d 2f 73 65 74  75 70 2f 69 63 6f 6e 2e  |.ic=/setup/icon.|
00000110  70 6e 67 0f 66 6e 3d 56  69 7a 69 6f 20 4d 36 30  |png.fn=Vizio M60|
00000120  2d 44 31 07 63 61 3d 32  30 35 33 04 73 74 3d 30  |-D1.ca=2053.st=0|
00000130  03 62 73 3d 03 72 73 3d  27 4d 36 30 2d 44 31 2d  |.bs=.rs='M60-D1-|
00000140  37 63 35 35 36 37 39 31  35 38 38 64 63 64 33 37  |7c556791588dcd37|
00000150  63 37 37 32 33 31 61 30  64 64 31 35 61 35 30 36  |c77231a0dd15a506|
00000160  0b 5f 67 6f 6f 67 6c 65  63 61 73 74 04 5f 74 63  |._googlecast._tc|
00000170  70 05 6c 6f 63 61 6c 00  00 21 80 01 00 00 00 78  |p.local..!.....x|
00000180  00 32 00 00 00 00 1f 49  24 37 63 35 35 36 37 39  |.2.....I$7c55679|
00000190  31 2d 35 38 38 64 2d 63  64 33 37 2d 63 37 37 32  |1-588d-cd37-c772|
000001a0  2d 33 31 61 30 64 64 31  35 61 35 30 36 05 6c 6f  |-31a0dd15a506.lo|
000001b0  63 61 6c 00 24 37 63 35  35 36 37 39 31 2d 35 38  |cal.$7c556791-58|
000001c0  38 64 2d 63 64 33 37 2d  63 37 37 32 2d 33 31 61  |8d-cd37-c772-31a|
000001d0  30 64 64 31 35 61 35 30  36 05 6c 6f 63 61 6c 00  |0dd15a506.local.|
000001e0  00 01 80 01 00 00 00 78  00 04 c0 a8 00 3e        |.......x.....>|
000001ee

SmartCast Sound Bar 3851-D0  :- Sound bar

00000000  00 00 84 00 00 00 00 01  00 00 00 03 0b 5f 67 6f  |............._go|
00000010  6f 67 6c 65 63 61 73 74  04 5f 74 63 70 05 6c 6f  |oglecast._tcp.lo|
00000020  63 61 6c 00 00 0c 00 01  00 00 00 78 00 22 09 53  |cal........x.".S|
00000030  42 33 38 35 31 2d 44 30  0b 5f 67 6f 6f 67 6c 65  |B3851-D0._google|
00000040  63 61 73 74 04 5f 74 63  70 05 6c 6f 63 61 6c 00  |cast._tcp.local.|
00000050  09 53 42 33 38 35 31 2d  44 30 0b 5f 67 6f 6f 67  |.SB3851-D0._goog|
00000060  6c 65 63 61 73 74 04 5f  74 63 70 05 6c 6f 63 61  |lecast._tcp.loca|
00000070  6c 00 00 10 80 01 00 00  11 94 00 8a 23 69 64 3d  |l...........#id=|
00000080  64 35 36 63 33 66 30 39  36 66 30 31 30 31 36 66  |d56c3f096f01016f|
00000090  33 64 37 30 65 62 62 63  36 32 36 34 65 66 33 31  |3d70ebbc6264ef31|
000000a0  05 76 65 3d 30 34 1e 6d  64 3d 53 6d 61 72 74 43  |.ve=04.md=SmartC|
000000b0  61 73 74 20 53 6f 75 6e  64 20 42 61 72 20 33 38  |ast Sound Bar 38|
000000c0  35 31 2d 44 30 12 69 63  3d 2f 73 65 74 75 70 2f  |51-D0.ic=/setup/|
000000d0  69 63 6f 6e 2e 70 6e 67  0c 66 6e 3d 53 42 33 38  |icon.png.fn=SB38|
000000e0  35 31 2d 44 30 07 63 61  3d 32 30 35 32 04 73 74  |51-D0.ca=2052.st|
000000f0  3d 30 0f 62 73 3d 46 46  46 46 46 46 46 46 46 46  |=0.bs=FFFFFFFFFF|
00000100  46 46 03 72 73 3d 09 53  42 33 38 35 31 2d 44 30  |FF.rs=.SB3851-D0|
00000110  0b 5f 67 6f 6f 67 6c 65  63 61 73 74 04 5f 74 63  |._googlecast._tc|
00000120  70 05 6c 6f 63 61 6c 00  00 21 80 01 00 00 00 78  |p.local..!.....x|
00000130  00 17 00 00 00 00 1f 49  09 53 42 33 38 35 31 2d  |.......I.SB3851-|
00000140  44 30 05 6c 6f 63 61 6c  00 09 53 42 33 38 35 31  |D0.local..SB3851|
00000150  2d 44 30 05 6c 6f 63 61  6c 00 00 01 80 01 00 00  |-D0.local.......|
00000160  00 78 00 04 c0 a8 00 26                           |.x.....&|
00000168

ZChromecast Toshiba :- TV ?

00000000  00 00 84 00 00 00 00 01  00 00 00 03 0b 5f 67 6f  |............._go|
00000010  6f 67 6c 65 63 61 73 74  04 5f 74 63 70 05 6c 6f  |oglecast._tcp.lo|
00000020  63 61 6c 00 00 0c 00 01  00 00 00 78 00 2e 2b 43  |cal........x..+C|
00000030  68 72 6f 6d 65 63 61 73  74 2d 64 61 30 33 34 62  |hromecast-da034b|
00000040  31 39 30 30 34 34 62 63  38 65 35 33 30 31 34 37  |190044bc8e530147|
00000050  63 37 61 65 33 32 63 35  61 33 c0 0c c0 2e 00 10  |c7ae32c5a3......|
00000060  80 01 00 00 11 94 00 87  23 69 64 3d 64 61 30 33  |........#id=da03|
00000070  34 62 31 39 30 30 34 34  62 63 38 65 35 33 30 31  |4b190044bc8e5301|
00000080  34 37 63 37 61 65 33 32  63 35 61 33 03 72 6d 3d  |47c7ae32c5a3.rm=|
00000090  05 76 65 3d 30 35 0d 6d  64 3d 43 68 72 6f 6d 65  |.ve=05.md=Chrome|
000000a0  63 61 73 74 12 69 63 3d  2f 73 65 74 75 70 2f 69  |cast.ic=/setup/i|
000000b0  63 6f 6e 2e 70 6e 67 16  66 6e 3d 5a 43 68 72 6f  |con.png.fn=ZChro|
000000c0  6d 65 63 61 73 74 20 54  6f 73 68 69 62 61 07 63  |mecast Toshiba.c|
000000d0  61 3d 34 31 30 31 04 73  74 3d 30 0f 62 73 3d 46  |a=4101.st=0.bs=F|
000000e0  41 38 46 43 41 37 30 32  30 36 32 03 72 73 3d c0  |A8FCA702062.rs=.|
000000f0  2e 00 21 80 01 00 00 00  78 00 2d 00 00 00 00 1f  |..!.....x.-.....|
00000100  49 24 64 61 30 33 34 62  31 39 2d 30 30 34 34 2d  |I$da034b19-0044-|
00000110  62 63 38 65 2d 35 33 30  31 2d 34 37 63 37 61 65  |bc8e-5301-47c7ae|
00000120  33 32 63 35 61 33 c0 1d  c1 01 00 01 80 01 00 00  |32c5a3..........|
00000130  00 78 00 04 c0 a8 00 1b                           |.x......|
00000138

*/

/* 

	Can get info from Chromecast http server at http://XX.XX.XX.XX:8008/ssdp/device-desc.xml

	Get info about the app running: http://XX.XX.XX.XX:8008/apps/ChromeCast

*/


