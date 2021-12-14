
#ifndef OEMARCH_H

 /* OEM archive access library. */
 /* This supports installing OEM data files */

/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   13/11/2012
 *
 * Copyright 2006 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 */

#ifdef __cplusplus
	extern "C" {
#endif

#define MAX_ARCH_LISTS 50

/* Possible type of files/stage of processing */
typedef enum {
	file_none              = 0x0000,	/* None */
	file_vol               = 0x0001,	/* Volume name */
	file_arch              = 0x0002,	/* Archive file */
	file_dllcab            = 0x0004,	/* .dll or .cab - msi ?? */
	file_data              = 0x0008		/* final data file */
} file_type;

/* Possible type of target */
typedef enum {
	targ_none              = 0x0000,	/* None */
	targ_spyd1_pld         = 0x0001,	/* Spyder1 PLD pattern (???) */
	targ_spyd2_pld         = 0x0002,	/* Spyder2 PLD pattern */
	targ_spyd_cal          = 0x0004,	/* Spyder spectral calibration */
	targ_i1d3_edr          = 0x0008,	/* i1d3 .edr or .ccss */
	targ_ccmx              = 0x0010,	/* .ccmx */
	targ_unknown           = 0x8000		/* Unrecognized file format */
} targ_type;

/* An install path, volume name or archive name */
typedef struct {
	char *path;
	targ_type ttype;		/* Hint to target type */
	file_type ftype;		/* Hint to file type (instpaths only) */
} oem_source;

/* A definition of an install target */
typedef struct {
	oem_source instpaths[MAX_ARCH_LISTS]; /* Possible archive install paths, NULL terminated */
	oem_source volnames[MAX_ARCH_LISTS];  /* Possible volume names, NULL terminated */
	oem_source archnames[MAX_ARCH_LISTS]; /* Possible archive file names, NULL terminated */
} oem_target;

/* A list of files stored in memory. */
typedef struct {
	char *name;				/* Name of file, NULL for last entry */
	unsigned char *buf;		/* It's contents */
	size_t len;				/* The length of the contents */
	file_type ftype;		/* Hint to file type */
	targ_type ttype;		/* Target type */
} xfile;

/* return a list with the given number of available entries */
xfile *new_xf(int n);

/* Add an entry to the list. Create the list if it is NULL */
/* Return point to that entry */
xfile *add_xf(xfile **l);

/* Append an entry and copy details to the list. Create the list if it is NULL */
/* Return point to that entry */
xfile *new_add_xf(xfile **l, char *name, unsigned char *buf, unsigned long len,
                                               file_type ftype, targ_type ttype);

/* Clear this (last) entry on the list so that it becomes the end marker */
void rm_xf(xfile *xf);

/* Free up a whole list */
void del_xf(xfile *l);

/* Allocate and load the given entry. */
/* Return NZ on error */
int load_xfile(xfile *xf, int verb);

/* Save a file to the given sname, or of it is NULL, */
/* to prefix + file name (path is ignored) */
/* Error on failure */
void save_xfile(xfile *xf, char *sname, char *pfx, int verb);

/* Given a list of source archives or files, convert them to a list of install files, */
/* or if no files are given look for installed files or files from a CD. */
/* Return NULL if none found. files is deleted. */
xfile *oemarch_get_ifiles(xfile *files, int verb);

#ifdef __cplusplus
	}
#endif

#define OEMARCH_H
#endif /* OEMARCH_H */
