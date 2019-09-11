
#ifndef BASE64_H

/* 
 * Argyll Color Correction System
 *
 * Very simple & concise base64 encoder/decoder
 */

/*
 * Author: Graeme W. Gill
 *
 * Copyright 2014, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* The maximum encoded length given decoded length */
#define EBASE64LEN(len) (((len) * 4 + 2)/3)

/* Encode slen bytes into nul terminated string. */
/* if dlen is !NULL, then set it to resulting string length excluding nul */
/* We assume that the destination buffer is long enough at EBASE64LEN */
void ebase64(int *dlen, char *dst, unsigned char *src, int slen);

/* The maximum decoded length given encoded length */
#define DBASE64LEN(len) (((len) * 3)/4)

/* Decode nul terminated string into bytes, ignoring illegal characters. */
/* if dlen is !NULL, then set it to resulting byte length */
/* We assume that the destination buffer is long enough at DBASE64LEN */
void dbase64(int *dlen, unsigned char *dst, char *src);

#ifdef __cplusplus
	}
#endif

#define BASE64_H
#endif /* BASE64_H */
