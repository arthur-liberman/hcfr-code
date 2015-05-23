/* A set of linked list utilities, implimented as macros. */
/* Copyright 1995, 2000 Graeme W. Gill */

#ifndef _LLIST_H_

/* A link list structure */
#define LINKSTRUCT(obj) \
struct {				\
	obj   *fwd;			/* Forward link */		\
	obj   *bwd;			/* Backwards link */		\
} _llistp

/* Initialize the linked list */
#define INIT_LIST(head) \
	((head) = NULL)

/* test if the list is empty */
#define IS_LIST_EMPTY(head) \
	((head) == NULL)

/* Add an item to the top of a list */
#define ADD_ITEM_TO_TOP(head,objp) \
	(IS_LIST_EMPTY(head) ? (INIT_LINK(objp), (head) = objp) : \
	(ADD_LINK_BWD((head),(objp)), (head) = objp))
	
/* Add an item to the bottom of a list */
#define ADD_ITEM_TO_BOT(head,objp) \
	(IS_LIST_EMPTY(head) ? (INIT_LINK(objp), (head) = objp) : \
	(ADD_LINK_BWD((head),(objp))))
	
/* Insert a new item forward of the old one in linked list */
#define ADD_LINK_FWD(oob,nob)	\
	((nob)->_llistp.fwd = (oob)->_llistp.fwd, \
	(nob)->_llistp.bwd = (oob), \
	(oob)->_llistp.fwd->_llistp.bwd = (nob),	\
	(oob)->_llistp.fwd = (nob))

/* Insert a new item backward of the old one in linked list */
#define ADD_LINK_BWD(oob,nob)	\
	((nob)->_llistp.bwd = (oob)->_llistp.bwd, \
	(nob)->_llistp.fwd = (oob), \
	(oob)->_llistp.bwd->_llistp.fwd = (nob),	\
	(oob)->_llistp.bwd = (nob))

/* Delete an object from the list. */
#define DEL_LINK(head,objp)	\
    (IS_ONE_ITEM(objp) ? (head) = NULL : \
	((objp) == (head) ? (head) = (head)->_llistp.fwd : 0, \
    (objp)->_llistp.fwd->_llistp.bwd = (objp)->_llistp.bwd, \
	(objp)->_llistp.bwd->_llistp.fwd = (objp)->_llistp.fwd, \
	(objp)->_llistp.fwd = (objp)->_llistp.bwd = (objp)))

/* Combine two linked lists. Doesn't fix parent/child stuff */
/* Breaks lists backward of oob, and forward of nob */
#define MERGE_LINK(oob,nob) \
	(((nob)->_llistp.fwd)->_llistp.bwd = (oob)->_llistp.bwd, \
	((oob)->_llistp.bwd)->_llistp.fwd = (nob)->_llistp.fwd, \
	(nob)->_llistp.fwd = (oob), \
	(oob)->_llistp.bwd = (nob))


/* Scan through all of the items in a linked list */
/* (Robust version, copes with deleting the items) */
#define FOR_ALL_ITEMS(objtype, objp)			\
	if (objp != NULL)	\
		{							\
		objtype *_end, *_next;	\
		_end = objp->_llistp.bwd;	\
		_next = objp->_llistp.fwd;	\
		do {

#define END_FOR_ALL_ITEMS(objp)		\
		} while (objp == _end ? 0 :	\
		         (objp = _next, _next = objp->_llistp.fwd, 1)); \
		}

#define NEXT_FWD(objp)				\
	         (objp == NULL ? NULL : objp->_llistp.fwd)

/* ----------------------------- */

/* initialise a list to have only one entry */
#define INIT_LINK(objp) \
	((objp)->_llistp.fwd = (objp)->_llistp.bwd = (objp))

/* test if the given object is the only one */
#define IS_ONE_ITEM(objp) \
	((objp) == (objp)->_llistp.fwd)

#define _LLIST_H_
#endif /* _LLIST_H_ */

