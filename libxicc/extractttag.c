
/* 
 * Extract a CGATS file from an ICC profile tag.
 * (Can also extract a tag of unknown format as a binary lump).
 *
 * Author:  Graeme W. Gill
 * Date:    2008/5/18
 * Version: 1.00
 *
 * Copyright 2008 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * TTBD:
 *
 * Should uncompress ZXML type tag using zlib.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "icc.h"
#include "xicc.h"
#include "ui.h"

#define MXTGNMS 30

void usage(char *diag, ...) {
	int i;
	fprintf(stderr,"Extract a text tag from an ICC profile, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: extractttag  [-v] infile%s outfile\n",ICC_FILE_EXT);
	fprintf(stderr," -v            Verbose\n");
	fprintf(stderr," -t tag        Extract this tag rather than default 'targ'\n");
	fprintf(stderr," -c            Extract calibration file from 'targ' tag\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;					/* argument we're looking at */
	char in_name[MAXNAMEL+1];	/* TIFF input name */
	char out_name[MAXNAMEL+1];	/* ICC output name */
	char tag_name[MXTGNMS] = { 't','a','r','g' };
	int docal = 0;
	icc *icco;
	icTagSignature sig;
	icmText *ro;
	icmUnknown *uro;
	icmFile *ifp, *ofp;
	int verb = 0;
	int  size = 0;
	void *buf = NULL;
	int rv = 0;
	
	error_program = argv[0];

	if (argc < 3)
		usage("Too few parameters");

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
				usage(NULL);

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			/* Tag name */
			else if (argv[fa][1] == 't' || argv[fa][1] == 'T') {
				fa = nfa;
				if (na == NULL) usage("Expect tag name after -t");
				strncpy(tag_name,na,4);
				tag_name[4] = '\000';
			}

			/* Calibration */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C') {
				docal = 1;
			}

			else 
				usage("Unknown flag '%c'",argv[fa][1]);
		} else
			break;
	}

    if (fa >= argc || argv[fa][0] == '-') usage("Missing input ICC profile");
    strncpy(in_name,argv[fa++],MAXNAMEL); in_name[MAXNAMEL] = '\000';

    if (fa >= argc || argv[fa][0] == '-') usage("Missing output filename");
    strncpy(out_name,argv[fa++],MAXNAMEL); out_name[MAXNAMEL] = '\000';

	/* - - - - - - - - - - - - - - - */

	/* Open up the file for reading */
	if ((ifp = new_icmFileStd_name(in_name,"r")) == NULL)
		error ("Can't open file '%s'",in_name);

	if ((icco = new_icc()) == NULL)
		error ("Creation of ICC object failed");

	if ((rv = icco->read(icco,ifp,0)) != 0)
		error ("%d, %s",rv,icco->err);

	sig = str2tag(tag_name);

	if ((ro = (icmText *)icco->read_tag_any(icco, sig)) == NULL) {
		error("%d, %s",icco->errc, icco->err);
	}

	if (ro->ttype == icmSigUnknownType) {
		uro = (icmUnknown *)ro;
	} else if (ro->ttype != icSigTextType) {
		error("Tag isn't TextType or UnknownType");
	}

	if (docal) {
		cgatsFile *cgf;
		cgats *icg;
		int tab, oi;
		xcal *cal;

		if ((icg = new_cgats()) == NULL) {
			error("new_cgats() failed");
		}
		if ((cgf = new_cgatsFileMem(ro->data, ro->size)) == NULL)  {
			error("new_cgatsFileMem() failed");
		}
		icg->add_other(icg, "CTI3");
		oi = icg->add_other(icg, "CAL");

		if (icg->read(icg, cgf) != 0) {
			error("failed to parse tag contents as a CGATS file");
		}

		for (tab = 0; tab < icg->ntables; tab++) {
			if (icg->t[tab].tt == tt_other && icg->t[tab].oi == oi) {
				break;
			}
		}
		if (tab >= icg->ntables) {
			error("Failed to locate CAL table in CGATS");
		}
		
		if ((cal = new_xcal()) == NULL) {
			error("new_xcal() failed");
		}
		if (cal->read_cgats(cal, icg, tab, in_name) != 0)  {
			error("Parsing CAL table failed");
		}
		icg->del(icg);
		cgf->del(cgf);

		if (cal->write(cal, out_name) != 0) {
			error("writing to file '%s' failed\n",out_name);
		}
	} else {
		if ((ofp = new_icmFileStd_name(out_name, "w")) == NULL) {
			error("unable to open output file '%s'",out_name);
		}
	
		if (ro->ttype == icmSigUnknownType) {
			if (ofp->write(ofp, uro->data, 1, uro->size) != (uro->size)) {
				error("writing to file '%s' failed",out_name);
			}
		} else {
			if (ofp->write(ofp, ro->data, 1, ro->size-1) != (ro->size-1)) {
				error("writing to file '%s' failed",out_name);
			}
		}
	
		if (ofp->del(ofp) != 0) {
			error("closing file '%s' failed",out_name);
		}
	}

	icco->del(icco);
	ifp->del(ifp);

	return 0;
}
