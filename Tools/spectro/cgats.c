
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

/*

	To make this more portable for independent use,
	should save/set/restore LC_NUMERIC locale before
	printf/scanf from file. e.g.

		include <locale.h>
		char *old_locale, *saved_locale;
     
		old_locale = setlocale (LC_NUMERIC, NULL);
		saved_locale = strdup (old_locale);
		if (saved_locale == NULL)
			error ("Out of memory");
		setlocale (LC_NUMERIC, "C");

		.... read or write ...
     
		setlocale (LC_NUMERIC, saved_locale);
		free (saved_locale);

	Also apply to pars.c
 */

#define _CGATS_C_				/* Turn on implimentation code */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#undef DEBUG				/* Debug only in slected places */

#ifdef DEBUG
# define DBGA g_log, 0 		/* First argument to DBGF() */
# define DBGF(xx)	a1logd xx
#else
# define DBGF(xx)
#endif

#ifdef STANDALONE_TEST
extern void error(const char *fmt, ...), warning(const char *fmt, ...);
#endif

#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "pars.h"
#include "cgats.h"

#define REAL_SIGDIG 6		/* Number of significant digits in real representation */

static int cgats_read(cgats *p, cgatsFile *fp);
static int find_kword(cgats *p, int table, const char *ksym);
static int find_field(cgats *p, int table, const char *fsym);
static int add_table(cgats *p, table_type tt, int oi);
static int set_table_flags(cgats *p, int table, int sup_id, int sup_kwords, int sup_fields);
static int set_cgats_type(cgats *p, const char *osym);
static int add_other(cgats *p, const char *osym);
static int get_oi(cgats *p, const char *osym);
static int add_kword(cgats *p, int table, const char *ksym, const char *kdata, const char *kcom);
static int add_field(cgats *p, int table, const char *fsym, data_type ftype);
static int add_set(cgats *p, int table, ...);
static int add_setarr(cgats *p, int table, cgats_set_elem *args);
static int get_setarr(cgats *p, int table, int set_index, cgats_set_elem *args);
static int cgats_write(cgats *p, cgatsFile *fp);
static int cgats_error(cgats *p, char **mes);
static void cgats_del(cgats *p);

static void cgats_table_free(cgats_table *t);
static void *alloc_copy_data_type(cgatsAlloc *al, data_type ktype, void *dpoint);
static int reserved_kword(const char *ksym);
static int standard_kword(const char *ksym);
static data_type standard_field(const char *fsym);
static int cs_has_ws(const char *cs);
static char *quote_cs(cgatsAlloc *al, const char *cs);
static int clear_fields(cgats *p, int table);
static int add_kword_at(cgats *p, int table, int  pos, const char *ksym, const char *kdatak, const char *kcom);
static int add_data_item(cgats *p, int table, void *data);
static void unquote_cs(char *cs);
static data_type guess_type(const char *cs);
static void real_format(double value, int nsd, char *fmt);

#ifdef COMBINED_STD
static int cgats_read_name(cgats *p, const char *filename);
static int cgats_write_name(cgats *p, const char *filename);
#endif

static const char *data_type_desc[] =
	{ "real", "integer", "char string", "non-quoted char string", "no type" };

/* Create an empty cgats object */
/* Return NULL on error */
cgats *new_cgats_al(
cgatsAlloc *al			/* memory allocator */
) {
	cgats *p;

	if ((p = (cgats *) al->calloc(al, sizeof(cgats), 1)) == NULL) {
		return NULL;
	}
	p->al = al;				/* Heap allocator */

	/* Initialize the methods */
	p->find_kword = find_kword;
	p->find_field = find_field;
	p->read       = cgats_read;
	p->add_table  = add_table;
	p->set_table_flags = set_table_flags;
	p->set_cgats_type  = set_cgats_type;
	p->add_other  = add_other;
	p->get_oi     = get_oi;
	p->add_kword  = add_kword;
	p->add_field  = add_field;
	p->add_set    = add_set;
	p->add_setarr = add_setarr;
	p->get_setarr = get_setarr;
	p->write      = cgats_write;
	p->error      = cgats_error;
	p->del        = cgats_del;
	
#ifndef SEPARATE_STD
	p->read_name  = cgats_read_name;
	p->write_name = cgats_write_name;
#else
	p->read_name  = NULL;
	p->write_name = NULL;
#endif

	return p;
}

static int err(cgats *p, int errc, const char *fmt, ...);

/* new_cgats() with default malloc allocator */

#ifndef SEPARATE_STD
#define COMBINED_STD

#include "cgatsstd.c"

#undef COMBINED_STD
#endif /* SEPARATE_STD */

/* ------------------------------------------- */

/* Implimentation function - register an error */
/* Return the error number */
static int
err(cgats *p, int errc, const char *fmt, ...) {
	va_list args;

	p->errc = errc;
	va_start(args, fmt);
	vsprintf(p->err, fmt, args);
	va_end(args);

	/* If this is the first registered error */
	if (p->ferrc != 0) {
		p->ferrc = p->errc;
		strcpy(p->ferr, p->err);
	}

	return errc;
}

/* Define methods */

/* Return error code and message */
/* for the first error, if any error */
/* has occured since object creation. */
static int cgats_error(
cgats *p,
char **mes
) {
	if (p->ferrc != 0) {
		if (mes != NULL)
			*mes = p->ferr;
		return p->ferrc;
	}
	return 0;
}

/* ------------------------------------------- */

/* Free the cgats object */
static void
cgats_del(cgats *p) {
	int i;
	cgatsAlloc *al = p->al;
	int del_al     = p->del_al;

	/* Free all the user defined file identifiers */
	if (p->cgats_type != NULL) {
		al->free(al, p->cgats_type);
	}

	if (p->others != NULL) {
		for (i = 0; i < p->nothers; i++)
			if(p->others[i] != NULL)
				al->free(al, p->others[i]);
		al->free(al, p->others);
	}

	/* Free contents of all the tables */
	for (i = 0; i < p->ntables; i++)
		cgats_table_free(&p->t[i]);

	/* Free the table structures */
	if (p->t != NULL)
		al->free(al, p->t);

	al->free(al, p);

	if (del_al)			/* We are responsible for deleting allocator */
		al->del(al);
}

/* Free up the contents of a cgats_table struct */
static void
cgats_table_free(cgats_table *t) {
	cgatsAlloc *al = t->al;
	int i,j;

	/* Free all the keyword symbols */
	if (t->ksym != NULL) {
		for (i = 0; i < t->nkwords; i++)
			if(t->ksym[i] != NULL)
				al->free(al, t->ksym[i]);
		al->free(al, t->ksym);
	}
	/* Free all the keyword values */
	if (t->kdata != NULL) {
		for (i = 0; i < t->nkwords; i++)
			if(t->kdata[i] != NULL)
				al->free(al, t->kdata[i]);
		al->free(al, t->kdata);
	}
	/* Free all the keyword comments */
	if (t->kcom != NULL) {
		for (i = 0; i < t->nkwords; i++)
			if(t->kcom[i] != NULL)
				al->free(al, t->kcom[i]);
		al->free(al, t->kcom);
	}

	/* Free all the field symbols */
	if (t->fsym != NULL) {
		for (i = 0; i < t->nfields; i++)
			if(t->fsym[i] != NULL)
				al->free(al, t->fsym[i]);
		al->free(al, t->fsym);
	}
	/* Free array of field types */
	if (t->ftype != NULL)
		al->free(al, t->ftype);
	/* Free all the original fields text values */
	if (t->rfdata != NULL) {
		for (j = 0; j < t->nsets; j++)
			if (t->rfdata[j] != NULL) {
				for (i = 0; i < t->nfields; i++)
					if(t->rfdata[j][i] != NULL)
						al->free(al, t->rfdata[j][i]);
				al->free(al, t->rfdata[j]);
			}
		al->free(al, t->rfdata);
	}
	/* Free all the fields values */
	if (t->fdata != NULL) {
		for (j = 0; j < t->nsets; j++)
			if (t->fdata[j] != NULL) {
				for (i = 0; i < t->nfields; i++)
					if(t->fdata[j][i] != NULL)
						al->free(al, t->fdata[j][i]);
				al->free(al, t->fdata[j]);
			}
		al->free(al, t->fdata);
	}
}

/* Return index of the keyword, -1 on fail */
/* -2 on illegal table index, message in err & errc */
static int
find_kword(cgats *p, int table, const char *ksym) {
	int i;
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';

	if (table < 0 || table >= p->ntables)
		return err(p, -2, "cgats.find_kword(), table number '%d' is out of range",table);
	t = &p->t[table];

	if (ksym == NULL || ksym[0] == '\000')
		return -1;

	for (i = 0; i < t->nkwords; i ++) {
		if (t->ksym[i] != NULL && t->kdata[i] != NULL
		    && strcmp(t->ksym[i],ksym) == 0)
			return i;
	}

	return -1;
}

/* Return index of the field, -1 on fail */
/* -2 on illegal table index, message in err & errc */
static int
find_field(cgats *p, int table, const char *fsym) {
	int i;
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';

	if (table < 0 || table >= p->ntables)
		return err(p, -2, "cgats.find_field(), table number '%d' is out of range",table);
	t = &p->t[table];

	if (fsym == NULL || fsym[0] == '\000')
		return -1;

	for (i = 0; i < t->nfields; i ++)
		if (strcmp(t->fsym[i],fsym) == 0)
			return i;

	return -1;
}

/* Read a cgats file into structure */
/* returns 0 normally, -ve if there was an error, */
/* and p->errc and p->err will be valid */
static int
cgats_read(cgats *p, cgatsFile *fp) {
	parse *pp;
/* Read states */
#define R_IDENT 0		/* Reading file identifier */
#define R_KWORDS 1		/* Reading keywords */
#define R_KWORD_VALUE 2	/* Reading keywords values */
#define R_FIELDS 3		/* Reading field declarations */
#define R_DATA 4		/* Reading data in set */
	int rstate = R_IDENT;
	int tablef = 0;		/* Current table we should be filling */
	int expsets = 0;	/* Expected number of sets */
	char *kw = NULL;	/* keyword symbol */

	p->errc = 0;
	p->err[0] = '\000';

	if ((pp = new_parse_al(p->al, fp)) == NULL) {
		DBGF((DBGA,"Failed to open parser for file\n"));
		return err(p, -1, "Unable to create file parser for file '%s'",fp->fname(fp));
	}

	/* Setup our token parsing charaters */
	/* Terminators, Not Read, Comment start, Quote characters */
	pp->add_del(pp, " \t"," \t", "#", "\"");

	/* Read in the file */
	for (;;) {
		char *tp;			/* Token string */

		/* Fetch the next token */
		while ((tp = pp->get_token(pp)) == NULL) {
			int rc;
			if (pp->errc != 0) {		/* get_token got an error */
				err(p, -1, "%s", pp->err);
				pp->del(pp);
				DBGF((DBGA,"Get token got error '%s'\n",pp->err));
				return p->errc;
			}
			if ((rc = pp->read_line(pp)) == 0)
				break;		/* End of file */
			else if (rc == -1) {		/* read_line got an error */
				err(p, -1, "%s", pp->err);
				pp->del(pp);
				DBGF((DBGA,"Read line got error '%s'\n",pp->err));
				return p->errc;
			}
		}
		if (tp == NULL)
			break;		/* EOF */

		switch(rstate) {
			case R_IDENT: 		/* Expecting file identifier */
			case R_KWORDS: {	/* Expecting keyword, field def or data */
				table_type tt = tt_none;
				int oi = 0;		/* Index if tt_other */

				DBGF((DBGA,"Got kword '%s'\n",tp));
				if (rstate == R_IDENT) {
					DBGF((DBGA,"Expecting file identifier\n"));
				}

				/* The standard says that keywords have to be at the start of a line */
				if (pp->token != 1)			/* Be robust and ignore any problems */
					break;

				/* See if we have a file identifier */
				if(strcmp(tp,"IT8.7/1") == 0)
					tt = it8_7_1;
				else if(strcmp(tp,"IT8.7/2") == 0)
					tt = it8_7_2;
				else if(strcmp(tp,"IT8.7/3") == 0)
					tt = it8_7_3;
				else if(strcmp(tp,"IT8.7/4") == 0)
					tt = it8_7_4;
				else if(strcmp(tp,"CGATS.5") == 0)
					tt = cgats_5;
				else if(strncmp(tp,"CGATS.",6) == 0) {	/* Variable CGATS type */
					tt = cgats_X;
					if (p->cgats_type != NULL)
						p->al->free(p->al, p->cgats_type);
					if ((p->cgats_type = (char *)p->al->malloc(p->al,
					                     (strlen(tp)+1) * sizeof(char))) == NULL) {
						err(p,-1,"Failed to malloc space for CGATS.X keyword");
						pp->del(pp);
						return p->errc;
					}
					strcpy(p->cgats_type,tp);
					DBGF((DBGA,"Found CGATS file identifier\n"));
					rstate = R_KWORDS;
				} else {	/* See if it is an 'other' file identifier */
					int iswild = 0;	
					DBGF((DBGA,"Checking for 'other' identifier\n"));

					/* Check for non-wildcard "other" */
					for (oi = 0; oi < p->nothers; oi++) {

						if (p->others[oi][0] == '\000') {	/* Wild card */
							iswild = 1;
							continue;
						}
						/* If "other" is a specific string */
						if(strcmp(tp,p->others[oi]) == 0) {
							DBGF((DBGA,"Matches 'other' %s\n",p->others[oi]));
							tt = tt_other;
							rstate = R_KWORDS;
							break;
						}
					}

					if (tt == tt_none
					 && iswild
					 && rstate == R_IDENT				/* First token after a table */
					 && standard_kword(tp) == 0			/* And not an obvious kword */
					 && reserved_kword(tp) == 0) {
						DBGF((DBGA,"Matches 'other' wildcard\n"));
						if ((oi = add_other(p, tp)) == -2) {
							pp->del(pp);
							DBGF((DBGA,"add_other for wilidcard failed\n"));
							return p->errc;
						}
						tt = tt_other;
						rstate = R_KWORDS;
					}
				}

				/* First ever token must be file identifier */
				if (tt == tt_none && p->ntables == 0) {
					err(p,-1,"Error at line %d of file '%s': No CGATS file identifier found",pp->line,fp->fname(fp));
					pp->del(pp);
					DBGF((DBGA,"Failed to match file identifier\n"));
					return p->errc;
				}

				/* Any token after previous table has data finished */
				/* causes a new table to be created. */
				if (p->ntables == tablef) {

					if (tt != tt_none) {			/* Current token is a file identifier */
						DBGF((DBGA,"Got file identifier, adding plain table\n"));
	        			if (add_table(p, tt, oi) < 0) {
							pp->del(pp);
							DBGF((DBGA,"Add table failed\n"));
							return p->errc;
						}
					} else {	/* Carry everything over from previous table the table type */
						int i;
						cgats_table *pt;
						int ct;

						DBGF((DBGA,"No file identifier, adding table copy of previous\n"));

	        			if (add_table(p, p->t[p->ntables-1].tt, p->t[p->ntables-1].oi) < 0) {
							pp->del(pp);
							DBGF((DBGA,"Add table failed\n"));
							return p->errc;
						}

						pt = &p->t[p->ntables-2];
						ct = p->ntables-1;

						for (i = 0; i < pt->nkwords; i++) {
							if (p->add_kword(p, ct, pt->ksym[i], pt->kdata[i], pt->kcom[i]) < 0) {
								pp->del(pp);
								DBGF((DBGA,"Add keyword failed\n"));
								return p->errc;
							}
						}
						for (i = 0; i < pt->nfields; i++)
							if (p->add_field(p, ct, pt->fsym[i], none_t) < 0) {
								pp->del(pp);
								DBGF((DBGA,"Add field failed\n"));
								return p->errc;
							}
					}
				}

				/* If not a file identifier */
				if (tt == tt_none) {
					/* See if we're starting the field declarations */
					if(strcmp(tp,"BEGIN_DATA_FORMAT") == 0) {
						rstate = R_FIELDS;
						if (clear_fields(p, p->ntables-1) < 0) {
							pp->del(pp);
							DBGF((DBGA,"Clear field failed\n"));
							return p->errc;
						}
						break;
					}
					if(strcmp(tp,"SAMPLE_ID") == 0) {	/* Faulty table - cope gracefully */
						rstate = R_FIELDS;
						if (clear_fields(p, p->ntables-1) < 0) {
							pp->del(pp);
							DBGF((DBGA,"Clear field failed\n"));
							return p->errc;
						}
						goto first_field;
					}

					if(strcmp(tp,"BEGIN_DATA") == 0) {
						rstate = R_DATA;
						break;
					}
					/* Else must be a keyword */
					if ((kw = (char *)alloc_copy_data_type(p->al, cs_t, (void *)tp)) == NULL) {
						err(p, -2, "cgats.alloc_copy_data_type() malloc fail");
						pp->del(pp);
						DBGF((DBGA,"Alloc data type failed\n"));
						return p->errc;
					}
					rstate = R_KWORD_VALUE;
				}
				break;
			}
			case R_KWORD_VALUE: {
				/* Add a keyword and its value */
				
				DBGF((DBGA,"Got keyword value '%s'\n",kw));

				/* Special case for read() use */
				if(strcmp(kw,"NUMBER_OF_SETS") == 0)
					expsets = atoi(tp);

				if (!reserved_kword(kw)) {	/* Don't add reserved keywords */
					int ix;

					/* Replace keyword if it already exists */
					unquote_cs(tp);
					if ((ix = find_kword(p, p->ntables-1, kw)) < -1) {
						pp->del(pp);
						DBGF((DBGA,"Failed to find keyword\n"));
						return p->errc;
					}
					if (add_kword_at(p, p->ntables-1, ix, kw, tp, NULL) < 0) {
						pp->del(pp);
						DBGF((DBGA,"Failed to add keyword '%s'\n",kw));
						return p->errc;
					}
				}
				p->al->free(p->al, kw);
				rstate = R_KWORDS;
				break;
			}
			case R_FIELDS: {
				DBGF((DBGA,"Got fields value '%s'\n",tp));

				/* Add a list of field name declarations */
				if(strcmp(tp,"END_DATA_FORMAT") == 0) {
					rstate = R_KWORDS;
					break;
				}
				if(strcmp(tp,"BEGIN_DATA") == 0) {	/* Faulty table - cope gracefully */
					rstate = R_DATA;
					break;
				}
				if(strcmp(tp,"DEVICE_NAME") == 0) {	/* Faulty CB table - cope gracefully */
					/* It's unlikely anyone will use DEVICE_NAME as a field name */
					/* Assume this is a keyword */
					if ((kw = (char *)alloc_copy_data_type(p->al, cs_t, (void *)tp)) == NULL) {
						err(p, -2, "cgats.alloc_copy_data_type() malloc fail");
						pp->del(pp);
						DBGF((DBGA,"Alloc data type failed\n"));
						return p->errc;
					}
					rstate = R_KWORD_VALUE;
					break;
				}
			  first_field:;	/* Direct leap - cope with faulty table */
				if (p->add_field(p, p->ntables-1, tp, none_t) < 0)	/* none == cs untill figure type */ {
					pp->del(pp);
					DBGF((DBGA,"Add field failed\n"));
					return p->errc;
				}
				break;
			}
			case R_DATA: {
				cgats_table *ct = &p->t[p->ntables-1];

				DBGF((DBGA,"Got data value '%s'\n",tp));
				if(strcmp(tp,"END_DATA") == 0) {
					int i,j;
#ifdef NEVER
					if (ct->nsets == 0) {
						err(p,-1,"Error at line %d of file '%s': End of data without any data being read",pp->line,fp->fname(fp));
						pp->del(pp);
						DBGF((DBGA,"End of data without any data being read\n"));
						return p->errc;
					}
#endif // NEVER
					if (expsets != 0 && ct->nsets != expsets) {
						err(p,-1,"Error at line %d of file '%s': Read %d sets, expected %d sets",pp->line,fp->fname(fp),ct->nsets,expsets);
						pp->del(pp);
						DBGF((DBGA,"End of mimatch in number of sets\n"));
						return p->errc;
					}
					if (ct->ndf != 0) {
						err(p,-1,"Error at line %d of file '%s': Data was not an integer multiple of fields (remainder %d out of %d)",pp->line,fp->fname(fp),ct->ndf,ct->nfields);
						pp->del(pp);
						DBGF((DBGA,"Not an interger multiple of fields\n"));
						return p->errc;
					}

					/* We now need to determine the data types */
					/* and convert them appropriately */
					for (i = 0; i < ct->nfields; i++) {
						data_type bt = i_t, st;
						for (j = 0; j < ct->nsets; j++) {
							data_type ty;
							ty = guess_type(((char *)ct->rfdata[j][i]));
							if (ty == cs_t) {
								bt = cs_t;
								break;		/* Early out */
							} else if (ty == nqcs_t) {
								if (bt == i_t || bt == r_t)
									bt = ty;
							} else if (ty == r_t) {
								if (bt == i_t)
									bt = ty;
							} else { /* ty == i_t */
								bt = ty;
							}
						}
						/* Got guessed type bt. Sanity check against known field types */
						/* and promote if that seems reasonable */
						st = standard_field(ct->fsym[i]);
						if ((st == r_t && bt == i_t)		/* If ambiguous, use standard field */
						 || ((st == cs_t || st == nqcs_t) && bt == i_t)	/* Promote any to string */
						 || ((st == cs_t || st == nqcs_t) && bt == r_t)	/* Promote any to string */
						 || (st == nqcs_t && bt == cs_t)
						 || (st == cs_t && bt == nqcs_t))
							bt = st;

						/* If standard type doesn't match what it should, throw an error */
						if (st != none_t && st != bt) {
							err(p, -1,"Error in file '%s': Field '%s' has unexpected type, should be '%s', is '%s'",fp->fname(fp),ct->fsym[i],data_type_desc[st],data_type_desc[bt]);
							pp->del(pp);
							DBGF((DBGA,"Standard field has unexpected data type\n"));
							return p->errc;
						}

						/* Set field type, and then convert the fields to correct type. */
						ct->ftype[i] = bt;
						for (j = 0; j < ct->nsets; j++) {
							switch(bt) {
								case r_t: {
									double dv;
									dv = atof((char *)ct->rfdata[j][i]);
									if ((ct->fdata[j][i] = alloc_copy_data_type(p->al, bt, (void *)&dv)) == NULL) {
										err(p, -2, "cgats.alloc_copy_data_type() malloc fail");
										pp->del(pp);
										DBGF((DBGA,"Alloc copy data type failed\n"));
										return p->errc;
									}
									break;
								}
								case i_t: {
									int iv;
									iv = atoi((char *)ct->rfdata[j][i]);
									if ((ct->fdata[j][i] = alloc_copy_data_type(p->al, bt, (void *)&iv)) == NULL) {
										err(p, -2, "cgats.alloc_copy_data_type() malloc fail");
										pp->del(pp);
										DBGF((DBGA,"Alloc copy data type failed\n"));
										return p->errc = -2;
									}
									break;
								}
								case cs_t:
								case nqcs_t: {
									char *cv;
									cv = ct->rfdata[j][i];
									if ((ct->fdata[j][i]
= alloc_copy_data_type(p->al, bt, (void *)cv)) == NULL) {
										err(p, -2, "cgats.alloc_copy_data_type() malloc fail");
										pp->del(pp);
										DBGF((DBGA,"Alloc copy data type failed\n"));
										return p->errc = -2;
									}
									unquote_cs((char *)ct->fdata[j][i]);
									break;
								}
								case none_t:
									break;
							}
						}
					}
					
					tablef = p->ntables;	/* Finished data for current table */
					rstate = R_IDENT;
					break;
				}

				/* Make sure fields have been decalared */
				if (ct->nfields == 0) {
					err(p, -1,"Error at line %d of file '%s': Found data without field definitions",pp->line,fp->fname(fp));
					pp->del(pp);
					DBGF((DBGA,"Found data without field definition\n"));
					return p->errc;
				}
				/* Add the data item */
				if (add_data_item(p, p->ntables-1, tp) < 0) {
					pp->del(pp);
					DBGF((DBGA,"Adding data item failed\n"));
					return p->errc;
				}
				break;
			}
		}
	}

	pp->del(pp);		/* Clean up the parse file */
	return 0;
}

/* Define the (one) variable CGATS type */
/* Return -2 & set errc and err on system error */
static int
set_cgats_type(cgats *p, const char *osym) {
	cgatsAlloc *al = p->al;

	p->errc = 0;
	p->err[0] = '\000';
	if (p->cgats_type != NULL)
		al->free(al, p->cgats_type);
	if ((p->cgats_type = (char *)al->malloc(al, (strlen(osym)+1) * sizeof(char))) == NULL)
		return err(p,-2,"cgats.add_cgats_type(), malloc failed!");
	strcpy(p->cgats_type,osym);
	return 0;
}

/* Add an 'other' file identifier string, and return the oi. */
/* Use a zero length string to indicate a wildcard. */
/* Return -2 & set errc and err on system error */
static int
add_other(cgats *p, const char *osym) {
	cgatsAlloc *al = p->al;

	p->errc = 0;
	p->err[0] = '\000';
	p->nothers++;
	if ((p->others = (char **)al->realloc(al, p->others, p->nothers * sizeof(char *))) == NULL)
		return err(p,-2, "cgats.add_other(), realloc failed!");
	if ((p->others[p->nothers-1] =
	                      (char *)al->malloc(al, (strlen(osym)+1) * sizeof(char))) == NULL)
		return err(p,-2,"cgats.add_other(), malloc failed!");
	strcpy(p->others[p->nothers-1],osym);
	return p->nothers-1;
}

/* Return the oi of the given other type */
/* return -ve and errc and err set on error */
static int get_oi(cgats *p, const char *osym) {
	int oi;

	p->errc = 0;
	p->err[0] = '\000';

	for (oi = 0; oi < p->nothers; oi++) {
		if (strcmp(p->others[oi], osym) == 0)
			return oi;
	}
	return err(p,-1,"cgats.get_oi(), failed to find '%s'!",osym);
}

/* Add a new (empty) table to the structure */
/* Return the index of the table. */
/* tt defines the table type, and oi is used if tt = tt_other */
/* Return -2 & set errc and err on system error */
static int
add_table(cgats *p, table_type tt, int oi) {
	cgatsAlloc *al = p->al;
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';
	p->ntables++;
	if ((p->t = (cgats_table *) al->realloc(al, p->t, p->ntables * sizeof(cgats_table))) == NULL)
		return err(p,-2, "cgats.add_table(), realloc failed!");
	memset(&p->t[p->ntables-1],0,sizeof(cgats_table));
	t = &p->t[p->ntables-1];

	t->al = al;		/* Pointer to allocator */
	t->tt = tt;
	t->oi = oi;

	return p->ntables-1;
}

/* set or reset table flags */
/* The sup_id flag suppreses the writing of the file identifier string for the table */
/* The sup_kwords flag suppreses the writing of the standard keyword definitions for the table */
/* The sup_fields flag suppreses the writing of the field definitions for the table */
/* The assumption is that the previous tables id and/or fields will be correct for */
/* the table that have these flags set. */
/* Return -1 & set errc and err on error */
static int set_table_flags(cgats *p, int table, int sup_id, int sup_kwords, int sup_fields) {
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables)
		return err(p,-1, "cgats.set_table_flags(), table number '%d' is out of range",table);
	t = &p->t[table];

	if (sup_id == 0 && (sup_kwords != 0 || sup_fields != 0))
		return err(p,-1, "cgats.set_table_flags(), Can't suppress kwords or fields if ID is not suppressed");
	t->sup_id = sup_id;
	t->sup_kwords = sup_kwords;
	t->sup_fields = sup_fields;

	return 0;
}


/* Append a new keyword/value + optional comment pair to the table */
/* If no comment is provided, kcom should be set to NULL */
/* A comment only line can be inserted amongst the keywords by providing */
/* NULL values for ksym and kdata, and the comment in kcom */
/* Return the index of the new keyword, or -1, err & errc on error */
static int
add_kword(cgats *p, int table, const char *ksym, const char *kdata, const char *kcom) {
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables)
		return err(p,-1, "cgats.add_kword(), table number '%d' is out of range",table);
	t = &p->t[table];

	return add_kword_at(p, table, t->nkwords, ksym, kdata, kcom);
}

/* Replace or append a new keyword/value pair + optional comment */
/* to the table in the given position. The keyword will be appended */
/* if it is < 0. */
/* If no comment is provided, kcom should be set to NULL */
/* A comment only line can be inserted amongst the keywords by providing */
/* NULL values for ksym and kdata, and the comment in kcom */
/* Return the index of the keyword, or -1, err & errc on error */
static int
add_kword_at(cgats *p, int table, int pos, const char *ksym, const char *kdata, const char *kcom) {
	cgatsAlloc *al = p->al;
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables) {
		DBGF((DBGA,"add_kword_at: table is invalid\n"));
		return err(p,-1, "cgats.add_kword(), table number '%d' is out of range",table);
	}
	t = &p->t[table];

	if (ksym != NULL && cs_has_ws(ksym)) {	/* oops */
		DBGF((DBGA,"add_kword_at: keyword '%s' is illegal (embedded white space, quote or comment character)\n",ksym));
		return err(p,-1, "cgats.add_kword(), keyword '%s'is illegal",ksym);
	}

	if (ksym != NULL && reserved_kword(ksym)) { 	/* oops */
		DBGF((DBGA,"add_kword_at: keyword '%s' is illegal (reserved)\n",ksym));
		return err(p,-1, "cgats.add_kword(), keyword '%s'is generated automatically",ksym);
	}

	if (pos < 0 || pos >= t->nkwords) {	/* This is an append */
		t->nkwords++;
		if (t->nkwords > t->nkwordsa) {	/* Allocate keyword pointers in groups of 8 */
			t->nkwordsa += 8;
			if ((t->ksym = (char **)al->realloc(al, t->ksym, t->nkwordsa * sizeof(char *))) == NULL)
				return err(p,-2, "cgats.add_kword(), realloc failed!");
			if ((t->kdata = (char **)al->realloc(al, t->kdata, t->nkwordsa * sizeof(char *)))
			                                                                        == NULL)
				return err(p,-2, "cgats.add_kword(), realloc failed!");
			if ((t->kcom = (char **)al->realloc(al, t->kcom, t->nkwordsa * sizeof(char *)))
			                                                                      == NULL)
				return err(p,-2, "cgats.add_kword(), realloc failed!");
		}
		pos = t->nkwords-1;
	} else {	/* This is a replacement */
		if (t->ksym[pos] != NULL)
			al->free(al, t->ksym[pos]);
		if (t->kdata[pos] != NULL)
			al->free(al, t->kdata[pos]);
		if (t->kcom[pos] != NULL)
			al->free(al, t->kcom[pos]);
	}

	if (ksym != NULL) {
		if ((t->ksym[pos] = (char *)alloc_copy_data_type(al, cs_t, (void *)ksym)) == NULL)
			return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
	} else
		t->ksym[pos] = NULL;

	if (kdata != NULL) {
		if ((t->kdata[pos] = (char *)alloc_copy_data_type(al, cs_t, (void *)kdata)) == NULL)
			return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
	} else
		t->kdata[pos] = NULL;

	if (kcom != NULL) {
		if ((t->kcom[pos] = (char *)alloc_copy_data_type(al, cs_t, (void *)kcom)) == NULL)
			return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
	} else
		t->kcom[pos] = NULL;
	return pos;
}

/* Add a new field to the table */
/* Return the index of the field */
/* return -1 or -2, errc & err on error */
static int
add_field(cgats *p, int table, const char *fsym, data_type ftype) {
	cgatsAlloc *al = p->al;
	cgats_table *t;
	data_type st;

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables)
		return err(p,-1,"cgats.add_field(), table parameter out of range");
	t = &p->t[table];

	if (t->nsets != 0)
		return err(p,-1,"cgats.add_field(), attempt to add field to non-empty table");

	/* Check the field name is reasonable */
	if (cs_has_ws(fsym))
		return err(p,-1,"cgats.add_kword(), field name '%s'is illegal",fsym);

	if (ftype == none_t)
		ftype = cs_t;					/* Fudge - unknown type yet, used for reads */
	else {
		/* Check that the data type is reasonable */
		st = standard_field(fsym);
		if (st == nqcs_t && ftype == cs_t)	/* Fudge - standard type to non-quoted if normal */
			ftype = nqcs_t;
		if (st != none_t && st != ftype)
			return err(p,-1,"cgats.add_field(): unexpected data type for standard field name");
	}

	t->nfields++;
	if (t->nfields > t->nfieldsa) {
		/* Allocate fields in groups of 4 */
		t->nfieldsa += 4;
		if ((t->fsym = (char **)al->realloc(al, t->fsym, t->nfieldsa * sizeof(char *))) == NULL)
			return err(p,-2,"cgats.add_field(), realloc failed!");
		if ((t->ftype = (data_type *)al->realloc(al, t->ftype, t->nfieldsa * sizeof(data_type)))
		                                                                               == NULL)
			return err(p,-2,"cgats.add_field(), realloc failed!");
	}
	if ((t->fsym[t->nfields-1] = (char *)alloc_copy_data_type(al, cs_t, (void *)fsym)) == NULL)
		return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
	t->ftype[t->nfields-1] = ftype;

	return t->nfields-1;
}

/* Clear all fields in the table */
/* return 0 if OK */
/* return -1 or -2, errc & err on error */
static int
clear_fields(cgats *p, int table) {
	cgatsAlloc *al = p->al;
	int i;
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables)
		return err(p,-1,"cgats.clear_field(), table parameter out of range");
	t = &p->t[table];

	if (t->nsets != 0)
		return err(p,-1,"cgats.clear_field(), attempt to clear fields in a non-empty table");

	/* Free all the field symbols */
	if (t->fsym != NULL) {
		for (i = 0; i < t->nfields; i++)
			if(t->fsym[i] != NULL)
				al->free(al, t->fsym[i]);
		al->free(al, t->fsym);
		t->fsym = NULL;
	}

	/* Free array of field types */
	if (t->ftype != NULL)
		al->free(al, t->ftype);
	t->ftype = NULL;

	/* Zero all the field counters */
	t->nfields = 0;
	t->nfieldsa = 0;

	return 0;
}

/* Add a set of data */
/* return 0 normally. */
/* return -2, -1, errc & err on error */
static int
add_set(cgats *p, int table, ...) {
	cgatsAlloc *al = p->al;
	va_list args;
	int i;
	cgats_table *t;

	va_start(args, table);

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables)
		return err(p,-1,"cgats.add_kword(), table parameter out of range");
	t = &p->t[table];

	if (t->nfields == 0)
		return err(p,-1,"cgats.add_set(), attempt to add set when no fields are defined");

	t->nsets++;
	
	if (t->nsets > t->nsetsa) /* Allocate space for more sets */ {
		/* Allocate set pointers in groups of 100 */
		t->nsetsa += 100;
		if ((t->fdata = (void ***)al->realloc(al, t->fdata, t->nsetsa * sizeof(void **))) == NULL)
			return err(p,-2,"cgats.add_set(), realloc failed!");
	}
	/* Allocate set pointer to data element values */
	if ((t->fdata[t->nsets-1] = (void **)al->malloc(al, t->nfields * sizeof(void *))) == NULL)
		return err(p,-2,"cgats.add_set(), malloc failed!");

	/* Allocate and copy data to new set */
	for (i = 0; i < t->nfields; i++) {
		switch(t->ftype[i]) {
			case r_t: {
				double dv;
				dv = va_arg(args, double);
				if ((t->fdata[t->nsets-1][i] = alloc_copy_data_type(al, t->ftype[i], (void *)&dv)) == NULL)
					return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
				break;
			}
			case i_t: {
				int iv;
				iv = va_arg(args, int);
				if ((t->fdata[t->nsets-1][i] = alloc_copy_data_type(al, t->ftype[i], (void *)&iv)) == NULL)
					return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
				break;
			}
			case cs_t:
			case nqcs_t: {
				char *sv;
				sv = va_arg(args, char *);
				if ((t->fdata[t->nsets-1][i] = alloc_copy_data_type(al, t->ftype[i], (void *)sv)) == NULL)
					return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
				break;
			}
			default:
				return err(p,-1,"cgats.add_set(), field has unknown data type");
		}
	}
	va_end(args);

	return 0;
}

/* Add a set of data from void array */
/* (Courtesy of Neil Okamoto) */
/* return 0 normally. */
/* return -2, -1, errc & err on error */
static int
add_setarr(cgats *p, int table, cgats_set_elem *args) {
	cgatsAlloc *al = p->al;
	int i;
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables)
		return err(p,-1,"cgats.add_setarr(), table parameter out of range");
	t = &p->t[table];

	if (t->nfields == 0)
		return err(p,-1,"cgats.add_setarr(), attempt to add set when no fields are defined");

	t->nsets++;
	
	if (t->nsets > t->nsetsa) /* Allocate space for more sets */ {
		/* Allocate set pointers in groups of 100 */
		t->nsetsa += 100;
		if ((t->fdata = (void ***)al->realloc(al,t->fdata, t->nsetsa * sizeof(void **))) == NULL)
			return err(p,-2,"cgats.add_set(), realloc failed!");
	}
	/* Allocate set pointer to data element values */
	if ((t->fdata[t->nsets-1] = (void **)al->malloc(al,t->nfields * sizeof(void *))) == NULL)
		return err(p,-2,"cgats.add_set(), malloc failed!");

	/* Allocate and copy data to new set */
	for (i = 0; i < t->nfields; i++) {
		switch(t->ftype[i]) {
			case r_t: {
				double dv;
				dv = args[i].d;
				if ((t->fdata[t->nsets-1][i] = alloc_copy_data_type(al, t->ftype[i], (void *)&dv)) == NULL)
					return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
				break;
			}
			case i_t: {
				int iv;
				iv = args[i].i;
				if ((t->fdata[t->nsets-1][i] = alloc_copy_data_type(al, t->ftype[i], (void *)&iv)) == NULL)
					return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
				break;
			}
			case cs_t:
			case nqcs_t: {
				char *sv;
				sv = args[i].c;
				if ((t->fdata[t->nsets-1][i] = alloc_copy_data_type(al, t->ftype[i], (void *)sv)) == NULL)
					return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");
				break;
			}
			default:
				return err(p,-1,"cgats.add_set(), field has unknown data type");
		}
	}
	return 0;
}

/* Fill a suitable set_element with a set of data. */
/* Note a returned char pointer is to a string in *p */
/* return 0 normally. */
/* return -2, -1, errc & err on error */
static int
get_setarr(cgats *p, int table, int set_index, cgats_set_elem *args) {
	int i;
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables)
		return err(p,-1,"cgats.get_setarr(), table parameter out of range");
	t = &p->t[table];
	if (set_index < 0 || set_index >= t->nsets)
		return err(p,-1,"cgats.get_setarr(), set parameter out of range");

	for (i = 0; i < t->nfields; i++) {
		switch(t->ftype[i]) {
			case r_t:
				args[i].d = *((double *)t->fdata[set_index][i]);
				break;
			case i_t:
				args[i].i = *((int *)t->fdata[set_index][i]);
				break;
			case cs_t:
			case nqcs_t:
				args[i].c = ((char *)t->fdata[set_index][i]);
				break;
			default:
				return err(p,-1,"cgats.get_setarr(), field has unknown data type");
		}
	}
	return 0;
}

/* Add an item of data to rgdata[][] from the read file. */
/* return 0 normally. */
/* return -2, -1, errc & err on error */
static int
add_data_item(cgats *p, int table, void *data) {
	cgatsAlloc *al = p->al;
	cgats_table *t;

	p->errc = 0;
	p->err[0] = '\000';
	if (table < 0 || table >= p->ntables)
		return err(p,-1,"cgats.add_kword(), table parameter out of range");
	t = &p->t[table];

	if (t->nfields == 0)
		return err(p,-1,"cgats.add_item(), attempt to add data when no fields are defined");

	if (t->ndf == 0) {	/* We're about to do the first element of a new set */
		t->nsets++;
		
		if (t->nsets > t->nsetsa) { /* Allocate space for more sets */
			/* Allocate set pointers in groups of 100 */
			t->nsetsa += 100;
			if ((t->rfdata = (char ***)al->realloc(al, t->rfdata, t->nsetsa * sizeof(void **)))
			                                                                        == NULL)
				return err(p,-2,"cgats.add_item(), realloc failed!");
			if ((t->fdata = (void ***)al->realloc(al, t->fdata, t->nsetsa * sizeof(void **)))
			                                                                        == NULL)
				return err(p,-2,"cgats.add_item(), realloc failed!");
		}
		/* Allocate set pointer to data element values */
		if ((t->rfdata[t->nsets-1] = (char **)al->malloc(al, t->nfields * sizeof(void *))) == NULL)
			return err(p,-2,"cgats.add_item(), malloc failed!");
		if ((t->fdata[t->nsets-1] = (void **)al->malloc(al, t->nfields * sizeof(void *))) == NULL)
			return err(p,-2,"cgats.add_item(), malloc failed!");
	}

	/* Data type is always cs_t at this point, because we haven't decided the type */
	if ((t->rfdata[t->nsets-1][t->ndf] = alloc_copy_data_type(al, cs_t, data)) == NULL)
		return err(p,-2,"cgats.alloc_copy_data_type() malloc fail");

	if (++t->ndf >= t->nfields)
		t->ndf = 0;

	return 0;
}

/* Write structure into cgats file */
/* Return -ve, errc & err if there was an error */
static int
cgats_write(cgats *p, cgatsFile *fp) {
	cgatsAlloc *al = p->al;
	int i;
	int table,set,field;
	int *sfield = NULL;	/* Standard field flag */
	p->errc = 0;
	p->err[0] = '\000';

	DBGF((DBGA,"CGATS write called, ntables = %d\n",p->ntables));
	for (table = 0; table < p->ntables; table++) {
		cgats_table *t = &p->t[table];

		DBGF((DBGA,"CGATS writing table %d\n",table));

		/* Figure out the standard and non-standard fields */
		if (t->nfields > 0)
			if ((sfield = (int *)al->malloc(al, t->nfields * sizeof(int))) == NULL)
				return err(p,-2,"cgats.write(), malloc failed!");
		for (field = 0; field < t->nfields; field++) {
				if (standard_field(t->fsym[field]) != none_t)
					sfield[field] = 1;	/* Is standard */
				else
					sfield[field] = 0;
			}
	
		if (!t->sup_kwords)	/* If not suppressed */ {
			/* Make sure table has basic keywords */
			if ((i = p->find_kword(p,table,"ORIGINATOR")) < 0)	/* Create it */
				if (p->add_kword(p,table,"ORIGINATOR","Not specified", NULL) < 0) {
					al->free(al, sfield);
					return p->errc;
				}
			if ((i = p->find_kword(p,table,"DESCRIPTOR")) < 0)	/* Create it */
				if (p->add_kword(p,table,"DESCRIPTOR","Not specified", NULL) < 0) {
					al->free(al, sfield);
					return p->errc;
				}
			if ((i = p->find_kword(p,table,"CREATED")) < 0) {	/* Create it */
				static char *amonths[] = {"January","February","March","April",
				                          "May","June","July","August","September",
				                          "October","November","December"};
				time_t ctime;
				struct tm *ptm;
				char tcs[100];
				ctime = time(NULL);
				ptm = localtime(&ctime);
				sprintf(tcs,"%s %d, %d",amonths[ptm->tm_mon],ptm->tm_mday,1900+ptm->tm_year);
				if (p->add_kword(p,table,"CREATED",tcs, NULL) < 0) {
					al->free(al, sfield);
					return p->errc;
				}
			}
	
			/* And table type specific keywords */
			/* (Not sure this is correct - CGATS.5 appendix J is not specific enough) */
			switch(t->tt) {
				case it8_7_1:
				case it8_7_2:	/* Physical target reference files */
					if ((i = p->find_kword(p,table,"MANUFACTURER")) < 0)	/* Create it */
						if (p->add_kword(p,table,"MANUFACTURER","Not specified", NULL) < 0) {
							al->free(al, sfield);
							return p->errc;
						}
					if ((i = p->find_kword(p,table,"PROD_DATE")) < 0)	/* Create it */
						if (p->add_kword(p,table,"PROD_DATE","Not specified", NULL) < 0) {
							al->free(al, sfield);
							return p->errc;
						}
					if ((i = p->find_kword(p,table,"SERIAL")) < 0)	/* Create it */
						if (p->add_kword(p,table,"SERIAL","Not specified", NULL) < 0) {
							al->free(al, sfield);
							return p->errc;
						}
					if ((i = p->find_kword(p,table,"MATERIAL")) < 0)	/* Create it */
						if (p->add_kword(p,table,"MATERIAL","Not specified", NULL) < 0) {
							al->free(al, sfield);
							return p->errc;
						}
					break;
				case it8_7_3:	/* Target measurement files */
				case it8_7_4:
				case cgats_5:
				case cgats_X:
					if ((i = p->find_kword(p,table,"INSTRUMENTATION")) < 0)	/* Create it */
						if (p->add_kword(p,table,"INSTRUMENTATION","Not specified", NULL) < 0) {
							al->free(al, sfield);
							return p->errc;
						}
					if ((i = p->find_kword(p,table,"MEASUREMENT_SOURCE")) < 0)	/* Create it */
						if (p->add_kword(p,table,"MEASUREMENT_SOURCE","Not specified", NULL) < 0) {
							al->free(al, sfield);
							return p->errc;
						}
					if ((i = p->find_kword(p,table,"PRINT_CONDITIONS")) < 0)	/* Create it */
						if (p->add_kword(p,table,"PRINT_CONDITIONS","Not specified", NULL) < 0) {
							al->free(al, sfield);
							return p->errc;
						}
					break;
				case tt_other:
					/* We enforce no pre-defined keywords for user defined file types */
					break;
				default:
					break;
			}
		}

		/* Output the table */

		/* First the table identifier */
		if (!t->sup_id)	/* If not suppressed */ {
			switch(t->tt) {
				case it8_7_1:
					if (fp->gprintf(fp,"IT8.7/1\n\n") < 0)
						goto write_error;
					break;
				case it8_7_2:
					if (fp->gprintf(fp,"IT8.7/2\n\n") < 0)
						goto write_error;
					break;
				case it8_7_3:
					if (fp->gprintf(fp,"IT8.7/3\n\n") < 0)
						goto write_error;
					break;
				case it8_7_4:
					if (fp->gprintf(fp,"IT8.7/4\n\n") < 0)
						goto write_error;
					break;
				case cgats_5:
					if (fp->gprintf(fp,"CGATS.5\n\n") < 0)
						goto write_error;
					break;
				case cgats_X:				/* variable CGATS type */
					if (p->cgats_type == NULL)
						goto write_error;
					if (fp->gprintf(fp,"%-7s\n\n", p->cgats_type) < 0)
						goto write_error;
					break;
				case tt_other:	/* User defined file identifier */
					if (fp->gprintf(fp,"%-7s\n\n",p->others[t->oi]) < 0)
						goto write_error;
					break;
				case tt_none:
					break;
			}
		} else {	/* At least space the next table out a bit */
			if (table == 0) {
				al->free(al, sfield);
				return err(p,-1,"cgats_write(), ID should not be suppressed on first table");
			}
			if (t->tt != p->t[table-1].tt || (t->tt == tt_other && t->oi != p->t[table-1].oi)) {
				al->free(al, sfield);
				return err(p,-1,"cgats_write(), ID should not be suppressed when table %d type is not the same as previous table",table);
			}
			if (fp->gprintf(fp,"\n\n") < 0)
				goto write_error;
		}

		/* Then all the keywords */
		for (i = 0; i < t->nkwords; i++) {
			char *qs = NULL;

			DBGF((DBGA,"CGATS writing keyword %d\n",i));

			/* Keyword and data if it is present */
			if (t->ksym[i] != NULL && t->kdata[i] != NULL) {
				if (!standard_kword(t->ksym[i])) {	/* Do the right thing */
					if ((qs = quote_cs(al, t->ksym[i])) == NULL) {
						al->free(al, sfield);
						return err(p,-2,"quote_cs() malloc failed!");
					}
					if (fp->gprintf(fp,"KEYWORD %s\n",qs) < 0) {
						al->free(al, qs);
						goto write_error;
					}
					al->free(al, qs);
				}
	
				if ((qs = quote_cs(al, t->kdata[i])) == NULL) {
					al->free(al, sfield);
					return err(p,-2,"quote_cs() malloc failed!");
				}
				if (fp->gprintf(fp,"%s %s%s",t->ksym[i],qs,
				    t->kcom[i] == NULL ? "\n":"\t") < 0) {
					al->free(al, qs);
					goto write_error;
				}
				al->free(al, qs);
			}
			/* Comment if its present */
			if (t->kcom[i] != NULL) {
				if (fp->gprintf(fp,"# %s\n",t->kcom[i]) < 0) {
					al->free(al, qs);
					goto write_error;
				}
			}
		}

		/* Then the field specification */
		if (!t->sup_fields) {	/* If not suppressed */
			if (fp->gprintf(fp,"\n") < 0)
				goto write_error;
	
			/* Declare any non-standard fields */
			for (field = 0; field < t->nfields; field++) {
				if (!sfield[field])	/* Non-standard */ {
					char *qs;
					if ((qs = quote_cs(al, t->fsym[field])) == NULL) {
						al->free(al, sfield);
						return err(p,-2,"quote_cs() malloc failed!");
					}
					if (fp->gprintf(fp,"KEYWORD %s\n",qs) < 0) {
						al->free(al, qs);
						goto write_error;
					}
					al->free(al, qs);
				}
			}
	
			if (fp->gprintf(fp,"NUMBER_OF_FIELDS %d\n",t->nfields) < 0)
				goto write_error;
			if (fp->gprintf(fp,"BEGIN_DATA_FORMAT\n") < 0)
				goto write_error;
			for (field = 0; field < t->nfields; field ++) {
				DBGF((DBGA,"CGATS writing field %d\n",field));
				if (fp->gprintf(fp,"%s ",t->fsym[field]) < 0)
					goto write_error;
			}
			if (fp->gprintf(fp,"\nEND_DATA_FORMAT\n") < 0)
				goto write_error;
		} else { /* Check that it is safe to suppress fields */
			cgats_table *pt = &p->t[table-1];
			if (table == 0) {
				al->free(al, sfield);
				return err(p,-1,"cgats_write(), Fields should not be suppressed on first table");
			}
			if (t->nfields != pt->nfields) {
				al->free(al, sfield);
				return err(p,-1,"cgats_write(), Fields should not be suppressed when table %d different number than previous table",table);
			}
			for (field = 0; field < t->nfields; field ++)
				if (strcmp(t->fsym[field],pt->fsym[field]) != 0
				 || t->ftype[field] != pt->ftype[field]) {
					al->free(al, sfield);
					return err(p,-1,"cgats_write(), Fields should not be suppressed when table %d types is not the same as previous table",table);
				}
		}

		/* Then the actual data */
		if (fp->gprintf(fp,"\nNUMBER_OF_SETS %d\n",t->nsets) < 0)
			goto write_error;
		if (fp->gprintf(fp,"BEGIN_DATA\n") < 0)
			goto write_error;
		for (set = 0; set < t->nsets; set++) {
			DBGF((DBGA,"CGATS writing set %d\n",set));
			for (field = 0; field < t->nfields; field++) {
				data_type tt;
				if (t->ftype[field] == r_t) {
					char fmt[30];
					double val = *((double *)t->fdata[set][field]);
					real_format(val, REAL_SIGDIG, fmt);
 					strcat(fmt," ");
					if (fp->gprintf(fp,fmt,val) < 0)
						goto write_error;
				} else if (t->ftype[field] == i_t) {
					if (fp->gprintf(fp,"%d ",*((int *)t->fdata[set][field])) < 0)
						goto write_error;
				} else if (t->ftype[field] == nqcs_t
				      && !cs_has_ws((char *)t->fdata[set][field])
				      && (sfield[field] || (tt = guess_type((char *)t->fdata[set][field]),
				                            tt != i_t && tt != r_t))) {
					/* We can only print a non-quote string if it doesn't contain white space, */
					/* quote or comment characters, and if it is a standard field or */
					/* can't be mistaken for a number. */
					if (fp->gprintf(fp,"%s ",(char *)t->fdata[set][field]) < 0)
						goto write_error;
				} else if (t->ftype[field] == nqcs_t
				      || t->ftype[field] == cs_t) {
					char *qs;
					if ((qs = quote_cs(al, (char *)t->fdata[set][field])) == NULL) {
						al->free(al, sfield);
						return err(p,-2,"quote_cs() malloc failed!");
					}
					if (fp->gprintf(fp,"%s ",qs) < 0) {
						al->free(al, qs);
						goto write_error;
					}
					al->free(al, qs);
				} else {
					al->free(al, sfield);
					return err(p,-1,"cgats_write(), illegal data type found");
				}
			}
			if (fp->gprintf(fp,"\n") < 0)
				goto write_error;
		}
		if (fp->gprintf(fp,"END_DATA\n") < 0)
			goto write_error;

		if (sfield != NULL)
			al->free(al, sfield);
		sfield = NULL;
	}
	return 0;

write_error:
	err(p,-1,"Write error to file '%s'",fp->fname(fp));
	if (sfield != NULL)
		al->free(al, sfield);
	return p->errc;
}

/* Allocate space for data with given type, and copy it from source */
/* Return NULL if alloc failed, or unknown data type */
static void *
alloc_copy_data_type(cgatsAlloc *al, data_type dtype, void *dpoint) {
	switch(dtype) {
		case r_t: {	/* Real value */
			double *p;
			if ((p = (double *)al->malloc(al, sizeof(double))) == NULL)
				return NULL;
			*p = *((double *)dpoint);
			return (void *)p;
		}
		case i_t: {	/* Integer value */
			int *p;
			if ((p = (int *)al->malloc(al, sizeof(int))) == NULL)
				return NULL;
			*p = *((int *)dpoint);
			return (void *)p;
		}
		case cs_t:	/* Character string */
		case nqcs_t: {	/* Character string */
			char *p;
			if ((p = (char *)al->malloc(al, (strlen(((char *)dpoint))+1) * sizeof(char))) == NULL)
				return NULL;
			strcpy(p, (char *)dpoint);
			return (void *)p;
		}
		case none_t:
		default:
			return NULL;
	}
	return NULL;	/* Shut the compiler up */
}

/* See if the keyword name is a standard one */
/* Return non-zero if it is standard */
static int
standard_kword(const char *ksym) {
	if (ksym == NULL)
		return 0;
	if (strcmp(ksym,"ORIGINATOR") == 0)
		return 1;
	if (strcmp(ksym,"DESCRIPTOR") == 0)
		return 1;
	if (strcmp(ksym,"CREATED") == 0)
		return 1;
	if (strcmp(ksym,"MANUFACTURER") == 0)
		return 1;
	if (strcmp(ksym,"PROD_DATE") == 0)
		return 1;
	if (strcmp(ksym,"SERIAL") == 0)
		return 1;
	if (strcmp(ksym,"MATERIAL") == 0)
		return 1;
	if (strcmp(ksym,"INSTRUMENTATION") == 0)
		return 1;
	if (strcmp(ksym,"MEASUREMENT_SOURCE") == 0)
		return 1;
	if (strcmp(ksym,"PRINT_CONDITIONS") == 0)
		return 1;
	return 0;
}

/* See if the keyword name is reserved. */
/* (code generates it automatically) */
/* Return non-zero if it is reserved */
static int
reserved_kword(const char *ksym) {
	if (ksym == NULL)
		return 0;
	if (strcmp(ksym,"NUMBER_OF_FIELDS") == 0)
		return 1;
	if (strcmp(ksym,"BEGIN_DATA_FORMAT") == 0)
		return 1;
	if (strcmp(ksym,"END_DATA_FORMAT") == 0)
		return 1;
	if (strcmp(ksym,"NUMBER_OF_SETS") == 0)
		return 1;
	if (strcmp(ksym,"BEGIN_DATA") == 0)
		return 1;
	if (strcmp(ksym,"END_DATA") == 0)
		return 1;
	if (strcmp(ksym,"KEYWORD") == 0)
		return 1;
	return 0;
}

/* See if the field name is a standard one */
/* with an expected data type */
static data_type
standard_field(const char *fsym) {
	if (strcmp(fsym,"SAMPLE_ID") == 0)
		return nqcs_t;
	if (strcmp(fsym,"STRING") == 0)
		return cs_t;
	if (strncmp(fsym,"CMYK_",5) == 0) {
		if (fsym[5] == 'C')
			return r_t;
		if (fsym[5] == 'M')
			return r_t;
		if (fsym[5] == 'Y')
			return r_t;
		if (fsym[5] == 'K')
			return r_t;
		return none_t;
	}
	/* Non standard, but logical */
	if (strncmp(fsym,"CMY_",4) == 0) {
		if (fsym[4] == 'C')
			return r_t;
		if (fsym[4] == 'M')
			return r_t;
		if (fsym[4] == 'Y')
			return r_t;
		return none_t;
	}
	if (strncmp(fsym,"D_",2) == 0) {
		if (strcmp(fsym + 2, "RED") == 0)
			return r_t;
		if (strcmp(fsym + 2, "GREEN") == 0)
			return r_t;
		if (strcmp(fsym + 2, "BLUE") == 0)
			return r_t;
		if (strcmp(fsym + 2, "VIS") == 0)
			return r_t;
		return none_t;
	}
	if (strncmp(fsym,"RGB_",4) == 0) {
		if (fsym[4] == 'R')
			return r_t;
		if (fsym[4] == 'G')
			return r_t;
		if (fsym[4] == 'B')
			return r_t;
		return none_t;
	}
	if (strncmp(fsym,"SPECTRAL_",9) == 0) {
		if (strcmp(fsym + 9, "NM") == 0)
			return r_t;
		if (strcmp(fsym + 9, "PCT") == 0)
			return r_t;
		return none_t;
	}
	if (strncmp(fsym,"XYZ_",4) == 0) {
		if (fsym[4] == 'X')
			return r_t;
		if (fsym[4] == 'Y')
			return r_t;
		if (fsym[4] == 'Z')
			return r_t;
		return none_t;
	}
	if (strncmp(fsym,"XYY_",4) == 0) {
		if (fsym[4] == 'X')
			return r_t;
		if (fsym[4] == 'Y')
			return r_t;
		if (strcmp(fsym + 4, "CAPY") == 0)
			return r_t;
		return none_t;
	}
	if (strncmp(fsym,"LAB_",4) == 0) {
		if (fsym[4] == 'L')
			return r_t;
		if (fsym[4] == 'A')
			return r_t;
		if (fsym[4] == 'B')
			return r_t;
		if (fsym[4] == 'C')
			return r_t;
		if (fsym[4] == 'H')
			return r_t;
		if (strcmp(fsym + 4, "DE") == 0)
			return r_t;
		return none_t;
	}
	if (strncmp(fsym,"STDEV_",6) == 0) {
		if (fsym[6] == 'X')
			return r_t;
		if (fsym[6] == 'Y')
			return r_t;
		if (fsym[6] == 'Z')
			return r_t;
		if (fsym[6] == 'L')
			return r_t;
		if (fsym[6] == 'A')
			return r_t;
		if (fsym[6] == 'B')
			return r_t;
		if (strcmp(fsym + 6, "DE") == 0)
			return r_t;
		return none_t;
	}
	return none_t;
}

/* Return non-zero if char string has an embedded */
/* white space, quote or comment character. */
static int
cs_has_ws(const char *cs)
	{
	int i;
	for (i = 0; cs[i] != '\000'; i++)
		{
		switch (cs[i])
			{
			case ' ':
			case '\r':
			case '\n':
			case '\t':
			case '"':
			case '#':
				return 1;
			}
		}
	return 0;
	}

/* Return a string with quotes added */
/* Return NULL if malloc failed */
/* (Returned string should be free()'d after use) */
static char *
quote_cs(cgatsAlloc *al, const char *cs) {
	int i,j;
	char *rs;
	/* see how much space we need for returned string */
	for (i = 0, j = 3; cs[i] != '\000'; i++, j++)
		if (cs[i] == '"')
			j++;
	if ((rs = (char *)al->malloc(al, j * sizeof(char))) == NULL) {
		return NULL;
	}

	j = 0;
	rs[j++] = '"';
	for (i = 0; cs[i] != '\000'; i++, j++) {
		if (cs[i] == '"')
			rs[j++] = '"';
		rs[j] = cs[i];
	}
	rs[j++] = '"';
	rs[j] = '\000';
	return rs;
}

/* Remove quotes from the string */
static void
unquote_cs(char *cs) {
	int sl,i,j;
	int s;		/* flag - set if skipped last character */

	sl = strlen(cs);

	if (sl < 2 || cs[0] != '"' || cs[sl-1] != '"')
		return;

	for (i = 1, j = 0, s = 1; i < (sl-1); i++) {
		if (s == 1 || cs[i-1] != '"' || cs[i] != '"') {
			cs[j++] = cs[i];	/* skip second " in a row */
			s = 0;
		} else
			s = 1;
	}
	cs[j] = '\000';

	return;
}

/* Guess the data type from the string */
static data_type
guess_type(const char *cs)
	{
	int i;
	int rf = 0;

	/* See if its a quoted string */
	if (cs[0] == '"')
		{
		for (i = 1;cs[i] != '\000';i++);
		if (i >= 2 && cs[i-1] == '"')
			return cs_t;
		return nqcs_t;
		}
	/* See if its an integer or real */
	for (i = 0;;i++)
		{
		char c = cs[i];
		if (c == '\000')
			break;
		if (c >= '0' && c <= '9')	/* Strictly numeric */
			continue;
		if (c == '-' &&		/* Allow appropriate '-' */
			( i == 0 ||
			 ( i > 0 && (cs[i-1] == 'e' || cs[i-1] == 'E'))))
			continue;
		if (c == '+' &&		/* Allow appropriate '+' */
			( i == 0 ||
			 ( i > 0 && (cs[i-1] == 'e' || cs[i-1] == 'E'))))
			continue;
		if (!(rf & 3) && c == '.')	/* Allow one '.' before 'e' in real */
			{
			rf |= 1;
			continue;
			}
		if (!(rf & 2) && (c == 'e' || c == 'E'))	/* Allow one 'e' in real */
			{
			rf |= 2;
			continue;
			}
		/* Not numeric or incorrect 'e' or '.' or '-' or '+' */
		return nqcs_t;
		}
	if (rf)
		return r_t;
	return i_t;
	}

/* Set the character format to the appropriate printf() */
/* format given the real value and the desired number of significant digits. */
/* We try to do this while not using the %e format for normal values. */
/* The fmt string space is assumed to be big enough to contain the format */
static void
real_format(double value, int nsd, char *fmt) {
	int ndigs;
	int tot = nsd + 1;
	int xtot = tot;
	if (value == 0.0) {
		sprintf(fmt,"%%%d.%df",tot,tot-2);
		return;
	}
	if (value != value) {		/* Hmm. A nan */
		sprintf(fmt,"%%f");
		return;
	}
	if (value < 0.0) {
		value = -value;
		xtot++;
	}
	if (value < 1.0) {
		ndigs = (int)(log10(value));
		if (ndigs <= -2) {
			sprintf(fmt,"%%%d.%de",xtot,tot-2);
			return;
		}
		sprintf(fmt,"%%%d.%df",xtot-ndigs,nsd-ndigs);
		return;
	} else {
		ndigs = (int)(log10(value));
		if (ndigs >= (nsd -1))
			{
			sprintf(fmt,"%%%d.%de",xtot,tot-2);
			return;
			}
		sprintf(fmt,"%%%d.%df",xtot,(nsd-1)-ndigs);
		return;
	}
}

/* ---------------------------------------------------------- */
/* Debug and test code */
/* ---------------------------------------------------------- */

#ifdef STANDALONE_TEST

/* Dump the contents of a cgats structure to the given output */
static void
cgats_dump(cgats *p, cgatsFile *fp) {
	int tn;

	fp->gprintf(fp,"Number of tables = %d\n",p->ntables);
	for (tn = 0; tn < p->ntables; tn++) {
		cgats_table *t;
		int i,j;
		t = &p->t[tn];

	
		fp->gprintf(fp,"\nTable %d:\n",tn);

		switch(t->tt)	/* Table identifier */
			{
			case it8_7_1:
				fp->gprintf(fp,"Identifier = 'IT8.7/1'\n");
				break;
			case it8_7_2:
				fp->gprintf(fp,"Identifier = 'IT8.7/2'\n");
				break;
			case it8_7_3:
				fp->gprintf(fp,"Identifier = 'IT8.7/3'\n");
				break;
			case it8_7_4:
				fp->gprintf(fp,"Identifier = 'IT8.7/4'\n");
				break;
			case cgats_5:
				fp->gprintf(fp,"Identifier = 'CGATS.5'\n");
				break;
			case cgats_X:
				fp->gprintf(fp,"Identifier = '%s'\n",p->cgats_type);
				break;
			case tt_other:	/* User defined file identifier */
				fp->gprintf(fp,"Identifier = '%s'\n",p->others[t->oi]);
				break;
			default:
				fp->gprintf(fp,"**ILLEGAL**\n");
				break;
			}

		fp->gprintf(fp,"\nNumber of keywords = %d\n",t->nkwords);
		
		/* Dump all the keyword symbols and values */
		for (i = 0; i < t->nkwords; i++) {
			if (t->ksym[i] != NULL && t->kdata[i] != NULL)
				{
				if (t->kcom[i] != NULL)
					fp->gprintf(fp,"Keyword '%s' has value '%s' and comment '%s'\n",
					        t->ksym[i],t->kdata[i],t->kcom[i]);
				else
					fp->gprintf(fp,"Keyword '%s' has value '%s'\n",t->ksym[i],t->kdata[i]);
				}
			if (t->kcom[i] != NULL)
				fp->gprintf(fp,"Comment '%s'\n",t->kcom[i]);
		}
			
		fp->gprintf(fp,"\nNumber of field defs = %d\n",t->nfields);

		/* Dump all the field symbols */
		for (i = 0; i < t->nfields; i++) {		
			char *fname;
			switch(t->ftype[i]) {
				case r_t:
					fname = "real";
					break;
				case i_t:
					fname = "integer";
					break;
				case cs_t:
					fname = "character string";
					break;
				case nqcs_t:
					fname = "non-quoted char string";
					break;
				default:
					fname = "illegal";
					break;
			}
			fp->gprintf(fp,"Field '%s' has type '%s'\n",t->fsym[i],fname);
		}

		fp->gprintf(fp,"\nNumber of sets = %d\n",t->nsets);

		/* Dump all the set values */
		for (j = 0; j < t->nsets; j++) {
			for (i = 0; i < t->nfields; i++) {
				switch(t->ftype[i]) {
					case r_t: {
						char fmt[30];
						double val = *((double *)t->fdata[j][i]);
						fmt[0] = ' ';
						real_format(val, REAL_SIGDIG, fmt+1);
						fp->gprintf(fp,fmt,*((double *)t->fdata[j][i]));
						break;
					}
					case i_t:
						fp->gprintf(fp," %d",*((int *)t->fdata[j][i]));
						break;
					case cs_t:
					case nqcs_t:
						fp->gprintf(fp," %s",((char *)t->fdata[j][i]));
						break;
					default:
						fp->gprintf(fp," illegal");
						break;
				}
			}
			fp->gprintf(fp,"\n");
		}
	}
}

int
main(int argc, char *argv[]) {
	char *fn;
	cgatsFile *fp;
	cgats *pp;

	if (argc == 1) {
		/* Write test */
		pp = new_cgats();	/* Create a CGATS structure */
	
		if (pp->add_table(pp, cgats_5, 0) < 0	/* Start the first table */
		 || pp->add_kword(pp, 0, NULL, NULL, "Comment only test comment") < 0
		 || pp->add_kword(pp, 0, "TEST_KEY_WORD", "try this out", "Keyword comment") < 0
		 || pp->add_kword(pp, 0, "TEST_KEY_WORD2", "try this\" out \"\" huh !", NULL) < 0
	
		 || pp->add_field(pp, 0, "SAMPLE_ID", cs_t) < 0
		 || pp->add_field(pp, 0, "SAMPLE_LOC", nqcs_t) < 0
		 || pp->add_field(pp, 0, "XYZ_X", r_t) < 0)
			error("Initial error: '%s'",pp->err);
	
		if (pp->add_set(pp, 0, "1", "A1",  0.000012345678) < 0
		 || pp->add_set(pp, 0, "2", "A2",  0.00012345678) < 0
		 || pp->add_set(pp, 0, "3 ", "A#5",0.0012345678) < 0
		 || pp->add_set(pp, 0, "4", "A5",  0.012345678) < 0
		 || pp->add_set(pp, 0, "5", "A5",  0.12345678) < 0
		 || pp->add_set(pp, 0, "5", "A5",  0.00000000) < 0
		 || pp->add_set(pp, 0, "6", "A5",  1.2345678) < 0
		 || pp->add_set(pp, 0, "7", "A5",  12.345678) < 0
		 || pp->add_set(pp, 0, "8", "A5",  123.45678) < 0
		 || pp->add_set(pp, 0, "9", "A5",  1234.5678) < 0
		 || pp->add_set(pp, 0, "10", "A5", 12345.678) < 0
		 || pp->add_set(pp, 0, "12", "A5", 123456.78) < 0
		 || pp->add_set(pp, 0, "13", "A5", 1234567.8) < 0
		 || pp->add_set(pp, 0, "14", "A5", 12345678.0) < 0
		 || pp->add_set(pp, 0, "15", "A5", 123456780.0) < 0
		 || pp->add_set(pp, 0, "16", "A5", 1234567800.0) < 0
		 || pp->add_set(pp, 0, "17", "A5", 12345678000.0) < 0
		 || pp->add_set(pp, 0, "18", "A5", 123456780000.0) < 0)
			error("Adding set error '%s'",pp->err);
		
		if (pp->add_table(pp, cgats_5, 0) < 0			/* Start the second table */
		 || pp->set_table_flags(pp, 1, 1, 1, 1) < 0		/* Suppress id, kwords and fields */
		 || pp->add_kword(pp, 1, NULL, NULL, "Second Comment only test comment") < 0)
			error("Adding table error '%s'",pp->err);
	
		if (pp->add_field(pp, 1, "SAMPLE_ID", cs_t) < 0	/* Need to define fields same as table 0 */
		 || pp->add_field(pp, 1, "SAMPLE_LOC", nqcs_t) < 0
		 || pp->add_field(pp, 1, "XYZ_X", r_t) < 0)
			error("Adding field error '%s'",pp->err);
	
		if (pp->add_set(pp, 1, "4", "A4",  -0.000012345678) < 0
		 || pp->add_set(pp, 1, "5", "A5",  -0.00012345678) < 0
		 || pp->add_set(pp, 1, "6", "A5",  -0.0012345678) < 0
		 || pp->add_set(pp, 1, "7", "A5",  -0.012345678) < 0
		 || pp->add_set(pp, 1, "8", "A5",  -0.12345678) < 0
		 || pp->add_set(pp, 1, "9", "A5",  -1.2345678) < 0
		 || pp->add_set(pp, 1, "10", "A5", -12.345678) < 0
		 || pp->add_set(pp, 1, "11", "A5", -123.45678) < 0
		 || pp->add_set(pp, 1, "12", "A5", -1234.5678) < 0
		 || pp->add_set(pp, 1, "13", "A5", -12345.678) < 0
		 || pp->add_set(pp, 1, "14", "A5", -123456.78) < 0
		 || pp->add_set(pp, 1, "15", "A5", -1234567.8) < 0
		 || pp->add_set(pp, 1, "16", "A5", -12345678.0) < 0
		 || pp->add_set(pp, 1, "17", "A5", -123456780.0) < 0
		 || pp->add_set(pp, 1, "18", "A5", -1234567800.0) < 0
		 || pp->add_set(pp, 1, "19", "A5", -12345678000.0) < 0
		 || pp->add_set(pp, 1, "20", "A5", -123456780000.0) < 0)
			error("Adding set 2 error '%s'",pp->err);
	
		if ((fp = new_cgatsFileStd_name("fred.it8", "w")) == NULL)
			error("Error opening '%s' for writing","fred.it8");
	
		if (pp->write(pp, fp))
			error("Write error : %s",pp->err);
	
		fp->del(fp);		/* Close file */
		pp->del(pp);		/* Clean up */
	}

	/* Read test */
	pp = new_cgats();	/* Create a CGATS structure */

	/* Setup to cope with Argyll files */
	if (pp->add_other(pp, "CTI1") == -2
	 || pp->add_other(pp, "CTI2") == -2
	 || pp->add_other(pp, "CTI3") == -2)
		error("Adding other error '%s'",pp->err); 

	/* Setup to cope with colorblind files */
	if (pp->add_other(pp, "CBSC") == -2
	 || pp->add_other(pp, "CBTA") == -2
	 || pp->add_other(pp, "CBPR") == -2)
		error("Adding other 2 error '%s'",pp->err); 

	if (argc == 2)
		fn = argv[1];
	else
		fn = "fred.it8";

	if ((fp = new_cgatsFileStd_name(fn, "r")) == NULL)
		error("Error opening '%s' for reading",fn);
	if (pp->read(pp, fp))
		error("Read error : %s",pp->err);
	fp->del(fp);		/* Close file */

	if ((fp = new_cgatsFileStd_fp(stdout)) == NULL)
		error("Error opening stdout");
	cgats_dump(pp, fp);
	fp->del(fp);		/* delete file object */

	pp->del(pp);		/* Clean up */
	return 0;
}


/* Basic printf type error() and warning() routines */

void
error(const char *fmt, ...) {
	va_list args;

	fprintf(stderr,"cgats: Error - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit (-1);
}

void
warning(const char *fmt, ...) {
	va_list args;

	fprintf(stderr,"cgats: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

#endif /* STANDALONE_TEST */
/* ---------------------------------------------------------- */
