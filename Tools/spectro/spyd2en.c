

/* 
 * Argyll Color Correction System
 *
 * ColorVision Spyder 2 related software.
 *
 * Author: Graeme W. Gill
 * Date:   17/9/2007
 *
 * Copyright 2006 - 2007, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* This program enables the Spyder 2 instrument for Argyll, */
/* by searching for the vendor driver files to locate the */
/* Spyder 2 PLD firmare pattern. If it is found, it transfers it to */
/* a spyd2PLD.bin so that the user can use the Argyll driver with their */
/* device. If the vendor instalation files are not in expected places, */
/* then the instalation file (e.g. the MSWindows instalation */
/* CVSpyder.dll, or full MSWindows instalation archive setup/setup.exe) */
/* can be provided as a command line argument. Note that even though */
/* the vendor install files are platform dependent, spyd2en is platform */
/* neutral. */

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

/* --------------------------------------------------------- */
/* code for handling the driver or install file */

/* structure holding the archive information */
struct _archive {
	int verb;
	int isvise;				/* Is this an archive ? */
	unsigned char *abuf;	/* Buffer holding archive file */
	unsigned int asize;		/* Nuber of bytes in file */
	unsigned int off;		/* Current offset */
	unsigned char *dbuf;	/* Room for decompressed driver file */
	unsigned int dsize;		/* size of decompressed file */
	unsigned int maxdsize;	/* maximum size allowed for */

	unsigned char *firmware;	/* Pointer to firmware */
	unsigned int firmwaresize;

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

	/* return the address of the firmware and its size */
	void (*get_firmware)(struct _archive *p, unsigned char **buf, unsigned int *size);

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
				p->off += 0x10000;
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

/* return the address of the firmware and its size */
void get_firmware_arch(archive *p, unsigned char **buf, unsigned int *size) {
	*buf = p->firmware;
	*size = p->firmwaresize;
}

/* Unget the 16 bits from the archive. */
void unget16_arch(archive *p) {

	if (p->off > 1)
		p->off -= 2;
}

/* --------------------------------------------------------- */
/* Interface with vinflate.c */

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
	unsigned int bread;
	int rv = 0;
	archive *p;

	if ((p = (archive *)calloc(sizeof(archive),1)) == NULL)
		error("Malloc failed!");

	p->locate_file = locate_file_arch;
	p->setoff = setoff_arch;
	p->verb = verb;
	p->get16 = get16_arch;
	p->unget16 = unget16_arch;
	p->get_firmware = get_firmware_arch;
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

	if (p->asize >= 0x10100
	 && p->abuf[0x10003] == 'V'
	 && p->abuf[0x10002] == 'I'
	 && p->abuf[0x10001] == 'S'
	 && p->abuf[0x10000] == 'E')
		p->isvise = 1;

	if (p->isvise) {		/* Need to locate driver file and decompress it */
		char *dname = "CVSpyder.dll";

		if (verb)
			printf("Input file '%s' is a VISE archive file\n",name);
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
		
	}

	/* Locate the Spyder firmware image */
	{
		unsigned char *buf;
		unsigned int size;
		unsigned int i, j;
		unsigned int rfsize;
		/* First few bytes of the standard Xilinx XCS05XL PLD pattern */
		unsigned char magic[4] = { 0xff, 0x04, 0xb0, 0x0a };

		p->firmwaresize = 6817;
		rfsize = (p->firmwaresize + 7) & ~7;

		if (p->isvise) {
			buf = p->dbuf;
			size = p->dsize;
		} else {
			buf = p->abuf;
			size = p->asize;
		}

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
		if ((i + rfsize) >= size)
			error("Failed to locate Spyder 2 firmware");

		if (verb)
			printf("Located firmware in driver file\n");
		p->firmware = buf + i;
	} 
	return p;
}

/* --------------------------------------------------------- */

char *location_magic = "XCS05XL firmware pattern";

/* Create a firmware header source file */
/* return nz if something goes wrong */
int write_header(char *name, unsigned char *fbuf, unsigned int fsize, int verb) {
	FILE *ofp;
	int i, j;

	if (verb)
		printf("About to transfer firmware to source file '%s'\n",name);

	/* open output file */
	if ((ofp = fopen(name,"w")) == NULL) {
		warning("Can't open file '%s'",name);
		return 1;
	}


	fprintf(ofp, "\n");
	fprintf(ofp, "/* Spyder 2 Colorimeter Xilinx  XCS05XL firmware pattern, */\n");
	fprintf(ofp, "/* transfered from the Spyder 2 vendor instalation by spyden. */\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "/* This firmware pattern is (presumably) Copyright Datacolor Inc., */\n");
	fprintf(ofp, "/* and cannot be distributed without their permision. */\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "static unsigned int pld_size = %d;  /* Number of bytes to download */\n",fsize);
	fsize = (fsize + 7) & ~7;		/* Round up to 4 bytes */
	fprintf(ofp, "static unsigned char pld_space = %d;\n",fsize);
	fprintf(ofp, "static unsigned char pld_bytes[%d] = {\n",fsize);

	fprintf(ofp, "	");
	for (i = 0, j = 0; i < fsize; i++, j++) {
		if (j == 8) {
			j = 0;
			if (i < (fsize-1))
				fprintf(ofp, ",\n	");
			else
				fprintf(ofp, "\n	");
		}
		if (j > 0)
			fprintf(ofp, ", ");
		fprintf(ofp, "0x%02x",fbuf[i]);
	}
	fprintf(ofp, "\n};\n");
	fprintf(ofp, "\n");


	if (fclose(ofp) != 0) {
		warning("Failed to close output file '%s'",name);
		return 1;
	}
	return 0;
}

/* --------------------------------------------------------- */

/* Patch an executable */
/* return nz if something goes wrong */
int patch_exe(char *name, unsigned char *fbuf, unsigned int fsize, int verb) {
	FILE *fp;
	unsigned char *buf = NULL;
	unsigned int size;
	unsigned int rfsize;	/* rounded fsize */
	int lms;				/* Location magic string size */
	int i, j;

	if (verb)
		printf("About to patch executable '%s'\n",name);

	/* open executable file */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	if ((fp = fopen(name,"r+b")) == NULL)
#else
	if ((fp = fopen(name,"r+")) == NULL)
#endif
	{
		warning("Unable to open executable '%s' for patching",name);
		return 1;
	}

	/* Figure out how big it is */
	if (fseek(fp, 0, SEEK_END)) {
		warning("Seek to EOF failed on '%s", name);
		fclose(fp);
		return 1;
	}
	size = (unsigned long)ftell(fp);

	if (fseek(fp, 0, SEEK_SET)) {
		warning("Seek to SOF failed on '%s'", name);
		fclose(fp);
		return 1;
	}

	if ((buf = (unsigned char *)malloc(size)) == NULL) {
		error("Failed to allocate buffer for file '%s'",name);
		fclose(fp);
		return 1;
	}

	if (fread(buf, 1, size, fp) != size) {
		error("Failed to read patch file '%s'\n",name);
		free(buf);
		fclose(fp);
		return 1;
	}

	if (fseek(fp, 0, SEEK_SET)) {
		warning("Seek to SOF failed on '%s'", name);
		free(buf);
		fclose(fp);
		return 1;
	}

	/* Now, locate the space allowed for the pld pattern */

	lms = strlen(location_magic);

	/* Search for the location magic string */
	for (i = 0; i < (size - lms) ;i++) {
	
		if (buf[i] == location_magic[0]) {
			for (j = 0; j < lms; j++) {
				if (buf[i + j] != location_magic[j])
					break;		/* Got it */
			}
			if (j >= lms)
				break;		/* found it */
		}
	}
	if (i >= (size - lms)) {
		warning("Failed to patch '%s' - already patched ?",name);
		free(buf);
		fclose(fp);
		return 1;
	}

	rfsize = (fsize + 7) & ~7;

	/* Copy the firmware into place */
	for (j = 0; j < rfsize; j++) {
		buf[i + j] = fbuf[j];
	} 

	/* Locate the size */
	for (j = i; j > (i - 16); j-- ) {
		if (buf[j] == 0x44 || buf[j] == 0x11)
			break;
	}
	if (j <= (i = 16)) {
		warning("Failed to locate size in '%s'",name);
		free(buf);
		fclose(fp);
		return 1;
	}
	if (buf[j] == 0x44) { 	/* big endian */
		buf[j]   = ((fsize >> 0)  & 0xff);
		buf[j-1] = ((fsize >> 8)  & 0xff);
		buf[j-2] = ((fsize >> 16) & 0xff);
		buf[j-3] = ((fsize >> 24) & 0xff);
	} else {				/* Little endian */
		buf[j]   = ((fsize >> 24) & 0xff);
		buf[j-1] = ((fsize >> 16) & 0xff);
		buf[j-2] = ((fsize >> 8)  & 0xff);
		buf[j-3] = ((fsize >> 0)  & 0xff);
	}
	
	if (fwrite(buf, 1, size, fp) != size) {
		error("Failed to write patch file '%s'\n",name);
		free(buf);
		fclose(fp);
		return 1;
	}

	if (fclose(fp) != 0) {
		warning("Failed to close executable file '%s'",name);
		free(buf);
		return 1;
	}
	free(buf);

	if (verb)
		printf("Executable '%s' sucessfuly patches\n",name);
	return 0;
}
/* --------------------------------------------------------- */

/* Write out a binary file */
/* return nz if something goes wrong */
int write_bin(char *name, unsigned char *fbuf, unsigned int fsize, int verb) {
	FILE *fp;
	unsigned int rfsize;	/* rounded fsize */
	int i, j;

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
	system("umount /Volumes/ColorVision_ISO");
	system("rmdir /Volumes/ColorVision_ISO");
#endif /* UNIX && __APPLE__ */
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char in_name[MAXNAMEL+1] = "\000" ;
	char patch_name[MAXNAMEL+1] = "\000" ;
	char *header_name = "spyd2PLD.h";
	char *bin_name = "color/spyd2PLD.bin";
	char **bin_paths = NULL;
	int no_paths = 0;
	xdg_scope scope = xdg_user;
	int verb = 0;
	int header = 0;
	int patch = 0;
	int amount = 0;			/* We mounted the CDROM */
	archive *va;
	unsigned char *fbuf;
	unsigned int fsize;
	int i;
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

			/* Creat header */
			else if (argv[fa][1] == 'h' || argv[fa][1] == 'H') {
				header = 1;
			}

			/* Patch an executable */
			else if (argv[fa][1] == 'p' || argv[fa][1] == 'P') {
				patch = 1;
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

			char *paths[] = {		/* Typical instalation locations */
				"\\ColorVision\\Spyder2express\\CVSpyder.dll",
				"\\PANTONE COLORVISION\\ColorPlus\\CVSpyder.dll",
				""
			};

			char *vols[] = {		/* Typical volume names the CDROM may have */
				"ColorVision",
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
								strcat(in_name, "setup\\setup.exe");
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
			/* (Note we're only looking for the entry level package. */
			/* We should perhaps search eveything starting with "Spyder" ?) */
			strcpy(in_name, "/Applications/Spyder2express 2.2/Spyder2express.app/Contents/MacOSClassic/Spyder.lib");
			if (verb) {
				printf("Looking for Apple OS X install at\n '%s' .. ",in_name);
				fflush(stdout);
			}
			if (access(in_name, 0) == 0) {
				if (verb) printf("found\n");
			} else {		/* Not accessible, so look for the install CDROM */

				in_name[0] = '\000';
				if (verb) printf("not found\n");
				if (verb) { printf("Looking for Spyder 2 install CDROM .. "); fflush(stdout); }

				/* In case it's already mounted (abort of spyd2en ?) */
				if (access("/Volumes/ColorVision_ISO/setup/setup.exe", 0) == 0) {
					strcpy(in_name, "/Volumes/ColorVision_ISO/setup/setup.exe");
					if (verb) printf("found\n");
				} else if (access("/Volumes/ColorVision", 0) == 0) {
					struct statfs buf;
					char sbuf[400];
					int sl, rv;
					if (verb) { printf("found\nMounting ISO partition .. "); fflush(stdout); }

					/* but we need the ISO partition */
					/* Locate the mount point */
					if (statfs("/Volumes/ColorVision", &buf) != 0) 
						error("\nUnable to locate mount point of install CDROM"); 
					if ((sl = strlen(buf.f_mntfromname)) > 3
					 && buf.f_mntfromname[sl-2] == 's'
					 && buf.f_mntfromname[sl-1] >= '1'
					 && buf.f_mntfromname[sl-1] <= '9')
						 buf.f_mntfromname[sl-2] = '\000';
					else
						error("\nUnable to determine CDROM mount point from '%s'",buf.f_mntfromname);

					if ((rv = system("mkdir /Volumes/ColorVision_ISO")) != 0)
						error("\nCreating ISO9660 volume of CDROM failed with %d",rv); 
					sprintf(sbuf, "mount_cd9660 %s /Volumes/ColorVision_ISO", buf.f_mntfromname);
					if ((rv = system(sbuf)) != 0) {
						system("rmdir /Volumes/ColorVision_ISO");
						error("\nMounting ISO9660 volume of CDROM failed with %d",rv); 
					}
					if (verb)
						printf("done\n");
					strcpy(in_name, "/Volumes/ColorVision_ISO/setup/setup.exe");
					amount = 1;		/* Remember to unmount */
				}
			}
		}
#endif /* UNIX && __APPLE__ */

#if defined(UNIX) && !defined(__APPLE__)
		{
			int i;

			/* Since there is no vendor driver instalation for Linux, we */
			/* must look for the CDROM. */
			char *vols[] = {		/* Typical volumes the CDROM could be mounted under */
				"/media/ColorVision",
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
				strcat(in_name, "/setup/setup.exe");
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

	/* Locate the firmware in the driver or install file */
	if ((va = new_arch(in_name, verb)) == NULL) {
		if(amount) umiso();
		error("Failed to create archive object from '%s'",in_name);
	}

	if (amount) umiso();
 	
	/* Get path. This may drop uid/gid if we are su */
	if ((no_paths = xdg_bds(NULL, &bin_paths, xdg_data, xdg_write, scope, bin_name)) < 1) {
		error("Failed to find/create XDG_DATA path");
	}

	get_firmware_arch(va, &fbuf, &fsize);
	
	if (verb)
		printf("Firmware is being save to '%s'\n",bin_paths[0]);

	/* Create a binary file containing the firmware */
	/* that drivers for the Spyder 2 can use */
	write_bin(bin_paths[0], fbuf, fsize, verb);
	xdg_free(bin_paths, no_paths);

	/* Create a header file that can be compiled in */
	if (header)
		write_header(header_name, fbuf, fsize, verb);

	/* Patch an executable so it behaves as if it's been compiled in */
	if (patch)
		patch_exe(patch_name, fbuf, fsize, verb);

	va->del(va);

	return 0;
}

/* ------------------------------------------------------- */

