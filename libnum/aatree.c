
/*
  Andersson binary balanced tree library

    > Created (Julienne Walker): September 10, 2005
    > Corrections (James Bucanek): April 10, 2006
      1) Typo in jsw_aerase:
           up != 0 should be top != 0
      2) Bug in jsw_aerase:
           skew ( path[top] ) should be skew ( up )
           split ( path[top] ) should be split ( up )
      3) Bug in skew and split macros:
           Condition should test for nil
      4) Bug in jsw_aerase:
           Search for successor should save the path
    > Fixed duplicate handling (Graeme W. Gill): August 16, 2009
*/

#include "aatree.h"

#ifdef __cplusplus
#include <cstdlib>

using std::malloc;
using std::free;
using std::size_t;
#else
#include <stdlib.h>
#endif

#ifndef HEIGHT_LIMIT
#define HEIGHT_LIMIT 64 /* Tallest allowable tree */
#endif

typedef struct aat_anode {
  int               level;   /* Horizontal level for balance */
  void             *data;    /* User-defined content */
  struct aat_anode *link[2]; /* Left (0) and right (1) links */
} aat_anode_t;

struct aat_atree {
  aat_anode_t *root; /* Top of the tree */
  aat_anode_t *nil;  /* End of tree sentinel */
  cmp_f        cmp;  /* Compare two items */
  size_t       size; /* Number of items (user-defined) */
};

struct aat_atrav {
  aat_atree_t *tree;               /* Paired tree */
  aat_anode_t *it;                 /* Current node */
  aat_anode_t *path[HEIGHT_LIMIT]; /* Traversal path */
  size_t       top;                /* Top of stack */
};

/* Remove left horizontal links */
#define skew(t) do {                                      \
  if ( t->link[0]->level == t->level && t->level != 0 ) { \
    aat_anode_t *save = t->link[0];                       \
    t->link[0] = save->link[1];                           \
    save->link[1] = t;                                    \
    t = save;                                             \
  }                                                       \
} while(0)

/* Remove consecutive horizontal links */
#define split(t) do {                                              \
  if ( t->link[1]->link[1]->level == t->level && t->level != 0 ) { \
    aat_anode_t *save = t->link[1];                                \
    t->link[1] = save->link[0];                                    \
    save->link[0] = t;                                             \
    t = save;                                                      \
    ++t->level;                                                    \
  }                                                                \
} while(0)

/* Version of cmp that ensure there are no duplicates */
/* Set res to comparison result */
#define AAT_CMP( res, tree, p1, p2)									\
{																	\
	if ((res = tree->cmp ( p1, p2 )) == 0) {						\
		if (p1 < p2)												\
			res = -1;												\
		else if (p1 > p2)											\
			res = 1;												\
	}																\
}

/* Create a new node that points to our data */
/* Return tree-nil if malloc failed. */
static aat_anode_t *new_node ( aat_atree_t *tree, void *data )
{
  aat_anode_t *rn = (aat_anode_t *)malloc ( sizeof *rn );

  if ( rn == NULL )
    return tree->nil;

  rn->level = 1;
  rn->data = data;
  rn->link[0] = rn->link[1] = tree->nil;

  return rn;
}

/* Create a new empty tree */
aat_atree_t *aat_anew ( cmp_f cmp )
{
  aat_atree_t *rt = (aat_atree_t *)malloc ( sizeof *rt );

  if ( rt == NULL )
    return NULL;

  /* Initialize sentinel */
  rt->nil = (aat_anode_t *)malloc ( sizeof *rt->nil );
  if ( rt->nil == NULL ) {
    free ( rt );
    return NULL;
  }

  rt->nil->data = NULL; /* Simplifies some ops */
  rt->nil->level = 0;
  rt->nil->link[0] = rt->nil->link[1] = rt->nil;

  /* Initialize tree */
  rt->root = rt->nil;
  rt->cmp = cmp;
  rt->size = 0;

  return rt;
}

/* Delete the whole tree (but not the data) */
void aat_adelete ( aat_atree_t *tree )
{
  aat_anode_t *it = tree->root;
  aat_anode_t *save;

  /* Destruction by rotation */
  while ( it != tree->nil ) {
    if ( it->link[0] == tree->nil ) {
      /* Remove node */
      save = it->link[1];
      free ( it );
    }
    else {
      /* Rotate right */
      save = it->link[0];
      it->link[0] = save->link[1];
      save->link[1] = it;
    }

    it = save;
  }

  /* Finalize destruction */
  free ( tree->nil );
  free ( tree );
}

/* Locate a entry based on the data's value */
void *aat_afind ( aat_atree_t *tree, void *data )
{
  aat_anode_t *it = tree->root;

  while ( it != tree->nil ) {
    int cmp;
	AAT_CMP(cmp, tree, it->data, data);

    if ( cmp == 0 )
      break;

    it = it->link[cmp < 0];
  }

  /* nil->data == NULL */
  return it->data;
}

/* Insert an entry based on the data's value. */
/* Take a copy of the pointer to the entry. */
/* Return 0 if malloc failed */
int aat_ainsert ( aat_atree_t *tree, void *data )
{
  if ( tree->root == tree->nil ) {
    /* Empty tree case */
    tree->root = new_node ( tree, data );
    if ( tree->root == tree->nil )
      return 0;
  }
  else {
    aat_anode_t *it = tree->root;
    aat_anode_t *path[HEIGHT_LIMIT];
    int top = 0, dir;

    /* Find a spot and save the path */
    for ( ; ; ) {
	  int cmp;

      path[top++] = it;
	  AAT_CMP(cmp, tree, it->data, data);
      dir = cmp < 0;

      if ( it->link[dir] == tree->nil )
        break;

      it = it->link[dir];
    }

    /* Create a new item */
    it->link[dir] = new_node ( tree, data );
    if ( it->link[dir] == tree->nil )
      return 0;

    /* Walk back and rebalance */
    while ( --top >= 0 ) {
      /* Which child? */
      if ( top != 0 )
        dir = path[top - 1]->link[1] == path[top];

      skew ( path[top] );
      split ( path[top] );

      /* Fix the parent */
      if ( top != 0 )
        path[top - 1]->link[dir] = path[top];
      else
        tree->root = path[top];
    }
  }

  ++tree->size;

  return 1;
}

/* Delete an given entry by locating it by */
/* its value, then removing it from the tree. */
/* The item itself isn't deleted. */
/* Return 0 if the item wasn't found */
int aat_aerase ( aat_atree_t *tree, void *data )
{
  if ( tree->root == tree->nil )
    return 0;
  else {
    aat_anode_t *it = tree->root;
    aat_anode_t *path[HEIGHT_LIMIT];
    int top = 0, dir, cmp;

    /* Find node to remove and save path */
    for ( ; ; ) {
      path[top++] = it;

      if ( it == tree->nil )
        return 0;

	  AAT_CMP(cmp, tree, it->data, data);
      if ( cmp == 0 )
        break;

      dir = cmp < 0;
      it = it->link[dir];
    }

    /* Remove the found node */
    if ( it->link[0] == tree->nil
      || it->link[1] == tree->nil )
    {
      /* Single child case */
      int dir2 = it->link[0] == tree->nil;

      /* Unlink the item */
      if ( --top != 0 )
        path[top - 1]->link[dir] = it->link[dir2];
      else
        tree->root = it->link[1];

      free ( it );
    }
    else {
      /* Two child case */
      aat_anode_t *heir = it->link[1];
      aat_anode_t *prev = it;

      while ( heir->link[0] != tree->nil ) {
        path[top++] = prev = heir;
        heir = heir->link[0];
      }

      /*
        Order is important!
        (free item, replace item, free heir)
      */
      it->data = heir->data;
      prev->link[prev == it] = heir->link[1];
      free ( heir );
    }

    /* Walk back up and rebalance */
    while ( --top >= 0 ) {
      aat_anode_t *up = path[top];

      if ( top != 0 )
        dir = path[top - 1]->link[1] == up;

      /* Rebalance (aka. black magic) */
      if ( up->link[0]->level < up->level - 1
        || up->link[1]->level < up->level - 1 )
      {
        if ( up->link[1]->level > --up->level )
          up->link[1]->level = up->level;

        /* Order is important! */
        skew ( up );
        skew ( up->link[1] );
        skew ( up->link[1]->link[1] );
        split ( up );
        split ( up->link[1] );
      }

      /* Fix the parent */
      if ( top != 0 )
        path[top - 1]->link[dir] = up;
      else
        tree->root = up;
    }
  }

  --tree->size;

  return 1;
}

/* Return the current number of items */
size_t aat_asize ( aat_atree_t *tree )
{
  return tree->size;
}

/* Return a traversal object */
aat_atrav_t *aat_atnew ( void )
{
  return malloc ( sizeof ( aat_atrav_t ) );
}

/* Destroy a traversal object */
void aat_atdelete ( aat_atrav_t *trav )
{
  free ( trav );
}

/*
  First step in traversal,
  handles min and max
*/
static void *start ( aat_atrav_t *trav,
  aat_atree_t *tree, int dir )
{
  trav->tree = tree;
  trav->it = tree->root;
  trav->top = 0;

  /* Build a path to work with */
  if ( trav->it != tree->nil ) {
    while ( trav->it->link[dir] != tree->nil ) {
      trav->path[trav->top++] = trav->it;
      trav->it = trav->it->link[dir];
    }
  }

  /* Could be nil, but nil->data == NULL */
  return trav->it->data;
}

/*
  Subsequent traversal steps,
  handles ascending and descending
*/
static void *move ( aat_atrav_t *trav, int dir )
{
  aat_anode_t *nil = trav->tree->nil;

  if ( trav->it->link[dir] != nil ) {
    /* Continue down this branch */
    trav->path[trav->top++] = trav->it;
    trav->it = trav->it->link[dir];

    while ( trav->it->link[!dir] != nil ) {
      trav->path[trav->top++] = trav->it;
      trav->it = trav->it->link[!dir];
    }
  }
  else {
    /* Move to the next branch */
    aat_anode_t *last;

    do {
      if ( trav->top == 0 ) {
        trav->it = nil;
        break;
      }

      last = trav->it;
      trav->it = trav->path[--trav->top];
    } while ( last == trav->it->link[dir] );
  }

  /* Could be nil, but nil->data == NULL */
  return trav->it->data;
}

/* Return first item */
void *aat_atfirst ( aat_atrav_t *trav, aat_atree_t *tree )
{
  return start ( trav, tree, 0 ); /* Min value */
}

/* Return last item */
void *aat_atlast ( aat_atrav_t *trav, aat_atree_t *tree )
{
  return start ( trav, tree, 1 ); /* Max value */
}

/* Move to next */
void *aat_atnext ( aat_atrav_t *trav )
{
  return move ( trav, 1 ); /* Toward larger items */
}

/* Move to previous */
void *aat_atprev ( aat_atrav_t *trav )
{
  return move ( trav, 0 ); /* Toward smaller items */
}
