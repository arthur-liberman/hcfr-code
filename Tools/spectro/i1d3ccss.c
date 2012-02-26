/* 
 * Argyll Color Correction System
 *
 * X-Rite i1 DIsplay 3 related software.
 *
 * Author: Graeme W. Gill
 * Date:   18/8/2011
 *
 * Copyright 2006 - 2011, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* This program allows translating the EDR calibration files that */
/* come with the i1d3 instrument into Argyll .ccss calibration files, */
/* and then installing them. It also allows installing .ccss files */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#if defined (NT)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef UNIX
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>
#endif /* UNIX */
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
#include "xspect.h"
#include "conv.h"
#include "ccss.h"
#include "LzmaDec.h"

#define MAX_EDR_FILES 200

#undef PLOT_SAMPLES

/* --------------------------------------------------------- */

/* A list of files stored in memory. The last entry of the list has name == NULL */
typedef struct {
	char *name;				/* Name of file */
	unsigned char *buf;		/* It's contents */
	unsigned long len;		/* The length of the contents */
} xfile;

xfile *new_xf(int n);
xfile *add_xf(xfile **l);
void del_xf(xfile *l);

static xfile *load_file(char *sname, int verb);
static xfile *inno_extract(xfile *xi, char *tfilename, int verb);
static xfile *msi_extract(xfile *xi, char *tname, int verb);
static xfile *cab_extract(xfile *xi, char *text, int verb);

static ccss *parse_EDR(unsigned char *buf, unsigned long len, char *name, int verb);
static ccss *read_EDR_file(char *name, int verb);

/*
	"i1Profiler" "/Installer/Setup.exe"
	"ColorMunki Displ", "/Installer/ColorMunkiDisplaySetup.exe"
 */

/* Cleanup function for transfer on Apple OS X */
void umiso(char *amount_path) {
#if defined(UNIX) && defined(__APPLE__)
	char sbuf[MAXNAMEL+1 + 100];
	sprintf(sbuf, "umount \"%s\"",amount_path);
	system(sbuf);
	sprintf(sbuf, "rmdir \"%s\"",amount_path);
	system(sbuf);
#endif /* UNIX && __APPLE__ */
}

void usage(void) {
	fprintf(stderr,"Translate X-Rite EDR to Argyll .ccss format, & install, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL Version 2 or later\n");
	fprintf(stderr,"usage: i1d3edr [-options] [infile(s)]\n");
	fprintf(stderr," -v                Verbose\n");
	fprintf(stderr," -n                Don't install, save files to current directory\n");
	fprintf(stderr," -S d              Specify the install scope u = user (def.), l = local system]\n");
	fprintf(stderr," infile            setup.exe file source .edr files from \n");
	fprintf(stderr," infile.[edr|ccss] EDR file(s) to translate and install or CCSS files to install\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char *ccss_names[MAX_EDR_FILES];
	ccss *ccss_objs[MAX_EDR_FILES];
	int no_ccss = 0;			/* Number of files provided */
	int not_edr = 0;			/* NZ if first file is not a .edr or .ccss */
	xdg_scope scope = xdg_user;
	int verb = 0;
	int install = 1;
	int amount = 0;			/* We mounted the CDROM */
	char *amount_path = "";	/* Path we mouted */
	char *install_dir = NULL;
	int i;
	
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

	/* If filename(s) are provided */
	if (fa < argc) {
		for (no_ccss = 0; fa < argc; no_ccss++, fa++) {
			if (no_ccss >= MAX_EDR_FILES)
				error("Too many ccss files (max %d)",MAX_EDR_FILES);

			if ((ccss_names[no_ccss] = strdup(argv[fa])) == NULL)
				error("malloc failed on ccss_names");

			if ((strlen(ccss_names[no_ccss]) > 4
			 && (strncmp(ccss_names[no_ccss] + strlen(ccss_names[no_ccss]) -4, ".edr", 4) == 0
			  || strncmp(ccss_names[no_ccss] + strlen(ccss_names[no_ccss]) -4, ".EDR", 4) == 0))
			 || (strlen(ccss_names[no_ccss]) > 5
			 && (strncmp(ccss_names[no_ccss] + strlen(ccss_names[no_ccss]) -5, ".ccss", 5) == 0
			  || strncmp(ccss_names[no_ccss] + strlen(ccss_names[no_ccss]) -5, ".CCSS", 5) == 0))) {
				/* */
			} else {
				not_edr = 1;
			}
		}
		if (not_edr && no_ccss > 1)
			error("Need either an archive name or list of .edr or .ccss files");

	/* Assume we are looking for install files or Setup.exe on a CD */
	} else {
		

		/* Look for the CD */
#ifdef NT
		{
			int i;
			char *pf = NULL;

			/* Typical instalation locations */
			char *paths[] = {
				"/X-Rite/Devices/i1d3/Calibrations/*.edr",
				""
			};

			/* Typical volume names the CDROM may have */
			char *vols[] = {
				"i1Profiler",
				"ColorMunki Displ",
				""
			};

			/* Corresponding setup filenames for each volume name */
			char *setupnames[] = {
				"Installer\\Setup.exe",
				"Installer\\ColorMunkiDisplaySetup.exe",
				""
			};

			/* See if the files are already installed for the OEM */
			for (i = 0;;i++) {
				char tname[MAXNAMEL+1] = { '\000' };
				aglob ag;

				if (paths[i][0] == '\000')
					break;

				/* Where the normal instalation goes */
				if ((pf = getenv("PROGRAMFILES")) != NULL)
					strcpy(tname, pf);
				else
					strcpy(tname, "C:\\Program Files");

				strcat(tname, paths[i]);
				if (verb) {
					printf("Looking for MSWindows install at '%s' .. ",tname);
					fflush(stdout);
				}
				if (aglob_create(&ag, tname))
					error("Searching for '%s' malloc error",paths[i]);

				for (;;) {
					char *pp;
					if ((pp = aglob_next(&ag)) == NULL)
						break;
					if (no_ccss >= MAX_EDR_FILES)
						error("Too many ccss files (max %d)",MAX_EDR_FILES);
					ccss_names[no_ccss++] = pp;
				}
				aglob_cleanup(&ag);

				if (verb && no_ccss == 0) printf("not found\n");
			}
			/* No EDR files found, so look for CD */
			if (no_ccss == 0) {
				char buf[400];
				char vol_name[MAXNAMEL+1] = { '\000' };
				char tname[MAXNAMEL+1] = { '\000' };
				int len, j;

				if (verb) { printf("Looking for i1d3 install CDROM .. "); fflush(stdout); }

				len = GetLogicalDriveStrings(400, buf);
				if (len > 400)
					error("GetLogicalDriveStrings too large");
				for (i = 0; ;) {		/* For all drives */
					if (buf[i] == '\000')
						break;
					if (GetDriveType(buf+i) == DRIVE_CDROM) {
						DWORD maxvoll, fsflags;
						if (GetVolumeInformation(buf + i, vol_name, MAXNAMEL,
						                     NULL, &maxvoll, &fsflags, NULL, 0) != 0) {
							for (j = 0;;j++) {	/* For all volume names */
								if (vols[j][0] == '\000')
									break;
								if (strcmp(vol_name, vols[j]) == 0) {
									/* Found the instalation CDROM volume name */
									strcpy(tname, buf+i);
									strcat(tname, setupnames[j]);
									if (verb)
										printf("found Vol '%s' file '%s'\n",vols[j], tname);
									if (no_ccss >= MAX_EDR_FILES)
										error("Too many ccss files (max %d)",MAX_EDR_FILES);
									if ((ccss_names[no_ccss++] = strdup(tname)) == NULL)
										error("malloc failed on ccss_names");
									not_edr = 1;
									break;
								}
							}
							if (vols[j][0] != '\000')
								break;
						}
					}
					i += strlen(buf + i) + 1;
				}
				if (buf[i] == '\000' && verb)
					printf("No volume & Setup.exe found\n");

			}
		}
#endif /* NT */

#if defined(UNIX) && defined(__APPLE__)
		{
			int i;

			/* Typical instalation locations */
			char *paths[] = {
				"/Library/Application Support/X-Rite/Devices/i1d3xrdevice/Contents/Resources/Calibrations/*.edr",
				""
			};

			/* Typical native volume names the CDROM may have */
			char *vols[] = {
				"/Volumes/i1Profiler",
				"/Volumes/ColorMunki Display",
				""
			};
			/* Corresponding setup filenames for each volume name */
			char *setupnames[] = {
				"Installer/Setup.exe",
				"Installer/ColorMunkiDisplaySetup.exe",
				""
			};

			/* See if the files are already installed for the OEM */
			for (i = 0;;i++) {
				aglob ag;

				if (paths[i][0] == '\000')
					break;

				if (verb) {
					printf("Looking for OS X install at '%s' .. ",paths[i]);
					fflush(stdout);
				}
				if (aglob_create(&ag, paths[i]))
					error("Searching for '%s' malloc error",paths[i]);

				for (;;) {
					char *pp;
					if ((pp = aglob_next(&ag)) == NULL)
						break;
					if (no_ccss >= MAX_EDR_FILES)
						error("Too many ccss files (max %d)",MAX_EDR_FILES);
					ccss_names[no_ccss++] = pp;
				}
				aglob_cleanup(&ag);

				if (verb && no_ccss == 0) printf("not found\n");
			}

			/* No EDR files found, so look for CD */
			if (no_ccss == 0) {
				int j;
				char tname[MAXNAMEL+1] = { '\000' };

				if (verb) { printf("Looking for i1d3 install CDROM .. \n"); fflush(stdout); }

				for (j = 0; ; j++) {
					if (vols[j][0] == '\000')
						break;

					/* In case it's already mounted (abort of spyd2en ?) */
					strcpy(tname, vols[j]);
					strcat(tname, "_ISO");
					if ((amount_path = strdup(tname)) == NULL)
						error("Malloc of amount_path failed");
					strcat(tname, "/");
					strcat(tname, setupnames[j]);
					if (verb) { printf("Checking if '%s' is already mounted .. ",tname); fflush(stdout); }
					if (access(tname, 0) == 0) {
						if (no_ccss >= MAX_EDR_FILES)
							error("Too many ccss files (max %d)",MAX_EDR_FILES);
						if ((ccss_names[no_ccss++] = strdup(tname)) == NULL)
							error("malloc failed on ccss_names");
						if (verb) printf("yes\n");
						not_edr = 1;
						amount = 1;		/* Remember to unmount */
						break;
					}

					/* Not already mounted. */
					if (access(vols[j], 0) == 0) {
						struct statfs buf;
						char sbuf[MAXNAMEL+1 + 100];
						int sl, rv;
						if (verb) { printf("no\nMounting ISO partition .. "); fflush(stdout); }
	
						/* but we need the ISO partition */
						/* Locate the mount point */
						if (statfs(vols[j], &buf) != 0) 
							error("\nUnable to locate mount point for '%s' of install CDROM",vols[j]); 
						if ((sl = strlen(buf.f_mntfromname)) > 3
						 && buf.f_mntfromname[sl-2] == 's'
						 && buf.f_mntfromname[sl-1] >= '1'
						 && buf.f_mntfromname[sl-1] <= '9')
							 buf.f_mntfromname[sl-2] = '\000';
						else
							error("\nUnable to determine CDROM mount point from '%s'",buf.f_mntfromname);
	
						strcpy(tname, vols[j]);
						strcat(tname, "_ISO");
						sprintf(sbuf, "mkdir \"%s\"", tname);
						if ((rv = system(sbuf)) != 0)
							error("\nCreating ISO9660 volume of CDROM failed with %d",rv); 
						sprintf(sbuf, "mount_cd9660 %s \"%s\"", buf.f_mntfromname,tname);
						if ((rv = system(sbuf)) != 0) {
							sprintf(sbuf, "rmdir \"%s\"", tname);
							system(sbuf);
							error("\nMounting ISO9660 volume of CDROM failed with %d",rv); 
						}
						if (verb)
							printf("done\n");
						strcat(tname, "/");
						strcat(tname, setupnames[j]);
						if (no_ccss >= MAX_EDR_FILES)
							error("Too many ccss files (max %d)",MAX_EDR_FILES);
						if ((ccss_names[no_ccss++] = strdup(tname)) == NULL)
							error("malloc failed on ccss_names");
						not_edr = 1;
						amount = 1;		/* Remember to unmount */
						break;
					}
				}
				if (no_ccss == 0)
					if (verb) printf("not found\n");
			}
		}
#endif /* UNIX && __APPLE__ */

#if defined(UNIX) && !defined(__APPLE__)
		{
			char tname[MAXNAMEL+1] = { '\000' };
			int j, k;

			/* Since there is no vendor driver instalation for Linux, we */
			/* must look for the CDROM. */
			char *vols[] = {		/* Typical volumes the CDROM could be mounted under */
				"/media/i1Profiler",
				"/media/ColorMunki Displ",
				"/mnt/cdrom",
				"/mnt/cdrecorder",
				"/media/cdrom",
				"/media/cdrecorder",
				"/cdrom",
				"/cdrecorder",
				""
			};
			char *setupnames[] = {
				"Installer/Setup.exe",
				"Installer/ColorMunkiDisplaySetup.exe",
				""
			};

			if (verb)
				if (verb) { printf("Looking for i1d3 install CDROM .. "); fflush(stdout); }
			/* See if we can see what we're looking for on one of the volumes */
			/* It would be nice to be able to read the volume name ! */
			for (j = 0;;j++) {
				if (vols[j][0] == '\000')
					break;
				for (k = 0;;k++) {
					if (setupnames[k][0] == '\000')
						break;
				
					strcpy(tname, vols[j]);
					strcat(tname, "/");
					strcat(tname, setupnames[k]);
					if (access(tname, 0) == 0) {
						if (verb) printf("found '%s'\n",tname);
						if (no_ccss >= MAX_EDR_FILES)
							error("Too many ccss files (max %d)",MAX_EDR_FILES);
						if ((ccss_names[no_ccss++] = strdup(tname)) == NULL)
							error("malloc failed on ccss_names");
						not_edr = 1;
						break;		
					}
				}
				if (setupnames[k][0] != '\000')	/* We got it */
					break;
			}
			if (no_ccss == 0)
				if (verb) printf("not found\n");
		}
#endif	/* UNIX */
	}

	if (no_ccss == 0) {
		if (amount) umiso(amount_path);
		error("No CD found, no Setup.exe or list of EDR or CCSS files provided");
	}

	if (not_edr) {	/* Must be Setup.exe */
		xfile *xf0 = NULL;
#ifdef NEVER
		xfile *xf1 = NULL;
#endif
		xfile *xf2 = NULL;
		xfile *xf3 = NULL;

		/* Load up the Seup.exe file */
		xf0 = load_file(ccss_names[0], verb);

#ifdef NEVER
		/* Extract .msi from it */
		xf1 = inno_extract(xf0, "{tmp}\\XRD i1d3.msi", verb);
		del_xf(xf0);

		/* Extract the .cab from it */
		xf2 = msi_extract(xf1, "XRD_i1d3.cab", verb);
		del_xf(xf1);
#else

		/* Extract the .cab directly from Setup.exe */
		xf2 = msi_extract(xf0, "XRD_i1d3.cab", verb);
		del_xf(xf0);
#endif

		/* Extract the .edr's from it */
		xf3 = cab_extract(xf2, ".edr", verb);
		del_xf(xf2);

		free(ccss_names[0]);
		no_ccss = 0;

		/* Translate all the .edr's */
		for (i = 0; xf3[i].name != NULL; i++) {
			if (verb)
				printf("Translating '%s'\n",xf3[i].name);
			if ((ccss_objs[no_ccss] = parse_EDR(xf3[i].buf, xf3[i].len, xf3[i].name, verb))
			                                                                         == NULL) {
				warning("Failed to parse EDR '%s'\n",xf3[i].name);
			} else  {
				if (no_ccss >= MAX_EDR_FILES)
					error("Too many ccss files (max %d)",MAX_EDR_FILES);
				if ((ccss_names[no_ccss++] = strdup(xf3[i].name)) == NULL)
					error("malloc failed on ccss_names");
			}
		}
		del_xf(xf3);

		if(amount) umiso(amount_path);

		/* Also look for any ccss files in ../ref */
		{
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
					if ((pp = aglob_next(&ag)) == NULL)
						break;
					if (no_ccss >= MAX_EDR_FILES)
						error("Too many ccss files (max %d)",MAX_EDR_FILES);
					ccss_names[no_ccss] = pp;
					if (verb)
						printf("Reading '%s'\n",ccss_names[no_ccss]);
					if ((ccss_objs[no_ccss] = new_ccss()) == NULL)
						error("new_ccss failed");
					if (ccss_objs[i]->read_ccss(ccss_objs[no_ccss], ccss_names[no_ccss])) {
						warning("Reading CCSS file '%s' failed with '%s' - skipping",ccss_names[no_ccss],ccss_objs[i]->err);
						free(ccss_names[no_ccss]);
						ccss_names[no_ccss] = NULL;
						continue;
					}
					no_ccss++;
				}
				aglob_cleanup(&ag);
			}
		}

	/* Given .edr and .ccss files */
	} else {

		/* Load up each .edr or .ccss */
		for (i = 0; i < no_ccss; i++) {
			/* If .ccss */
		 	if ((strlen(ccss_names[i]) > 5
				 && (strncmp(ccss_names[i] + strlen(ccss_names[i]) -5, ".ccss", 5) == 0
				  || strncmp(ccss_names[i] + strlen(ccss_names[i]) -5, ".CCSS", 5) == 0))) {
				if (verb)
					printf("Reading '%s'\n",ccss_names[i]);
				if ((ccss_objs[i] = new_ccss()) == NULL)
					error("new_ccss failed");
				if (ccss_objs[i]->read_ccss(ccss_objs[i], ccss_names[i])) {
					warning("Reading CCSS file '%s' failed with '%s' - skipping",ccss_names[i],ccss_objs[i]->err);
					free(ccss_names[i]);
					ccss_names[i] = NULL;
					continue;
				}

			/* If .edr */
			} else {
				if (verb)
					printf("Translating '%s' to ccss\n",ccss_names[i]);
				if ((ccss_objs[i] = read_EDR_file(ccss_names[i], verb)) == NULL) {
					warning("Reading EDR file '%s' failed - skipping",ccss_names[i]);
					free(ccss_names[i]);
					ccss_names[i] = NULL;
					continue;
				}
			}

		}
	}

	/* We now have all the ccss objects loaded into an array. Save or install them all */
 	
	if (install)
		install_dir = "color";		/* Install them in color sub-directory */

	/* Save each ccss file */
	for (i = 0; i < no_ccss; i++) {
		char *edrname;
		int len;
		char *install_name = NULL;

		if (ccss_names[i] == NULL)		/* Couldn'te load this */
			continue;

		/* Get basename of file */
		if ((edrname = strrchr(ccss_names[i], '/')) == NULL)
			edrname = ccss_names[i];
		else
			edrname++;

		/* Create install path and name */

		len = 0;
		if (install_dir != NULL)
			len += strlen(install_dir) + 1;

		len += strlen(edrname) + 2;

		if ((install_name = malloc(len)) == NULL)
			error("Malloc failed on install_name\n");
		install_name[0] = '\000';

		if (install_dir != NULL && install_dir[0] != '\000') {
			strcpy(install_name, install_dir);
			strcat(install_name, "/");
		}
		strcat(install_name, edrname);

		/* Fix extension */
		if (strlen(install_name) > 4
		 && (strncmp(install_name + strlen(install_name) -4, ".edr", 4) == 0
		  || strncmp(install_name + strlen(install_name) -4, ".EDR", 4) == 0))
			strcpy(install_name + strlen(install_name) -4, ".ccss");

		if (install) {
			char **paths = NULL;
			int npaths = 0;

			/* Get destination path. This may drop uid/gid if we are su */
			if ((npaths = xdg_bds(NULL, &paths, xdg_data, xdg_write, scope, install_name)) < 1) {
				error("Failed to find/create XDG_DATA path '%s'",install_name);
			}
			if (verb)
				printf("Writing '%s'\n",paths[0]);
			if (ccss_objs[i]->write_ccss(ccss_objs[i], paths[0])) {
				warning("Writing ccss file '%s' failed - skipping",paths[0]);
			}
			xdg_free(paths, npaths);

		} else {
			if (verb)
				printf("Writing '%s'\n",install_name);
			if (ccss_objs[i]->write_ccss(ccss_objs[i], install_name)) {
				warning("Writing ccss file '%s' failed - skipping",install_name);
			}
		}
		ccss_objs[i]->del(ccss_objs[i]);
		free(ccss_names[i]);

		if (install_name != NULL)
			free(install_name);
	}

	return 0;
}

/* ========================================================= */

/* Take a 64 sized return buffer, and convert it to an ORD64 */
static ORD64 buf2ord64(unsigned char *buf) {
	ORD64 val;
	val = buf[7];
	val = ((val << 8) + (0xff & buf[6]));
	val = ((val << 8) + (0xff & buf[5]));
	val = ((val << 8) + (0xff & buf[4]));
	val = ((val << 8) + (0xff & buf[3]));
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a word sized return buffer, and convert it to an unsigned int */
static unsigned int buf2uint(unsigned char *buf) {
	unsigned int val;
	val = buf[3];
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a word sized return buffer, and convert it to an int */
static int buf2int(unsigned char *buf) {
	int val;
	val = buf[3];
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a short sized return buffer, and convert it to an int */
static int buf2short(unsigned char *buf) {
	int val;
	val = buf[1];
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

// Print bytes as hex to fp
static void dump_bytes(FILE *fp, char *pfx, unsigned char *buf, int len) {
	int i, j, ii;
	for (i = j = 0; i < len; i++) {
		if ((i % 16) == 0)
			fprintf(fp,"%s%04x:",pfx,i);
		fprintf(fp," %02x",buf[i]);
		if ((i+1) >= len || ((i+1) % 16) == 0) {
			for (ii = i; ((ii+1) % 16) != 0; ii++)
				fprintf(fp,"   ");
			fprintf(fp,"  ");
			for (; j <= i; j++) {
				if (isprint(buf[j]))
					fprintf(fp,"%c",buf[j]);
				else
					fprintf(fp,".",buf[j]);
			}
			fprintf(fp,"\n");
		}
	}
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* A list of files stored in memory. The last entry of the list has name == NULL */

/* return a list with the given number of available entries */
xfile *new_xf(int n) {
	xfile *l;

	if ((l = (xfile *)calloc((n + 1), sizeof(xfile))) == NULL) {
		fprintf(stderr,"Failed to allocate xfile structure\n");
		exit(-1);
	}

	return l;
}

/* Add an entry to the list. Return the pointer to that entry */
xfile *add_xf(xfile **l) {
	int n;
	xfile *ll;

	for (ll = *l, n = 0; ll->name != NULL; ll++, n++)
		;

	if ((*l = (xfile *)realloc(*l, (n+2) * sizeof(xfile))) == NULL) {
		fprintf(stderr,"Failed to realloc xfile structure\n");
		exit(-1);
	}
	(*l)[n+1].name = NULL;		/* End marker */
	(*l)[n+1].buf = NULL;
	(*l)[n+1].len = 0;

	return &(*l)[n];
}


/* Free up a list */
void del_xf(xfile *l) {
	int n;

	if (l != NULL) {
		for (n = 0; l[n].name != NULL; n++) {
			free(l[n].name);
			if (l[n].buf != NULL)
				free(l[n].buf);
		}
		free(l);
	}
}

/* ================================================================ */
/* Load a disk file into an xfile */

static xfile *load_file(char *sname, int verb) {
	FILE *fp;
	unsigned char *ibuf;
	unsigned long ilen, bread;
	xfile *xf;

	if (verb) printf("Loading file '%s'\n",sname);

	/* Open up the file for reading */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	if ((fp = fopen(sname,"rb")) == NULL)
#else
	if ((fp = fopen(sname,"r")) == NULL)
#endif
	{
		fprintf(stderr,"Can't open file '%s'\n",sname);
		exit(-1);
	}

	/* Figure out how big it is */
	if (fseek(fp, 0, SEEK_END)) {
		fprintf(stderr,"Seek to EOF failed\n");
		exit(-1);
	}
	ilen = (unsigned long)ftell(fp);

	if (verb > 1) printf("Size of file '%s' is %lu bytes\n",sname, ilen);

	if (fseek(fp, 0, SEEK_SET)) {
		fprintf(stderr,"Seek to SOF of file '%s' failed\n",sname);
		exit(-1);
	}

	if ((ibuf = (unsigned char *)malloc(ilen)) == NULL) {
		fprintf(stderr,"malloc buffer for file '%s' failed\n",sname);
		exit(-1);
	}

	if (verb > 1) printf("(Reading file '%s')\n",sname);

	if ((bread = fread(ibuf, 1, ilen, fp)) != ilen) {
		fprintf(stderr,"Failed to read file '%s', read %lu out of %lu bytes\n",sname,bread,ilen);
		exit(-1);
	}
	fclose(fp);

	xf = new_xf(1);

	if ((xf[0].name = strdup(sname)) == NULL) {
		fprintf(stderr,"mmaloc failed on filename '%s'\n", sname);
		exit(-1);
	}

	xf[0].buf = ibuf;
	xf[0].len = ilen;

	return xf;
}

/* ========================================================= */
/* EDR to CCSS */

/* Given a buffer holding and EDR file, parse it and return a ccss */
/* Return NULL on error */
static ccss *parse_EDR(
	unsigned char *buf,
	unsigned long len,
	char *name,				/* Name of the file */
	int verb				/* Verbose flag */
) {
	int j;
	unsigned char *nbuf;
	unsigned long nlen;
	ORD64 edrdate;
	char creatdate[30];
	char dispdesc[256];
	int ttmin, ttmax;	/* Min & max technology strings (inclusive) */
	char **ttstrings;	/* Corresponding technology strings */
	int ttype;			/* Technology type idex */
	int nsets, set;
	xspect *samples = NULL, sp;
	int hasspec;
	double nmstart, nmend, nmspace;
	int nsamples;
	ccss *rv;

	/* We hard code the Technology strings for now. In theory we could */
	/* read them from the TechnologyStrings.txt file that comes with the .edr's */
	{
		ttmin = 0;
		ttmax = 22;
		if ((ttstrings = malloc(sizeof(char *) * (ttmax - ttmin + 2))) == NULL) {
			if (verb) printf("Malloc failed\n");
			return NULL;
		}
		ttstrings[0]  = "Color Matching Function";
		ttstrings[1]  = "Custom";
		ttstrings[2]  = "CRT";
		ttstrings[3]  = "LCD CCFL IPS";
		ttstrings[4]  = "LCD CCFL VPA";
		ttstrings[5]  = "LCD CCFL TFT";
		ttstrings[6]  = "LCD CCFL Wide Gamut IPS";
		ttstrings[7]  = "LCD CCFL Wide Gamut VPA";
		ttstrings[8]  = "LCD CCFL Wide Gamut TFT";
		ttstrings[9]  = "LCD White LED IPS";
		ttstrings[10] = "LCD White LED VPA";
		ttstrings[11] = "LCD White LED TFT";
		ttstrings[12] = "LCD RGB LED IPS";
		ttstrings[13] = "LCD RGB LED VPA";
		ttstrings[14] = "LCD RGB LED TFT";
		ttstrings[15] = "LED OLED";
		ttstrings[16] = "LED AMOLED";
		ttstrings[17] = "Plasma";
		ttstrings[18] = "RG Phosphor";
		ttstrings[19] = "Projector RGB Filter Wheel";
		ttstrings[20] = "Projector RGBW Filter Wheel";
		ttstrings[21] = "Projector RGBCMY Filter Wheel";
		ttstrings[22] = "Projector";
		ttstrings[23] = "Unknown";
	}

	if (len < 600) {
		if (verb) printf("Unable to read '%s' header\n",name);
		return NULL;
	}
	nbuf = buf + 600;		/* Next time we "read" the file */
	nlen = len - 600;

	/* See if it has the right file ID */
	if (strncmp("EDR DATA1", (char *)buf, 16) != 0) {
		if (verb) printf("File '%s' isn't an EDR\n",name);
		return NULL;
	}

	/* Creation Date&time of EDR */
	edrdate = buf2ord64(buf + 0x0018);
	strcpy(creatdate, ctime_64(&edrdate));
	
	/* Creation tool string @ 0x0020 */

	/* Display description */
	strncpy(dispdesc, (char *)buf + 0x0060, 255); dispdesc[255] = '\000';

	/* Technology type index. */
	ttype = buf2int(buf + 0x0160);

	/* Number of data sets */
	nsets = buf2int(buf + 0x0164);

	if (nsets < 3 || nsets > 100) {
		if (verb) printf("File '%s' number of data sets %d out of range\n",name,nsets);
		return NULL;
	}

	/* Model number string @ 0x0168 ? */
	/* Part code string @ 0x01a8 ? */
	/* Another number string @ 0x01e8 ? */

	/* Unknown Flag/number = 1 @ 0x022c */

	/* "has spectral data" flag ? */
	hasspec = buf2short(buf + 0x022E);
	if (hasspec != 1) {
		if (verb) printf("Has Data flag != 1 in EDR file '%s'\n",name);
		return NULL;
	}
	
	/* The spectral sample characteristics */
	nmstart = IEEE754_64todouble(buf2ord64(buf + 0x0230));
	nmend   = IEEE754_64todouble(buf2ord64(buf + 0x0238));
	nmspace = IEEE754_64todouble(buf2ord64(buf + 0x0240));
	
	/* Setup prototype spectral values */
	sp.spec_wl_short = nmstart;
	sp.spec_wl_long = nmend;
	sp.spec_n = (int)(1.0 + (nmend - nmstart)/nmspace + 0.5);
	sp.norm = 1.0;

	/* Unknown Flag/number = 0 @ 0x0248 */
	/* Error if not 0 ??? */
	
	if (hasspec) {
		/* Allocate space for the sets */
		if ((samples = (xspect *)malloc(sizeof(xspect) * nsets)) == NULL) {
			if (verb) printf("Malloc of spectral samples failed\n");
			return NULL;
		}
	}

	/* Read the sets of data */
	for (set = 0; set < nsets; set++) {

		/* "Read" in the 128 byte data set header */
		buf = nbuf;
		len = nlen;
		if (len < 128) {
			if (verb) printf("Unable to read file '%s' set %d data header\n",name,set);
			if (samples != NULL) free(samples);
			return NULL;
		}
		nbuf = buf + 128;		/* Next time we "read" the file */
		nlen = len - 128;

		/* See if it has the right ID */
		if (strncmp("DISPLAY DATA", (char *)buf, 16) != 0) {
			if (verb) printf("File '%s' set %d data header has unknown identifier\n",name,set);
			if (samples != NULL) free(samples);
			return NULL;
		}

		/* double Yxy(z) of sample at +0x0058 in data header ? */

		if (hasspec == 0)	/* No spectral data, so skip it */
			continue;

		/* Read in the 28 byte data set header */
		buf = nbuf;
		len = nlen;
		if (len < 28) {
			if (verb) printf("Unable to read file '%s' set %d spectral data  header\n",name,set);
			if (samples != NULL) free(samples);
			return NULL;
		}
		nbuf = buf + 28;		/* Next time we "read" the file */
		nlen = len - 28;

		/* See if it has the right ID */
		if (strncmp("SPECTRAL DATA", (char *)buf, 16) != 0) {
			if (verb) printf("File '%s' set %d data header has unknown identifier\n",name,set);
			if (samples != NULL) free(samples);
			return NULL;
		}

		/* Number of doubles in set */
		nsamples = buf2int(buf + 0x0010);

		if (nsamples != sp.spec_n) {
			if (verb) printf("File '%s' set %d number of samles %d doesn't match wavelengths %d\n",name,set,nsamples,sp.spec_n);
			if (samples != NULL) free(samples);
			return NULL;
		}

		/* Read in the spectral values */
		buf = nbuf;
		len = nlen;
		if (len < 8 * sp.spec_n) {
			if (verb) printf("Unable to read file '%s' set %d spectral data\n",name,set);
			if (samples != NULL) free(samples);
			return NULL;
		}
		nbuf = buf + 8 * sp.spec_n;		/* Next time we "read" the file */
		nlen = len - 8 * sp.spec_n;

		XSPECT_COPY_INFO(&samples[set], &sp);

		/* Read the spectral values for this sample */
		for (j = 0; j < sp.spec_n; j++) {
			samples[set].spec[j] = IEEE754_64todouble(buf2ord64(buf + j * 8));
		}
#ifdef PLOT_SAMPLES
		/* Plot the spectra */
		{
			double xx[500];
			double y1[500];
	
			for (j = 0; j < samples[set].spec_n; j++) {
				xx[j] = XSPECT_XWL(&sp, j);
				y1[j] = samples[set].spec[j];
			}
			printf("EDR sample %d (uncorrected)\n",set+1);
			do_plot6(xx, y1, NULL, NULL, NULL, NULL, NULL, samples[set].spec_n);
		}
#endif /* PLOT_SAMPLES */

	}

	/* After the spectral data comes the "correction" data. This seems to be */
	/* related to the measurement instrument (typically a Minolta CS1000). */
	/* It's not obvious why this isn't already applied to the spectral data, */
	/* and doesn't cover the same wavelength range as the samples. */

	/* Try and read in the 92 byte correction header */
	buf = nbuf;
	len = nlen;
	if (len >= 92) {
		nbuf = buf + 92;		/* Next time we "read" the file */
		nlen = len - 92;

		/* See if it has the right ID */
		if (strncmp("CORRECTION DATA", (char *)buf, 16) != 0) {
			if (verb) printf("File '%s' correction data header has unknown identifier\n",name);
			if (samples != NULL) free(samples);
			return NULL;
		}

		/* Number of doubles in set */
		nsamples = buf2int(buf + 0x0050);

		if (nsamples == 351) {
			sp.spec_wl_short = 380.0;
			sp.spec_wl_long = 730.0;
			sp.spec_n = nsamples;
			sp.norm = 1.0;
		} else if (nsamples == 401) {
			sp.spec_wl_short = 380.0;
			sp.spec_wl_long = 780.0;
			sp.spec_n = nsamples;
			sp.norm = 1.0;
		} else {
			if (verb) printf("File '%s' correction data has unknown range %d\n\n",name,nsamples);
			if (samples != NULL) free(samples);
			return NULL;
		}

		/* Read in the spectral values */
		buf = nbuf;
		len = nlen;
		if (len < 8 * sp.spec_n) {
			if (verb) printf("Unable to read file '%s' correction spectral data\n",name);
			if (samples != NULL) free(samples);
			return NULL;
		}
		nbuf = buf + 8 * sp.spec_n;		/* Next time we "read" the file */
		nlen = len - 8 * sp.spec_n;

		for (j = 0; j < sp.spec_n; j++) {
			sp.spec[j] = IEEE754_64todouble(buf2ord64(buf + j * 8));
		}

#ifdef PLOT_SAMPLES
		/* Plot the spectra */
		{
			double xx[500];
			double y1[500];
	
			for (j = 0; j < sp.spec_n; j++) {
				xx[j] = XSPECT_XWL(&sp, j);
				y1[j] = sp.spec[j];
			}
			printf("Correction data\n");
			do_plot6(xx, y1, NULL, NULL, NULL, NULL, NULL, sp.spec_n);
		}
#endif /* PLOT_SAMPLES */

		/* Apply the correction to all the spectral samples */
		for (set = 0; set < nsets; set++) {
			for (j = 0; j < samples[set].spec_n; j++)
				samples[set].spec[j] *= value_xspect(&sp, XSPECT_XWL(&samples[set], j));
		}
	}

	/* Creat a ccss */
	if ((rv = new_ccss()) == NULL) {
		if (verb) printf("Unable to read file '%s' correction spectral data\n",name);
		if (samples != NULL) free(samples);
		return NULL;
	}

	if (ttype < ttmin || ttype > ttmax) {
		ttype = ttmax + 1;			/* Set to Unknown */
	}

	/* Set it's values */
	rv->set_ccss(rv, "X-Rite", creatdate, NULL, dispdesc, ttstrings[ttype], "CS1000", samples, nsets);	

	free(ttstrings);
	free(samples);

	return rv;
}

/* Read am EDR file and return a ccss class */
/* Return NULL on error */
static ccss *read_EDR_file(
	char *name,				/* File to read */
	int verb				/* Verbose flag */
) {
	FILE *fp;
	unsigned char *ibuf;
	unsigned long ilen, bread;
	ccss *rv;

	/* Open up the file for reading */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	if ((fp = fopen(name,"rb")) == NULL)
#else
	if ((fp = fopen(name,"r")) == NULL)
#endif
	{
		if (verb) printf("Unable to open file file '%s'\n",name);
		return NULL;
	}

	/* Figure out how big it is */
	if (fseek(fp, 0, SEEK_END)) {
		if (verb) printf("Seek to EOF '%s' failed'\n",name);
		return NULL;
	}
	ilen = (unsigned long)ftell(fp);

	if (fseek(fp, 0, SEEK_SET)) {
		if (verb) printf("Seek to SOF '%s' failed'\n",name);
		return NULL;
	}

	if ((ibuf = (unsigned char *)malloc(ilen)) == NULL) {
		if (verb) printf("Malloc of buffer for file '%s' failed\n",name);
		return NULL;
	}

	if ((bread = fread(ibuf, 1, ilen, fp)) != ilen) {
		if (verb) printf("Failed to read file '%s', read %lu out of %lu bytes\n",name,bread,ilen);
		free(ibuf);
		return NULL;
	}
	fclose(fp);

	rv = parse_EDR(ibuf, ilen, name, verb);

	free(ibuf);

	return rv;
}

/* ========================================================= */
/* Extract a file from an inno archive. */

/* (It turns out we don't actually need this inno parsing code, */
/*  since the .cab file is uncompressed and contiguous, so */
/*  we can locate and save it directly from Setup.exe.) */

/* Function callbacks for LzmaDec */
static void *SzAlloc(void *p, size_t size) { p = p; return malloc(size); }
static void SzFree(void *p, void *address) { p = p; free(address); }
ISzAlloc alloc = { SzAlloc, SzFree };


/* Version of lzma decode that skips the 4 byte CRC before each 4096 byte block */
static SRes LzmaDecodeX(
Byte *dest, unsigned long *destLen, const Byte *src, unsigned long *srcLen,
ELzmaFinishMode finishMode, ELzmaStatus *status, ISzAlloc *alloc) {
	SRes res;
	CLzmaDec state;
	SizeT slen = *srcLen;
	SizeT dlen = *destLen;
	SizeT len;
	int doneheader = 0;

	if (slen < (4 + 5))
		return SZ_ERROR_INPUT_EOF;

	LzmaDec_Construct(&state);
	res = LzmaDec_Allocate(&state, src + 4, LZMA_PROPS_SIZE, alloc);
	if (res != SZ_OK)
		return res;

	*srcLen = 0;
	*destLen = 0;
	LzmaDec_Init(&state);
	
	for (;slen > 0 && dlen > 0;) {
		SizeT ddlen;

		if (slen < 4)
			return SZ_ERROR_INPUT_EOF;
		if (dlen == 0)
			return SZ_ERROR_OUTPUT_EOF;

		/* Skip the CRC */
		src += 4;
		slen -= 4;
		*srcLen += 4;

		len = 4096;				/* Bytes to next CRC */

		if (doneheader == 0) {		/* Skip the 5 + 8 byte header */
			int inc = 5;
			len -= inc;
			src += inc;
			slen -= inc;
			*srcLen += inc;
			doneheader = 1;
		}

		if (len > slen)
			len = slen;
		ddlen = dlen;

		res = LzmaDec_DecodeToBuf(&state, dest, &ddlen, src, &len, finishMode, status);
		if (res != SZ_OK) {
			LzmaDec_Free(&state, alloc);
			return res;
		}

		src += len;
		slen -= len;
		*srcLen += len;

		dest += ddlen;
		dlen -= ddlen;
		*destLen += ddlen;

		if (*status == LZMA_STATUS_FINISHED_WITH_MARK
		 || *status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) {
			break;
		}
	}
	LzmaDec_Free(&state, alloc);

	return res;
}

/* extract the given file from Setup.exe */

/* Return a list of xfiles, with one entry if successful */
/* print error message and exit othewise. */
static xfile *inno_extract(xfile *xi, char *tfilename, int verb) {
	int i, j;
	unsigned char *ibuf;
	unsigned long ilen;
	char *headerid = "Inno Setup Setup Data (5.3.10)";
	int headerlen = strlen(headerid);
	unsigned int ldrbase = 0;
	unsigned long haddr, srclen;
	unsigned char *d1buf, *d2buf;		/* Decompress buffer */
	unsigned long d1sz, d2sz;
	unsigned char *msibuf;
	unsigned long msisz;
	unsigned long cblocklen;
	SRes rv;
	ELzmaStatus dstat;
	unsigned long ix;
	int filelen = strlen(tfilename);
	unsigned long fileso, filesz;
	char *cp;
	xfile *xf = NULL;

	ibuf = xi[0].buf;
	ilen = xi[0].len;

	/* Search for the start of the loader. All the file offsets */
	/* are relative to this */
	for (i = 0; i < (ilen - 4); i++) {
		if (ibuf[i + 0] == 0x4d
		 && ibuf[i + 1] == 0x5a
		 && ibuf[i + 2] == 0x90
		 && ibuf[i + 3] == 0x00) {
			if (verb > 1) printf("Found archive base 0x%x\n",i);
			ldrbase = i;
			break;
		}
	}
	if (i >= (ilen - 4)) {
		fprintf(stderr,"Failed to locate loader base\n");
		exit(-1);
	}

	/* Search for inno header pattern. */
	for (i = 0; i < (ilen - 64 - 4 - 5 - 4); i++) {
		if (ibuf[i] == headerid[0]
		 && strncmp((char *)ibuf + i, headerid, headerlen) == 0
		 && ibuf[i + 64] != 'I'
		 && ibuf[i + 65] != 'n'
		 && ibuf[i + 66] != 'n'
		 && ibuf[i + 67] != 'o') {
			haddr = i;
		}
	}

	if (verb > 1) printf("Found header at 0x%x\n",i);

	/* Use the last header found (or cound search all found ?) */
	haddr += 64;	/* Skip Inno header */

	/* Next 9 bytes are the compression header */
	haddr += 4;		/* Skip Inno header CRC */
	cblocklen = buf2uint(ibuf + haddr);		/* Compressed block length */

	if (verb > 1) printf("Header compression block length = %lu\n",cblocklen); 

	if ((haddr + cblocklen) > ilen) {
		fprintf(stderr,"Compression block is longer than setup.exe\n");
		exit(-1);
	}

	if (ibuf[haddr + 4] != 1) {
		fprintf(stderr,"Inno header is expected to be compressed\n");
		exit(-1);
	}
	haddr += 5;	/* Skip compression header */

	/* We'r now at the start of the compressed data */

	d1sz = cblocklen * 30;
	if ((d1buf = (unsigned char *)malloc(d1sz)) == NULL) {
		fprintf(stderr,"Failed to allocate decompression buffer\n");
		exit(-1);
	}

	// if (verb) printf("CRC + lzma at %d\n",haddr);

	srclen = ilen - haddr;
	
	/* Decode using CRC + 4096 byte blocks */
	rv = LzmaDecodeX(d1buf, &d1sz, ibuf + haddr, &srclen, LZMA_FINISH_END, &dstat, &alloc);

	if (rv != SZ_OK) {
		fprintf(stderr,"lzma decode failed with rv %d and status %d\n",rv, dstat);
		exit(-1);
	}
	if (verb > 1) printf("Decoded %lu bytes to created %lu bytes of Header output (ratio %.1f)\n",srclen,d1sz,(double)d1sz/srclen);

//	dump_bytes(stdout, "  ", d1buf, d1sz);

	/* - - - - - - - - - - - - - - - - -*/

	/* Skip to the start of the next compression block header */
	haddr += cblocklen;

	if (verb > 1) printf("Expect File Location compressed block to be at 0x%lx\n",haddr);
	if ((haddr + 20) > ilen) {
		fprintf(stderr,"Setup.exe too short for 2nd header block\n");
		exit(-1);
	}

	/* Next 9 bytes are the compression header */
	haddr += 4;		/* Skip Inno header CRC */
	cblocklen = buf2uint(ibuf + haddr);		/* Compressed block length */

	if (verb > 1) printf("File Location compression block length = %lu\n",cblocklen); 

	if ((haddr + cblocklen) > ilen) {
		fprintf(stderr,"2nd compression block is longer than setup.exe\n");
		exit(-1);
	}

	if (ibuf[haddr + 4] != 1) {
		fprintf(stderr,"Inno 2nd header is expected to be compressed\n");
		exit(-1);
	}
	haddr += 5;	/* Skip compression header */

	/* We're now at the start of the compressed data */

	d2sz = cblocklen * 10;
	if ((d2buf = (unsigned char *)malloc(d2sz)) == NULL) {
		fprintf(stderr,"Failed to allocate 2nd block decompression buffer\n");
		exit(-1);
	}

	//if (verb) printf("CRC + lzma at %d\n",haddr);
	srclen = ilen - haddr;
	
	/* Decode using CRC + 4096 byte blocks */
	rv = LzmaDecodeX(d2buf, &d2sz, ibuf + haddr, &srclen, LZMA_FINISH_END, &dstat, &alloc);

	if (rv != SZ_OK) {
		fprintf(stderr,"lzma decode of 2nd block failed with rv %d and status %d\n",rv, dstat);
		exit(-1);
	}
	if (verb > 1) printf("Decoded %lu bytes to created %lu bytes of File Location output (ratio %.1f)\n",srclen,d1sz,(double)d1sz/srclen);

//	dump_bytes(stdout, "  ", d2buf, d2sz);

	if (verb > 1) printf("Searching for file '%s' in Header\n",tfilename);
	for (i = 0; i < (d1sz - 101); i++) {
		if (d1buf[i+4] == tfilename[0]
		 && strncmp((char *)d1buf + 4 + i, tfilename, filelen) == 0
		 && d1buf[i+0] == filelen
		 && d1buf[i+1] == 0
		 && d1buf[i+2] == 0
		 && d1buf[i+3] == 0) {
			if (verb > 1) printf("Found it at 0x%x\n",i);
			break;
		}
	}

	/* Need to skip 8 more strings */
	i += 4 + filelen;
	for (j = 0; j < 8; j++) {
		unsigned long len;
		len = buf2uint(d1buf + i);
		i += 4 + len;
	}
	/* Skip another 40 bytes to location entry index */
	i += 20;

	ix = buf2uint(d1buf + i);

	if (verb > 1) printf("Got file location index %lu at 0x%x\n",ix,i);


	/* Now get the ix file entry information. */
	/* They are in 74 byte structures */
	i = ix * 74;

	if ((i + 74) > d2sz) {
		fprintf(stderr,"File location structure is out of range\n");
		exit(-1);
	}
	
	/* The start offset is at 8 */
	fileso = buf2uint(d2buf + i + 8);

	/* The original and compresses sizes are at 20 and 28 */
	filesz = buf2uint(d2buf + i + 20);		/* Actually 64 bit, but we don't need it */
	if (filesz != buf2uint(d2buf + i + 28)) {
		fprintf(stderr,"Can't handle compressed '%s'\n",tfilename);
		exit(-1);
	}

	if (verb > 1) printf("File '%s' is at offset 0x%lx, length %lu\n",tfilename,fileso,filesz);

	if ((fileso + ldrbase) > ilen
	 || (fileso + ldrbase + filesz-1) > ilen) {
		printf("File '%s' is outsize file '%s' range \n",tfilename,xi[0].name);
		exit(-1);
	}
	/* Sanity check */
	if (ibuf[ldrbase + fileso + 0] != 0xd0
     || ibuf[ldrbase + fileso + 1] != 0xcf
     || ibuf[ldrbase + fileso + 2] != 0x11
     || ibuf[ldrbase + fileso + 3] != 0xe0) {
		fprintf(stderr,"File '%s' doesn't seem to be at location\n",tfilename);
		exit(-1);
	}

	/* Copy to new buffer and free everything */
	msisz = filesz;
	if ((msibuf = (unsigned char *)malloc(msisz)) == NULL) {
		fprintf(stderr,"Failed to allocate file '%s' buffer\n",tfilename);
		exit(-1);
	}
	memmove(msibuf, ibuf + ldrbase + fileso, filesz);

	free(d1buf);
	free(d2buf);

	xf = new_xf(1);

	/* Just return base filename */
	if ((cp = strrchr(tfilename, '/')) == NULL
	 && (cp = strrchr(tfilename, '\\')) == NULL)
		cp = tfilename;
	else
		cp++;

	if ((xf[0].name = strdup(cp)) == NULL) {
		fprintf(stderr,"mmaloc failed on filename\n");
		exit(-1);
	}
	xf[0].buf = msibuf;
	xf[0].len = filesz;

	if (verb) printf("Returning '%s' length %lu from '%s'\n",xf[0].name,xf[0].len, xi[0].name);

	return xf;
}

/* ====================================================== */
/* Extract the .cab file from another file. */

/* It's stored in the .msi uncompressed and contiguous, so we */
/* just need to identify where it is and its length. */
/* (This will work on any file that has the .cab file uncompressed and contiguous) */
static xfile *msi_extract(xfile *xi, char *tname, int verb) {
	int i ;
	xfile *xf = NULL;
	char *fid = "i1d3.xrdevice";		/* File in .cab to look for */
	unsigned long fle = strlen(fid);

	if (verb) printf("Attempting to extract '%s' from '%s'\n",tname,xi[0].name);

	/* Search for a filename in the .cab */
	for (i = 0; i < (xi[0].len - fle - 2); i++) {
		if (xi[0].buf[i + 0] == 0x00
		 && xi[0].buf[i + 1] == fid[0]
		 && strncmp((char *)xi[0].buf + i + 1, fid, fle) == 0) {
			if (verb > 1) printf("Found file name '%s' in '%s' at 0x%x\n",fid,xi[0].name,i);
			break;
		}
	}

	/* Search backwards for .cab signature */
	for (; i >= 0; i--) {
		if (xi[0].buf[i + 0] == 0x4d
		 && xi[0].buf[i + 1] == 0x53
		 && xi[0].buf[i + 2] == 0x43
		 && xi[0].buf[i + 3] == 0x46
		 && xi[0].buf[i + 4] == 0x00
		 && xi[0].buf[i + 5] == 0x00
		 && xi[0].buf[i + 6] == 0x00
		 && xi[0].buf[i + 7] == 0x00) {
			if (verb > 1) printf("Found '%s' at 0x%x\n",tname,i);
			break;
		}
	}

	xf = new_xf(1);

	/* Lookup the .cab size (really 64 bit, but we don't care) */
	xf[0].len = buf2uint(xi[0].buf + i + 8);

	if (verb > 1) printf("'%s' is length %lu\n",tname,xf[0].len);

	if ((xi[0].len - i) < xf[0].len) {
		fprintf(stderr,"Not enough room for .cab file in source\n");
		exit(-1);
	}  

	if ((xf[0].buf = malloc(xf[0].len)) == NULL) {
		fprintf(stderr,"maloc of .cab buffer failed\n");
		exit(-1);
	}
	memmove(xf[0].buf, xi[0].buf + i ,xf[0].len);

	if ((xf[0].name = strdup(tname)) == NULL) {
		fprintf(stderr,"maloc of .cab name failed\n");
		exit(-1);
	}

	if (verb) printf("Extacted '%s' length %lu\n",xf[0].name,xf[0].len);

	return xf;
}


/* ================================================================ */
/* Extract files of a given type from a .cab file */

/* Interface with inflate.c */
/* We use globals for this */

unsigned char *i_buf = NULL;
unsigned long i_len = 0;
unsigned long i_ix = 0;

unsigned char *o_buf = NULL;
unsigned long o_len = 0;
unsigned long o_ix = 0;

/* Interface to inflate */

/* fetch the next 8 bits */
/* if we get 0xffffffff, we are at EOF */
unsigned int inflate_get_byte() {
	if (i_ix < i_len)
		return i_buf[i_ix++];
	return 0xff;
}

/* unget 8 bits */
void inflate_unget_byte() {
	if (i_ix > 0)
		i_ix--;
}

/* Save the decompressed file to the buffer */
int inflate_write_output(unsigned char *buf, unsigned int len) {
	if ((o_ix + len) > o_len) {
		fprintf(stderr,"Uncompressed buffer is unexpectedly large (%lu > %lu)!\n", o_ix + len, o_len);
		return 1;
	}
	memmove(o_buf + o_ix, buf, len);
	o_ix += len;
	return 0;
}

/* Extract all the .edr files from the .cab */
static xfile *cab_extract(xfile *xi, char *text, int verb) {
	int j, k;
	xfile *xf = NULL;
	unsigned char *buf = xi[0].buf;
	unsigned long len = xi[0].len;
	unsigned long off;
	unsigned long filesize, headeroffset, datastart;
	int nofolders, nofiles, flags, comptype;
	unsigned int totubytes;
	int ufiles = 0;
	unsigned char *obuf;
	unsigned long olen;

	if (verb) printf("Attempting to extract '*%s' from '%s'\n",text, xi[0].name);

	/* Check it is a .cab file */
	if (len < 0x2c
	 || buf[0] != 0x4d
     || buf[1] != 0x53
	 || buf[2] != 0x43
	 || buf[3] != 0x46
	 || buf[4] != 0x00
	 || buf[5] != 0x00
	 || buf[6] != 0x00
	 || buf[7] != 0x00) {
		fprintf(stderr,"'%s' is not a .cab file\n",xi[0].name);
		exit(-1);
	}

	filesize = buf2uint(buf + 0x08);
	headeroffset = buf2uint(buf + 0x10);
	nofolders = buf2short(buf + 0x1a);
	nofiles = buf2short(buf + 0x1c);
	flags = buf2short(buf + 0x1e);

	if (filesize != len) {
		fprintf(stderr,"'%s' filesize desn't match\n",xi[0].name);
		exit(-1);
	}
	if (nofolders != 1) {
		fprintf(stderr,"'%s' has more than one folder\n",xi[0].name);
		exit(-1);
	}
	if (flags != 0) {
		fprintf(stderr,"'%s' has non-zero flags\n",xi[0].name);
		exit(-1);
	}

	/* Read the first folders info (assumed flags == 0) */
	datastart = buf2uint(buf + 0x24);
	comptype = buf[0x2a];
	if (comptype!= 1) {
		fprintf(stderr,"'%s' doesn't use MSZip compression\n",xi[0].name);
		exit(-1);
	}

	if (verb > 1) printf(".cab headeroffset = 0x%lx, datastart = 0x%lx, nofiles = %d\n",headeroffset,datastart,nofiles);

	/* Look at each file */
	for (off = headeroffset, k = 0; k < nofiles; k++) {
		unsigned long fsize;		/* Uncompressed size */
		unsigned long foff;
		short ffix;
		char fname[95];
		
		if (off > (len - 80)) {
			fprintf(stderr,"'%s' too short for directory\n",xi[0].name);
			exit(-1);
		}

		fsize = buf2uint(buf + off + 0x00);
		foff  = buf2uint(buf + off + 0x04);
		ffix  = buf2short(buf + off + 0x08);

		strncpy(fname, (char *)buf + off + 0x10, 94);
		fname[94] = '\000';

		if (verb > 1) printf("file %d is '%s' at 0x%lx length %lu\n",k,fname, foff,fsize);

		off += 0x10 + strlen(fname) + 1;		/* Next entry */
	}

	/* Now come the data blocks */
	totubytes = 0;
	for (off = datastart, j = 0; ; j++) {
		unsigned long chsum;
		unsigned long cbytes;
		unsigned long ubytes;

		if (off > (len - 8)) {
			if (verb > 1) printf("Got to end of data blocks at 0x%lx\n",off);
			break;
		}

		chsum = buf2uint(buf + off + 0x00);
		cbytes = buf2short(buf + off + 0x04);
		ubytes = buf2short(buf + off + 0x06);

		if (verb > 1) printf("Compression block %d, cbytes %lu, ubytes %lu\n",j,cbytes,ubytes);

		totubytes += ubytes;

		off += 8 + cbytes;
	}


	if (verb > 1) printf("Total uncompressed bytes = %d\n",totubytes);

	olen = totubytes;
	if ((obuf = malloc(olen)) == NULL) {
		fprintf(stderr,"maloc of uncompressed output buffer failed\n");
		exit(-1);
	}

	o_buf = obuf;
	o_len = olen;
	o_ix = 0;

	for (off = datastart, j = 0; ; j++) {
		unsigned long chsum;
		unsigned long cbytes;
		unsigned long ubytes;

		if (off > (len - 8))
			break;

		chsum = buf2uint(buf + off + 0x00);
		cbytes = buf2short(buf + off + 0x04);
		ubytes = buf2short(buf + off + 0x06);

		i_buf = buf + off + 8;
		i_len = cbytes;
		i_ix = 0;

		/* MSZIP has a two byte signature at the start of each block */
		if (inflate_get_byte() != 0x43 || inflate_get_byte() != 0x4B) {	
			printf(".cab block doesn't start with 2 byte MSZIP signature\n");
			exit (-1);
		}

		if (inflate()) {
			fprintf(stderr, "inflate of '%s' failed at i_ix 0x%lx, o_ix 0x%lx\n",xi[0].name,i_ix,o_ix);
			exit (-1);
		}

		/* The history buffer is meant to survive from one block to the next. */
		/* Not sure what that means, as it seems to work as is... */

		off += 8 + cbytes;
	}

	xf = new_xf(0);

	/* Create a buffer for each file */
	for (ufiles = 0, off = headeroffset, k = 0; k < nofiles; k++) {
		unsigned long fsize;		/* Uncompressed size */
		unsigned long foff;
		short ffix;
		char fname[95], *cp;
		int namelen;
		
		fsize = buf2uint(buf + off + 0x00);
		foff  = buf2uint(buf + off + 0x04);
		ffix  = buf2short(buf + off + 0x08);

		strncpy(fname, (char *)buf + off + 0x10, 94);
		fname[94] = '\000';
		namelen = strlen(fname);

		/* Lop of the junk in the filename */
		if ((cp = strrchr(fname, '.')) != NULL)
			*cp = '\000';
		
		/* See if it's the type of file we want */
		if ((cp = strrchr(fname, '.')) != NULL
		 && strcmp(cp, text) == 0) {
			xfile *xx;

			xx = add_xf(&xf);

			if (foff >= olen || (foff + fsize) > olen) {
				fprintf(stderr,"file '%s' doesn't fit in decomressed buffer\n", fname);
				exit(-1);
			}
			if ((xx->buf = malloc(fsize)) == NULL) {
				fprintf(stderr,"maloc of file '%s' buffer len %lu failed\n",fname,fsize);
				exit(-1);
			}
			xx->len = fsize;
			memmove(xx->buf, obuf + foff, fsize);

			if ((xx->name = strdup(fname)) == NULL) {
				fprintf(stderr,"maloc of .edr name failed\n");
				exit(-1);
			}
			ufiles++;
		}
		off += 0x10 + namelen + 1;		/* Next entry */
	}

	if (verb) printf("Found %d %s files out of %d files in .cab\n",ufiles, text, nofiles);

	return xf;
}



