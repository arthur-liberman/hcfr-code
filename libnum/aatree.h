#ifndef AATREE_H
#define AATREE_H

/*
  Andersson binary balanced tree library

  > Created (Julienne Walker): September 10, 2005

  This code is in the public domain. Anyone may
  use it or change it in any way that they see
  fit. The author assumes no responsibility for 
  damages incurred through use of the original
  code or any variations thereof.

  It is requested, but not required, that due
  credit is given to the original author and
  anyone who has modified the code through
  a header comment, such as this one.
*/

#ifdef __cplusplus
#include <cstddef>

using std::size_t;

extern "C" {
#else
#include <stddef.h>
#endif

/* Opaque types */
typedef struct aat_atree aat_atree_t;
typedef struct aat_atrav aat_atrav_t;

/* User-defined item handling */
typedef int   (*cmp_f) ( const void *p1, const void *p2 );

/* Andersson tree functions */
aat_atree_t *aat_anew ( cmp_f cmp );
void         aat_adelete ( aat_atree_t *tree );
void        *aat_afind ( aat_atree_t *tree, void *data );
int          aat_ainsert ( aat_atree_t *tree, void *data );
int          aat_aerase ( aat_atree_t *tree, void *data );
size_t       aat_asize ( aat_atree_t *tree );

/* Traversal functions */
aat_atrav_t *aat_atnew ( void );
void         aat_atdelete ( aat_atrav_t *trav );
void        *aat_atfirst ( aat_atrav_t *trav, aat_atree_t *tree );
void        *aat_atlast ( aat_atrav_t *trav, aat_atree_t *tree );
void        *aat_atnext ( aat_atrav_t *trav );
void        *aat_atprev ( aat_atrav_t *trav );

#ifdef __cplusplus
}
#endif

#endif	/* AATREE */
