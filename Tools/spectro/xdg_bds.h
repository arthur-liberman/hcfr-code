
#ifndef XDG_BDS_H

 /* XDG Base Directory Specifications support library */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   28/7/2010
 *
 * Copyright 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* Which type of storage */
typedef enum {
	xdg_data,
	xdg_conf,
	xdg_cache			/* Note there is no xdg_local cache */
} xdg_storage_type;

/* What operation is being performed */
typedef enum {
	xdg_write,			/* Create or write */
	xdg_read			/* Read */
} xdg_op_type;

/* What scope to write to */
/* (For write only. Read always searches */
/* the user context then the local system context.) */
typedef enum {
	xdg_user,			/* User context */
	xdg_local			/* Local system wide context */
} xdg_scope;

/* An error code */
typedef enum {
	xdg_ok = 0,
	xdg_alloc,		/* A memory allocation failed */
	xdg_nohome,		/* There is no $HOME */
	xdg_noalluserprofile,	/* There is no $ALLUSERSPROFILE */
	xdg_nopath,		/* There is no resulting path */
	xdg_mallformed	/* Malfomed path */
} xdg_error;

/* Return the number of matching full paths to the given subpath for the */
/* type of storage and access required. Return 0 if there is an error. */
/* The files are always unique (ie. the first match to a given filename */
/* in the possible XDG list of directories is returned, and files with */
/* the same name in other XDG directories are ignored) */
/* Wildcards should only be for the filename portion, and not be used for xdg_write. */
/* The list should be free'd using xdg_free() after use. */
/* XDG environment variables and the subpath are assumed to be using */
/* the '/' path separator. */
/* When "xdg_write", the necessary path to the file will be created. */
/* If we're running as sudo and are creating a user dir/file, */
/* we drop to using the underlying SUDO_UID/GID. If we are creating a */
/* local system dir/file as sudo and have dropped to the SUDO_UID/GID, */
/* then revert back to root uid/gid. */
int xdg_bds(
	xdg_error *er,			/* Return an error code */
	char ***paths,			/* Return array of pointers to paths */
	xdg_storage_type st,	/* Specify the storage type */
	xdg_op_type op,			/* Operation type */
	xdg_scope sc,			/* Scope if write */
	char *spath				/* Sub-path and file name or file pattern */
);

/* Free the list */
void xdg_free(char **paths, int nopaths);

/* Return a string corresponding to the error value */
char *xdg_errstr(xdg_error er);


#define XDG_BDS_H
#endif /* XDG_BDS_H */

#ifdef __cplusplus
	}
#endif

