#ifndef CGATS_H

/* 
 * Committee for Graphics Arts Technologies Standards
 * CGATS.5 and IT8.7 family file I/O class
 * Version 2.05
 *
 * Author: Graeme W. Gill
 * Date:   20/12/95
 *
 * Copyright 1995, 1996, 2002, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License.txt file in this directory for licensing details.
 */

/* Version of cgatslib release */

#define CGATSLIB_VERSION 0x020005
#define CGATSLIB_VERSION_STR "2.05"

#define CGATS_ERRM_LENGTH 200

#ifdef __cplusplus
	extern "C" {
#endif

#include "pars.h"		/* We use the ASCII parsing class */

/* Possible table types */
typedef enum { it8_7_1, it8_7_2, it8_7_3, it8_7_4, cgats_5, cgats_X, tt_other, tt_none } table_type;

/* Possible data types - real, integer, char string, non-quoted char string, no value */
typedef enum { r_t, i_t, cs_t, nqcs_t, none_t } data_type;

union _cgats_set_elem {
	int i;
	double d;
	char *c;
}; typedef union _cgats_set_elem cgats_set_elem;

struct _cgats_table {
	cgatsAlloc *al;		/* Copy of parent memory allocator */
	table_type tt;		/* Table type */
	int oi;				/* other type index */

	/* Read only */
	int nkwords;		/* Number of keywords */
	int nfields;		/* Number of fields of data */
	int nsets;			/* Number of data sets */

	char **ksym;		/* Pointer to [nkwords] array of pointers to keyword symbols */
	char **kdata;		/* Pointer to [nkwords] array of pointers to keyword values */
	
	char **fsym;		/* Pointer to [nfields] array of pointers to field symbols */
	data_type *ftype;	/* Pointer to [nfields] array of field types */
	char ***rfdata;		/* Pointer to [nsets] array of pointers */
						/*         to [nfields] array of pointers to read file field text values */
	void ***fdata;		/* Pointer to [nsets] array of pointers */
						/*         to [nfields] array of pointers to field set values of ftype */
	/* Private */
	int nkwordsa;		/* Number of keywords allocated */
	int nfieldsa;		/* Number of fields allocated */
	int nsetsa;			/* Number of sets allocated */
	char **kcom;		/* Pointer to [nkwords] array of pointers to keyword comments */
	int ndf;			/* Next data field - used by add_data_item() */
	int sup_id;			/* Set to non-zero if table ID output is to be suppressed */
	int sup_kwords;		/* Set to non-zero if table default keyword output is to be suppressed */
	int sup_fields;		/* Set to non-zero if table field output is to be suppressed */
}; typedef struct _cgats_table cgats_table;

struct _cgats {
	/* Private */
	cgatsAlloc *al;		/* Memory allocator */
	int del_al;			/* Flag to indicate we al->del() */

	/* Read only Variables */
	int ntables;		/* Number of tables */

	cgats_table *t;		/* Pointer to an array of ntable table structures */

	/* Undefined CGATS table type */
	char *cgats_type;

	/* User defined table types */
	int nothers;		/* Number of other identifiers */
	char **others;		/* Other file type identifiers */

	/* Public Methods */
	int (*set_cgats_type)(struct _cgats *p, const char *osym);
											/* Define the (one) variable CGATS type */
											/* Return -2, set errc & err on system error */

	int (*add_other)(struct _cgats *p, const char *osym);
											/* Add a user defined file identifier string. */
											/* Use a zero length string for wildcard. */
											/* Return the oi */
											/* Return -2, set errc & err on system error */

	int (*get_oi)(struct _cgats *p, const char *osym);
											/* Return the oi of the given other type */
											/* return -ve and errc and err set on error */

	int (*read)(struct _cgats *p, cgatsFile *fp);	/* Read a cgats file into structure */
												/* return -ve and errc and err set on error */

	/* NULL if SEPARATE_STD is defined: */ 
	int (*read_name)(struct _cgats *p, const char *filename);	/* Standard file I/O */
												/* return -ve and errc and err set on error */

	int (*find_kword)(struct _cgats *p, int table, const char *ksym);
												/* Return index of the keyword, -1 on fail */
												/* -2 on illegal table index, errc & err */
	int (*find_field)(struct _cgats *p, int table, const char *fsym);
												/* Return index of the field, -1 on fail */
												/* -2 on illegal table index, errc & err */

	int (*add_table)(struct _cgats *p, table_type tt, int oi);
	                                        /* Add a new (empty) table to the structure */
											/* Return the index of the table */
											/* Return -2, set errc & err on system error */
											/* if tt is tt_other, io sets the other index */
	int (*set_table_flags)(struct _cgats *p, int table, int sup_id,int sup_kwords,int sup_fields);
						/* Set or reset table output suppresion flags */
						/* Return -ve, set errc & err on error */
	int (*add_kword)(struct _cgats *p, int table, const char *ksym, const char *kdata, const char *kcom);
						/* Add a new keyword/value pair + optional comment to the table */
						/* Return index of new keyword, or -1, errc & err on error */
	int (*add_field)(struct _cgats *p, int table, const char *fsym, data_type ftype);
						/* Add a new field to the table */
						/* Return index of new field, or -1, -2, errc and err on error */
	int (*add_set)(struct _cgats *p, int table, ...);	/* Add a set of data */
						/* Return 0 normally, -1, -2, errc & err if error */
	int (*add_setarr)(struct _cgats *p, int table, cgats_set_elem *ary); /* Add data from array */
						/* Return 0 normally, -1, -2, errc & err if error */
	int (*write)(struct _cgats *p, cgatsFile *fp);	/* Write structure into cgats file */
										/* return -ve and errc and err set on error */

	int (*get_setarr)(struct _cgats *p, int table, int set_index, cgats_set_elem *ary);
						/* Fill a suitable set_element with a line of data */
						/* Return 0 normally, -1, -2, errc & err if error */
	/* NULL if SEPARATE_STD is defined: */ 
	int (*write_name)(struct _cgats *p, const char *filename);	/* Standard file I/O */
										/* return -ve and errc and err set on error */


	int (*error)(struct _cgats *p, char **mes);		/* Return error code and message */
													/* for the first error, if any error */
													/* has occured since object creation. */
	void (*del)(struct _cgats *p);					/* Delete the object */

	char err[CGATS_ERRM_LENGTH];		/* Error message */
	int errc;			/* Error code */
	char ferr[CGATS_ERRM_LENGTH];		/* First error message */
	int ferrc;			/* First error code */
}; typedef struct _cgats cgats;

/* Creator */
extern cgats *new_cgats_al(cgatsAlloc *al);	/* with allocator object */

#ifdef COMBINED_STD
#define CGATS_STATIC static
#else
#define CGATS_STATIC
#endif

/* Available if SEPARATE_STD is not defined */ 
extern cgats *new_cgats(void);							/* Standard allocator */

/* Available from cgatsstd.obj SEPARATE_STD is defined: */ 
CGATS_STATIC int cgats_read_name(cgats *p, const char *filename);
CGATS_STATIC int cgats_write_name(cgats *p, const char *filename);

#ifdef __cplusplus
	}
#endif

#define CGATS_H
#endif /* CGATS_H */
