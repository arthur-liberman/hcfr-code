
/* 
 * xcolorant lookup/test utility
 *
 * Author:  Graeme W. Gill
 * Date:    24/4/2002
 * Version: 1.00
 *
 * Copyright 2002 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "icc.h"
#include "xcolorants.h"

void error(char *fmt, ...), warning(char *fmt, ...);

void usage(void) {
	fprintf(stderr,"Translate colors through xcolorant model, V1.00\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: xcolorantlu \n");
	fprintf(stderr," -v         Verbose\n");
	fprintf(stderr," -a         Additive model (default subtractive)\n");
	fprintf(stderr," -m mask    Colorant mask combination, from:\n");
	fprintf(stderr,"    C         Cyan\n");
	fprintf(stderr,"    M         Magenta\n");
	fprintf(stderr,"    Y         Yellow\n");
	fprintf(stderr,"    K         Black\n");
	fprintf(stderr,"    O         Orange\n");
	fprintf(stderr,"    R         Red\n");
	fprintf(stderr,"    G         Green\n");
	fprintf(stderr,"    B         Blue\n");
	fprintf(stderr,"    W         White\n");
	fprintf(stderr,"    c         Light Cyan\n");
	fprintf(stderr,"    m         Light Magenta\n");
	fprintf(stderr,"    y         Light Yellow\n");
	fprintf(stderr,"    k         Light Black\n");
	fprintf(stderr,"    2c        Medium Cyan\n");
	fprintf(stderr,"    2m        Medium Magenta\n");
	fprintf(stderr,"    2y        Medium Yellow\n");
	fprintf(stderr,"    2k        Medium Black\n");
	fprintf(stderr," -x         XYZ output (default L*a*b*)\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"    The colors to be translated should be fed into stdin,\n");
	fprintf(stderr,"    one input color per line, white space separated.\n");
	fprintf(stderr,"    A line starting with a # will be ignored.\n");
	fprintf(stderr,"    A line not starting with a number will terminate the program.\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	int verb = 0;
	int rv = 0;
	int mask = 0;
	int xyz = 0;
	char buf[200];
	double in[MAX_CHAN], out[MAX_CHAN];
	int inn, outn = 3;
	icxColorantLu *luo;
	char *ident;
	char *odent;

	if (argc < 2)
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

			/* Additive */
			else if (argv[fa][1] == 'a' || argv[fa][1] == 'A') {
				mask ^= ICX_ADDITIVE;
			}

			/* XYZ output */
			else if (argv[fa][1] == 'x' || argv[fa][1] == 'X') {
				verb = 1;
			}

			/* Mask */
			else if (argv[fa][1] == 'm' || argv[fa][1] == 'm') {
				int tm;
				fa = nfa;
				if (na == NULL) usage();
				if ((tm = icx_char2inkmask(na)) == 0)
					usage();
				mask ^= tm;
			}

			else 
				usage();
		} else
			break;
	}

	inn = icx_noofinks(mask);

	/* Create a icxColorantLu conversion object */
	if ((luo = new_icxColorantLu(mask)) == NULL)
		error ("Creating xcolorant lookup failed\n");

	ident = icx_inkmask2char(mask, 1); 
	if (xyz)
		odent = "XYZ";
	else
		odent = "Lab";

	/* Process colors to translate */
	for (;;) {
		int i,j;
		char *bp, *nbp;

		/* Read in the next line */
		if (fgets(buf, 200, stdin) == NULL)
			break;
		if (buf[0] == '#') {
			fprintf(stdout,"%s\n",buf);
			continue;
		}
		/* For each input number */
		for (nbp = buf, i = 0; i < MAX_CHAN; i++) {
			bp = nbp;
			in[i] = strtod(bp, &nbp);
			if (nbp == bp)
				break;			/* Failed */
		}
		if (i == 0)
			break;

		/* Do conversion */
		if (xyz)
			luo->dev_to_XYZ(luo, out, in);
		else
			luo->dev_to_rLab(luo, out, in);

		/* Output the results */
		for (j = 0; j < inn; j++) {
			if (j > 0)
				fprintf(stdout," %f",in[j]);
			else
				fprintf(stdout,"%f",in[j]);
		}
		printf(" [%s] -> ", ident);

		for (j = 0; j < outn; j++) {
			if (j > 0)
				fprintf(stdout," %f",out[j]);
			else
				fprintf(stdout,"%f",out[j]);
		}
		printf(" [%s]", odent);

		if (rv == 0)
			fprintf(stdout,"\n");
		else
			fprintf(stdout," (clip)\n");

	}

	/* Done with lookup object */
	luo->del(luo);
	free(ident);

	return 0;
}


/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"icclu: Error - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit (-1);
}

void
warning(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"icclu: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}
