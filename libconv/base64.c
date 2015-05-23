
/* 
 * Argyll Color Correction System
 *
 * Very simple & concise base64 encoder/decoder
 *
 * Author: Graeme W. Gill
 *
 * Copyright 2014, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */


#include <stdio.h>
# define DLL_LOCAL
#include "base64.h"

static int enc(int val) {
	val &= 0x3f;

	if (val <= 25)
		return ('A' + val);
	if (val <= 51)
		return ('a' + val - 26);
	if (val <= 61)
		return ('0' + val - 52);
	if (val == 62)
		return '+';
	return '/';
}

/* Encode slen bytes into nul terminated string. */
/* if dlen is !NULL, then set it to resulting string length excluding nul */
/* We assume that the destination buffer is long enough at EBASE64LEN */
void DLL_LOCAL ebase64(int *dlen, char *dst, unsigned char *src, int slen) {
	unsigned char buf[3];
	int i, j;

	/* Do this 3 characters at a time */
	for (j = i = 0; i < slen; i += 3) { 
		int ib;

		/* Grab 3 bytes or zero pad, and count bits */
		buf[0] = src[i]; ib = 8;
		buf[1] = (i+1) < slen ? (ib += 8, src[i+1]) : 0;
		buf[2] = (i+2) < slen ? (ib += 8, src[i+2]) : 0;

		/* Output up to 4 encoded characters */
		dst[j] = enc(buf[0] >> 2), j++;
		if (ib > 6) {
			dst[j] = enc(buf[0] << 4 | buf[1] >> 4), j++;
			if (ib > 12) {
				dst[j] = enc(buf[1] << 2 | buf[2] >> 6), j++;
				if (ib > 18)
					dst[j] = enc(buf[2]), j++;
			}
		}
	}
	if (dlen != NULL)
		*dlen = j;

	dst[j++] = '\000';
}

/* Return binary value corresponding to encoded character, */
/* -1 for invalid character, -2 if nul terminator */
/* (We're assuming ASCII encoding) */
static int dec(int val) {
	val &= 0xff;

	if (val == 0)
		return -2;
	if (val == '+')
		return 62;
	if (val == '/')
		return 63;
	if (val < '0')
		return -1;
	if (val <= '9')
		return val - '0' + 52;
	if (val < 'A')
		return -1;
	if (val <= 'Z')
		return val - 'A';
	if (val < 'a')
		return -1;
	if (val <= 'z')
		return val - 'a' + 26;
	return -1;
}

/* Decode nul terminated string into bytes, ignoring illegal characters. */
/* if dlen is !NULL, then set it to resulting byte length */
/* We assume that the destination buffer is long enough at DBASE64LEN */
void DLL_LOCAL dbase64(int *dlen, unsigned char *dst, char *src) {
	int buf[4];
	int j;

	/* Do this 4 characters at a time */
	for (j = 0;;) { 
		int ib;

		/* Grab 4 characters skipping illegals, and count bits */
		while ((buf[0] = dec(*src++)) == -1)
			;
		if (buf[0] == -2)
			break;
		ib = 6;
		while ((buf[1] = dec(*src++)) == -1)
			;
		if (buf[1] != -2) {
			ib += 6;
			while ((buf[2] = dec(*src++)) == -1)
				;
			if (buf[2] != -2) {
				ib += 6;
				while ((buf[3] = dec(*src++)) == -1)
					;
				if (buf[3] != -2)
					ib += 6;
				else
					buf[3] = 0;
			} else
				buf[2] = 0;
		} else
			buf[1] = 0;

		/* Output up to 3 decoded bytes */
		dst[j] = buf[0] << 2 | buf[1] >> 4, j++;
		if (ib > 12) {
			dst[j] = buf[1] << 4 | buf[2] >> 2, j++;
			if (ib > 18) {
				dst[j] = buf[2] << 6 | buf[3], j++;
				continue;		/* We didn't run out */
			}
		}
		break;		/* Must be done if didn't continue */
	}
	if (dlen != NULL)
		*dlen = j;
}


#ifdef STANDALONE_TEST
/* test base64 code */

#include <numlib.h>

#define MAXLEN 30

int main() {
	unsigned char src[MAXLEN], check[MAXLEN];
	char dst[EBASE64LEN(MAXLEN) + 1];
	int n, slen, dlen, chlen, i, j;

	printf("Testing base64 encode/decode:\n");

	for (n = 0; n < 10000000; n++) {

		/* Create a random binary */
		slen = i_rand(0, MAXLEN);
		for (i = 0; i < slen; i++)
			src[i] = i_rand(0, 255);

		/* Encode it */
		ebase64(&dlen, dst, src, slen);

		if (dlen != EBASE64LEN(slen)) {
			error("%d: Wrong encoded length, src %d, is %d should be %d\n",n,slen,dlen,EBASE64LEN(slen));
		}

		/* Decode it */
		dbase64(&chlen, check, dst);

		if (chlen != DBASE64LEN(dlen)) {
			error("%d: Wrong decoded length, enc %d, is %d should be %d\n",n,dlen,chlen,DBASE64LEN(dlen));
		}

		/* Check characters match */
		for (i = 0; i < slen; i++) {
			if (src[i] != check[i]) {
				for (j = 0; j < slen; j++) {
					printf("%02X ",src[j]);
					if ((j % 3) == 2)
						printf(" ");
				}
				printf("\n");

				for (j = 0; j < dlen; j++) {
					printf("%c ",dst[j]);
					if ((j % 4) == 3)
						printf("  ");
				}
				printf("\n");
				for (j = 0; j < slen; j++) {
					printf("%02X ",check[j]);
					if ((j % 3) == 2)
						printf(" ");
				}
				printf("\n\n");
				printf("src len %d, dst len %d\n",slen, dlen);
				error("%d: Verify error at byte %d, is 0x%x should be 0x%x\n",n,i,check[i],src[i]);
			}
		}

		if ((n % 10000) == 0)
			printf("done %d tests\n",n);
	}
	printf("Testing %d conversions base64 complete\n",n);
}

#endif /* STANDALONE_TEST */
