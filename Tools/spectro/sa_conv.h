#ifndef SA_CONV_H

#ifdef SALONEINSTLIB
/*
 * A very small subset of icclib and numlib for the standalone instrument lib.
 */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2008/2/9
 *
 * Copyright 1996 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 * 
 * Derived from icoms.h
 */

#if defined (NT)
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#  if defined _WIN32_WINNT
#   undef _WIN32_WINNT
#  endif
#  define _WIN32_WINNT 0x0501
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <io.h>
#endif

#if defined (UNIX)
# include <unistd.h>
# include <glob.h>
# include <pthread.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* A subset of icclib */

#ifndef MAX_CHAN
# define MAX_CHAN 15
#endif

typedef enum {
    sa_SigXYZData                        = 0x58595A20L,  /* 'XYZ ' */
    sa_SigLabData                        = 0x4C616220L,  /* 'Lab ' */
    sa_SigLuvData                        = 0x4C757620L,  /* 'Luv ' */
    sa_SigYCbCrData                      = 0x59436272L,  /* 'YCbr' */
    sa_SigYxyData                        = 0x59787920L,  /* 'Yxy ' */
    sa_SigRgbData                        = 0x52474220L,  /* 'RGB ' */
    sa_SigGrayData                       = 0x47524159L,  /* 'GRAY' */
    sa_SigHsvData                        = 0x48535620L,  /* 'HSV ' */
    sa_SigHlsData                        = 0x484C5320L,  /* 'HLS ' */
    sa_SigCmykData                       = 0x434D594BL,  /* 'CMYK' */
    sa_SigCmyData                        = 0x434D5920L,  /* 'CMY ' */

    sa_Sig2colorData                     = 0x32434C52L,  /* '2CLR' */
    sa_Sig3colorData                     = 0x33434C52L,  /* '3CLR' */
    sa_Sig4colorData                     = 0x34434C52L,  /* '4CLR' */
    sa_Sig5colorData                     = 0x35434C52L,  /* '5CLR' */
    sa_Sig6colorData                     = 0x36434C52L,  /* '6CLR' */
    sa_Sig7colorData                     = 0x37434C52L,  /* '7CLR' */
    sa_Sig8colorData                     = 0x38434C52L,  /* '8CLR' */
    sa_Sig9colorData                     = 0x39434C52L,  /* '9CLR' */
    sa_Sig10colorData                    = 0x41434C52L,  /* 'ACLR' */
    sa_Sig11colorData                    = 0x42434C52L,  /* 'BCLR' */
    sa_Sig12colorData                    = 0x43434C52L,  /* 'CCLR' */
    sa_Sig13colorData                    = 0x44434C52L,  /* 'DCLR' */
    sa_Sig14colorData                    = 0x45434C52L,  /* 'ECLR' */
    sa_Sig15colorData                    = 0x46434C52L,  /* 'FCLR' */

    sa_SigMch5Data                       = 0x4D434835L,  /* 'MCH5' Colorsync ? */
    sa_SigMch6Data                       = 0x4D434836L,  /* 'MCH6' Hexachrome: CMYKOG */
    sa_SigMch7Data                       = 0x4D434837L,  /* 'MCH7' Colorsync ? */
    sa_SigMch8Data                       = 0x4D434838L,  /* 'MCH8' Colorsync ? */
	sa_SigNamedData                      = 0x6e6d636cL,  /* 'nmcl' ??? */

    sa_MaxEnumData                       = -1   
} sa_ColorSpaceSignature;

typedef enum {
    sa_SigInputClass                     = 0x73636E72L,  /* 'scnr' */
    sa_SigDisplayClass                   = 0x6D6E7472L,  /* 'mntr' */
    sa_SigOutputClass                    = 0x70727472L,  /* 'prtr' */
    sa_SigLinkClass                      = 0x6C696E6BL,  /* 'link' */
    sa_SigAbstractClass                  = 0x61627374L,  /* 'abst' */
    sa_SigColorSpaceClass                = 0x73706163L,  /* 'spac' */
    sa_SigNamedColorClass                = 0x6e6d636cL,  /* 'nmcl' */
    sa_MaxEnumClass                      = -1
} sa_ProfileClassSignature;

typedef struct {
    double  X;
    double  Y;
    double  Z;
} sa_XYZNumber;

unsigned int sa_CSSig2nchan(sa_ColorSpaceSignature sig);
extern sa_XYZNumber sa_D50;
extern sa_XYZNumber sa_D65;
extern sa_XYZNumber sa_D50_100;
extern sa_XYZNumber sa_D65_100;
void sa_SetUnity3x3(double mat[3][3]);
void sa_Cpy3x3(double out[3][3], double mat[3][3]);
void sa_MulBy3x3(double out[3], double mat[3][3], double in[3]);
void sa_Mul3x3_2(double dst[3][3], double src1[3][3], double src2[3][3]);
int sa_Inverse3x3(double out[3][3], double in[3][3]);
void sa_Transpose3x3(double out[3][3], double in[3][3]);
void sa_Scale3(double out[3], double in[3], double rat);
void sa_Clamp3(double out[3], double in[3]);
double sa_LabDE(double *in0, double *in1);
double sa_CIE94sq(double *in0, double *in1);
void sa_Lab2XYZ(sa_XYZNumber *w, double *out, double *in);
void sa_XYZ2Lab(sa_XYZNumber *w, double *out, double *in);
void sa_Yxy2XYZ(double *out, double *in);

#define icColorSpaceSignature sa_ColorSpaceSignature
#define icSigXYZData sa_SigXYZData
#define icSigLabData sa_SigLabData
#define icSigLuvData sa_SigLuvData
#define icSigYCbCrData sa_SigYCbCrData
#define icSigYxyData sa_SigYxyData 
#define icSigRgbData sa_SigRgbData
#define icSigGrayData sa_SigGrayData
#define icSigHsvData sa_SigHsvData
#define icSigHlsData sa_SigHlsData
#define icSigCmykData sa_SigCmykData
#define icSigCmyData sa_SigCmyData
#define icSig2colorData sa_Sig2colorData
#define icSig3colorData sa_Sig3colorData
#define icSig4colorData sa_Sig4colorData
#define icSig5colorData sa_Sig5colorData
#define icSig6colorData sa_Sig6colorData
#define icSig7colorData sa_Sig7colorData
#define icSig8colorData sa_Sig8colorData
#define icSig9colorData sa_Sig9colorData
#define icSig10colorData sa_Sig10colorData
#define icSig11colorData sa_Sig11colorData
#define icSig12colorData sa_Sig12colorData
#define icSig13colorData sa_Sig13colorData
#define icSig14colorData sa_Sig14colorData
#define icSig15colorData sa_Sig15colorData
#define icSigMch5Data sa_SigMch5Data
#define icSigMch6Data sa_SigMch6Data
#define icSigMch7Data sa_SigMch7Data
#define icSigMch8Data sa_SigMch8Data
#define icSigNamedData sa_SigNamedData
#define icMaxEnumData sa_MaxEnumData

#define icProfileClassSignature sa_ProfileClassSignature
#define icSigInputClass sa_SigInputClass
#define icSigDisplayClass sa_SigDisplayClass
#define icSigOutputClass sa_SigOutputClass
#define icSigLinkClass sa_SigLinkClass
#define icSigAbstractClass sa_SigAbstractClass
#define icSigColorSpaceClass sa_SigColorSpaceClass
#define icSigNamedColorClass sa_SigNamedColorClass

#define icmCSSig2nchan sa_CSSig2nchan

#define icmXYZNumber sa_XYZNumber
#define icmD50 sa_D50
#define icmD65 sa_D65
#define icmD50_100 sa_D50_100
#define icmD65_100 sa_D65_100

#define icmSetUnity3x3 sa_SetUnity3x3
#define icmCpy3x3 sa_Cpy3x3
#define icmMulBy3x3 sa_MulBy3x3
#define icmMul3x3_2 sa_Mul3x3_2
#define icmInverse3x3 sa_Inverse3x3
#define icmTranspose3x3 sa_Transpose3x3

#define icmCpy3(d_ary, s_ary) ((d_ary)[0] = (s_ary)[0], (d_ary)[1] = (s_ary)[1], \
                              (d_ary)[2] = (s_ary)[2])
#define icmScale3 sa_Scale3
#define icmClamp3 sa_Clamp3

#define icmAry2XYZ(xyz, ary) ((xyz).X = (ary)[0], (xyz).Y = (ary)[1], (xyz).Z = (ary)[2])

#define icmLabDE sa_LabDE
#define icmCIE94sq sa_CIE94sq
#define icmXYZ2Lab sa_XYZ2Lab
#define icmLab2XYZ sa_Lab2XYZ
#define icmYxy2XYZ sa_Yxy2XYZ

/* Lpt isn't used by instlib, so dummy it out */
#define icmXYZ2Lpt sa_XYZ2Lab
#define icmLpt2XYZ sa_Lab2XYZ

/* A helper object that computes MD5 checksums */
struct _sa_MD5 {
  /* Private: */
	int fin;				/* Flag, nz if final has been called */
	ORD32 sum[4];			/* Current/final checksum */
	unsigned int tlen;		/* Total length added in bytes */
	ORD8 buf[64];			/* Partial buffer */

  /* Public: */
	void (*reset)(struct _sa_MD5 *p);	/* Reset the checksum */
	void (*add)(struct _sa_MD5 *p, ORD8 *buf, unsigned int len);	/* Add some bytes */
	void (*get)(struct _sa_MD5 *p, ORD8 chsum[16]);		/* Finalise and get the checksum */
	void (*del)(struct _sa_MD5 *p);		/* We're done with the object */

}; typedef struct _sa_MD5 sa_MD5;

/* Create a new MD5 checksumming object, with a reset checksum value */
/* Return it or NULL if there is an error. */
extern sa_MD5 *new_sa_MD5(void);

#define icmMD5 sa_MD5
#define new_icmMD5 new_sa_MD5

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* A subset of numlib */

int sa_lu_psinvert(double **out, double **in, int m, int n);

#define lu_psinvert sa_lu_psinvert

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef __cplusplus
	}
#endif

#endif /* SALONEINSTLIB */

#define SA_CONV_H
#endif /* SA_CONV_H */
