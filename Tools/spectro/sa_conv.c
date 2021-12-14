
#ifdef SALONEINSTLIB

/*
 * A very small subset of icclib, copied to here.
 * This is just enough to support the standalone instruments
 */

/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   28/9/97
 *
 * Copyright 1997 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include "sa_config.h"
#include "numsup.h"
#include "sa_conv.h"

#include <stdio.h>
#include <stdlib.h>

sa_XYZNumber sa_D50 = {
    0.9642, 1.0000, 0.8249
};

sa_XYZNumber sa_D65 = {
    0.9505, 1.0000, 1.0890
};

sa_XYZNumber sa_D50_100 = {
    96.42, 100.00, 82.49
};

sa_XYZNumber sa_D65_100 = {
    95.05, 100.00, 108.90
};

unsigned int sa_CSSig2nchan(icColorSpaceSignature sig) {
	switch(sig) {
		case icSigXYZData:
			return 3;
		case icSigLabData:
			return 3;
		case icSigLuvData:
			return 3;
		case icSigYCbCrData:
			return 3;
		case icSigYxyData:
			return 3;
		case icSigRgbData:
			return 3;
		case icSigGrayData:
			return 1;
		case icSigHsvData:
			return 3;
		case icSigHlsData:
			return 3;
		case icSigCmykData:
			return 4;
		case icSigCmyData:
			return 3;
		case icSig2colorData:
			return 2;
		case icSig3colorData:
			return 3;
		case icSig4colorData:
			return 4;
		case icSig5colorData:
		case icSigMch5Data:
			return 5;
		case icSig6colorData:
		case icSigMch6Data:
			return 6;
		case icSig7colorData:
		case icSigMch7Data:
			return 7;
		case icSig8colorData:
		case icSigMch8Data:
			return 8;
		case icSig9colorData:
			return 9;
		case icSig10colorData:
			return 10;
		case icSig11colorData:
			return 11;
		case icSig12colorData:
			return 12;
		case icSig13colorData:
			return 13;
		case icSig14colorData:
			return 14;
		case icSig15colorData:
			return 15;

#ifdef NEVER
		/* Non-standard and Pseudo spaces */
		case icmSigYData:
			return 1;
		case icmSigLData:
			return 1;
		case icmSigL8Data:
			return 1;
		case icmSigLV2Data:
			return 1;
		case icmSigLV4Data:
			return 1;
		case icmSigPCSData:
			return 3;
		case icmSigLab8Data:
			return 3;
		case icmSigLabV2Data:
			return 3;
		case icmSigLabV4Data:
			return 3;
#endif /* NEVER */

		default:
			break;
	}
	return 0;
}

void sa_SetUnity3x3(double mat[3][3]) {
	int i, j;
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			if (i == j)
				mat[j][i] = 1.0;
			else
				mat[j][i] = 0.0;
		}
	}
	
}

void sa_Cpy3x3(double dst[3][3], double src[3][3]) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src[j][i];
	}
}

void sa_MulBy3x3(double out[3], double mat[3][3], double in[3]) {
	double tt[3];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1] + mat[0][2] * in[2];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1] + mat[1][2] * in[2];
	tt[2] = mat[2][0] * in[0] + mat[2][1] * in[1] + mat[2][2] * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

void sa_Mul3x3_2(double dst[3][3], double src1[3][3], double src2[3][3]) {
	int i, j, k;
	double td[3][3];		/* Temporary dest */

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			double tt = 0.0;
			for (k = 0; k < 3; k++)
				tt += src1[j][k] * src2[k][i];
			td[j][i] = tt;
		}
	}

	/* Copy result out */
	for (j = 0; j < 3; j++)
		for (i = 0; i < 3; i++)
			dst[j][i] = td[j][i];
}


/* Matrix Inversion by Richard Carling from "Graphics Gems", Academic Press, 1990 */
#define det2x2(a, b, c, d) (a * d - b * c)

static void adjoint(
double out[3][3],
double in[3][3]
) {
    double a1, a2, a3, b1, b2, b3, c1, c2, c3;

    /* assign to individual variable names to aid  */
    /* selecting correct values  */

	a1 = in[0][0]; b1 = in[0][1]; c1 = in[0][2];
	a2 = in[1][0]; b2 = in[1][1]; c2 = in[1][2];
	a3 = in[2][0]; b3 = in[2][1]; c3 = in[2][2];

    /* row column labeling reversed since we transpose rows & columns */

    out[0][0]  =   det2x2(b2, b3, c2, c3);
    out[1][0]  = - det2x2(a2, a3, c2, c3);
    out[2][0]  =   det2x2(a2, a3, b2, b3);
        
    out[0][1]  = - det2x2(b1, b3, c1, c3);
    out[1][1]  =   det2x2(a1, a3, c1, c3);
    out[2][1]  = - det2x2(a1, a3, b1, b3);
        
    out[0][2]  =   det2x2(b1, b2, c1, c2);
    out[1][2]  = - det2x2(a1, a2, c1, c2);
    out[2][2]  =   det2x2(a1, a2, b1, b2);
}

static double sa_Det3x3(double in[3][3]) {
    double a1, a2, a3, b1, b2, b3, c1, c2, c3;
    double ans;

	a1 = in[0][0]; b1 = in[0][1]; c1 = in[0][2];
	a2 = in[1][0]; b2 = in[1][1]; c2 = in[1][2];
	a3 = in[2][0]; b3 = in[2][1]; c3 = in[2][2];

    ans = a1 * det2x2(b2, b3, c2, c3)
        - b1 * det2x2(a2, a3, c2, c3)
        + c1 * det2x2(a2, a3, b2, b3);
    return ans;
}

#define SA__SMALL_NUMBER	1.e-8

int sa_Inverse3x3(double out[3][3], double in[3][3]) {
    int i, j;
    double det;

    /*  calculate the 3x3 determinant
     *  if the determinant is zero, 
     *  then the inverse matrix is not unique.
     */
    det = sa_Det3x3(in);

    if ( fabs(det) < SA__SMALL_NUMBER)
        return 1;

    /* calculate the adjoint matrix */
    adjoint(out, in);

    /* scale the adjoint matrix to get the inverse */
    for (i = 0; i < 3; i++)
        for(j = 0; j < 3; j++)
		    out[i][j] /= det;
	return 0;
}

#undef SA__SMALL_NUMBER	
#undef det2x2

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Transpose a 3x3 matrix */
void sa_Transpose3x3(double out[3][3], double in[3][3]) {
	int i, j;
	if (out != in) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				out[i][j] = in[j][i];
	} else {
		double tt[3][3];
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				tt[i][j] = in[j][i];
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				out[i][j] = tt[i][j];
	}
}

/* Scale a 3 vector by the given ratio */
void sa_Scale3(double out[3], double in[3], double rat) {
	out[0] = in[0] * rat;
	out[1] = in[1] * rat;
	out[2] = in[2] * rat;
}

/* Clamp a 3 vector to be +ve */
void sa_Clamp3(double out[3], double in[3]) {
	int i;
	for (i = 0; i < 3; i++)
		out[i] = in[i] < 0.0 ? 0.0 : in[i];
}

/* Return the normal Delta E given two Lab values */
double sa_LabDE(double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	rv += tt * tt;

	return sqrt(rv);
}

/* Return the CIE94 Delta E color difference measure, squared */
double sa_CIE94sq(double Lab0[3], double Lab1[3]) {
	double desq, dhsq;
	double dlsq, dcsq;
	double c12;

	{
		double dl, da, db;
		dl = Lab0[0] - Lab1[0];
		dlsq = dl * dl;		/* dl squared */
		da = Lab0[1] - Lab1[1];
		db = Lab0[2] - Lab1[2];

		/* Compute normal Lab delta E squared */
		desq = dlsq + da * da + db * db;
	}

	{
		double c1, c2, dc;

		/* Compute chromanance for the two colors */
		c1 = sqrt(Lab0[1] * Lab0[1] + Lab0[2] * Lab0[2]);
		c2 = sqrt(Lab1[1] * Lab1[1] + Lab1[2] * Lab1[2]);
		c12 = sqrt(c1 * c2);	/* Symetric chromanance */

		/* delta chromanance squared */
		dc = c1 - c2;
		dcsq = dc * dc;
	}

	/* Compute delta hue squared */
	if ((dhsq = desq - dlsq - dcsq) < 0.0)
		dhsq = 0.0;
	{
		double sc, sh;

		/* Weighting factors for delta chromanance & delta hue */
		sc = 1.0 + 0.045 * c12;
		sh = 1.0 + 0.015 * c12;
		return dlsq + dcsq/(sc * sc) + dhsq/(sh * sh);
	}
}

/* Return the CIE94 Delta E color difference measure */
double sa_CIE94(double Lab0[3], double Lab1[3]) {
	return sqrt(sa_CIE94sq(Lab0, Lab1));
}

/* Return the CIE94 Delta E color difference measure for two XYZ values */
double sa_XYZCIE94(sa_XYZNumber *w, double *in0, double *in1) {
	double lab0[3], lab1[3];

	sa_XYZ2Lab(w, lab0, in0);
	sa_XYZ2Lab(w, lab1, in1);
	return sqrt(sa_CIE94sq(lab0, lab1));
}

/* CIE XYZ to perceptual CIE 1976 L*a*b* */
void
sa_XYZ2Lab(sa_XYZNumber *w, double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double x,y,z,fx,fy,fz;

	x = X/w->X;
	y = Y/w->Y;
	z = Z/w->Z;

	if (x > 0.008856451586)
		fx = pow(x,1.0/3.0);
	else
		fx = 7.787036979 * x + 16.0/116.0;

	if (y > 0.008856451586)
		fy = pow(y,1.0/3.0);
	else
		fy = 7.787036979 * y + 16.0/116.0;

	if (z > 0.008856451586)
		fz = pow(z,1.0/3.0);
	else
		fz = 7.787036979 * z + 16.0/116.0;

	out[0] = 116.0 * fy - 16.0;
	out[1] = 500.0 * (fx - fy);
	out[2] = 200.0 * (fy - fz);
}

void sa_Lab2XYZ(sa_XYZNumber *w, double *out, double *in) {
	double L = in[0], a = in[1], b = in[2];
	double x,y,z,fx,fy,fz;

	fy = (L + 16.0)/116.0;
	fx = a/500.0 + fy;
	fz = fy - b/200.0;

	if (fy > 24.0/116.0)
		y = pow(fy,3.0);
	else
		y = (fy - 16.0/116.0)/7.787036979;

	if (fx > 24.0/116.0)
		x = pow(fx,3.0);
	else
		x = (fx - 16.0/116.0)/7.787036979;

	if (fz > 24.0/116.0)
		z = pow(fz,3.0);
	else
		z = (fz - 16.0/116.0)/7.787036979;

	out[0] = x * w->X;
	out[1] = y * w->Y;
	out[2] = z * w->Z;
}


void sa_Yxy2XYZ(double *out, double *in) {
	double Y = in[0];
	double x = in[1];
	double y = in[2];
	double z = 1.0 - x - y;
	double sum;
	if (y < 1e-9) {
		out[0] = out[1] = out[2] = 0.0;
	} else {
		sum = Y/y;
		out[0] = x * sum;
		out[1] = Y;
		out[2] = z * sum;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Object for computing RFC 1321 MD5 checksums. */
/* Derived from Colin Plumb's 1993 public domain code. */

/* Reset the checksum */
static void sa_MD5_reset(sa_MD5 *p) {
	p->tlen = 0;

	p->sum[0] = 0x67452301;
	p->sum[1] = 0xefcdab89;
	p->sum[2] = 0x98badcfe;
	p->sum[3] = 0x10325476;

	p->fin = 0;
}

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

#define MD5STEP(f, w, x, y, z, pp, xtra, s) \
		data = (pp)[0] + ((pp)[3] << 24) + ((pp)[2] << 16) + ((pp)[1] << 8); \
		w += f(x, y, z) + data + xtra; \
		w = (w << s) | (w >> (32-s)); \
		w += x;

/* Add another 64 bytes to the checksum */
static void sa_MD5_accume(sa_MD5 *p, ORD8 *in) {
	ORD32 data, a, b, c, d;

	a = p->sum[0];
	b = p->sum[1];
	c = p->sum[2];
	d = p->sum[3];

	MD5STEP(F1, a, b, c, d, in + (4 * 0),  0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 1),  0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 2),  0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 3),  0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 4),  0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 5),  0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 6),  0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 7),  0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 8),  0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 9),  0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 10), 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 11), 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in + (4 * 12), 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in + (4 * 13), 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in + (4 * 14), 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in + (4 * 15), 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in + (4 * 1),  0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 6),  0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 11), 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 0),  0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 5),  0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 10), 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 15), 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 4),  0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 9),  0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 14), 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 3),  0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 8),  0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in + (4 * 13), 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in + (4 * 2),  0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in + (4 * 7),  0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in + (4 * 12), 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in + (4 * 5),  0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 8),  0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 11), 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 14), 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 1),  0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 4),  0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 7),  0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 10), 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 13), 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 0),  0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 3),  0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 6),  0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in + (4 * 9),  0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in + (4 * 12), 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in + (4 * 15), 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in + (4 * 2),  0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in + (4 * 0),  0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 7),  0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 14), 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 5),  0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 12), 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 3),  0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 10), 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 1),  0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 8),  0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 15), 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 6),  0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 13), 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in + (4 * 4),  0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in + (4 * 11), 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in + (4 * 2),  0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in + (4 * 9),  0xeb86d391, 21);

	p->sum[0] += a;
	p->sum[1] += b;
	p->sum[2] += c;
	p->sum[3] += d;
}

#undef F1
#undef F2
#undef F3
#undef F4
#undef MD5STEP

/* Add some bytes */
static void sa_MD5_add(sa_MD5 *p, ORD8 *ibuf, unsigned int len) {
	unsigned int bs;

	if (p->fin)
		return;				/* This is actually an error */

	bs = p->tlen;			/* Current bytes added */
	p->tlen = bs + len;		/* Update length after adding this buffer */
	bs &= 0x3f;				/* Bytes already in buffer */

	/* Deal with any existing partial bytes in p->buf */
	if (bs) {
		ORD8 *np = (ORD8 *)p->buf + bs;	/* Next free location in partial buffer */

		bs = 64 - bs;		/* Free space in partial buffer */

		if (len < bs) {		/* Not enought new to make a full buffer */
			memmove(np, ibuf, len);
			return;
		}

		memmove(np, ibuf, bs);	/* Now got one full buffer */
		sa_MD5_accume(p, np);
		ibuf += bs;
		len -= bs;
	}

	/* Deal with input data 64 bytes at a time */
	while (len >= 64) {
		sa_MD5_accume(p, ibuf);
		ibuf += 64;
		len -= 64;
	}

	/* Deal with any remaining bytes */
	memmove(p->buf, ibuf, len);
}

/* Finalise the checksum and return the result. */
static void sa_MD5_get(sa_MD5 *p, ORD8 chsum[16]) {
	int i;
	unsigned count;
	ORD32 bits1, bits0;
	ORD8 *pp;

	if (p->fin == 0) {
	
		/* Compute number of bytes processed mod 64 */
		count = p->tlen & 0x3f;

		/* Set the first char of padding to 0x80.  This is safe since there is
		   always at least one byte free */
		pp = p->buf + count;
		*pp++ = 0x80;

		/* Bytes of padding needed to make 64 bytes */
		count = 64 - 1 - count;

		/* Pad out to 56 mod 64, allowing 8 bytes for length in bits. */
		if (count < 8) {	/* Not enough space for padding and length */

			memset(pp, 0, count);
			sa_MD5_accume(p, p->buf);

			/* Now fill the next block with 56 bytes */
			memset(p->buf, 0, 56);
		} else {
			/* Pad block to 56 bytes */
			memset(pp, 0, count - 8);
		}

		/* Compute number of bits */
		bits1 = 0x7 & (p->tlen >> (32 - 3)); 
		bits0 = p->tlen << 3;

		/* Append number of bits */
		p->buf[64 - 8] = bits0 & 0xff; 
		p->buf[64 - 7] = (bits0 >> 8) & 0xff; 
		p->buf[64 - 6] = (bits0 >> 16) & 0xff; 
		p->buf[64 - 5] = (bits0 >> 24) & 0xff; 
		p->buf[64 - 4] = bits1 & 0xff; 
		p->buf[64 - 3] = (bits1 >> 8) & 0xff; 
		p->buf[64 - 2] = (bits1 >> 16) & 0xff; 
		p->buf[64 - 1] = (bits1 >> 24) & 0xff; 

		sa_MD5_accume(p, p->buf);

		p->fin = 1;
	}

	/* Return the result, lsb to msb */
	pp = chsum;
	for (i = 0; i < 4; i++) {
		*pp++ = p->sum[i] & 0xff; 
		*pp++ = (p->sum[i] >> 8) & 0xff; 
		*pp++ = (p->sum[i] >> 16) & 0xff; 
		*pp++ = (p->sum[i] >> 24) & 0xff; 
	}
}


/* Delete the instance */
static void sa_MD5_del(sa_MD5 *p) {

	/* This object */
	if (p != NULL)
		free(p);
}

/* Create a new MD5 checksumming object, with a reset checksum value */
/* Return it or NULL if there is an error */
sa_MD5 *new_sa_MD5() {
	sa_MD5 *p;

	if ((p = (sa_MD5 *)calloc(1,sizeof(sa_MD5))) == NULL)
		return NULL;

	p->reset    = sa_MD5_reset;
	p->add      = sa_MD5_add;
	p->get      = sa_MD5_get;
	p->del      = sa_MD5_del;

	p->reset(p);

	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* A sub-set of ludecomp code from numlib          */

int sa_lu_decomp(double **a, int n, int *pivx, double *rip) {
	int    i, j;
	double *rscale, RSCALE[10];		

	if (n <= 10)
		rscale = RSCALE;
	else
		rscale = dvector(0, n-1);

	for (i = 0; i < n; i++) {
		double big;
		for (big = 0.0, j=0; j < n; j++) {
			double temp;
			temp = fabs(a[i][j]);
			if (temp > big)
				big = temp;
		}
		if (fabs(big) <= DBL_MIN) {
			if (rscale != RSCALE)
				free_dvector(rscale, 0, n-1);
			return 1;		
		}
		rscale[i] = 1.0/big;	
	}

	for (*rip = 1.0, j = 0; j < n; j++) {
		double big;
		int k, bigi = 0;

		for (i = 0; i < j; i++) {
			double sum;
			sum = a[i][j];
			for (k = 0; k < i; k++)
				sum -= a[i][k] * a[k][j];
			a[i][j] = sum;
		}

		for (big = 0.0, i = j; i < n; i++) {
			double sum, temp;
			sum = a[i][j];
			for (k = 0; k < j; k++)
				sum -= a[i][k] * a[k][j];
			a[i][j] = sum;
			temp = rscale[i] * fabs(sum);	
			if (temp >= big) {
				big = temp;		
				bigi = i;		
			}
		}
		
		if (j != bigi) {
			{	
				double *temp;
				temp = a[bigi];
				a[bigi] = a[j];
				a[j] = temp;
			}
			*rip = -(*rip);				
			rscale[bigi] = rscale[j];	
		}
		
		pivx[j] = bigi;					
		if (fabs(a[j][j]) <= DBL_MIN) {
			if (rscale != RSCALE)
				free_dvector(rscale, 0, n-1);
			return 1; 					
		}

		if (j != (n-1)) {
			double temp;
			temp = 1.0/a[j][j];
			for (i = j+1; i < n; i++)
				a[i][j] *= temp;
		}
	}
	if (rscale != RSCALE)
		free_dvector(rscale, 0, n-1);
	return 0;
}

void sa_lu_backsub(double **a, int n, int *pivx, double  *b) {
	int i, j;
	int nvi;		
	
	for (nvi = -1, i = 0; i < n; i++) {
		int px;
		double sum;

		px = pivx[i];
		sum = b[px];
		b[px] = b[i];
		if (nvi >= 0) {
			for (j = nvi; j < i; j++)
				sum -= a[i][j] * b[j];
		} else {
			if (sum != 0.0)
				nvi = i;			
		}
		b[i] = sum;
	}

	for (i = (n-1); i >= 0; i--) {
		double sum;
		sum = b[i];
		for (j = i+1; j < n; j++)
			sum -= a[i][j] * b[j];
		b[i] = sum/a[i][i];
	}
}

int sa_lu_invert(double **a, int n) {
	int i, j;
	double rip;		
	int *pivx, PIVX[10];
	double **y;

	if (n <= 10)
		pivx = PIVX;
	else
		pivx = ivector(0, n-1);

	if (sa_lu_decomp(a, n, pivx, &rip)) {
		if (pivx != PIVX)
			free_ivector(pivx, 0, n-1);
		return 1;
	}

	y = dmatrix(0, n-1, 0, n-1);
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			y[i][j] = a[i][j];
		}
	}

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++)
			a[i][j] = 0.0;
		a[i][i] = 1.0;
		sa_lu_backsub(y, n, pivx, a[i]);
	}

	free_dmatrix(y, 0, n-1, 0, n-1);
	if (pivx != PIVX)
		free_ivector(pivx, 0, n-1);

	return 0;
}

int sa_lu_psinvert(double **out, double **in, int m, int n) {
	int rv = 0;
	double **tr;		
	double **sq;		

	tr = dmatrix(0, n-1, 0, m-1);
	matrix_trans(tr, in, m,  n);

	if (m > n) {
		sq = dmatrix(0, n-1, 0, n-1);
		if ((rv = matrix_mult(sq, n, n, tr, n, m, in, m, n)) == 0) {
			if ((rv = sa_lu_invert(sq, n)) == 0) {
				rv = matrix_mult(out, n, m, sq, n, n, tr, n, m);
			}
		}
		free_dmatrix(sq, 0, n-1, 0, n-1);
	} else {
		sq = dmatrix(0, m-1, 0, m-1);
		if ((rv = matrix_mult(sq, m, m, in, m, n, tr, n, m)) == 0) {
			if ((rv = sa_lu_invert(sq, m)) == 0) {
				rv = matrix_mult(out, n, m, tr, n, m, sq, m, m);
			}
		}
		free_dmatrix(sq, 0, m-1, 0, m-1);
	}

	free_dmatrix(tr, 0, n-1, 0, m-1);
	return rv;
}


#endif /* SALONEINSTLIB */


