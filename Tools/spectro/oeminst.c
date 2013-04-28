/* 
 * Argyll Color Correction System
 *
 * OEM data file installer.
 *
 * Author: Graeme W. Gill
 * Date:   13/11/2012
 *
 * Copyright 2006 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#else /* SALONEINSTLIB */
#include <fcntl.h>
#include "sa_config.h"
#include "numsup.h"
#endif /* SALONEINSTLIB */
#include "xdg_bds.h"
#include "conv.h"
#include "aglob.h"
#include "oemarch.h"
#include "xspect.h"
#include "ccmx.h"
#include "ccss.h"

void usage(void) {
	fprintf(stderr,"Install OEM data files, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL Version 2 or later\n");
	fprintf(stderr,"usage: oeminst [-options] [infile(s)]\n");
	fprintf(stderr," -v                      Verbose\n");
	fprintf(stderr," -n                      Don't install, show where files would be installed\n");
	fprintf(stderr," -c                      Don't install, save files to current directory\n");
	fprintf(stderr," -S d                    Specify the install scope u = user (def.), l = local system]\n");
	fprintf(stderr," infile                  setup.exe CD install file(s) or .dll(s) containing install files\n");
	fprintf(stderr," infile.[edr|ccss|ccmx]  EDR file(s) to translate and install or CCSS or CCMX files to install\n");
	fprintf(stderr,"                         If no file is provided, oeminst will look for the install CD.\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;					/* argument we're looking at */
	xdg_scope scope = xdg_user;
	int verb = 0;
	int install = 1;
	int local = 0;
	int has_ccss = 0;
	char *install_dir = "";
	int i;
	xfile *files = NULL;
	int rv = 0;
	
	set_exe_path(argv[0]);		/* Set global exe_path and error_program */

	if (argc < 1)
		usage();

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage();

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			/* No install */
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				install = 0;
			}

			/* Save to current directory */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C') {
				local = 1;
			}

			/* Install scope */
			else if (argv[fa][1] == 'S') {
				fa = nfa;
				if (na == NULL) usage();
					else if (na[0] == 'l' || na[0] == 'L')
						scope = xdg_local;
					else if (na[0] == 'u' || na[0] == 'U')
						scope = xdg_user;
			}
			else 
				usage();
		}
		else
			break;
	}

	if (fa > argc || (fa < argc && argv[fa][0] == '-')) usage();

	/* If filename(s) are provided, load the files up. */
	/* We don't know what they are, but oemarch_get_ifiles() will figure it out */
	for (; fa < argc; fa++) {
		xfile *xf;
		xf = new_add_xf(&files, argv[fa], NULL, 0, file_arch | file_dllcab | file_data,
		                                     targ_spyd_pld | targ_spyd_cal
		                                   | targ_i1d3_edr | targ_ccmx);
		if (load_xfile(xf, verb))
			error("Unable to load file '%s'",xf->name);
	}

	/* Turn files into installables */
	if ((files = oemarch_get_ifiles(files, verb)) == NULL)
		error("Didn't locate any files to install - no CD present ?\n");

	/* If there are any ccss files to install, supliment with any found in ../ref */
	for (i = 0; files[i].name != NULL; i++) {
		if (files[i].ftype == file_data
		 && files[i].ttype == targ_i1d3_edr)
			has_ccss = 1;
	}
	if (has_ccss) {
		int len;
		char *pp;
		char tname[MAXNAMEL+1] = { '\000' };
		aglob ag;

		strcpy(tname, exe_path);

		len = strlen(tname);
		if ((pp = strrchr(tname, '/')) != NULL)
			*pp = '\000';
		if ((pp = strrchr(tname, '/')) != NULL) {
			strcpy(pp, "/ref/*.ccss");

			if (aglob_create(&ag, tname))
				error("Searching for '%s' malloc error",tname);

			for (;;) {
				char *cp;
				xfile *xf;
				if ((pp = aglob_next(&ag)) == NULL)
					break;

				/* Leave just base filename */
				if ((cp = strrchr(pp, '/')) == NULL
				 && (cp = strrchr(pp, '\\')) == NULL)
					cp = pp;
				else
					cp++;

				xf = new_add_xf(&files, pp, NULL, 0, file_data, targ_i1d3_edr);
				if (load_xfile(xf, verb))
					error("Unable to load file '%s'",xf->name);
				free(xf->name);
				if ((xf->name = strdup(cp)) == NULL)
					error("strdup failed");
			}
			aglob_cleanup(&ag);
		}
	}

#ifdef NEVER
	for (i = 0; files[i].name != NULL; i++) {
		printf("Got '%s' size %d ftype 0x%x ttype 0x%x to install\n",files[i].name,files[i].len,files[i].ftype,files[i].ttype);
	}
#endif

	/* Do a special check of ccmx and ccss files, to warn if there will be problems with them */
	for (i = 0; files[i].name != NULL; i++) {
		xfile *xf = &files[i];

		if (xf->ttype & targ_ccmx) {
			ccmx *cx;
			if ((cx = new_ccmx()) == NULL)
				error("new_ccmx failed");
			if (cx->buf_read_ccmx(cx, xf->buf, xf->len)) {
				error("Reading '%s' failed with '%s'\n",xf->name,cx->err);
			}
			if (cx->cbid <= 0)
				error("'%s' doesn't contain DISPLAY_TYPE_BASE_ID field :- it can't be installed without this!",xf->name);
			if (cx->refrmode < 0)
				warning("'%s' doesn't contain DISPLAY_TYPE_REFRESH field :- non-refresh will be assumed!",xf->name);
			cx->del(cx);
		} else if (xf->ttype & targ_i1d3_edr) {
			ccss *ss;
			if ((ss = new_ccss()) == NULL)
				error("new_ccss failed");
			if (ss->buf_read_ccss(ss, xf->buf, xf->len)) {
				error("Reading '%s' failed with '%s'\n",xf->name,ss->err);
			}
			if (ss->refrmode < 0)
				warning("'%s' doesn't contain DISPLAY_TYPE_REFRESH field :- non-refresh will be assumed!",xf->name);
			ss->del(ss);
		}
	}

	/* We now have all the install files loaded into files. Save or install them all */
 	
	if (!local) /* Install them in ArgyllCMS sub-directory */
		install_dir = "ArgyllCMS/";

	/* Save all the install file */
	for (i = 0; files[i].name != NULL; i++) {
		xfile *xf = &files[i];
		size_t len;
		char *install_name = NULL, *cp;

		if (xf->ftype != file_data) {
			if (verb) printf("Skipping '%s' as its type is unknown\n",xf->name);
			continue;
		}

		/* Create install path and name */
		len = strlen(install_dir) + strlen(xf->name);
		if ((install_name = malloc(len)) == NULL)
			error("malloc install_name %d bytes failed",len);

		strcpy(install_name, install_dir);

		/* Append just the basename of the xf */
		if ((cp = strrchr(xf->name, '/')) == NULL
		 && (cp = strrchr(xf->name, '\\')) == NULL)
			cp = xf->name;
		else
			cp++;

		strcat(install_name, cp);

		if (!local) {
			char **paths = NULL;
			int npaths = 0;

			/* Get destination path. This may drop uid/gid if we are su */
			if ((npaths = xdg_bds(NULL, &paths, xdg_data, xdg_write, scope, install_name)) < 1) {
				error("Failed to find/create XDG_DATA path '%s'",install_name);
			}
			if (install)
				save_xfile(xf, paths[0], NULL, verb);
			else
				printf("Would install '%s'\n",paths[0]);
			xdg_free(paths, npaths);

		} else {
			if (install)
				save_xfile(xf, install_name, NULL, verb);
			else
				printf("Would save '%s'\n",install_name);
		}
		free(install_name);
	}

	del_xf(files);

	return 0;
}
