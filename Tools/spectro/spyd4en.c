

/* 
 * Argyll Color Correction System
 *
 * Datacolor Spyder 4 related software.
 *
 * Author: Graeme W. Gill
 * Date:   17/9/2007
 *
 * Copyright 2006 - 2012, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* This program enables the full range of normal Spyder 4 calibration for Argyll, */
/* by searching for the vendor driver files to locate the */
/* Spyder 4 dccmtr.dll. It then looks for the display spectral data */
/* in the file and transfers it to a spyd4cal.bin so that the user can */
/* select from the standard range of Spyder4 calibrations. */
/* If the vendor instalation files are not in expected places, */
/* then the instalation file (e.g. the MSWindows instalation */
/* dccmtr.dll, or full MSWindows instalation archive Data/setup.exe) */
/* can be provided as a command line argument. Note that even though */
/* the vendor install files are platform dependent, spyd4en is platform */
/* neutral. */

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

/* --------------------------------------------------------- */
/* code for handling the driver or install file */

/* structure holding the archive information */
struct _archive {
	int verb;
	int isvise;				/* Is this an archive ? */
	unsigned int vbase;		/* Base address of archive */
	unsigned char *abuf;	/* Buffer holding archive file */
	unsigned int asize;		/* Nuber of bytes in file */
	unsigned int off;		/* Current offset */
	unsigned char *dbuf;	/* Room for decompressed driver file */
	unsigned int dsize;		/* size of decompressed file */
	unsigned int maxdsize;	/* maximum size allowed for */

	unsigned char *caldata;	/* Pointer to cal data */
	unsigned int caldatasize;	/* Length of cal data */

	/* Locate the given name, and set the offset */
	/* to the locatation the compressed data is at */
	/* return nz if not located */
	int (*locate_file)(struct _archive *p, char *name);

	/* Set the current file offset */
	void (*setoff)(struct _archive *p, unsigned int off);

	/* Get the next (big endian) 16 bits from the archive */
	/* return 0 if past the end of the file */
	unsigned int (*get16)(struct _archive *p);

	/* Unget 16 bytes */
	void (*unget16)(struct _archive *p);

	/* return the address of the cal data and its size */
	void (*get_caldata)(struct _archive *p, unsigned char **buf, unsigned int *size);

	/* Destroy ourselves */
	void (*del)(struct _archive *p);

}; typedef struct _archive archive;

struct _archive *new_arch(char *name, int verb);

/* Code to handle the VISE installer archive file */

void del_arch(archive *p) {

	if (p->abuf != NULL)
		free(p->abuf);
	if (p->dbuf != NULL)
		free(p->dbuf);
	free(p);
} 

/* Set the current file offset */
void setoff_arch(archive *p, unsigned int off) {
	if (off >= p->asize)
		off = p->asize-1;
	p->off = off;
}

/* Locate the given name, and set the offset */
/* to the locatation the compressed data is at */
/* return nz if not located */
int locate_file_arch(archive *p, char *name) {
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
unsigned int get16_arch(archive *p) {
	unsigned int val;

	if (p->off >= (p->asize-1)) {
		error("Went past end of archive");
	}

    val =               (0xff & p->abuf[p->off]);
    val = ((val << 8) + (0xff & p->abuf[p->off+1]));
	p->off += 2;
	return val;
}

/* return the address of the calibration data and its size */
void get_caldata_arch(archive *p, unsigned char **buf, unsigned int *size) {
	*buf = p->caldata;
	*size = p->caldatasize;
}

/* Unget the 16 bits from the archive. */
void unget16_arch(archive *p) {

	if (p->off > 1)
		p->off -= 2;
}

/* --------------------------------------------------------- */
/* Interface with vinflate.c */

int vinflate(void);

archive *g_va = NULL;

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
	if ((g_va->dsize + len) >= g_va->maxdsize) {
		warning("Uncompressed buffer is unexpectedly large (%d > %d)!",
		(g_va->dsize + len), g_va->maxdsize);
		return 1;
	}
	memmove(g_va->dbuf + g_va->dsize, buf, len);
	g_va->dsize += len;
	return 0;
}

/* --------------------------------------------------------- */

/* create an archive by reading the file into memory */
archive *new_arch(char *name, int verb) {
	FILE *ifp;
	unsigned int bread, i;
	archive *p;

	if ((p = (archive *)calloc(sizeof(archive),1)) == NULL)
		error("Malloc failed!");

	p->locate_file = locate_file_arch;
	p->setoff = setoff_arch;
	p->verb = verb;
	p->get16 = get16_arch;
	p->unget16 = unget16_arch;
	p->get_caldata = get_caldata_arch;
	p->del = del_arch;

	/* Open up the file for reading */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	if ((ifp = fopen(name,"rb")) == NULL)
#else
	if ((ifp = fopen(name,"r")) == NULL)
#endif
		error ("Can't open file '%s'",name);

	/* Figure out how big it is */
	if (fseek(ifp, 0, SEEK_END))
		error ("Seek to EOF failed");
	p->asize = (unsigned long)ftell(ifp);

	if (verb)
		printf("Size of input file '%s' is %d bytes\n",name, p->asize);

	if (fseek(ifp, 0, SEEK_SET))
		error ("Seek to SOF failed");

	if ((p->abuf = (unsigned char *)malloc(p->asize)) == NULL)
		error("Failed to allocate buffer for VISE archive");

	if ((bread = fread(p->abuf, 1, p->asize, ifp)) != p->asize)
		error("Failed to read VISE archive into buffer, read %d out of %d",bread,p->asize);
	
	fclose(ifp);

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

	if (p->isvise) {		/* Need to locate driver file and decompress it */
		char *dname = "dccmtr.dll";

		if (verb)
			printf("Input file '%s' is a VISE archive file base 0x%x\n",name,p->vbase);
		p->dsize = 0;
		p->maxdsize = 650000;
		if ((p->dbuf = (unsigned char *)malloc(p->asize)) == NULL)
			error("Failed to allocate buffer for VISE archive");

		if (p->locate_file(p,dname))
			error("Failed to locate file '%s'\n",dname);

		g_va = p;
		if (vinflate()) {
			error("Inflating file '%s' failed",dname);
		}
		if (verb)
			printf("Located and decompressed driver file '%s' from archive\n",dname);

		/* Don't need this now */
		if (p->abuf != NULL)
			free(p->abuf);
		p->abuf = NULL;
		
	} else {
		if (verb)
			printf("Input file '%s' is NOT a VISE archive file - assume it's a .dll\n",name);
	}

	/* Locate the Spyder spectral calibration information */
	{
		unsigned char *buf;
		unsigned int size;
		unsigned int i, j;
		int nocals;
		unsigned int rfsize;
		/* First 41 x 8 bytes are the following pattern: */
		unsigned char cal2vals[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f };
		unsigned char cal2evals[7] = { '3', '3', '3', '7', '1', '0', '-' };

		if (p->isvise) {
			buf = p->dbuf;
			size = p->dsize;
		} else {
			buf = p->abuf;
			size = p->asize;
		}

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
		if (i >= (size - 41 * 8 - 7))
			error("Failed to locate Spyder 4 calibration data");

		p->caldata = buf + i;

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
		if (i >= (size-7))
			error("Failed to locate end of Spyder 4 calibration data");

		rfsize = buf + i - p->caldata;

		nocals = rfsize / (41 * 8);
		if (rfsize != (nocals * 41 * 8))
			error("Spyder 4 calibration data is not a multiple of %d bytes",41 * 8);

		if (nocals != 6)
			error("Spyder 4 calibration data has an unexpected number of entries (%d)",nocals);

		p->caldatasize = rfsize;

		if (verb)
			printf("Located calibration data file\n");
	} 
	return p;
}

/* --------------------------------------------------------- */

/* Write out a binary file */
/* return nz if something goes wrong */
int write_bin(char *name, unsigned char *fbuf, unsigned int fsize, int verb) {
	FILE *fp;
	unsigned int rfsize;	/* rounded fsize */

	if (verb)
		printf("About to write binary '%s'\n",name);

	/* open executable file */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	if ((fp = fopen(name,"wb")) == NULL)
#else
	if ((fp = fopen(name,"w")) == NULL)
#endif
	{
		warning("Unable to open binary '%s' for writing",name);
		return 1;
	}

	rfsize = (fsize + 7) & ~7;
	if (fwrite(fbuf, 1, rfsize, fp) != rfsize) {
		error("Failed to write to binary file '%s'\n",name);
		fclose(fp);
		return 1;
	}

	if (fclose(fp) != 0) {
		warning("Failed to close binary file '%s'",name);
		return 1;
	}

	if (verb)
		printf("Binary '%s' sucessfully written\n",name);
	return 0;
}


/* --------------------------------------------------------- */

void usage(void) {
	fprintf(stderr,"Transfer Spyder 2 PLD pattern from file, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL Version 2 or later\n");
	fprintf(stderr,"usage: spyd2en [-v] infile\n");
	fprintf(stderr," -v              Verbose\n");
	fprintf(stderr," -S d            Specify the install scope u = user (def.), l = local system]\n");
	fprintf(stderr," infile          Binary driver file to search\n");
	fprintf(stderr,"                 Creates spyd2PLD.bin\n");
	exit(1);
}

/* Cleanup function for transfer on Apple OS X */
void umiso() {
#if defined(UNIX) && defined(__APPLE__)
	system("umount /Volumes/Datacolor_ISO");
	system("rmdir /Volumes/Datacolor_ISO");
#endif /* UNIX && __APPLE__ */
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char in_name[MAXNAMEL+1] = "\000" ;
	char *bin_name = "color/spyd4cal.bin";
	char **bin_paths = NULL;
	int no_paths = 0;
	xdg_scope scope = xdg_user;
	int verb = 0;
	int amount = 0;			/* We mounted the CDROM */
	archive *va;
	unsigned char *fbuf;
	unsigned int fsize;
	
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

	if (fa < argc) {
		strncpy(in_name,argv[fa],MAXNAMEL); in_name[MAXNAMEL] = '\000';
	} else {

#ifdef NT
		{
			int i;
			char *pf = NULL;

			char *paths[] = {		/* Typical instalation locations ??? */
									/* These are almost certainly wrong. */
				"\\Datacolor\\Spyder4Express\\dccmtr.dll",
				"\\Datacolor\\Spyder4pro\\dccmtr.dll",
				"\\Datacolor\\Spyder4elite\\dccmtr.dll",
				""
			};

			char *vols[] = {		/* Typical volume names the CDROM may have */
				"Datacolor",
				""
			};

			for (i = 0;;i++) {
				if (paths[i][0] == '\000')
					break;

				/* Where the normal instalation goes */
				if ((pf = getenv("PROGRAMFILES")) != NULL)
					strcpy(in_name, pf);
				else
					strcpy(in_name, "C:\\Program Files");

				strcat(in_name, paths[i]);
				if (verb) {
					printf("Looking for MSWindows install at '%s' .. ",in_name);
					fflush(stdout);
				}
				if (_access(in_name, 0) == 0) {
					if (verb) printf("found\n");
					break;
				}
				if (verb) printf("not found\n");
			}
			/* Not accessible, so look for the install CDROM */
			if (paths[i][0] == '\000') {
				char buf[400];
				char vol_name[MAXNAMEL+1] = { '\000' };
				int len, i, j;

				in_name[0] = '\000';
				if (verb) { printf("Looking for Spyder 2 install CDROM .. "); fflush(stdout); }

				len = GetLogicalDriveStrings(400, buf);
				if (len > 400)
					error("GetLogicalDriveStrings too large");
				for (i = 0; ;) {
					if (buf[i] == '\000')
						break;
					if (GetDriveType(buf+i) == DRIVE_CDROM) {
						DWORD maxvoll, fsflags;
						if (GetVolumeInformation(buf + i, vol_name, MAXNAMEL,
						                     NULL, &maxvoll, &fsflags, NULL, 0) != 0) {
							for(j = 0;;j++) {
								if (vols[j][0] == '\000'
						  		 || strcmp(vol_name, vols[j]) == 0) {
									break;
								}
							}
							if (vols[j][0] != '\000') {
								/* Found the instalation CDROM */
								strcpy(in_name, buf+i);
								strcat(in_name, "Data\\setup.exe");
								if (verb)
									printf("found Vol '%s' file '%s'\n",vols[j], in_name);
								break;
							}
						}
					}
					i += strlen(buf + i) + 1;
				}
				if (buf[i] == '\000' && verb)
					printf("not found\n");
			}
		}
#endif /* NT */

#if defined(UNIX) && defined(__APPLE__)
		{
			/* Look for the install CDROM, since we don't really know */
			/* how the OS X driver software is arranged. */

			in_name[0] = '\000';
			if (verb) printf("not found\n");
			if (verb) { printf("Looking for Spyder 4 install CDROM .. "); fflush(stdout); }

			/* In case it's already mounted (abort of spyd2en ?) */
			if (access("/Volumes/Datacolor_ISO/Data/Setup.exe", 0) == 0) {
				strcpy(in_name, "/Volumes/Datacolor_ISO/Data/Setup.exe");
				if (verb) printf("found\n");
			} else if (access("/Volumes/Datacolor", 0) == 0) {
				struct statfs buf;
				char sbuf[400];
				int sl, rv;
				if (verb) { printf("found\nMounting ISO partition .. "); fflush(stdout); }

				/* but we need the ISO partition */
				/* Locate the mount point */
				if (statfs("/Volumes/Datacolor", &buf) != 0) 
					error("\nUnable to locate mount point of install CDROM"); 
				if ((sl = strlen(buf.f_mntfromname)) > 3
				 && buf.f_mntfromname[sl-2] == 's'
				 && buf.f_mntfromname[sl-1] >= '1'
				 && buf.f_mntfromname[sl-1] <= '9')
					 buf.f_mntfromname[sl-2] = '\000';
				else
					error("\nUnable to determine CDROM mount point from '%s'",buf.f_mntfromname);

				if ((rv = system("mkdir /Volumes/Datacolor_ISO")) != 0)
					error("\nCreating ISO9660 volume of CDROM failed with %d",rv); 
				sprintf(sbuf, "mount_cd9660 %s /Volumes/Datacolor_ISO", buf.f_mntfromname);
				if ((rv = system(sbuf)) != 0) {
					system("rmdir /Volumes/Datacolor_ISO");
					error("\nMounting ISO9660 volume of CDROM failed with %d",rv); 
				}
				if (verb)
					printf("done\n");
				strcpy(in_name, "/Volumes/Datacolor_ISO/Data/setup.exe");
				amount = 1;		/* Remember to unmount */
			}
		}
#endif /* UNIX && __APPLE__ */

#if defined(UNIX) && !defined(__APPLE__)
		{
			int i;

			/* Since there is no vendor driver instalation for Linux, we */
			/* must look for the CDROM. */
			char *vols[] = {		/* Typical volumes the CDROM could be mounted under */
				"/media/Datacolor",
				"/mnt/cdrom",
				"/mnt/cdrecorder",
				"/media/cdrom",
				"/media/cdrecorder",
				"/cdrom",
				"/cdrecorder",
				""
			};

			if (verb)
				if (verb) { printf("Looking for Spyder 2 install CDROM .. "); fflush(stdout); }

			/* See if we can see what we're looking for on one of the volumes */
			/* It would be nice to be able to read the volume name ! */
			for(i = 0;;i++) {
				if (vols[i][0] == '\000')
					break;
				strcpy(in_name, vols[i]);
				strcat(in_name, "/Data/setup.exe");
				if (access(in_name, 0) == 0) {
					if (verb) printf("found\n");
					break;		
				}
			}
			if (vols[i][0] == '\000') {
				if (verb) printf("not found\n");
				in_name[0] = '\000';
			}
		}
#endif

	}
	if (in_name[0] == '\000') {
		if(amount) umiso();
		error("No CD present, or install file to process");
	}

	/* Locate the cal data in the driver or install file */
	if ((va = new_arch(in_name, verb)) == NULL) {
		if(amount) umiso();
		error("Failed to create archive object from '%s'",in_name);
	}

	if (amount) umiso();
 	
	/* Get path. This may drop uid/gid if we are su */
	if ((no_paths = xdg_bds(NULL, &bin_paths, xdg_data, xdg_write, scope, bin_name)) < 1) {
		error("Failed to find/create XDG_DATA path");
	}

	get_caldata_arch(va, &fbuf, &fsize);
	
	if (verb)
		printf("Firmware is being save to '%s'\n",bin_paths[0]);

	/* Create a binary file containing the cal data */
	/* that drivers for the Spyder 2 can use */
	write_bin(bin_paths[0], fbuf, fsize, verb);
	xdg_free(bin_paths, no_paths);

	va->del(va);

	return 0;
}

/* ------------------------------------------------------- */

