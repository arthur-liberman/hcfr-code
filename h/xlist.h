/* A set of expandable list utilities, implimented as macros. */
/* Copyright 2006 Graeme W. Gill */

#ifndef _XLIST_H_

/* An expanding list structure */
#define XLIST(objtype, name)								\
struct {													\
	int no;					/* Number of items in list */	\
	int _no;				/* Allocated size of list */	\
	int objsz;				/* Size of object */			\
	objtype  *list;			/* list */						\
} name;

/* Initialize the list */
#define XLIST_INIT(objtype, xlp) 		\
	((xlp)->no = (xlp)->_no = 0,		\
	 (xlp)->objsz = sizeof(objtype),	\
	 (xlp)->list = NULL					\
	)

/* test if the list is empty */
#define IS_XLIST_EMPTY(xlp) \
	((xlp)->no == 0)

/* Return the number of items in the list */
#define XLIST_NO(xlp) \
	((xlp)->no)

/* Return the n'th item in the list */
#define XLIST_ITEM(xlp, n) \
	((xlp)->list[n])

/* Add an item to the end of a list */
/* We call error() if malloc failes */
#define XLIST_ADD(xlp, obj) \
	{																			\
		if ((xlp)->_no <= 0) {													\
			(xlp)->_no = 10;													\
			if (((xlp)->list = malloc((xlp)->objsz * (xlp)->_no)) == NULL)		\
				error("XLIST malloc failed on %d items of size %d", (xlp)->objsz, (xlp)->_no);		\
		} else if ((xlp)->_no <= (xlp)->no) {									\
			(xlp)->_no *= 2;		\
			if (((xlp)->list = realloc((xlp)->list, (xlp)->objsz * (xlp)->_no)) == NULL) \
				error("XLIST realloc failed on %d items of size %d", (xlp)->objsz, (xlp)->_no);		\
		}																		\
		(xlp)->list[(xlp)->no++] = (obj);										\
	}
	
/* Free up the list */
#define XLIST_FREE(xlp) \
		{ if ((xlp)->_no > 0) free((xlp)->list); }

#define _XLIST_H_
#endif /* _XLIST_H_ */

