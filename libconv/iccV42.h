
/*
 * This file started out as the standard ICC header provided by
 * the ICC. Since this file no longer seems to be maintained,
 * I've added new entries from the ICC spec., but dropped
 * things that are not needed for icclib.
 *
 * This version of the header file corresponds to the profile
 * Specification ICC.1:2003-09 (Version 4.2.0)
 *
 * All header file entries are pre-fixed with "ic" to help 
 * avoid name space collisions. Signatures are pre-fixed with
 * icSig.
 *
 * Version numbers used within file are file format version numers,
 * not spec. version numbers.
 *
 * Portions of this file are Copyright 2004 - 2005 Graeme W. Gill,
 * This material is licensed with an "MIT" free use license:-
 * see the License.txt file in this directory for licensing details.
 *
 *  Graeme Gill.
 */

/* Header file guard bands */
#ifndef ICCV42_H
#define ICCV42_H

#ifdef __cplusplus
	extern "C" {
#endif

/***************************************************************** 
 Copyright (c) 1994-1998 SunSoft, Inc.

                    Rights Reserved

Permission is hereby granted, free of charge, to any person 
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restrict- 
ion, including without limitation the rights to use, copy, modify, 
merge, publish distribute, sublicense, and/or sell copies of the 
Software, and to permit persons to whom the Software is furnished 
to do so, subject to the following conditions: 
 
The above copyright notice and this permission notice shall be 
included in all copies or substantial portions of the Software. 
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-
INFRINGEMENT.  IN NO EVENT SHALL SUNSOFT, INC. OR ITS PARENT 
COMPANY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
OTHER DEALINGS IN THE SOFTWARE. 
 
Except as contained in this notice, the name of SunSoft, Inc. 
shall not be used in advertising or otherwise to promote the 
sale, use or other dealings in this Software without written 
authorization from SunSoft Inc. 
******************************************************************/

/* 
   Spec name/version/file version relationship.
   
   Spec. Name      Spec. Version    ICC File Version
   ----------      -------------    ----------------
   ICC.1:2004-10      4.2            4.2.0
   ICC.1:2003-09      4.1            4.1.0
   ICC.1:2001-12      4.0            4.0.0
   ICC.1:2001-04      (3.7?)         2.4.0
   ICC.1A:1999-04     (3.6?)         2.3.0
   ICC.1:1998-09      (3.5?)         2.2.0
   (Version 3.4)      3.4            2.1.0 (a)
   (Version 3.3)      3.3            2.1.0
   (Version 3.2)      3.2            2.0.0 (b)
   (Version 3.01)     3.01           2.0.0 (a)
   (Version 3.0)      3.0            2.0.0
*/


#define icMaxTagVal -1

/*------------------------------------------------------------------------*/

/*
 * Defines used in the specification
 */

#define icMagicNumber                   0x61637370L     /* 'acsp' */
#define icVersionNumber                 0x02200000L     /* 2.2.0, BCD */
#define icVersionNumberV41              0x04100000L     /* 4.1.0, BCD */

/* Screening Encodings */
#define icPrtrDefaultScreensFalse       0x00000000L     /* Bit pos 0 */
#define icPrtrDefaultScreensTrue        0x00000001L     /* Bit pos 0 */
#define icLinesPerInch                  0x00000002L     /* Bit pos 1 */
#define icLinesPerCm                    0x00000000L     /* Bit pos 1 */

/* 
 * Device attributes, currently defined values correspond
 * to the least-significant 4 bytes of the 8 byte attribute
 * quantity, see the header for their location.
 */

#define icReflective                    0x00000000L     /* Bit pos 0 */
#define icTransparency                  0x00000001L     /* Bit pos 0 */
#define icGlossy                        0x00000000L     /* Bit pos 1 */
#define icMatte                         0x00000002L     /* Bit pos 1 */
#define icPositive                      0x00000000L     /* Bit pos 2 */
#define icNegative                      0x00000004L     /* Bit pos 2 */
#define icColor                         0x00000000L     /* Bit pos 3 */
#define icBlackAndWhite                 0x00000008L     /* Bit pos 3 */

/*
 * Profile header flags, the least-significant 16 bits are reserved
 * for consortium use.
 */

#define icEmbeddedProfileFalse          0x00000000L     /* Bit pos 0 */
#define icEmbeddedProfileTrue           0x00000001L     /* Bit pos 0 */
#define icUseAnywhere                   0x00000000L     /* Bit pos 1 */
#define icUseWithEmbeddedDataOnly       0x00000002L     /* Bit pos 1 */

/* Ascii or Binary data */
#define icAsciiData                     0x00000000L 
#define icBinaryData                    0x00000001L

/* Phosphor or Colorant sets */
#define icPhColUnknown                  0x0000          /* Specified */
#define icPhColITU_R_BT_709             0x0001          /* ITU-R BT.709 */
#define icPhColSMPTE_RP145_1994         0x0002          /* SMPTE RP145-1994 */
#define icPhColEBU_Tech_3213_E          0x0003          /* EBU Tech.3213-E */
#define icPhColP22                      0x0004          /* P22 */

/* Video card gamma formats (ColorSync 2.5 specific) */
#define icVideoCardGammaTable           0x00000000
#define icVideoCardGammaFormula         0x00000001


/*------------------------------------------------------------------------*/
/*
 * Use this area to translate platform definitions of long
 * etc into icXXX form. The rest of the header uses the icXXX
 * typedefs. Signatures are 4 byte quantities.
 */
#ifdef ORD32			/* Formal sizes defined elsewhere */

typedef INR32           icSignature;

/* Unsigned integer numbers */
typedef ORD8            icUInt8Number;
typedef ORD16           icUInt16Number;
typedef ORD32           icUInt32Number;
typedef ORD32           icUInt64Number[2];

/* Signed numbers */
typedef INR8            icInt8Number;
typedef INR16           icInt16Number;
typedef INR32	        icInt32Number;
typedef INR32           icInt64Number[2];

/* Fixed numbers */
typedef INR32           icS15Fixed16Number;
typedef ORD32           icU16Fixed16Number;

#else   /* default definitions for typical 32 bit architecture */

typedef long            icSignature;

/* Unsigned integer numbers */
typedef unsigned char   icUInt8Number;
typedef unsigned short  icUInt16Number;
typedef unsigned long   icUInt32Number;
typedef unsigned long   icUInt64Number[2];

/* Signed numbers */
typedef char            icInt8Number;
typedef short           icInt16Number;
typedef long            icInt32Number;
typedef long            icInt64Number[2];

/* Fixed numbers */
typedef long            icS15Fixed16Number;
typedef unsigned long   icU16Fixed16Number;

#endif  /* Not formal */

/*------------------------------------------------------------------------*/
/* public tags and sizes */
typedef enum {
	icSigAToB0Tag                          = 0x41324230L,  /* 'A2B0' */ 
	icSigAToB1Tag                          = 0x41324231L,  /* 'A2B1' */
	icSigAToB2Tag                          = 0x41324232L,  /* 'A2B2' */ 
	icSigBlueMatrixColumnTag               = 0x6258595AL,  /* 'bXYZ' */
	icSigBlueTRCTag                        = 0x62545243L,  /* 'bTRC' */
	icSigBToA0Tag                          = 0x42324130L,  /* 'B2A0' */
	icSigBToA1Tag                          = 0x42324131L,  /* 'B2A1' */
	icSigBToA2Tag                          = 0x42324132L,  /* 'B2A2' */
	icSigCalibrationDateTimeTag            = 0x63616C74L,  /* 'calt' */
	icSigCharTargetTag                     = 0x74617267L,  /* 'targ' */ 
	icSigChromaticAdaptationTag            = 0x63686164L,  /* 'chad' V2.4+ */
	icSigChromaticityTag                   = 0x6368726DL,  /* 'chrm' V2.3+ */ 
	icSigColorantOrderTag                  = 0x636C726FL,  /* 'clro' V4.0+ */
	icSigColorantTableTag                  = 0x636C7274L,  /* 'clrt' V4.0+ */
	icSigColorantTableOutTag               = 0x636C6F74L,  /* 'clot' V4.0+ */
	icSigCopyrightTag                      = 0x63707274L,  /* 'cprt' */
	icSigCrdInfoTag                        = 0x63726469L,  /* 'crdi' V2.1 - V4.0 */
	icSigDataTag                           = 0x64617461L,  /* 'data' ??? */
	icSigDateTimeTag                       = 0x6474696DL,  /* 'dtim' ???  */
	icSigDeviceMfgDescTag                  = 0x646D6E64L,  /* 'dmnd' */
	icSigDeviceModelDescTag                = 0x646D6464L,  /* 'dmdd' */
	icSigDeviceSettingsTag                 = 0x64657673L,  /* 'devs' V2.2 - V4.0 */
	icSigGamutTag                          = 0x67616D74L,  /* 'gamt' */
	icSigGrayTRCTag                        = 0x6b545243L,  /* 'kTRC' */
	icSigGreenMatrixColumnTag              = 0x6758595AL,  /* 'gXYZ' */
	icSigGreenTRCTag                       = 0x67545243L,  /* 'gTRC' */
	icSigLuminanceTag                      = 0x6C756d69L,  /* 'lumi' */
	icSigMeasurementTag                    = 0x6D656173L,  /* 'meas' */
	icSigMediaBlackPointTag                = 0x626B7074L,  /* 'bkpt' */
	icSigMediaWhitePointTag                = 0x77747074L,  /* 'wtpt' */
	icSigNamedColorTag                     = 0x6E636f6CL,  /* 'ncol' V2.0 */
	icSigNamedColor2Tag                    = 0x6E636C32L,  /* 'ncl2' V2.1+ */
	icSigOutputResponseTag                 = 0x72657370L,  /* 'resp' V2.2+ */
	icSigPerceptualRenderingIntentGamutTag = 0x72696730L,  /* 'rig0' ??? */
	icSigPreview0Tag                       = 0x70726530L,  /* 'pre0' */
	icSigPreview1Tag                       = 0x70726531L,  /* 'pre1' */
	icSigPreview2Tag                       = 0x70726532L,  /* 'pre2' */
	icSigProfileDescriptionTag             = 0x64657363L,  /* 'desc' */
	icSigProfileSequenceDescTag            = 0x70736571L,  /* 'pseq' */
	icSigPs2CRD0Tag                        = 0x70736430L,  /* 'psd0' V2.0 - V4.0 */
	icSigPs2CRD1Tag                        = 0x70736431L,  /* 'psd1' V2.0 - V4.0 */
	icSigPs2CRD2Tag                        = 0x70736432L,  /* 'psd2' V2.0 - V4.0 */
	icSigPs2CRD3Tag                        = 0x70736433L,  /* 'psd3' V2.0 - V4.0 */
	icSigPs2CSATag                         = 0x70733273L,  /* 'ps2s' V2.0 - V4.0 */
	icSigPs2RenderingIntentTag             = 0x70733269L,  /* 'ps2i' V2.0 - V4.0 */
	icSigRedMatrixColumnTag                = 0x7258595AL,  /* 'rXYZ' */
	icSigRedTRCTag                         = 0x72545243L,  /* 'rTRC' */
	icSigSaturationRenderingIntentGamutTag = 0x72696732L,  /* 'rig2' ??? */
	icSigScreeningDescTag                  = 0x73637264L,  /* 'scrd' V2.0 - V4.0 */
	icSigScreeningTag                      = 0x7363726EL,  /* 'scrn' V2.0 - V4.0 */
	icSigTechnologyTag                     = 0x74656368L,  /* 'tech' */
	icSigUcrBgTag                          = 0x62666420L,  /* 'bfd ' V2.0 - V4.0 */
	icSigVideoCardGammaTag                 = 0x76636774L,  /* 'vcgt' ColorSync 2.5+ */
	icSigViewingCondDescTag                = 0x76756564L,  /* 'vued' */
	icSigViewingConditionsTag              = 0x76696577L,  /* 'view' */
	icMaxEnumTag                           = icMaxTagVal 
} icTagSignature;

/* Aliases for backwards compatibility */
#define icSigBlueColorantTag  icSigBlueMatrixColumnTag		/* Prior to V4.0 */
#define icSigGreenColorantTag icSigGreenMatrixColumnTag		/* Prior to V4.0 */
#define icSigRedColorantTag   icSigRedMatrixColumnTag		/* Prior to V4.0 */

/* technology signature descriptions */
typedef enum {
    icSigDigitalCamera                  = 0x6463616DL,  /* 'dcam' */
    icSigFilmScanner                    = 0x6673636EL,  /* 'fscn' */
    icSigReflectiveScanner              = 0x7273636EL,  /* 'rscn' */
    icSigInkJetPrinter                  = 0x696A6574L,  /* 'ijet' */ 
    icSigThermalWaxPrinter              = 0x74776178L,  /* 'twax' */
    icSigElectrophotographicPrinter     = 0x6570686FL,  /* 'epho' */
    icSigElectrostaticPrinter           = 0x65737461L,  /* 'esta' */
    icSigDyeSublimationPrinter          = 0x64737562L,  /* 'dsub' */
    icSigPhotographicPaperPrinter       = 0x7270686FL,  /* 'rpho' */
    icSigFilmWriter                     = 0x6670726EL,  /* 'fprn' */
    icSigVideoMonitor                   = 0x7669646DL,  /* 'vidm' */
    icSigVideoCamera                    = 0x76696463L,  /* 'vidc' */
    icSigProjectionTelevision           = 0x706A7476L,  /* 'pjtv' */
    icSigCRTDisplay                     = 0x43525420L,  /* 'CRT ' */
    icSigPMDisplay                      = 0x504D4420L,  /* 'PMD ' */
    icSigAMDisplay                      = 0x414D4420L,  /* 'AMD ' */
    icSigPhotoCD                        = 0x4B504344L,  /* 'KPCD' */
    icSigPhotoImageSetter               = 0x696D6773L,  /* 'imgs' */
    icSigGravure                        = 0x67726176L,  /* 'grav' */
    icSigOffsetLithography              = 0x6F666673L,  /* 'offs' */
    icSigSilkscreen                     = 0x73696C6BL,  /* 'silk' */
    icSigFlexography                    = 0x666C6578L,  /* 'flex' */
    icMaxEnumTechnology                 = icMaxTagVal   
} icTechnologySignature;

/* type signatures */
typedef enum {
    icSigChromaticityType               = 0x6368726DL,  /* 'chrm' */ 
    icSigColorantOrderType              = 0x636C726FL,  /* 'clro' V4.0+ */
    icSigColorantTableType              = 0x636C7274L,  /* 'clrt' V4.0+ */
    icSigCrdInfoType                    = 0x63726469L,  /* 'crdi' V2.1 - V4.0 */
    icSigCurveType                      = 0x63757276L,  /* 'curv' */
    icSigDataType                       = 0x64617461L,  /* 'data' */
    icSigDateTimeType                   = 0x6474696DL,  /* 'dtim' */
    icSigDeviceSettingsType             = 0x64657673L,  /* 'devs' V2.2 - V4.0 */
    icSigLut16Type                      = 0x6d667432L,  /* 'mft2' */
    icSigLut8Type                       = 0x6d667431L,  /* 'mft1' */
    icSigLutAtoBType                    = 0x6d414220L,  /* 'mAB ' V4.0+ */
    icSigLutBtoAType                    = 0x6d424120L,  /* 'mBA ' V4.0+ */
    icSigMeasurementType                = 0x6D656173L,  /* 'meas' */
    icSigMultiLocalizedUnicodeType      = 0x6D6C7563L,  /* 'mluc' V4.0+ */
    icSigNamedColorType                 = 0x6E636f6CL,  /* 'ncol' V2.0 */
    icSigNamedColor2Type                = 0x6E636C32L,  /* 'ncl2' V2.0+ */
    icSigParametricCurveType            = 0x70617261L,  /* 'para' V4.0+ */
    icSigProfileSequenceDescType        = 0x70736571L,  /* 'pseq' */
    icSigResponseCurveSet16Type         = 0x72637332L,  /* 'rcs2' V2.2 - V4.0 */
    icSigS15Fixed16ArrayType            = 0x73663332L,  /* 'sf32' */
    icSigScreeningType                  = 0x7363726EL,  /* 'scrn' V2.0 - V4.0 */
    icSigSignatureType                  = 0x73696720L,  /* 'sig ' */
    icSigTextType                       = 0x74657874L,  /* 'text' */
    icSigTextDescriptionType            = 0x64657363L,  /* 'desc' V2.0 - V2.4 */
    icSigU16Fixed16ArrayType            = 0x75663332L,  /* 'uf32' */
    icSigUcrBgType                      = 0x62666420L,  /* 'bfd ' V2.0 - V4.0 */
    icSigUInt16ArrayType                = 0x75693136L,  /* 'ui16' */
    icSigUInt32ArrayType                = 0x75693332L,  /* 'ui32' */
    icSigUInt64ArrayType                = 0x75693634L,  /* 'ui64' */
    icSigUInt8ArrayType                 = 0x75693038L,  /* 'ui08' */
    icSigVideoCardGammaType             = 0x76636774L,  /* 'vcgt' ColorSync 2.5+ */
    icSigViewingConditionsType          = 0x76696577L,  /* 'view' */
    icSigXYZType                        = 0x58595A20L,  /* 'XYZ ' */
    icSigXYZArrayType                   = 0x58595A20L,  /* 'XYZ ' */
    icMaxEnumType                       = icMaxTagVal   
} icTagTypeSignature;

/* 
 * Color Space Signatures
 * Note that only icSigXYZData and icSigLabData are valid
 * Profile Connection Spaces (PCSs)
 */ 
typedef enum {
    icSigXYZData                        = 0x58595A20L,  /* 'XYZ ' */
    icSigLabData                        = 0x4C616220L,  /* 'Lab ' */
    icSigLuvData                        = 0x4C757620L,  /* 'Luv ' */
    icSigYCbCrData                      = 0x59436272L,  /* 'YCbr' */
    icSigYxyData                        = 0x59787920L,  /* 'Yxy ' */
    icSigRgbData                        = 0x52474220L,  /* 'RGB ' */
    icSigGrayData                       = 0x47524159L,  /* 'GRAY' */
    icSigHsvData                        = 0x48535620L,  /* 'HSV ' */
    icSigHlsData                        = 0x484C5320L,  /* 'HLS ' */
    icSigCmykData                       = 0x434D594BL,  /* 'CMYK' */
    icSigCmyData                        = 0x434D5920L,  /* 'CMY ' */

    icSig2colorData                     = 0x32434C52L,  /* '2CLR' */
    icSig3colorData                     = 0x33434C52L,  /* '3CLR' */
    icSig4colorData                     = 0x34434C52L,  /* '4CLR' */
    icSig5colorData                     = 0x35434C52L,  /* '5CLR' */
    icSig6colorData                     = 0x36434C52L,  /* '6CLR' */
    icSig7colorData                     = 0x37434C52L,  /* '7CLR' */
    icSig8colorData                     = 0x38434C52L,  /* '8CLR' */
    icSig9colorData                     = 0x39434C52L,  /* '9CLR' */
    icSig10colorData                    = 0x41434C52L,  /* 'ACLR' */
    icSig11colorData                    = 0x42434C52L,  /* 'BCLR' */
    icSig12colorData                    = 0x43434C52L,  /* 'CCLR' */
    icSig13colorData                    = 0x44434C52L,  /* 'DCLR' */
    icSig14colorData                    = 0x45434C52L,  /* 'ECLR' */
    icSig15colorData                    = 0x46434C52L,  /* 'FCLR' */

    icSigMch5Data                       = 0x4D434835L,  /* 'MCH5' Colorsync ? */
    icSigMch6Data                       = 0x4D434836L,  /* 'MCH6' Hexachrome: CMYKOG */
    icSigMch7Data                       = 0x4D434837L,  /* 'MCH7' Colorsync ? */
    icSigMch8Data                       = 0x4D434838L,  /* 'MCH8' Colorsync ? */
	icSigNamedData                      = 0x6e6d636cL,  /* 'nmcl' ??? */

    icMaxEnumData                       = icMaxTagVal   
} icColorSpaceSignature;

/* profileClass enumerations */
typedef enum {
    icSigInputClass                     = 0x73636E72L,  /* 'scnr' */
    icSigDisplayClass                   = 0x6D6E7472L,  /* 'mntr' */
    icSigOutputClass                    = 0x70727472L,  /* 'prtr' */
    icSigLinkClass                      = 0x6C696E6BL,  /* 'link' */
    icSigAbstractClass                  = 0x61627374L,  /* 'abst' */
    icSigColorSpaceClass                = 0x73706163L,  /* 'spac' */
    icSigNamedColorClass                = 0x6e6d636cL,  /* 'nmcl' */
    icMaxEnumClass                      = icMaxTagVal  
} icProfileClassSignature;

/* Platform Signatures */
typedef enum {
    icSigMacintosh                      = 0x4150504CL,  /* 'APPL' */
    icSigMicrosoft                      = 0x4D534654L,  /* 'MSFT' */
    icSigSolaris                        = 0x53554E57L,  /* 'SUNW' */
    icSigSGI                            = 0x53474920L,  /* 'SGI ' */
    icSigTaligent                       = 0x54474E54L,  /* 'TGNT' */
    icMaxEnumPlatform                   = icMaxTagVal  
} icPlatformSignature;

/* Rendering Intent Gamut Signatures */
typedef enum {
    icSigPerceptualReferenceMediumGamut = 0x70726d67L,  /* 'prmg' */
    icMaxEnumReferenceMediumGamut       = icMaxTagVal  
} icReferenceMediumGamutSignature;

/*------------------------------------------------------------------------*/
/*
 * Other enums
 */

/* Measurement Flare, used in the measurmentType tag */
typedef enum {
    icFlare0                            = 0x00000000L,  /* 0% flare */
    icFlare100                          = 0x00000001L,  /* 100% flare */
    icMaxFlare                          = icMaxTagVal   
} icMeasurementFlare;

/* Measurement Geometry, used in the measurmentType tag */
typedef enum {
    icGeometryUnknown                   = 0x00000000L,  /* Unknown */
    icGeometry045or450                  = 0x00000001L,  /* 0/45, 45/0 */
    icGeometry0dord0                    = 0x00000002L,  /* 0/d or d/0 */
    icMaxGeometry                       = icMaxTagVal   
} icMeasurementGeometry;

/* Rendering Intents, used in the profile header */
typedef enum {
    icPerceptual                        = 0,
    icRelativeColorimetric              = 1,
    icSaturation                        = 2,
    icAbsoluteColorimetric              = 3,
    icMaxEnumIntent                     = icMaxTagVal   
} icRenderingIntent;

/* Different Spot Shapes currently defined, used for screeningType */
typedef enum {
    icSpotShapeUnknown                  = 0,
    icSpotShapePrinterDefault           = 1,
    icSpotShapeRound                    = 2,
    icSpotShapeDiamond                  = 3,
    icSpotShapeEllipse                  = 4,
    icSpotShapeLine                     = 5,
    icSpotShapeSquare                   = 6,
    icSpotShapeCross                    = 7,
    icMaxEnumSpot                       = icMaxTagVal   
} icSpotShape;

/* Standard Observer, used in the measurmentType tag */
typedef enum {
    icStdObsUnknown                     = 0x00000000L,  /* Unknown */
    icStdObs1931TwoDegrees              = 0x00000001L,  /* 2 deg */
    icStdObs1964TenDegrees              = 0x00000002L,  /* 10 deg */
    icMaxStdObs                         = icMaxTagVal   
} icStandardObserver;

/* Pre-defined illuminants, used in measurement and viewing conditions type */
typedef enum {
    icIlluminantUnknown                 = 0x00000000L,
    icIlluminantD50                     = 0x00000001L,
    icIlluminantD65                     = 0x00000002L,
    icIlluminantD93                     = 0x00000003L,
    icIlluminantF2                      = 0x00000004L,
    icIlluminantD55                     = 0x00000005L,
    icIlluminantA                       = 0x00000006L,
    icIlluminantEquiPowerE              = 0x00000007L,  
    icIlluminantF8                      = 0x00000008L,  
    icMaxEnumIlluminant                 = icMaxTagVal   
} icIlluminant;

/* A not so exhaustive list of language codes */
typedef enum {
	icLanguageCodeEnglish               = 0x656E, /* 'en' */
	icLanguageCodeGerman                = 0x6465, /* 'de' */
	icLanguageCodeItalian               = 0x6974, /* 'it' */
	icLanguageCodeDutch                 = 0x6E6C, /* 'nl' */
	icLanguageCodeSweden                = 0x7376, /* 'sv' */
	icLanguageCodeSpanish               = 0x6573, /* 'es' */
	icLanguageCodeDanish                = 0x6461, /* 'da' */
	icLanguageCodeNorwegian             = 0x6E6F, /* 'no' */
	icLanguageCodeJapanese              = 0x6A61, /* 'ja' */
	icLanguageCodeFinish                = 0x6669, /* 'fi' */
	icLanguageCodeTurkish               = 0x7472, /* 'tr' */
	icLanguageCodeKorean                = 0x6B6F, /* 'ko' */
	icLanguageCodeChienese              = 0x7A68, /* 'zh' */
	icLanguageCodeFrench                = 0x6672, /* 'fr' */
	icMaxEnumLanguageCode               = 0xFFFF
} icEnumLanguageCode;

/* A not so exhaustive list of region codes. */
typedef enum {
	icRegionCodeUSA                      = 0x5553, /* 'US' */
	icRegionCodeUnitedKingdom            = 0x554B, /* 'UK' */
	icRegionCodeGermany                  = 0x4445, /* 'DE' */
	icRegionCodeItaly                    = 0x4954, /* 'IT' */
	icRegionCodeNetherlands              = 0x4E4C, /* 'NL' */
	icRegionCodeSpain                    = 0x4543, /* 'ES' */
	icRegionCodeDenmark                  = 0x444B, /* 'DK' */
	icRegionCodeNorway                   = 0x4E4F, /* 'ND' */
	icRegionCodeJapan                    = 0x4A50, /* 'JP' */
	icRegionCodeFinland                  = 0x4649, /* 'FI' */
	icRegionCodeTurkey                   = 0x5452, /* 'TR' */
	icRegionCodeKorea                    = 0x4B52, /* 'KR' */
	icRegionCodeChina                    = 0x434E, /* 'CN' */
	icRegionCodeTaiwan                   = 0x5457, /* 'TW' */
	icRegionCodeFrance                   = 0x4652, /* 'FR' */
	icMaxEnumRegionCode                  = 0xFFFF
} icEnumRegionCode;

/* media type for icSigDeviceSettingsTag */
typedef enum {
    icStandard                          = 1,
    icTrans                             = 2, /* transparency */
    icGloss                             = 3,
    icUser1                             = 256,
    icMaxDeviceMedia                    = icMaxTagVal
} icDeviceMedia;

/* halftone settings for icSigDeviceSettingTag */
typedef enum {
    icNone                              = 1,
    icCoarse                            = 2,
    icFine                              = 3,
    icLineArt                           = 4,
    icErrorDiffusion                    = 5,
    icReserved6                         = 6,
    icReserved7                         = 7,
    icReserved8                         = 8,
    icReserved9                         = 9,
    icGrayScale                         = 10,
    icUser2                             = 256,
    icMaxDither                         = icMaxTagVal
} icDeviceDither;

/* signatures for icSigDeviceSettingsTag */
typedef enum {
    icSigResolution                     = 0x72736c6eL, /* 'rsln' */
    icSigMedia                          = 0x6d747970L, /* 'mtyp' */
    icSigHalftone                       = 0x6866746eL, /* 'hftn' */
    icMaxSettings                       = icMaxTagVal
} icSettingsSig;

/* measurement units for the icResponseCurveSet16Type */
typedef enum {
    icStaA                              = 0x53746141L, /* 'StaA' */
    icStaE                              = 0x53746145L, /* 'StaE' */
    icStaI                              = 0x53746149L, /* 'StaI' */
    icStaT                              = 0x53746154L, /* 'StaT' */
    icStaM                              = 0x5374614dL, /* 'StaM' */
    icDN                                = 0x444e2020L, /* 'DN ' */
    icDNP                               = 0x444e2050L, /* 'DN P' */
    icDNN                               = 0x444e4e20L, /* 'DNN ' */
    icDNNP                              = 0x444e4e50L, /* 'DNNP' */
    icMaxUnits                          = icMaxTagVal
} icMeasUnitsSig;

/* Parametric curve types for icSigParametricCurveType */
typedef enum {
    icCurveFunction1                    = 0x0000,
    icCurveFunction3                    = 0x0001,
    icCurveFunction4                    = 0x0002,
    icCurveFunction5                    = 0x0003,
    icCurveFunction7                    = 0x0004
} icParametricCurveFunctionType;

#ifdef __cplusplus
	}
#endif

#endif /* ICCV42_H */
