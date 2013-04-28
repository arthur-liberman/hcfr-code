
 /* OEM archive access library. */
 /* This supports installing OEM data files */

/* 
 * Argyll Color Correction System
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#if defined (NT)
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#endif
#ifdef UNIX
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <ctype.h>
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
#include "aglob.h"
#include "ccss.h"
#include "LzmaDec.h"
#include "oemarch.h"

#undef DEBUG
#undef PLOT_SAMPLES		/* Plot spectral samples */

/* Configured target information */
oem_target oemtargs = {
#ifdef NT
	{	/* Installed files */
		{ "/ColorVision/Spyder2express/CVSpyder.dll",    targ_spyd_pld, file_dllcab },
		{ "/ColorVision/Spyder2pro/CVSpyder.dll",        targ_spyd_pld, file_dllcab },
		{ "/PANTONE COLORVISION/ColorPlus/CVSpyder.dll", targ_spyd_pld, file_dllcab },
		{ "/Datacolor/Spyder4Express/dccmtr.dll",        targ_spyd_cal, file_dllcab },
		{ "/Datacolor/Spyder4Pro/dccmtr.dll",            targ_spyd_cal, file_dllcab },
		{ "/Datacolor/Spyder4Elite/dccmtr.dll",          targ_spyd_cal, file_dllcab },
		{ "/Datacolor/Spyder4TV HD/dccmtr.dll",          targ_spyd_cal, file_dllcab },
		{ "/X-Rite/Devices/i1d3/Calibrations/*.edr",     targ_i1d3_edr, file_data },
		{ NULL }
	},
	{	/* Volume names */
		{ "ColorVision",      targ_spyd_pld | targ_spyd_cal },
		{ "Datacolor",        targ_spyd_pld | targ_spyd_cal },
		{ "i1Profiler",       targ_i1d3_edr },
		{ "ColorMunki Displ", targ_i1d3_edr },
		{ NULL }
	},
	{	/* Archive names */
		{ "/setup/setup.exe",                      targ_spyd_pld },
		{ "/Data/setup.exe",                       targ_spyd_cal },
		{ "/Installer/Setup.exe",                  targ_i1d3_edr },
		{ "/Installer/ColorMunkiDisplaySetup.exe", targ_i1d3_edr },
		{ NULL }
	}
#endif /* NT */
#ifdef __APPLE__
	{	/* Installed files */
		{ "/Applications/Spyder2express 2.2/Spyder2express.app/Contents/MacOSClassic/Spyder.lib", targ_spyd_pld },
		{ "/Applications/Spyder2pro 2.2/Spyder2pro.app/Contents/MacOSClassic/Spyder.lib", targ_spyd_pld },

		{ "/Library/Application Support/X-Rite/Devices/i1d3xrdevice/Contents/Resources/Calibrations/*.edr", targ_i1d3_edr },
		{ NULL }
	},
	{	/* Volume names */
		{ "/Volumes/ColorVision",        targ_spyd_pld | targ_spyd_cal },
		{ "/Volumes/Datacolor",          targ_spyd_pld | targ_spyd_cal },
		{ "/Volumes/i1Profiler",         targ_i1d3_edr },
		{ "/Volumes/ColorMunki Display", targ_i1d3_edr },
		{ NULL }
	},
	{	/* Archive names */
		{ "/setup/setup.exe",                      targ_spyd_pld },
		{ "/Data/setup.exe",                       targ_spyd_cal },
		{ "/Installer/Setup.exe",                  targ_i1d3_edr },
		{ "/Installer/ColorMunkiDisplaySetup.exe", targ_i1d3_edr },
		{ NULL }
	}
#endif /* __APPLE__ */
#ifdef UNIX_X11
	{	/* Installed files */
		{ NULL }
	},
	{	/* Volume names the CDROM may have */
		{ "/media/ColorVision", targ_spyd_pld | targ_spyd_cal },
		{ "/media/Datacolor",   targ_spyd_pld | targ_spyd_cal },
		{ "/media/i1Profiler",	                                targ_i1d3_edr },
		{ "/media/ColorMunki Displ",	                        targ_i1d3_edr },
		{ "/mnt/cdrom",         targ_spyd_pld | targ_spyd_cal | targ_i1d3_edr },
		{ "/mnt/cdrecorder",    targ_spyd_pld | targ_spyd_cal | targ_i1d3_edr },
		{ "/media/cdrom",       targ_spyd_pld | targ_spyd_cal | targ_i1d3_edr },
		{ "/media/cdrecorder",  targ_spyd_pld | targ_spyd_cal | targ_i1d3_edr },
		{ "/cdrom",             targ_spyd_pld | targ_spyd_cal | targ_i1d3_edr },
		{ "/cdrecorder",        targ_spyd_pld | targ_spyd_cal | targ_i1d3_edr },
		{ NULL }
	},
	{	/* Archive names */
		{ "/setup/setup.exe",                      targ_spyd_pld },
		{ "/Data/setup.exe",                       targ_spyd_cal },
		{ "/Installer/Setup.exe",                  targ_i1d3_edr },
		{ "/Installer/ColorMunkiDisplaySetup.exe", targ_i1d3_edr },
		{ NULL }
	}
#endif /* UNIX_X11 */
};

#if defined(__APPLE__)
/* Global: */
char *oemamount_path = NULL;
#endif 

/* Cleanup function for transfer on Apple OS X */
void oem_umiso() {
#if defined(__APPLE__)
	if (oemamount_path != NULL) {
		char sbuf[MAXNAMEL+1 + 100];
		sprintf(sbuf, "umount \"%s\"",oemamount_path);
		system(sbuf);
		sprintf(sbuf, "rmdir \"%s\"",oemamount_path);
		system(sbuf);
	}
#endif /* __APPLE__ */
}

static xfile *locate_volume(int verb);
static xfile *locate_read_archive(xfile *vol, int verb);
static xfile *locate_read_oeminstall(xfile **pxf, int verb);

void classify_file(xfile *xf, int verb);

/* Spyder related archive functions */
int is_vise(xfile *xf);
int is_dll(xfile *xf);
static xfile *vise_extract(xfile **pxf, xfile *xi, char *tfilename, int verb);
static xfile *spyd2pld_extract(xfile **pxf, xfile *dll, int verb);
static xfile *spyd4cal_extract(xfile **pxf, xfile *dll, int verb);
int is_s2pld(xfile *xf);
int is_s4cal(xfile *xf);

/* i1d3 archive related functions */
int is_inno(xfile *xf);
int is_cab(xfile *xf);
static xfile *inno_extract(xfile *xi, char *tfilename, int verb);
static xfile *msi_extract(xfile **pxf, xfile *xi, char *tname, int verb);
static xfile *cab_extract(xfile **pxf, xfile *xi, char *text, int verb);

/* edr to ccss functions */
int is_edr(xfile *xf);
static xfile *edr_convert(xfile **pxf, xfile *xi, int verb);
int is_ccss(xfile *xf);

/* ccmx functions */
int is_ccmx(xfile *xf);

#ifdef DEBUG
static void list_files(char *s, xfile *xf) {
	int i;

	if (xf == NULL)
		printf("%s: no files\n",s);
	else {
		printf("%s:\n",s);
		for (i = 0; xf[i].name != NULL; i++) {
			printf("Got '%s' size %d ftype 0x%x ttype 0x%x\n",xf[i].name,xf[i].len,xf[i].ftype,xf[i].ttype);
//			save_xfile(&xf[i], NULL, "oem/", 1);
		}
	}
}
#endif

/* Given a list of source archives or files, convert them to a list of install files, */
/* or if no files are given look for installed files or files from a CD. */
/* Return NULL if none found. files is deleted. */
xfile *oemarch_get_ifiles(xfile *files, int verb) {
	int i;
	xfile *ofiles, *nfiles = NULL;	/* Ping pong */

#ifdef DEBUG
	list_files("On entry", files);
#endif

	/* Classify the files we've been given */
	if (files != NULL) {
		for (i = 0; files[i].name != NULL; i++) {
			classify_file(&files[i], verb);
		}
#ifdef DEBUG
		list_files("After classification",files);
#endif
	}


	/* See if there are any OEM files installed */
	if (files == NULL) {

		locate_read_oeminstall(&files, verb);

#ifdef DEBUG
		list_files("OEM Installed files", files);
#endif
	}

	if (files == NULL) {
		/* Look for files on a CD */
		xfile *vol;		/* CD volume located */

		if ((vol = locate_volume(verb)) == NULL) {
			return NULL;
		}

		if ((files = locate_read_archive(vol, verb)) == NULL) {
			oem_umiso();
			return NULL;
		}
		oem_umiso();
		del_xf(vol); vol = NULL;

#ifdef DEBUG
		list_files("CD files", files);
#endif
	}

	if (files == NULL)
		return NULL;

	/* Now process any archives - extract dll & cab's */
	for (i = 0; files[i].name != NULL; i++) {
		xfile *arch = files + i;

		/* Preserve & skip non-archives */
		if (files[i].ftype != file_arch) {
			new_add_xf(&nfiles, files[i].name, files[i].buf, files[i].len,
			                                files[i].ftype, files[i].ttype);
			files[i].buf = NULL;	/* We've taken these */
			files[i].len = 0;
			continue;
		}

		/* If this could be spyder 2 PLD pattern: */
		if (arch->ttype & targ_spyd_pld) {
			xfile *dll = NULL;		/* dccmtr.dll */
	
			if (vise_extract(&nfiles, arch, "CVSpyder.dll", verb) != NULL)
				continue;
		}
	
		/* If this could be spyder 4 calibration file: */
		if (arch->ttype & targ_spyd_cal) {
			xfile *dll = NULL;		/* dccmtr.dll */
	
			if (vise_extract(&nfiles, arch, "dccmtr.dll", verb) != NULL)
				continue;
		}
	
		/* If this could be i1d3 .edr files: */
		if (arch->ttype & targ_i1d3_edr) {
			xfile *msi = NULL;		/* .msi */
	
#ifdef NEVER	/* Don't have to do it this way */
			/* Extract .msi from it */
			if ((msi = inno_extract(arch, "{tmp}\\XRD i1d3.msi", verb)) != NULL) {
	
				/* Extract the .cab from it */
				if (msi_extract(&nfiles, msi, "XRD_i1d3.cab", verb) != NULL) {
					del_xf(msi);
					continue;
				}
				del_xf(msi);
			}
#else
			/* Extract the .cab directly from Setup.exe */
			if (msi_extract(&nfiles, arch, "XRD_i1d3.cab", verb) != NULL)
				continue;
			if (msi_extract(&nfiles, arch, "XRD_Manager.cab", verb) != NULL)
				continue;
#endif
		}
		if (verb) printf("Warning: unhandled '%s' discarded\n",arch->name);
	}
	ofiles = files;		/* Swap to new list */
	files = nfiles;
	del_xf(ofiles);
	nfiles = NULL;
	
#ifdef DEBUG
	list_files("After de-archive", files);
#endif

	if (files == NULL)
		return NULL;

	/* Process any dll & cab files - extract individual files */
	for (i = 0; files[i].name != NULL; i++) {
		xfile *dllcab = files + i;

		/* Preserve non-dll/cab */
		if (files[i].ftype != file_dllcab) {
			new_add_xf(&nfiles, files[i].name, files[i].buf, files[i].len,
			                                files[i].ftype, files[i].ttype);
			files[i].buf = NULL;	/* We've taken these */
			files[i].len = 0;
			continue;
		}

		/* If this could be spyder 2 PLD pattern: */
		if (dllcab->ttype & targ_spyd_pld) {
	
			if (spyd2pld_extract(&nfiles, dllcab, verb) != NULL)
				continue;
		}
	
		/* If this could be spyder 4 calibration file: */
		if (dllcab->ttype & targ_spyd_cal) {
	
			if (spyd4cal_extract(&nfiles, dllcab, verb) != NULL)
				continue;
		}
	
		/* If this could be i1d3 .edr files: */
		if (dllcab->ttype & targ_i1d3_edr) {
	
			/* Extract the .edr's from it */
			if (cab_extract(&nfiles, dllcab, ".edr", verb) != NULL)
				continue;
		}
		if (verb) printf("Warning: unhandled '%s' discarded\n",dllcab->name);
	}
	ofiles = files;		/* Swap to new list */
	files = nfiles;
	del_xf(ofiles);
	nfiles = NULL;

#ifdef DEBUG
	list_files("After file extract", files);
#endif

	if (files == NULL)
		return NULL;

	/* Process any .edr files - convert to ccss */
	for (i = 0; files[i].name != NULL; i++) {

		/* Preserve non-edr */
		if (files[i].ftype != file_data
		 || (files[i].ttype & targ_i1d3_edr) == 0
		 || !is_edr(&files[i])
		) {
			new_add_xf(&nfiles, files[i].name, files[i].buf, files[i].len,
			                                files[i].ftype, files[i].ttype);
			files[i].buf = NULL;	/* We've taken these */
			files[i].len = 0;
			continue;
		}
		if (edr_convert(&nfiles, files + i, verb) != NULL)
			continue;

		if (verb) printf("Warning: unhandled '%s' discarded\n",files[i].name);
	}
	ofiles = files;		/* Swap to new list */
	files = nfiles;
	del_xf(ofiles);
	nfiles = NULL;

#ifdef DEBUG
	list_files("Returning", files);
	printf("\n");
#endif

	return files;
}

/* -------------------------------------------- */
/* Determine a file type */

void classify_file(xfile *xf, int verb) {
	if (is_dll(xf)) {
		xf->ftype = file_dllcab;
		xf->ttype &= (targ_spyd_pld | targ_spyd_cal);
		if (verb) printf("'%s' seems to be a .dll file\n",xf->name);
		return;
	}
	if (is_vise(xf)) {
		xf->ftype = file_arch;
		xf->ttype &= (targ_spyd_pld | targ_spyd_cal);
		if (verb) printf("'%s' seems to be a VISE archive\n",xf->name);
		return;
	}
	if (is_inno(xf)) {
		xf->ftype = file_arch;
		xf->ttype &= targ_i1d3_edr;
		if (verb) printf("'%s' seems to be an Inno archive\n",xf->name);
		return;
	}
	if (is_cab(xf)) {
		xf->ftype = file_dllcab;
		xf->ttype &= targ_i1d3_edr;
		if (verb) printf("'%s' seems to be a .cab file\n",xf->name);
		return;
	}
	if (is_edr(xf) || is_ccss(xf)) {
		xf->ftype = file_data;
		xf->ttype &= targ_i1d3_edr;
		if (verb) printf("'%s' seems to be a i1d3 calibration file or .ccss\n",xf->name);
		return;
	}
	if (is_ccmx(xf)) {
		xf->ftype = file_data;
		xf->ttype &= targ_ccmx;
		if (verb) printf("'%s' seems to be a .ccmx\n",xf->name);
		return;
	}
	if (is_s2pld(xf)) {
		xf->ftype = file_data;
		xf->ttype &= targ_spyd_pld;
		if (verb) printf("'%s' seems to be a Spyder 2 PLD file\n",xf->name);
		return;
	}
	if (is_s4cal(xf)) {
		xf->ftype = file_data;
		xf->ttype &= targ_spyd_cal;
		if (verb) printf("'%s' seems to be a Spyder 4 calibration file\n",xf->name);
		return;
	}
	/* Hmm. */
	if (verb) printf("'%s' is unknown\n",xf->name);
	xf->ftype = file_arch | file_dllcab | file_data;
}

/* ============================================================================= */

/* Locate a CD volume. Return NULL if no CD found */
/* free when done */
static xfile *locate_volume(int verb) {
	xfile *xf = NULL;		/* return value */

	if (verb) { printf("Looking for CDROM to install from .. \n"); fflush(stdout); }

#ifdef NT
	{
		char buf[1000];
		char vol_name[MAXNAMEL+1] = { '\000' };
		char drive[50];
		int len, i, j;
	
		len = GetLogicalDriveStrings(1000, buf);
		if (len > 1000)
			error("GetLogicalDriveStrings too large");
		for (i = 0; ;) {		/* For all drives */
			if (buf[i] == '\000')
				break;
			if (GetDriveType(buf+i) == DRIVE_CDROM) {
				DWORD maxvoll, fsflags;
				if (GetVolumeInformation(buf + i, vol_name, MAXNAMEL,
				                     NULL, &maxvoll, &fsflags, NULL, 0) != 0) {
					for (j = 0;;j++) {	/* For all volume names */
						if (oemtargs.volnames[j].path == NULL)
							break;
						if (strcmp(vol_name, oemtargs.volnames[j].path) == 0) {
							strcpy(drive, buf+i);
							drive[2] = '\000';	/* Remove '\' */
							/* Found the instalation CDROM volume name */
							new_add_xf(&xf, drive, NULL, 0, file_vol, oemtargs.volnames[j].ttype);
							if (verb)
								printf("Found Volume '%s' on drive '%s'\n",vol_name,drive);
							break;
						}
					}
					if (oemtargs.volnames[j].path != NULL)		/* Found a match */
						break;
				}
			}
			i += strlen(buf + i) + 1;
		}
	}
#endif /* NT */

#if defined(__APPLE__)
	{
		int j;
		char tname[MAXNAMEL+1] = { '\000' };
	
		for (j = 0; ; j++) {
			if (oemtargs.volnames[j].path == NULL)
				break;
	
			/* In case it's already mounted (previously aborted ?) */
			strcpy(tname, oemtargs.volnames[j].path);
			strcat(tname, "_ISO");
			if (oemamount_path == NULL) {		/* Remember to unmount it */
				if ((oemamount_path = strdup(tname)) == NULL)
					error("Malloc of amount_path failed");
			}
			if (verb) { printf("Checking if '%s' is already mounted .. ",tname); fflush(stdout); }
			if (access(tname, 0) == 0) {
				if (verb) printf("yes\n");
				new_add_xf(&xf, tname, NULL, 0, file_vol, oemtargs.volnames[j].ttype);
				break;
			} else if (verb)
				printf("no\n");
	
			/* Not already mounted. */
			if (access(oemtargs.volnames[j].path, 0) == 0) {
				struct statfs buf;
				char sbuf[MAXNAMEL+1 + 100];
				int sl, rv;
				if (verb) { printf("Mounting ISO partition .. "); fflush(stdout); }
	
				/* but we need the ISO partition */
				/* Locate the mount point */
				if (statfs(oemtargs.volnames[j].path, &buf) != 0) 
					error("\nUnable to locate mount point for '%s' of install CDROM",oemtargs.volnames[j].path); 
				if ((sl = strlen(buf.f_mntfromname)) > 3
				 && buf.f_mntfromname[sl-2] == 's'
				 && buf.f_mntfromname[sl-1] >= '1'
				 && buf.f_mntfromname[sl-1] <= '9')
					 buf.f_mntfromname[sl-2] = '\000';
				else
					error("\nUnable to determine CDROM mount point from '%s'",buf.f_mntfromname);
	
				strcpy(tname, oemtargs.volnames[j].path);
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
				if ((oemamount_path = strdup(tname)) == NULL) {
					oem_umiso();
					error("Malloc of amount_path failed");
				}
				new_add_xf(&xf, tname, NULL, 0, file_vol, oemtargs.volnames[j].ttype);
				break;
			}
		}
	}
#endif /* __APPLE__ */

#if defined(UNIX_X11)
		{
			int j;

			/* See if we can see what we're looking for on one of the volumes */
			/* It would be nice to be able to read the volume name ! */
			for (j = 0;;j++) {
				if (oemtargs.volnames[j].path == NULL)
					break;
				
				if (access(oemtargs.volnames[j].path, 0) == 0) {
					if (verb) printf("found '%s'\n",oemtargs.volnames[j].path);
					new_add_xf(&xf, oemtargs.volnames[j].path, NULL, 0,
					                         file_vol, oemtargs.volnames[j].ttype);
					break;		
				}
			}
		}
#endif	/* UNIX */

	return xf;
}

/* Locate and read the archive on the given volume. */
/* Return NULL if not found */
static xfile *locate_read_archive(xfile *vol, int verb) {
	int j;
	char buf[1000], *ap;
	xfile *xf = NULL;		/* return value */

	if (verb) { printf("Looking for archive on volume '%s' .. ",vol->name); fflush(stdout); }

	strcpy(buf, vol->name);
	ap = buf + strlen(buf);

	for (j = 0; oemtargs.archnames[j].path != NULL; j++) {
		targ_type ttype = vol->ttype & oemtargs.archnames[j].ttype;

		if (ttype == targ_none) {
			continue;
		}

		strcpy(ap, oemtargs.archnames[j].path);
		if (sys_access(buf, 0) == 0) {
			if (verb) printf("found\n");
			new_add_xf(&xf, buf, NULL, 0, file_arch, ttype);
			if (load_xfile(xf, verb))
				error("Failed to load file '%s'",xf->name);
			xf->ftype = file_arch;
			xf->ttype = oemtargs.archnames[j].ttype & vol->ttype;
			break;
		}
	}
	if (verb && oemtargs.archnames[j].path == NULL)
		printf("not found\n");

	return xf;
}

/* Locate and read any OEM install files. */
/* Return NULL if not found */
static xfile *locate_read_oeminstall(xfile **pxf, int verb) {
	int j;
	char tname[1000], *pf, *ap;
	xfile *xf = NULL;		/* return value */
	aglob ag;

	if (verb) { printf("Looking for OEM install files .. \n"); fflush(stdout); }

	tname[0] = '\000';

#ifdef NT
	/* Where the normal instalation goes */
	if ((pf = getenv("PROGRAMFILES")) != NULL)
		strcpy(tname, pf);
	else
		strcpy(tname, "C:/Program Files");
#endif /* NT */

	ap = tname + strlen(tname);

	for (j = 0; oemtargs.instpaths[j].path != NULL; j++) {

		strcpy(ap, oemtargs.instpaths[j].path);

		if (verb) printf("Looking for '%s'\n",tname);

		if (aglob_create(&ag, tname))
			error("Searching for '%s' malloc error",oemtargs.instpaths[j].path);

		for (;;) {
			char *pp;
			if ((pp = aglob_next(&ag)) == NULL)
				break;
			xf = new_add_xf(&xf, pp, NULL, 0, oemtargs.archnames[j].ftype,
			                                   oemtargs.archnames[j].ttype);
			if (load_xfile(xf, verb))
				error("Failed to load file '%s'",xf->name);
		}
		aglob_cleanup(&ag);
	}
	return xf;
}

/* ============================================================================= */
/* A list of files stored in memory. The last entry of the list has name == NULL */

/* return a list with the given number of available entries */
xfile *new_xf(int n) {
	xfile *l;

	if ((l = (xfile *)calloc((n + 1), sizeof(xfile))) == NULL)
		error("new_xf: Failed to allocate xfile structure");

	return l;
}

/* Add an entry to the list. Create the list if it is NULL */
/* Return point to that entry */
xfile *add_xf(xfile **l) {
	int n;
	xfile *ll;

	if (*l == NULL)
		*l = new_xf(0);

	/* Count number of existing entries */
	for (ll = *l, n = 0; ll->name != NULL; ll++, n++)
		;

	if ((*l = (xfile *)realloc(*l, (n+2) * sizeof(xfile))) == NULL)
		error("new_xf: Failed to realloc xfile structure");
	(*l)[n+1].name = NULL;		/* End marker */
	(*l)[n+1].buf = NULL;
	(*l)[n+1].len = 0;
	(*l)[n+1].ftype = 0;
	(*l)[n+1].ttype = 0;

	return &(*l)[n];
}

/* Add an entry and copy details to the list. Create the list if it is NULL */
/* Return point to that entry */
xfile *new_add_xf(xfile **pxf, char *name, unsigned char *buf, unsigned long len,
                                            file_type ftype, targ_type ttype) {
	xfile *xf;
	xf = add_xf(pxf);
	if ((xf->name = strdup(name)) == NULL) 
		error("new_add_xf: strdup failed");
	xf->buf = buf;
	xf->len = len;
	xf->ftype = ftype;
	xf->ttype = ttype;
	return xf;
}

/* Free up a whole list */
void del_xf(xfile *l) {
	int n;

	if (l != NULL) {
		for (n = 0; l[n].name != NULL; n++) {
			if (l[n].name != NULL)
				free(l[n].name);
			if (l[n].buf != NULL)
				free(l[n].buf);
		}
		free(l);
	}
}

/* Allocate and load the given entry. */
/* Return NZ on error */
int load_xfile(xfile *xf, int verb) {
	FILE *fp;
	int i, nfiles;
	unsigned char *ibuf;
	unsigned long ilen, bread;

	if (verb) { printf("Loading file '%s'..",xf->name); fflush(stdout); }

	/* Open up the file for reading */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	if ((fp = fopen(xf->name,"rb")) == NULL)
#else
	if ((fp = fopen(xf->name,"r")) == NULL)
#endif
	{
		if (verb) printf("fopen '%s' failed\n",xf->name);
		return 1;
	}

	/* Figure out how big it is */
	if (fseek(fp, 0, SEEK_END)) {
		if (verb) printf("fseek to EOF of '%s' failed\n",xf->name);
		return 1;
	}

	ilen = (unsigned long)ftell(fp);

	if (verb > 1) printf("Size of file '%s' is %ld bytes\n",xf->name, ilen);

	if (fseek(fp, 0, SEEK_SET)) {
		if (verb) printf("fseek to SOF of file '%s' failed\n",xf->name);
		return 1;
	}

	if ((ibuf = (unsigned char *)malloc(ilen)) == NULL)
		error("\nmalloc buffer for file '%s' failed",xf->name);

	if (verb > 1) printf("(Reading file '%s')\n",xf->name);

	if ((bread = fread(ibuf, 1, ilen, fp)) != ilen) {
		if (verb) printf("Failed to read file '%s', read %ld out of %ld bytes\n",xf->name,bread,ilen);
		return 1;
	}

	fclose(fp);

	if (xf->buf != NULL)
		free(xf->buf);
	xf->buf = ibuf;
	xf->len = ilen;

	if (verb) printf("done\n");

	return 0;
}

/* Save a file to the given sname, or of it is NULL, */
/* to prefix + xf-file name (path is ignored) */
/* Error on failure */
void save_xfile(xfile *xf, char *sname, char *pfx, int verb) {
	FILE *fp;
	char *cp, *fname;
	size_t len;

	if (sname != NULL) {
		fname = sname;
	} else {
		if ((cp = strrchr(xf->name, '/')) == NULL
		 && (cp = strrchr(xf->name, '\\')) == NULL)
			cp = xf->name;
		else
			cp++;

		len = strlen(pfx) + strlen(cp) + 1;
		if ((fname = malloc(len)) == NULL)
			error("malloc fname %d bytes failed",len);
		strcpy(fname, pfx);
		strcat(fname, cp);
	} 

	/* Open up the file for writing */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	if ((fp = fopen(fname,"wb")) == NULL)
#else
	if ((fp = fopen(fname,"w")) == NULL)
#endif
		error("Can't open file '%s' for writing",fname);

	if ((fwrite(xf->buf, 1, xf->len, fp)) != xf->len)
		error("Failed to write file '%s'",fname);

	if (fclose(fp))
		error("Failed to close file '%s' after writing",fname);

	if (verb) printf("Wrote '%s' %ld bytes\n",fname, xf->len);

	if (sname == NULL)
		free(fname);
}

/* ============================================================================= */
/* VISE archive file extraction */

/* structure holding the VISE archive information */
struct _visearch {
	int verb;
	int isvise;				/* Is this an archive ? */
	unsigned int vbase;		/* Base address of archive */
	unsigned char *abuf;	/* Buffer holding archive file */
	unsigned int asize;		/* Nuber of bytes in file */
	unsigned int off;		/* Current offset */
	unsigned char *dbuf;	/* Room for decompressed driver file */
	unsigned int dsize;		/* current write location/size of decompressed file */
	unsigned int dasize;	/* current allocated size of dbuf */

	/* Locate the given name, and set the offset */
	/* to the locatation the compressed data is at */
	/* return nz if not located */
	int (*locate_file)(struct _visearch *p, char *name);

	/* Set the current file offset */
	void (*setoff)(struct _visearch *p, unsigned int off);

	/* Get the next (big endian) 16 bits from the archive */
	/* return 0 if past the end of the file */
	unsigned int (*get16)(struct _visearch *p);

	/* Unget 16 bytes */
	void (*unget16)(struct _visearch *p);

	/* Destroy ourselves */
	void (*del)(struct _visearch *p);

}; typedef struct _visearch visearch;

static void del_arch(visearch *p) {
	if (p->dbuf != NULL)
		free(p->dbuf);
	free(p);
} 

/* Set the current file offset */
static void setoff_arch(visearch *p, unsigned int off) {
	if (off >= p->asize)
		off = p->asize-1;
	p->off = off;
}

/* Locate the given name, and set the offset */
/* to the locatation the compressed data is at */
/* return nz if not located */
static int locate_file_arch(visearch *p, char *name) {
	int i, sl;

	sl = strlen(name);	/* length excluding null */

	if (sl == 0)
		return 1;

	/* Locate the filname with a search */
	for (i = 0x10000; i < (p->asize - sl); i++) {
		if (p->abuf[i] == name[0]) {
			if (strncmp((char *)&p->abuf[i], name, sl) == 0) {
				int sto;
				/* Found it */
				if (p->verb)
					printf("Located driver entry '%s' at offset 0x%x\n",name,i);
				i += sl;
				if (i >= (p->asize - 1))
					return 1;
				sto = p->abuf[i];
				i += (4 + sto) * 4;				/* This is a complete hack. */
				if (i >= (p->asize - 4))
					return 1;
				p->off =  p->abuf[i + 0];
				p->off += (p->abuf[i + 1] << 8);
				p->off += (p->abuf[i + 2] << 16);
				p->off += (p->abuf[i + 3] << 24);
				p->off += p->vbase;
				if (p->off >= p->asize - 10)
					return 1;
				if (p->verb)
					printf("Located driver file '%s' at offset 0x%x\n",name,p->off);
				return 0;
			}
		}
	}
	return 1;
}

/* Get the next (big endian) 16 bits from the archive */
/* return 0 if past the end of the file */
static unsigned int get16_arch(visearch *p) {
	unsigned int val;

	if (p->off >= (p->asize-1)) {
		error("Went past end of archive");
	}

    val =               (0xff & p->abuf[p->off]);
    val = ((val << 8) + (0xff & p->abuf[p->off+1]));
	p->off += 2;
	return val;
}

/* Unget the 16 bits from the archive. */
static void unget16_arch(visearch *p) {

	if (p->off > 1)
		p->off -= 2;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Interface with vinflate.c */

int vinflate(void);

visearch *g_va = NULL;

/* fetch the next 16 bits, big endian */
/* if we get 0xffffffff, we are at EOF */
unsigned int vget_16bits() {
	return get16_arch(g_va);
}

/* unget 16 bits */
void vunget_16bits() {
	unget16_arch(g_va);
}

/* Save the decompressed file to the buffer */
int vwrite_output(unsigned char *buf, unsigned int len) {

	/* If run out of space */
	if ((g_va->dsize + len) >= g_va->dasize) {
		size_t nlen = g_va->dsize + len;

		/* Round up allocation */
		if (nlen <= 1024)
			nlen += 1024;
		else
			nlen += 4096;
		
		if ((g_va->dbuf = realloc(g_va->dbuf, nlen)) == NULL) 
			error("realloc failed on VISE decompress buffer (%d bytes)",nlen);

		g_va->dasize = nlen;
	}
	memmove(g_va->dbuf + g_va->dsize, buf, len);
	g_va->dsize += len;

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return NZ if the file appears to be a VISE archive */
int is_vise(xfile *xf) {
	int i;

	/* See if it looks like a VIZE archive */
	for (i = 0x10000; i < 0x11000 && i < (xf->len-4); i++) {
		if (xf->buf[i + 3] == 'V'
		 && xf->buf[i + 2] == 'I'
		 && xf->buf[i + 1] == 'S'
		 && xf->buf[i + 0] == 'E') {
			return 1;
		}
	}
	return 0;
}

/* create an archive from given memory data */
/* Return NULL if it is not a VISE archive */
visearch *new_arch(unsigned char *abuf, unsigned int asize, int verb) {
	int i, rv = 0;
	visearch *p;

	if ((p = (visearch *)calloc(sizeof(visearch),1)) == NULL)
		error("Malloc failed!");

	p->locate_file = locate_file_arch;
	p->setoff = setoff_arch;
	p->verb = verb;
	p->get16 = get16_arch;
	p->unget16 = unget16_arch;
	p->del = del_arch;
	p->abuf = abuf;
	p->asize = asize;

	/* See if it looks like a VIZE archive */
	for (i = 0x10000; i < 0x11000 && i < (p->asize-4); i++) {
		if (p->abuf[i + 3] == 'V'
		 && p->abuf[i + 2] == 'I'
		 && p->abuf[i + 1] == 'S'
		 && p->abuf[i + 0] == 'E') {
			p->isvise = 1;
			p->vbase = i;
		}
	}

	if (!p->isvise) {
		free(p);
		return NULL;
	}

	return p;
}

/* Extract a VISE file */
/* Return NULL if not found */
static xfile *vise_extract(xfile **pxf, xfile *arch, char *tfilename, int verb) {
	visearch *vi;
	char *cp;
	xfile *xf = NULL;

	if ((vi = new_arch(arch->buf, arch->len, verb)) == NULL) {
		return NULL;
	}
	
	if (verb)
		printf("Input file '%s' is a VISE archive file base 0x%x\n",arch->name,vi->vbase);

	if (vi->locate_file(vi, tfilename)) {
		if (verb) printf("Failed to locate file '%s' in VISE archive",tfilename);
		return NULL;
	}

	g_va = vi;
	if (vinflate()) {
		if (verb) printf("Inflating file '%s' failed",tfilename);
		return NULL;
	}
	g_va = NULL;

	if (verb)
		printf("Located and decompressed file '%s' from VISE archive\n",tfilename);

	xf = add_xf(pxf);

	/* Just return base filename */
	if ((cp = strrchr(tfilename, '/')) == NULL
	 && (cp = strrchr(tfilename, '\\')) == NULL)
		cp = tfilename;
	else
		cp++;

	if ((xf->name = strdup(cp)) == NULL) {
		fprintf(stderr,"strdup failed on filename\n");
		exit(-1);
	}
	xf->buf = vi->dbuf;
	xf->len = vi->dsize;
	xf->ftype = file_dllcab;
	xf->ttype = arch->ttype;

	vi->dbuf = NULL;		/* We've taken buffer */
	vi->dsize = vi->dasize = 0;
	vi->del(vi);

	if (verb) printf("Returning '%s' length %ld from '%s'\n",xf->name, xf->len, arch[0].name);


	return xf;
}

/* ============================================================================= */
/* Spyder 2 PLD pattern extraction */

int is_s2pld(xfile *xf) {

	if (xf->len != 6817)
		return 0;

	if (xf->buf[0] == 0xff
	 && xf->buf[1] == 0x04
	 && xf->buf[2] == 0xb0
	 && xf->buf[3] == 0x0a)
		return 1;
	return 0;
}

static xfile *spyd2pld_extract(xfile **pxf, xfile *dll, int verb) {
	unsigned char *buf;
	unsigned int size;
	unsigned int i, j;
	unsigned int rfsize;
	unsigned char *firmware;
	unsigned int firmwaresize;
	/* First few bytes of the standard Xilinx XCS05XL PLD pattern */
	unsigned char magic[4] = { 0xff, 0x04, 0xb0, 0x0a };
	xfile *xf = NULL;

	buf = dll->buf;
	size = dll->len;

	firmwaresize = 6817;
	rfsize = (firmwaresize + 7) & ~7;

	/* Search for start of PLD pattern */
	for(i = 0; (i + rfsize) < size ;i++) {
		if (buf[i] == magic[0]) {
			for (j = 0; j < 4; j++) {
				if (buf[i + j] != magic[j])
					break;
			}
			if (j >= 4)
				break;		/* found it */
		}
	}
	if ((i + rfsize) >= size) {
		if (verb) printf("Failed to locate Spyder 2 firmware in '%s'\n",dll->name);
		return NULL;
	}

	firmware = buf + i;

	xf = add_xf(pxf);

	if ((xf->name = strdup("spyd2PLD.bin")) == NULL) {
		fprintf(stderr,"strdup failed on filename\n");
		exit(-1);
	}
	if ((xf->buf = malloc(firmwaresize)) == NULL) {
		fprintf(stderr,"malloc failed for Spyder 2 PLD pattern (%d bytes)\n",firmwaresize);
		exit(-1);
	}
	memcpy(xf->buf, firmware, firmwaresize);
	xf->len = firmwaresize;
	xf->ftype = file_data;
	xf->ttype = targ_spyd_pld;

	if (verb) printf("Returning '%s' length %ld from '%s'\n",xf->name, xf->len, dll->name);

	return xf;
}

/* ============================================================================= */
/* Spyder 4 calibration table extraction */

int is_s4cal(xfile *xf) {
	int i;
	unsigned char cal2vals[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f };

	if (xf->len < 8)
		return 0;

	for (i = 0; i < 8 ; i++) {
		if (xf->buf[i] != cal2vals[i])
			return 0;
	}
	return 1;
}

/* Extract the spyder 4 calibration table from the .dll */
/* Return NULL if not found */
static xfile *spyd4cal_extract(xfile **pxf, xfile *dll, int verb) {
	unsigned char *buf, *caldata;
	unsigned int size;
	unsigned int i, j;
	int nocals;
	unsigned int rfsize;
	/* First 41 x 8 bytes are the following pattern: */
	unsigned char cal2vals[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f };
	unsigned char cal2evals[7] = { '3', '3', '3', '7', '1', '0', '-' };
	xfile *xf = NULL;

	buf = dll->buf;
	size = dll->len;

	/* Search for start of the calibration data */
	for(i = 0; i < (size - 41 * 8 - 7) ;i++) {
		if (buf[i] == cal2vals[0]) {
			for (j = 0; j < (41 * 8); j++) {
				if (buf[i + j] != cal2vals[j % 8])
					break;
			}
			if (j >= (41 * 8))
				break;		/* found first set of doubles */
		}
	}
	if (i >= (size - 41 * 8 - 7)) {
		if (verb) printf("Failed to locate Spyder 4 calibration data in '%s'\n",dll->name);
		return NULL;
	}

	caldata = buf + i;

	/* Search for end of the calibration data */
	for(i += (41 * 8); i < (size-7) ;i++) {
		if (buf[i] == cal2evals[0]) {
			for (j = 0; j < 7; j++) {
				if (buf[i + j] != cal2evals[j])
					break;
			}
			if (j >= 7)
				break;		/* found string after calibration data */
		}
	}
	if (i >= (size-7)) {
		if (verb) printf("Failed to locate end of Spyder 4 calibration data in '%s'\n",dll->name);
		return NULL;
	}

	rfsize = buf + i - caldata;

	nocals = rfsize / (41 * 8);
	if (rfsize != (nocals * 41 * 8)) {
		if (verb) printf("Spyder 4 calibration data is not a multiple of %d bytes\n",41 * 8);
		return NULL;
	}

	if (nocals != 6) {
		if (verb) printf("Spyder 4 calibration data has an unexpected number of entries (%d)\n",nocals);
		return NULL;
	}

	xf = add_xf(pxf);

	if ((xf->name = strdup("spyd4cal.bin")) == NULL) {
		fprintf(stderr,"strdup failed on filename\n");
		exit(-1);
	}
	if ((xf->buf = malloc(rfsize)) == NULL) {
		fprintf(stderr,"malloc failed for Spyder 4 calibration data (%d bytes)\n",rfsize);
		exit(-1);
	}
	memcpy(xf->buf, caldata, rfsize);
	xf->len = rfsize;
	xf->ftype = file_data;
	xf->ttype = targ_spyd_cal;

	if (verb) printf("Returning '%s' length %ld from '%s'\n",xf->name, xf->len, dll[0].name);

	return xf;
}

/* ============================================================================= */
/* EDR to CCSS */

static ccss *parse_EDR(unsigned char *buf, unsigned long len, char *name, int verb);
static ccss *read_EDR_file(char *name, int verb);

/* Do conversion. Return NZ on sucess */
static xfile *edr_convert(xfile **pxf, xfile *xi, int verb) {
	xfile *xf = NULL;
	ccss *c;
	
	if (verb) printf("Translating '%s' (%zd bytes)\n",xi->name, xi->len);

	if ((c = parse_EDR(xi->buf, xi->len, xi->name, verb)) == NULL) {
		if (verb) printf("Failed to parse EDR '%s'\n",xi->name);
		return NULL;
	} else {
		char *ccssname;
		char *edrname;
		unsigned char *buf;
		int len;
		if (c->buf_write_ccss(c, &buf, &len)) {
			error("Failed to create ccss for '%s'",xi->name);
		}
		/* Convert .edr file name to .ccss */
	
		/* Get basename of file */
		if ((edrname = strrchr(xi->name, '/')) == NULL)
			edrname = xi->name;
		else
			edrname++;
	
		if ((ccssname = malloc(strlen(edrname) + 10)) == NULL)
			error("Malloc failed on ccssname");
	
		strcpy(ccssname, edrname);
	
		/* Fix extension */
		if (strlen(ccssname) > 4
		 && (strncmp(ccssname + strlen(ccssname) -4, ".edr", 4) == 0
		  || strncmp(ccssname + strlen(ccssname) -4, ".EDR", 4) == 0))
			strcpy(ccssname + strlen(ccssname) -4, ".ccss");
		xf = new_add_xf(pxf, ccssname, buf, len, file_data, targ_i1d3_edr);
		free(ccssname);
	}
	return xf;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Lower level */

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
					fprintf(fp,".");
			}
			fprintf(fp,"\n");
		}
	}
}

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

/* Detect this file as an EDR file */
int is_edr(xfile *xf) {
	
	if (xf->len < 16)
		return 0;

	/* See if it has the right file ID */
	if (strncmp("EDR DATA1", (char *)xf->buf, 16) == 0)
		return 1;
	return 0;
}

/* Given a buffer holding and EDR file, parse it and return a ccss */
/* Return NULL on error */
static ccss *parse_EDR(
	unsigned char *buf,
	unsigned long len,
	char *name,				/* Name of the file */
	int verb				/* Verbose flag */
) {
	int i, j;
	FILE *fp;
	unsigned char *nbuf;
	unsigned long nlen;
	unsigned int off;
	unsigned char *dbuf = NULL;	/* Buffer for spectral samples */
	INR64 edrdate;
	char creatdate[30];
	char dispdesc[256];
	int ttmin, ttmax;	/* Min & max technology strings (inclusive) */
	int *trefmodes;		/* Corresponding refresh mode for tecnology */
	char **tsels;	/* Corresponding UI selection chars */
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
		if ((ttstrings = (char **)malloc(sizeof(char *) * (ttmax - ttmin + 2))) == NULL) {
			if (verb) printf("Malloc failed\n");
			return NULL;
		}
		if ((trefmodes = (int *)malloc(sizeof(int) * (ttmax - ttmin + 2))) == NULL) {
			free(ttstrings);
			if (verb) printf("Malloc failed\n");
			return NULL;
		}
		if ((tsels = (char **)malloc(sizeof(char *) * (ttmax - ttmin + 2))) == NULL) {
			free(trefmodes);
			free(ttstrings);
			if (verb) printf("Malloc failed\n");
			return NULL;
		}

		trefmodes[0]  = 0; tsels[0]  = NULL; ttstrings[0]  = "Color Matching Function";
		trefmodes[1]  = 0; tsels[1]  = NULL; ttstrings[1]  = "Custom";
		trefmodes[2]  = 1; tsels[2]  = "c";  ttstrings[2]  = "CRT";
		trefmodes[3]  = 0; tsels[3]  = "l";  ttstrings[3]  = "LCD CCFL IPS";
		trefmodes[4]  = 0; tsels[4]  = NULL; ttstrings[4]  = "LCD CCFL VPA";
		trefmodes[5]  = 0; tsels[5]  = NULL; ttstrings[5]  = "LCD CCFL TFT";
		trefmodes[6]  = 0; tsels[6]  = "L";  ttstrings[6]  = "LCD CCFL Wide Gamut IPS";
		trefmodes[7]  = 0; tsels[7]  = NULL; ttstrings[7]  = "LCD CCFL Wide Gamut VPA";
		trefmodes[8]  = 0; tsels[8]  = NULL; ttstrings[8]  = "LCD CCFL Wide Gamut TFT";
		trefmodes[9]  = 0; tsels[9]  = "e";  ttstrings[9]  = "LCD White LED IPS";
		trefmodes[10] = 0; tsels[10] = NULL; ttstrings[10] = "LCD White LED VPA";
		trefmodes[11] = 0; tsels[11] = NULL; ttstrings[11] = "LCD White LED TFT";
		trefmodes[12] = 0; tsels[12] = "b";  ttstrings[12] = "LCD RGB LED IPS";
		trefmodes[13] = 0; tsels[13] = NULL; ttstrings[13] = "LCD RGB LED VPA";
		trefmodes[14] = 0; tsels[14] = NULL; ttstrings[14] = "LCD RGB LED TFT";
		trefmodes[15] = 0; tsels[15] = "o";  ttstrings[15] = "LED OLED";
		trefmodes[16] = 0; tsels[16] = "a";  ttstrings[16] = "LED AMOLED";
		trefmodes[17] = 1; tsels[17] = "m";  ttstrings[17] = "Plasma";
		trefmodes[18] = 0; tsels[18] = NULL; ttstrings[18] = "LCD RG Phosphor";	// is this LCD ??
		trefmodes[19] = 1; tsels[19] = NULL; ttstrings[19] = "Projector RGB Filter Wheel";
		trefmodes[20] = 1; tsels[10] = NULL; ttstrings[20] = "Projector RGBW Filter Wheel";
		trefmodes[21] = 1; tsels[21] = NULL; ttstrings[21] = "Projector RGBCMY Filter Wheel";
		trefmodes[22] = 0; tsels[22] = "p";  ttstrings[22] = "Projector";
		trefmodes[23] = 0; tsels[23] = NULL; ttstrings[23] = "Unknown";
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

		/* Read the spectral values for this sample, */
		/* and convert it from W/nm/m^2 to mW/nm/m^2 */
		for (j = 0; j < sp.spec_n; j++) {
			samples[set].spec[j] = IEEE754_64todouble(buf2ord64(buf + j * 8));
			samples[set].spec[j] *= 1000.0;
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
	rv->set_ccss(rv, "X-Rite", creatdate, NULL, dispdesc, ttstrings[ttype], trefmodes[ttype], tsels[ttype], "CS1000", samples, nsets);	

	free(tsels);
	free(trefmodes);
	free(ttstrings);
	free(samples);

	return rv;
}

/* Return nz if this looks like a .ccss */
int is_ccss(xfile *xf) {
	if (xf->len < 7)
		return 0;

	/* See if it has the right file ID */
	if (strncmp("CCSS   ", (char *)xf->buf, 7) == 0)
		return 1;
	return 0;
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
		if (verb) printf("Failed to read file '%s', read %ld out of %ld bytes\n",name,bread,ilen);
		free(ibuf);
		return NULL;
	}
	fclose(fp);

	rv = parse_EDR(ibuf, ilen, name, verb);

	free(ibuf);

	return rv;
}

/* ========================================================= */

/* Return nz if this looks like a .ccmx */
int is_ccmx(xfile *xf) {
	if (xf->len < 7)
		return 0;

	/* See if it has the right file ID */
	if (strncmp("CCMX   ", (char *)xf->buf, 7) == 0)
		return 1;
	return 0;
}

/* ========================================================= */
/* i1d3 Archive support */
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
/* Return NULL if not found or not inno file */
static xfile *inno_extract(xfile *xi, char *tfilename, int verb) {
	int i, j, k;
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

	ibuf = xi->buf;
	ilen = xi->len;

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
		if (verb) printf("Failed to locate loader base\n");
		return NULL;
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
	if (i >= (ilen - 64 - 4 - 5 - 4)) {
		if (verb) printf("Failed to locate header pattern\n");
		return NULL;
	}

	if (verb > 1) printf("Found header at 0x%x\n",i);

	/* Use the last header found (or cound search all found ?) */
	haddr += 64;	/* Skip Inno header */

	/* Next 9 bytes are the compression header */
	haddr += 4;		/* Skip Inno header CRC */
	cblocklen = buf2uint(ibuf + haddr);		/* Compressed block length */

	if (verb > 1) printf("Header compression block length = %ld\n",cblocklen); 

	if ((haddr + cblocklen) > ilen) {
		if (verb) printf("Compression block is longer than setup.exe\n");
		return NULL;
	}

	if (ibuf[haddr + 4] != 1) {
		if (verb) printf("Inno header is expected to be compressed\n");
		return NULL;
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
		if (verb) printf("lzma decode failed with rv %d and status %d\n",rv, dstat);
		free(d1buf);
		return NULL;
	}
	if (verb > 1) printf("Decoded %ld bytes to created %ld bytes of Header output (ratio %.1f)\n",srclen,d1sz,(double)d1sz/srclen);

//	dump_bytes(stdout, "  ", d1buf, d1sz);

	/* - - - - - - - - - - - - - - - - -*/

	/* Skip to the start of the next compression block header */
	haddr += cblocklen;

	if (verb > 1) printf("Expect File Location compressed block to be at 0x%lx\n",haddr);
	if ((haddr + 20) > ilen) {
		if (verb) printf("Setup.exe too short for 2nd header block\n");
		free(d1buf);
		return NULL;
	}

	/* Next 9 bytes are the compression header */
	haddr += 4;		/* Skip Inno header CRC */
	cblocklen = buf2uint(ibuf + haddr);		/* Compressed block length */

	if (verb > 1) printf("File Location compression block length = %ld\n",cblocklen); 

	if ((haddr + cblocklen) > ilen) {
		if (verb) printf("2nd compression block is longer than setup.exe\n");
		free(d1buf);
		return NULL;
	}

	if (ibuf[haddr + 4] != 1) {
		if (verb) printf("Inno 2nd header is expected to be compressed\n");
		free(d1buf);
		return NULL;
	}
	haddr += 5;	/* Skip compression header */

	/* We're now at the start of the compressed data */

	d2sz = cblocklen * 10;
	if ((d2buf = (unsigned char *)malloc(d2sz)) == NULL) {
		if (verb) printf("Failed to allocate 2nd block decompression buffer\n");
		free(d1buf);
		free(d2buf);
		return NULL;
	}

	//if (verb) printf("CRC + lzma at %d\n",haddr);
	srclen = ilen - haddr;
	
	/* Decode using CRC + 4096 byte blocks */
	rv = LzmaDecodeX(d2buf, &d2sz, ibuf + haddr, &srclen, LZMA_FINISH_END, &dstat, &alloc);

	if (rv != SZ_OK) {
		if (verb) printf("lzma decode of 2nd block failed with rv %d and status %d\n",rv, dstat);
		free(d1buf);
		free(d2buf);
		return NULL;
	}
	if (verb > 1) printf("Decoded %ld bytes to created %ld bytes of File Location output (ratio %.1f)\n",srclen,d1sz,(double)d1sz/srclen);

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
	if (i >=  (d1sz - 101)) {
		if (verb) printf("Failed to find file '%s'\n",tfilename);
		free(d1buf);
		free(d2buf);
		return NULL;
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

	if (verb > 1) printf("Got file location index %ld at 0x%x\n",ix,i);


	/* Now get the ix file entry information. */
	/* They are in 74 byte structures */
	i = ix * 74;

	if ((i + 74) > d2sz) {
		if (verb) printf("File location structure is out of range\n");
		free(d1buf);
		free(d2buf);
		return NULL;
	}
	
	/* The start offset is at 8 */
	fileso = buf2uint(d2buf + i + 8);

	/* The original and compresses sizes are at 20 and 28 */
	filesz = buf2uint(d2buf + i + 20);		/* Actually 64 bit, but we don't need it */
	if (filesz != buf2uint(d2buf + i + 28)) {
		if (verb) printf("Can't handle compressed '%s'\n",tfilename);
		free(d1buf);
		free(d2buf);
		return NULL;
	}

	if (verb > 1) printf("File '%s' is at offset 0x%lx, length %ld\n",tfilename,fileso,filesz);

	if ((fileso + ldrbase) > ilen
	 || (fileso + ldrbase + filesz-1) > ilen) {
		if (verb) printf("File '%s' is outsize file '%s' range \n",tfilename,xi->name);
		free(d1buf);
		free(d2buf);
		return NULL;
	}
	/* Sanity check */
	if (ibuf[ldrbase + fileso + 0] != 0xd0
     || ibuf[ldrbase + fileso + 1] != 0xcf
     || ibuf[ldrbase + fileso + 2] != 0x11
     || ibuf[ldrbase + fileso + 3] != 0xe0) {
		if (verb) printf("File '%s' doesn't seem to be at location\n",tfilename);
		free(d1buf);
		free(d2buf);
		return NULL;
	}

	/* Copy to new buffer and free everything */
	msisz = filesz;
	if ((msibuf = (unsigned char *)malloc(msisz)) == NULL) {
		fprintf(stderr,"Failed to allocate file '%s' buffer\n",tfilename);
		free(d1buf);
		free(d2buf);
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

	if ((xf->name = strdup(cp)) == NULL) {
		fprintf(stderr,"strdup failed on filename\n");
		exit(-1);
	}
	xf->buf = msibuf;
	xf->len = filesz;

	if (verb) printf("Returning '%s' length %ld from '%s'\n",xf->name,xf->len, xi->name);

	return xf;
}

/* ====================================================== */
/* i1d3 Archive support */

int is_inno(xfile *xf) {
	int i;

	/* Search for the start of the loader. All the file offsets */
	/* are relative to this */
	for (i = 0; i < (xf->len - 4); i++) {
		if (xf->buf[i + 0] == 0x4d
		 && xf->buf[i + 1] == 0x5a
		 && xf->buf[i + 2] == 0x90
		 && xf->buf[i + 3] == 0x00) {
			break;
		}
	}
	if (i >= (xf->len - 4)) {
		return 0;
	}

	/* Search for an Inno header pattern. */
	for (i = 0; i < (xf->len - 64 - 4 - 5 - 4); i++) {
		if (xf->buf[i + 0] == 'I'
		 && xf->buf[i + 1] == 'n'
		 && xf->buf[i + 2] == 'n'
		 && xf->buf[i + 3] == 'o') {
			return 1;
		}
	}
	return 0;
}

/* Return NZ if this is  .cab file */
int is_cab(xfile *xf) {

	if (xf->len < 8)
		return 0;

	if (xf->buf[0] == 0x4d
	 && xf->buf[1] == 0x53
	 && xf->buf[2] == 0x43
	 && xf->buf[3] == 0x46
	 && xf->buf[4] == 0x00
	 && xf->buf[5] == 0x00
	 && xf->buf[6] == 0x00
	 && xf->buf[7] == 0x00)
		return 1;
	return 0;
}

/* Return NZ if this seems to be a .dll */
int is_dll(xfile *xf) {
	int off;
	int chars;

	if (xf->len < 0x40
	 || xf->buf[0] != 0x4d			/* "MZ" */
	 || xf->buf[1] != 0x5a)
		return 0;

	off =  xf->buf[0x3c]				/* Offset to PE header */
	    + (xf->buf[0x3d] << 8)
	    + (xf->buf[0x3e] << 16)
	    + (xf->buf[0x3f] << 24);

	if (xf->len < off + 0x18)
		return 0;
		
	if (xf->buf[off + 0] != 0x50		/* "PE" */
	 || xf->buf[off + 1] != 0x45
	 || xf->buf[off + 2] != 0x00
	 || xf->buf[off + 3] != 0x00)
		return 0;

	chars =  xf->buf[off + 0x16]			/* PE Characteristics */
	      + (xf->buf[off + 0x17] << 8);

	/*
		0x0002	 = executable (no unresolved refs)
		0x1000	 = .sys driver
	 */

	if (chars & 0x2000)					/* It is a DLL */
		return 1;
	
	return 0;
}

/* Extract the .cab file from another file. */
/* It's stored in the .msi uncompressed and contiguous, so we */
/* just need to identify where it is and its length. */
/* (This will work on any file that has the .cab file uncompressed and contiguous) */
/* Return NULL if not found */
static xfile *msi_extract(xfile **pxf, xfile *xi, char *tname, int verb) {
	int i, j, k;
	xfile *xf = NULL;
	char *fid = "i1d3.xrdevice";		/* File in .cab to look for */
	unsigned long fle = strlen(fid);
	unsigned long cabo, cabsz;
	size_t len;

	if (verb) printf("Attempting to extract '%s' from '%s'\n",tname,xi->name);

	/* Search for a filename in the .cab */
	for (i = 0; i < (xi->len - fle - 2); i++) {
		if (xi->buf[i + 0] == 0x00
		 && xi->buf[i + 1] == fid[0]
		 && strncmp((char *)xi->buf + i + 1, fid, fle) == 0) {
			if (verb > 1) printf("Found file name '%s' in '%s' at 0x%x\n",fid,xi->name,i);
			break;
		}
	}
	if (i >= (xi->len - fle - 2)) {
		if (verb) printf(".cab not found\n");
		return NULL;
	}

	/* Search backwards for .cab signature */
	for (; i >= 0; i--) {
		if (xi->buf[i + 0] == 0x4d
		 && xi->buf[i + 1] == 0x53
		 && xi->buf[i + 2] == 0x43
		 && xi->buf[i + 3] == 0x46
		 && xi->buf[i + 4] == 0x00
		 && xi->buf[i + 5] == 0x00
		 && xi->buf[i + 6] == 0x00
		 && xi->buf[i + 7] == 0x00) {
			if (verb > 1) printf("Found '%s' at 0x%x\n",tname,i);
			break;
		}
	}
	if (i < 0) {
		if (verb) printf(".cab not found\n");
		return NULL;
	}

	/* Lookup the .cab size (really 64 bit, but we don't care) */
	len = buf2uint(xi->buf + i + 8);

	if (verb > 1) printf("'%s' is length %ld\n",tname,len);

	if ((xi->len - i) < len) {
		if (verb) printf("Not enough room for .cab file in source\n");
		return NULL;
	}  

	xf = add_xf(pxf);
	xf->len = len;

	if ((xf->buf = malloc(xf->len)) == NULL) {
		fprintf(stderr,"maloc of .cab buffer failed\n");
		exit(-1);
	}
	memmove(xf->buf, xi->buf + i ,xf->len);

	if ((xf->name = strdup(tname)) == NULL) {
		fprintf(stderr,"maloc of .cab name failed\n");
		exit(-1);
	}

	xf->ftype = file_dllcab;
	xf->ttype = xi->ttype;

	if (verb) printf("Extacted '%s' length %ld\n",xf->name,xf->len);

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

int inflate(void);

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
		fprintf(stderr,"Uncompressed buffer is unexpectedly large (%ld > %ld)!\n", o_ix + len, o_len);
		return 1;
	}
	memmove(o_buf + o_ix, buf, len);
	o_ix += len;
	return 0;
}

/* Extract all the .edr files from the .cab */
/* Returns the last file extracted, or NULL if none found */
static xfile *cab_extract(xfile **pxf, xfile *xi, char *text, int verb) {
	int i, j, k;
	xfile *xf = NULL;
	unsigned char *buf = xi->buf;
	unsigned long len = xi->len;
	unsigned long off;
	unsigned long filesize, headeroffset, datastart;
	int nofolders, nofiles, flags, comptype;
	unsigned int totubytes;
	int ufiles = 0;
	unsigned char *obuf;
	unsigned long olen;

	if (verb) printf("Attempting to extract '*%s' from '%s'\n",text, xi->name);

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
		fprintf(stderr,"'%s' is not a .cab file\n",xi->name);
		exit(-1);
	}

	filesize = buf2uint(buf + 0x08);
	headeroffset = buf2uint(buf + 0x10);
	nofolders = buf2short(buf + 0x1a);
	nofiles = buf2short(buf + 0x1c);
	flags = buf2short(buf + 0x1e);

	if (filesize != len) {
		fprintf(stderr,"'%s' filesize desn't match\n",xi->name);
		exit(-1);
	}
	if (nofolders != 1) {
		fprintf(stderr,"'%s' has more than one folder\n",xi->name);
		exit(-1);
	}
	if (flags != 0) {
		fprintf(stderr,"'%s' has non-zero flags\n",xi->name);
		exit(-1);
	}

	/* Read the first folders info (assumed flags == 0) */
	datastart = buf2uint(buf + 0x24);
	comptype = buf[0x2a];
	if (comptype!= 1) {
		fprintf(stderr,"'%s' doesn't use MSZip compression\n",xi->name);
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
			fprintf(stderr,"'%s' too short for directory\n",xi->name);
			exit(-1);
		}

		fsize = buf2uint(buf + off + 0x00);
		foff  = buf2uint(buf + off + 0x04);
		ffix  = buf2short(buf + off + 0x08);

		strncpy(fname, (char *)buf + off + 0x10, 94);
		fname[94] = '\000';

		if (verb > 1) printf("file %d is '%s' at 0x%lx length %ld\n",k,fname, foff,fsize);

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

		if (verb > 1) printf("Compression block %d, cbytes %ld, ubytes %ld\n",j,cbytes,ubytes);

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
			fprintf(stderr, "inflate of '%s' failed at i_ix 0x%lx, o_ix 0x%lx\n",xi->name,i_ix,o_ix);
			exit (-1);
		}

		/* The history buffer is meant to survive from one block to the next. */
		/* Not sure what that means, as it seems to work as is... */

		off += 8 + cbytes;
	}

	xf = add_xf(pxf);

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

			xf = xx = add_xf(pxf);

			if (foff >= olen || (foff + fsize) > olen) {
				fprintf(stderr,"file '%s' doesn't fit in decomressed buffer\n",fname);
				exit(-1);
			}
			if ((xx->buf = malloc(fsize)) == NULL) {
				fprintf(stderr,"maloc of file '%s' buffer len %ld failed\n",fname,fsize);
				exit(-1);
			}
			xx->len = fsize;
			memmove(xx->buf, obuf + foff, fsize);

			if ((xx->name = strdup(fname)) == NULL) {
				fprintf(stderr,"maloc of .edr name failed\n");
				exit(-1);
			}
			xx->ftype = file_data;
			xx->ttype = targ_i1d3_edr;
			ufiles++;
		}
		off += 0x10 + namelen + 1;		/* Next entry */
	}

	if (verb) printf("Found %d %s files out of %d files in .cab\n",ufiles, text, nofiles);

	return xf;
}


