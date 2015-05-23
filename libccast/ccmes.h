
#ifndef CCMES

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

#ifdef __cplusplus
	extern "C" {
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* A single message */
typedef struct _ccmes {
	struct _ccmes *next;		/* Linked list */
	yajl_val tnode;				/* If not NULL, top node of parsed data */
	char *mtype;				/* If not NULL, "type". Points into tnode data */
	int rqid;					/* "requestId", 0 by default */

	char *source_id;			/* Source name */
	char *destination_id;		/* Destination name */
	char *namespace;			/* Channel id */
	int binary;					/* Binary data flag */
	ORD8 *data;					/* String or binary */
	ORD32 bin_len;				/* Binary data length */
} ccmes;

/* Transfer the data from one message to another */
void ccmes_transfer(ccmes *dst, ccmes *src);

/* Initialise a message */
void ccmes_init(ccmes *mes);

/* Free up just the contents */
void ccmes_empty(ccmes *mes);

/* Free up the message and its contents */
void ccmes_del(ccmes *mes);

/* - - - - - - - - - - - - - - - - - - - - - - - - */

typedef enum {
	ccmessv_OK = 0,
	ccmessv_malloc,				/* malloc failed */
	ccmessv_context,			/* Getting a ssl context failed */
	ccmessv_connect,			/* Connecting to host failed */
	ccmessv_ssl,				/* Establishing SSL connection to host failed */

	ccmessv_send,				/* Message body failed to send */
	ccmessv_recv,				/* No body or failed to read */
	ccmessv_unpack,				/* Failed to unpack protobufs */
	ccmessv_timeout,			/* Failed due to timeout on i/o operation */
	ccmessv_closed				/* Connection has been closed */
} ccmessv_err;

/* Error message from error number */
char *ccmessv_emes(ccmessv_err rv);


/* The central facility to send and receive messages */
typedef struct _ccmessv {

/* Public: */

	/* Delete the ccmessv */
	void (*del)(struct _ccmessv *p);

	/* Send a raw message */
	/* Return ccmessv_err on error */
	ccmessv_err (*send)(struct _ccmessv *p, ccmes *mes);

	/* Receive a message. mes->data should be free's after use */
	/* Return ccmessv_err on error */
	ccmessv_err (*receive)(struct _ccmessv *p, ccmes *mes);

	ccpacket *pk;

	amutex slock;					/* Send lock protecting */

} ccmessv;

/* Create a new ccmessv object, and hand it the working packet connection. */
/* (ccmessv does not close it when deleted) */
/* Return NULL on error */
ccmessv *new_ccmessv(ccpacket *pk);

#ifdef __cplusplus
	}
#endif

#define CCMES_H
#endif /* CCMES_H */
