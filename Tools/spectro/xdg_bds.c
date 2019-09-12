
 /* XDG Base Directory Specifications support library. */
 /* Implements equivalent cross platform functionality too. */

/*************************************************************************
 Copyright 2011, 2013 Graeme W. Gill

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 *************************************************************************/

/*
	This function provides support for the XDG Base Directory Specifications
	in a cross platform compatible way.

	[ Note that for MSWin each path in a set is separated by a ';' character.
	  and that DATA and CONF will be in the same directory. ]

	The following paths are used for each of the 5 XDG concepts, listed in order
	of priority:

		Per user application related data.

		Per user application configuration settings.

		Per user application cache storage area.

		Local system wide application related data.

		Local system wide application configuration settings.

	Unix:
		$XDG_DATA_HOME
		$HOME/.local/share

		$XDG_CONFIG_HOME
		$HOME/.config

		$XDG_CACHE_HOME
		$HOME/.cache

		$XDG_DATA_DIRS
		/usr/local/share:/usr/share

		$XDG_CONFIG_DIRS
		/etc/xdg

	OS X:
		$XDG_DATA_HOME
		$HOME/Library/Application Support

		$XDG_CONFIG_HOME
		$HOME/Library/Preferences

		$XDG_CACHE_HOME
		$HOME/Library/Caches

		$XDG_DATA_DIRS
		/Library/Application Support

		$XDG_CONFIG_DIRS
		/Library/Preferences

	MSWin:
		$XDG_DATA_HOME
		$APPDATA
		$HOME/.local/share

		$XDG_CONFIG_HOME
		$APPDATA
		$HOME/.config

		$XDG_CACHE_HOME
		$APPDATA/Cache
		$HOME/.cache

		$XDG_DATA_DIRS
		$ALLUSERSPROFILE

		$XDG_CONFIG_DIRS
		$ALLUSERSPROFILE
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#ifndef NT
# include <unistd.h>
# include <glob.h>
#else
# include <io.h>
# include <direct.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifndef SALONEINSTLIB
# include "copyright.h"
# include "aconfig.h"
#else
# include "sa_config.h"
#endif
#include "numsup.h"
#include "conv.h"
#include "aglob.h"
#include "xdg_bds.h"


#undef DEBUG

#ifdef DEBUG
#define DBGA g_log, 0
#define DBG(xxx) a1logd xxx ;
#else
#define DBG(xxx) 
#endif	/* DEBUG */

#ifdef NT
# define stat _stat
# define mode_t int
# define mkdir(A,B) _mkdir(A)
# define mputenv _putenv
# define unlink _unlink
# define rmdir _rmdir
#else
/* UNIX putenv is a pain.. */
static void mputenv(char *ss) {
	int ll = strlen(ss);
	ss = strdup(ss);
	if (ll > 0 && ss[ll-1]== '=') {
		ss[ll-1] = '\000';
		unsetenv(ss); 
	} else {
		putenv(ss);
	}
}
#endif

/* Allocate a copy of the string, and normalize the */
/* path separator to '/' */

/* Append a string. Free in. Return NULL on error. */
static char *append(char *in, char *app) {
	char *rv;

	if ((rv = malloc(strlen(in) + strlen(app) + 1)) == NULL) {
		a1loge(g_log, 1, "xdg_bds: append malloc failed\n"); 
		free(in);
		return NULL;
	}
	strcpy(rv, in);
	strcat(rv, app);
	free(in);

	return rv;
}

/* Append a ':' or ';' then a string. Free in. */
/* Return NULL on error. */
static char *cappend(char *in, char *app) {
	int inlen, aplen;
	char *rv;

	inlen = strlen(in);
	aplen = strlen(app);

	if ((rv = malloc(inlen + 1 + aplen + 1)) == NULL) {
		a1loge(g_log, 1, "xdg_bds: cappend malloc failed\n"); 
		free(in);
		return NULL;
	}
	strcpy(rv, in);
	if (inlen > 0 && in[inlen-1] != SSEP && aplen > 0)
		strcat(rv, SSEPS);
	strcat(rv, app);
	free(in);

	return rv;
}

/* Append a '/' then a string. Free in. */
/* Return NULL on error. */
static char *dappend(char *in, char *app) {
	int inlen, aplen;
	char *rv;

	inlen = strlen(in);
	aplen = strlen(app);

	if ((rv = malloc(inlen + 1 + aplen + 1)) == NULL) {
		a1loge(g_log, 1, "xdg_bds: dappend malloc failed\n"); 
		free(in);
		return NULL;
	}
	strcpy(rv, in);
	if (inlen > 0 && in[inlen-1] != '/')
		strcat(rv, "/");
	strcat(rv, app);
	free(in);

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Do internal cleanup */
static void xdg_ifree(char ***paths, char **fnames, int nopaths) {
	int i;

	if (paths != NULL) {
		if (*paths != NULL) {
			for (i = 0; i < nopaths; i++) {
				if ((*paths)[i] != NULL)
					free ((*paths)[i]);
			}
		}
		free(*paths);
		*paths = NULL;
	}
	if (fnames != NULL) {
		for (i = 0; i < nopaths; i++) {
			if (fnames[i] != NULL)
				free (fnames[i]);
		}
		free(fnames);
	}
}

/* Free a return value */
void xdg_free(char **paths, int nopaths) {
	int i;

	if (paths != NULL) {	
		for (i = 0; i < nopaths; i++) {
			if (paths[i] != NULL)
				free (paths[i]);
		}
		free(paths);
	}
}

/* Return the number of matching full paths to the given subpath for the */
/* type of storage and access required. Return 0 if there is an error. */
/* The files are always unique (ie. the first match to a given filename */
/* in the possible XDG list of directories is returned, and files with */
/* the same name in other XDG directories are ignored), unless the */
/* xdg_all_matches flag is set. */
/* If xdg_read, then first user then local scope are searched. */
/* If xdf_write, then only the specified scope is used. */
/* Wildcards should not be used for xdg_write. */
/* The list should be free'd using xdg_free() after use. */
/* XDG environment variables and the subpath are assumed to be using */
/* the '/' path separator. Multiple read paths are separated by SSEP */
/* When "xdg_write", the necessary path to the file will be created. */
/* If we're running as sudo and are creating a user dir/file, */
/* we drop to using the underlying SUDO_UID/GID. If we are creating a */
/* local system dir/file as sudo and have dropped to the SUDO_UID/GID, */
/* then revert back to root uid/gid. */
int xdg_bds(
	xdg_error *er,			/* Return an error code */
	char ***paths,			/* Retun array pointers to paths */
	xdg_storage_type st,	/* Specify the storage type */
	xdg_op_type op,			/* Operation type */
	xdg_scope sc,			/* Scope if write */
	xdg_options opt,		/* Options flags */
	char *pfname			/* Sub-path and file name(s) */
) {
	char *path = NULL;		/* Directory paths to search, separated by ':' or ';' */
	char **fnames = NULL;	/* Filename component of each path being returned */
	int npaths = 0;			/* Number of paths being returned */
	int napaths = 0;		/* Number of paths allocated */

	DBG((DBGA,"xdg_bds called with st %s, op %s, sc %s, pfnames '%s'\n",
	st == xdg_data ? "data" : st == xdg_conf ? "config" : st == xdg_cache ? "cache" : "unknown", 
	op == xdg_write ? "write" : op == xdg_read ? "read" : "unknown", 
	sc == xdg_user ? "user" : sc == xdg_local ? "local" : "unknown", 
	pfname))

	*paths = NULL;

	/* Initial, empty path */
	if ((path = strdup("")) == NULL) {
		if (er != NULL) *er = xdg_alloc;
		a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
		xdg_ifree(paths, fnames, npaths);
		return 0;
	}

	/* Create a set of ':'/';' separated search paths */

	/* User scope */
	if (op == xdg_read || sc == xdg_user) {
		if (st == xdg_data) {
			char *xdg, *home;
			if ((xdg = getenv("XDG_DATA_HOME")) != NULL) {
				if ((path = cappend(path, xdg)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#ifdef NT
			} else if (login_HOME() == NULL && (xdg = getenv("APPDATA")) != NULL) {
				if ((path = cappend(path, xdg)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#endif
			} else {
				if ((home = login_HOME()) == NULL
#ifdef NT
				  && (home = getenv("APPDATA")) == NULL
#endif
				) {
					if (er != NULL) *er = xdg_nohome;
					free(path);
					DBG((DBGA,"no $HOME\n"))
					return 0;
				}
				if ((path = cappend(path, home)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#ifdef NT
				if (login_HOME() != NULL)
					path = dappend(path, ".local/share");
#else
#ifdef UNIX_APPLE
				path = dappend(path, "Library/Application Support");
#else	/* Unix, Default */
				path = dappend(path, ".local/share");
#endif
#endif
				if (path == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
			}
		} else if (st == xdg_conf) {
			char *xdg, *home;
			if ((xdg = getenv("XDG_CONFIG_HOME")) != NULL) {
				if ((path = cappend(path, xdg)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#ifdef NT
			} else if (login_HOME() == NULL && (xdg = getenv("APPDATA")) != NULL) {
				if ((path = cappend(path, xdg)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#endif
			} else {
				if ((home = login_HOME()) == NULL
#ifdef NT
				  && (home = getenv("APPDATA")) == NULL
#endif
				) {
					if (er != NULL) *er = xdg_nohome;
					free(path);
					DBG((DBGA,"no $HOME\n"))
					return 0;
				}
				if ((path = cappend(path, home)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#ifdef NT
				if (login_HOME() != NULL)
					path = dappend(path, ".config");
#else
#ifdef UNIX_APPLE
				path = dappend(path, "Library/Preferences");
#else	/* Unix, Default */
				path = dappend(path, ".config");
#endif
#endif
				if (path == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
			}
		} else if (st == xdg_cache) {
			char *xdg, *home;
			if ((xdg = getenv("XDG_CACHE_HOME")) != NULL) {
				if ((path = cappend(path, xdg)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#ifdef NT
			} else if (login_HOME() == NULL && (xdg = getenv("APPDATA")) != NULL) {
				if ((path = cappend(path, xdg)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
				if ((path = dappend(path, "Cache")) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#endif
			} else {
				if ((home = login_HOME()) == NULL
#ifdef NT
				  && (home = getenv("APPDATA")) == NULL
#endif
				) {
					if (er != NULL) *er = xdg_nohome;
					free(path);
					DBG((DBGA,"no $HOME\n"))
					return 0;
				}
				if ((path = cappend(path, home)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
#ifdef NT
				if (login_HOME() != NULL)
					path = dappend(path, ".cache");
				else
					path = dappend(path, "Cache");
#else
#ifdef UNIX_APPLE
				path = dappend(path, "Library/Caches");
#else	/* Unix, Default */
				path = dappend(path, ".cache");
#endif
#endif
				if (path == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
			}
		}
	}
	/* Local system scope */
	if (op == xdg_read || sc == xdg_local) {
		char *xdg;
		if (st == xdg_data) {
			if ((xdg = getenv("XDG_DATA_DIRS")) != NULL) {
				if ((path = cappend(path, xdg)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
			} else {
#ifdef NT
				/*
					QT uses $COMMON_APPDATA expected to be
						C:\Documents and Settings\All Users\Application Data\
					while others use $CommonAppData.
					Both seem poorly supported, 
				 */
				char *home;
				if ((home = getenv("ALLUSERSPROFILE")) == NULL
				) {
					if (er != NULL) *er = xdg_noalluserprofile;
					free(path);
					DBG((DBGA,"no $ALLUSERSPROFILE\n"))
					return 0;
				}
				path = cappend(path, home);
#else
#ifdef UNIX_APPLE
				path = cappend(path, "/Library");
#else
				path = cappend(path, "/usr/local/share:/usr/share");
#endif
#endif
				if (path == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
			}
		} else if (st == xdg_conf) {
			if ((xdg = getenv("XDG_CONFIG_DIRS")) != NULL) {
				if ((path = cappend(path, xdg)) == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
			} else {
#ifdef NT
				char *home;
				if ((home = getenv("ALLUSERSPROFILE")) == NULL
				) {
					if (er != NULL) *er = xdg_noalluserprofile;
					free(path);
					DBG((DBGA,"no $ALLUSERSPROFILE\n"))
					return 0;
				}
				path = cappend(path, home);
#else
#ifdef UNIX_APPLE
				path = cappend(path, "/Library/Preferences");
#else
				path = cappend(path, "/etc/xdg");
#endif
#endif
				if (path == NULL) {
					if (er != NULL) *er = xdg_alloc;
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					return 0;
				}
			}
		}
	}

#ifdef NT
	/* Replace all backslashes with forward slashes */
	{
		char *cp;
		for (cp = path; *cp != '\000'; cp++) {
			if (*cp == '\\')	
				*cp = '/';
		}
	}
#endif

	DBG((DBGA,"Paths to search '%s'\n",path));

	/* Hmm. */
	if (strlen(path) == 0) {
		free(path);
		if (er != NULL) *er = xdg_nopath;
		*paths = NULL;
		return 0;
	}

	{
		char *spath = NULL;		/* sub path out of paths */
		char *cp, *ep;

		/* For each search path */
		for (cp = path; *cp != '\000';) {
			char *sname = NULL;		/* sub name out of pfnames */
			char *ncp, *nep;

			/* Copy search path */
			if ((ep = strchr(cp, SSEP)) == NULL)
				ep = cp + strlen(cp);
			if ((ep - cp) == 0) {
				free(path);
				if (er != NULL) *er = xdg_mallformed;
				xdg_ifree(paths, fnames, npaths);
				return 0;
			}
			if ((spath = (char *)malloc(ep - cp + 1)) == NULL) {
				a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
				free(path);
				if (er != NULL) *er = xdg_alloc;
				xdg_ifree(paths, fnames, npaths);
				return 0;
			}
			memmove(spath, cp, ep - cp);
			spath[ep - cp] = '\000';

			/* For each filename part */
			for (ncp = pfname; *ncp != '\000';) {
				int rlen = 0;	/* Number of chars of search path up to subpath & filename */
				char *pp;
				char *schpath;	/* Path to search */

				/* Copy filename path */
				if ((nep = strchr(ncp, SSEP)) == NULL)
					nep = ncp + strlen(ncp);
				if ((nep - ncp) == 0) {
					free(spath);
					free(path);
					if (er != NULL) *er = xdg_mallformed;
					xdg_ifree(paths, fnames, npaths);
					return 0;
				}
				if ((sname = (char *)malloc(nep - ncp + 1)) == NULL) {
					a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
					free(spath);
					free(path);
					if (er != NULL) *er = xdg_alloc;
					xdg_ifree(paths, fnames, npaths);
					return 0;
				}
				memmove(sname, ncp, nep - ncp);
				sname[nep - ncp] = '\000';

				/* append subpath & subname */
				if ((schpath = strdup(spath)) == NULL
				 || (schpath = dappend(schpath, sname)) == NULL) {
					free(sname);
					free(spath);
					free(path);
					if (er != NULL) *er = xdg_alloc;
					xdg_ifree(paths, fnames, npaths);
					return 0;
				}
				DBG((DBGA,"Full path to check '%s'\n",schpath));

				/* Figure out where the filename starts */
				if ((pp = strrchr(schpath, '/')) == NULL)
					rlen = 0;
				else
					rlen = pp - schpath + 1;

				if (op == xdg_read) {
					char *fpath;			/* Full path of matched */
					aglob gg;				/* Glob structure */

					/* Setup the file glob */
					if (aglob_create(&gg, schpath)) {
						free(schpath);
						free(sname);
						free(spath);
						free(path);
						if (er != NULL) *er = xdg_alloc;
						xdg_ifree(paths, fnames, npaths);
						return 0;
					}

					/* While we have matching filenames */
					DBG((DBGA,"Getting glob results for '%s'\n",schpath))
					free(schpath);
					for (;;) {
						int i;

						if ((fpath = aglob_next(&gg)) == NULL) {
							if (gg.merr) {	/* Malloc error */
								free(sname);
								free(spath);
								free(path);
								aglob_cleanup(&gg);
								if (er != NULL) *er = xdg_alloc;
								xdg_ifree(paths, fnames, npaths);
								return 0;
							}
							break;		/* No more matches */
						}
						DBG((DBGA,"Found match with '%s'\n",fpath))

						if (opt != xdg_all_matches) {
							/* Check that this one hasn't already been found */
							/* in a different search directory */
							for (i = 0; i < npaths; i++) {
								if (strcmp(fpath + rlen, fnames[i]) == 0) {
									/* Already been found earlier - ignore it */
									break;
								}
							}
							if (i < npaths) {
								free(fpath);
								DBG((DBGA,"Ignoring it because it's already in list\n"))
								continue;		/* Ignore it */
							}
						}

						/* Found a file, so append it to the list */
						if (npaths >= napaths) {	/* Need more space in arrays */
							napaths = napaths * 2 + 1;
							if ((*paths = realloc(*paths, sizeof(char *) * napaths)) == NULL
							 || (fnames = realloc(fnames, sizeof(char *) * napaths)) == NULL) {
								a1loge(g_log, 1, "xdg_bds: realloc failed\n"); 
								free(fpath);
								free(sname);
								free(spath);
								free(path);
								aglob_cleanup(&gg);
								if (er != NULL) *er = xdg_alloc;
								xdg_ifree(paths, fnames, npaths);
								return 0;
							}
						}
						if (((*paths)[npaths] = strdup(fpath)) == NULL) {
							a1loge(g_log, 1, "xdg_bds: strdup failed\n"); 
							free(fpath);
							free(sname);
							free(spath);
							free(path);
							aglob_cleanup(&gg);
							if (er != NULL) *er = xdg_alloc;
							xdg_ifree(paths, fnames, npaths);
							return 0;
						}
						/* The non-searchpath part of the name found */
						if ((fnames[npaths] = strdup(fpath + rlen)) == NULL) {
							a1loge(g_log, 1, "xdg_bds: strdup failed\n"); 
							free((*paths)[npaths]);
							free(fpath);
							free(sname);
							free(spath);
							free(path);
							aglob_cleanup(&gg);
							if (er != NULL) *er = xdg_alloc;
							xdg_ifree(paths, fnames, npaths);
							return 0;
						}
						free(fpath);
						fpath = NULL;
						npaths++;
					}
					aglob_cleanup(&gg);

					/* Fall through to next search path */

				} else {	/* op == xdg_write */
					char *pp = schpath;
					struct stat sbuf;
					mode_t mode = 0700;	/* Default directory mode */

					if (sc == xdg_user)
						mode = 0700;	/* Default directory mode for user */
					else
						mode = 0755;	/* Default directory mode local system shared */
#ifndef NT
					/* If we're creating a user dir/file and running as root sudo */
					if (sc == xdg_user && geteuid() == 0) {
						char *uids, *gids;
						int uid, gid;
						DBG((DBGA,"We're setting a user dir/file running as root\n"))
							
						if ((uids = getenv("SUDO_UID")) != NULL
						 && (gids = getenv("SUDO_GID")) != NULL) {
							uid = atoi(uids);
							gid = atoi(gids);
							if (setegid(gid) || seteuid(uid)) {
								DBG((DBGA,"seteuid or setegid failed\n"))
							} else {
								DBG((DBGA,"Set euid %d and egid %d\n",uid,gid))
							}
						}
					/* If setting local system dir/file and not effective root, but sudo */
					} else if (sc == xdg_local && getuid() == 0 && geteuid() != 0) {
						if (getenv("SUDO_UID") != NULL
						 && getenv("SUDO_GID") != NULL) {
							DBG((DBGA,"We're setting a local system dir/file with uid = 0 && euid != 0\n"))
							if (setegid(getgid()) || seteuid(getuid())) {
								DBG((DBGA,"seteuid or setegid failed\n"))
							} else {
								DBG((DBGA,"Set euid %d, egid %d\n",geteuid(),getegid()))
							}
						}
					}
#endif	/* !NT */

#ifdef NT
					if (*pp != '\000'		/* Skip drive number */
						&& ((*pp >= 'a' && *pp <= 'z') || (*pp >= 'A' && *pp <= 'Z'))
					    && pp[1] == ':')
						pp += 2;
#endif
					if (*pp == '/')
						pp++;			/* Skip root directory */

					/* Check each directory in hierarchy, and */
					/* create it if it doesn't exist. */
					DBG((DBGA,"About to check & create whole path '%s'\n",schpath))
					for (;pp != NULL && *pp != '\000';) {
						if ((pp = strchr(pp, '/')) != NULL) {
							*pp = '\000';
							DBG((DBGA,"Checking path '%s'\n",schpath))
							if (stat(schpath,&sbuf) != 0) {
								/* Doesn't exist */
								DBG((DBGA,"Path '%s' doesn't exist - creating it\n",schpath))
								if (mkdir(schpath, mode) != 0) {
									DBG((DBGA,"mkdir failed - giving up on this one\n"))
									break;
								}
							} else {
								mode = sbuf.st_mode;
							}
							*pp = '/';
							pp++;
						}
					}

					/* If we got to the end of the hierarchy, */
					/* then the path looks good to write to, */
					/* so create a list of one and we're done */
					if (pp == NULL || *pp == '\000') {
					
						if ((*paths = malloc(sizeof(char *))) == NULL) {
							a1loge(g_log, 1, "xdg_bds: malloc failed\n"); 
							free(schpath);
							free(sname);
							free(spath);
							free(path);
							if (er != NULL) *er = xdg_alloc;
							return 0;
						}
						if (((*paths)[npaths] = schpath) == NULL) {
							free(sname);
							free(spath);
							free(path);
							if (er != NULL) *er = xdg_alloc;
							free(*paths);
							return 0;
						}
						npaths++;
						DBG((DBGA,"Returning 0: '%s'\n",(*paths)[0]))
						free(sname);
						free(spath);
						free(path);
						return npaths;
					}
				}

				/* Move on to the next name part */
				free(sname); sname = NULL;
				if (*nep == SSEP)
					ncp = nep+1;
				else
					ncp = nep;
			}

			/* Move on to the next search path */
			free(spath); spath = NULL;
			if (*ep == SSEP)
				cp = ep+1;
			else
				cp = ep;
		}
	}

	/* We're done looking through search paths */
	free(path);

	if (npaths == 0) {		/* Didn't find anything */
		if (er != NULL) *er = xdg_nopath;
		xdg_ifree(paths, fnames, npaths);
	} else {
		xdg_ifree(NULL, fnames, npaths);
#ifdef DEBUG
		{
			int i;
			a1logd(DBGA,"Returning list\n");
			for (i = 0; i < npaths; i++) 
				a1logd(DBGA,"  %d: '%s'\n",i,(*paths)[i]);
		}
#endif
	}
	return npaths;
}

/* Return a string corresponding to the error value */
char *xdg_errstr(xdg_error er) {
	switch (er) {
		case xdg_ok:
			return "OK";
		case xdg_alloc:
			return "memory allocation failed";
		case xdg_nohome:
			return "There is no $HOME";
		case xdg_noalluserprofile:
			return "Theres no $ALLUSERSPROFILE is no $ALLUSERSPROFILE";
		case xdg_nopath:
			return "There is no resulting path";
		case xdg_mallformed:
			return "Malformed path fount";
		default:
			return "unknown";
	}
}


/* ---------------------------------------------------------------- */
#ifdef STANDALONE_TEST
/* test code */

/* Return nz on error */
static int touch(char *name) {
	FILE *fp;

	if ((fp = fopen(name,"w")) == NULL)
		return 1;

	if (fclose(fp))
		return 1;
	 
	return 0;
}

/* Check a file can be opened */
/* Return nz on error */
static int check(char *name) {
	FILE *fp;

	if ((fp = fopen(name,"r")) == NULL)
		return 1;

	if (fclose(fp))
		return 1;
	 
	return 0;
}

/* Delete a path and a file */
/* Return nz on error */
static int delpath(char *path, int depth) {
	int i;
	char *pp;

	for (i = 0; i < depth;) {
//		a1logd(DBGA,"deleting '%s'\n",path);
		if (i == 0) {
			if (unlink(path)) {
//				a1logd(DBGA,"unlink '%s' failed\n",path);
				return 1;
			}
		} else {
			if (rmdir(path)) {
//				a1logd(DBGA,"rmdir '%s' failed\n",path);
				return 1;
			}
		}
		i++;
		if (i == depth)
			return 0;

		if ((pp = strrchr(path, '/')) == NULL)
			return 0;
		*pp = '\000';
	}
	return 0;
}

/* Run a test */
static int runtest(
	xdg_storage_type st,	/* Specify the storage type */
	xdg_scope sc,			/* Scope if write */
	char *pfname,			/* Sub-path and file name */
	char *env,				/* Environment variable being set */
	char *envv,				/* Value to set it to */
	char *defv,				/* default variable needed for read */
	int depth				/* Cleanup depth */
) {
	xdg_error er;
	char *xval;
	char **paths;
	int nopaths;
	char buf[200];

	if ((xval = getenv(env)) != NULL)		/* Save value before mods */
		xval = strdup(xval);
	if (*env != '\000') {		/* If it is to be set */
		sprintf(buf, "%s=%s",env,envv);
		mputenv(buf);
	}

	printf("\nTesting Variable %s\n",env);
	if ((nopaths = xdg_bds(&er, &paths, st, xdg_write, sc, xdg_none, pfname)) == 0) {
		printf("Write test failed with %s\n",xdg_errstr(er));
		return 1;
	}
	printf("Create %s %s returned '%s'\n",
		st == xdg_data ? "Data" : st == xdg_data ? "Conf" : "Cache",
		sc == xdg_data ? "User" : "Local", paths[0]);
	if (touch(paths[0])) {
		printf("Creating file %s failed\n",paths[0]);
		return 1;
	}
	if (check(paths[0])) {
		printf("Checking file %s failed\n",paths[0]);
		return 1;
	}

	if (sc == xdg_local && *env != '\000') {	/* Add another path */
		sprintf(buf, "%s=xdgtestXXX%c%s",env,SSEP,envv);
		mputenv(buf);
	}

	if (defv != NULL) {
		sprintf(buf, "%s=xdg_NOT_%s",defv,defv);
		mputenv(buf);
	}
	xdg_free(paths, nopaths);

	if ((nopaths = xdg_bds(&er, &paths, st, xdg_read, sc, xdg_none, pfname)) < 1) {
		printf("Read test failed with %s\n",xdg_errstr(er));
		return 1;
	}
	printf("  Read %s %s returned '%s'\n",
		st == xdg_data ? "Data" : st == xdg_data ? "Conf" : "Cache",
		sc == xdg_data ? "User" : "Local",paths[0]);
	if (check(paths[0])) {
		printf("Checking file %s failed\n",paths[0]);
		return 1;
	}
	if (delpath(paths[0], depth)) {
		printf("Warning: Deleting file %s failed\n",paths[0]);
	}
	xdg_free(paths, nopaths);

	/* Restore variables value */
	if (xval == NULL)
		sprintf(buf, "%s=",env);
	else
		sprintf(buf, "%s=%s",env,xval);
	mputenv(buf);

	if (defv != NULL) {
		sprintf(buf, "%s=",defv);
		mputenv(buf);
	}

	return 0;
}

typedef struct {
	xdg_storage_type st;	/* Storage type */
	xdg_scope sc;			/* Scope if write */
	char *defv[2];			/* Default variables needed for user & local tests on read */
	char *envn[10];			/* Environment variable name to set */
} testcase;

int
main() {
	char buf1[200], buf2[200];
	int i, j;

#ifdef NT
	testcase cases[5] = {
		{ xdg_data, xdg_user, {"ALLUSERSPROFILE", NULL},
							{ "XDG_DATA_HOME", "APPDATA", "HOME", "APPDATA", NULL } },
		{ xdg_conf, xdg_user, {"ALLUSERSPROFILE", NULL},
							{ "XDG_CONFIG_HOME", "APPDATA", "HOME", "APPDATA", NULL } },
		{ xdg_cache, xdg_user, {NULL, NULL},
							{ "XDG_CACHE_HOME", "APPDATA", "HOME", "APPDATA", NULL } },
		{ xdg_data, xdg_local, {NULL, "HOME"},
							{ "XDG_DATA_DIRS", "ALLUSERSPROFILE", NULL } },
		{ xdg_conf, xdg_local, {NULL, "HOME"},
							{ "XDG_CONFIG_DIRS", "ALLUSERSPROFILE", NULL } }
	};
#else	/* Apple, Unix, Default */
	testcase cases[5] = {
		{ xdg_data, xdg_user, {NULL, NULL},
							{ "XDG_DATA_HOME", "HOME", NULL } },
		{ xdg_conf, xdg_user, {NULL, NULL},
							{ "XDG_CONFIG_HOME", "HOME", NULL } },
		{ xdg_cache, xdg_user, {NULL, NULL},
							{ "XDG_CACHE_HOME", "HOME", NULL } },
		{ xdg_data, xdg_local, {NULL, "HOME"},
							{ "XDG_DATA_DIRS", "", NULL } },
		{ xdg_conf, xdg_local, {NULL, "HOME"},
							{ "XDG_CONFIG_DIRS", "", NULL } }
	};
#endif

	/* First clear all the environment variables */
	for (i = 0; i < 5; i++) {
		for (j = 0; ;j++) {
			if (cases[i].envn[j] == NULL)
				break;
			sprintf(buf1, "%s=",cases[i].envn[j]);
			mputenv(buf1);
		}
	}

	/* Then run all the tests */
	for (i = 0; i < 5; i++) {
		for (j = 0; ;j++) {
			if (cases[i].envn[j] == NULL)
				break;
			sprintf(buf1, "xdgtest%d",i);
			sprintf(buf2, "application/%s",cases[i].st == xdg_data ? "data" :
			                   cases[i].st == xdg_conf ? "config" : "cache");
			if (runtest(cases[i].st, cases[i].sc, buf2, cases[i].envn[j],buf1,
			   cases[i].defv[cases[i].sc == xdg_user ? 0 : 1],9))
				exit(1);
		}
	}

	printf("Test completed OK\n");

	return 0;
}

#endif /* STANDALONE_TEST */


