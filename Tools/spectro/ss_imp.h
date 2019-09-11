
#ifndef SS_IMP_H

/* 
 * Argyll Color Correction System
 *
 * Gretag Spectrolino and Spectroscan related
 * defines and declarations - implementation.
 *
 * Author: Graeme W. Gill
 * Date:   13/7/2005
 *
 * Copyright 2005 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * This is an alternative driver to spm/gretag.
 */

/* 
   If you make use of the instrument driver code here, please note
   that it is the author(s) of the code who take responsibility
   for its operation. Any problems or queries regarding driving
   instruments with the Argyll drivers, should be directed to
   the Argyll's author(s), and not to any other party.

   If there is some instrument feature or function that you
   would like supported here, it is recommended that you
   contact Argyll's author(s) first, rather than attempt to
   modify the software yourself, if you don't have firm knowledge
   of the instrument communicate protocols. There is a chance
   that an instrument could be damaged by an incautious command
   sequence, and the instrument companies generally cannot and
   will not support developers that they have not qualified
   and agreed to support.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* Communication symbol definitions */
/* From the Gretag Spectrolino/Spectroscan */
/* Serial Interface manual. */
/* We are using the Hex communication method */

/* Timout values for commands */
#define SH_TMO	0.5			/* Short timout for establishing communications */
#define IT_TMO	18.0		/* Initialisation commands */
#define MV_TMO	10.0		/* Move commands */
#define DF_TMO	6.0			/* Other commands */

/* Actual Filter Type */
typedef enum {
	ss_aft_NoDefined    = 0x00,
	ss_aft_NoFilter     = 0x01,
	ss_aft_PolFilter    = 0x02,
	ss_aft_D65Filter    = 0x03,
	ss_aft_UVCutFilter  = 0x05,
	ss_aft_CustomFilter = 0x06
} ss_aft;

/* Article Number Type */

/* Baudrate Type */
typedef enum {
	ss_bt_110   = 0x00,
	ss_bt_150   = 0x01,
	ss_bt_300   = 0x02,
	ss_bt_600   = 0x03,
	ss_bt_1200  = 0x04,
	ss_bt_2400  = 0x05,
	ss_bt_4800  = 0x06,
	ss_bt_9600  = 0x07,
	ss_bt_19200 = 0x08,
	ss_bt_28080 = 0x09,
	ss_bt_57600 = 0x0A
} ss_bt;

/* Color Type */

/* COM Float Type */
typedef enum {
	ss_comft_vPhotometricYRef = 0x01		/* cd/m^2 */
} ss_comft;

/* Control Type */
typedef enum {
	ss_ctt_ProtokolWithXonXoff    = 0x1E,
	ss_ctt_ProtokolWithoutXonXoff = 0x1F,
	ss_ctt_ProtokolWithHardwareHS = 0xCF,
	ss_ctt_SetBaud110             = 0x20,
	ss_ctt_SetBaud150             = 0x21,
	ss_ctt_SetBaud300             = 0x22,
	ss_ctt_SetBaud600             = 0x23,
	ss_ctt_SetBaud1200            = 0x24,
	ss_ctt_SetBaud2400            = 0x25,
	ss_ctt_SetBaud4800            = 0x26,
	ss_ctt_SetBaud9600            = 0x27,
	ss_ctt_SetBaud19200           = 0x28,
	ss_ctt_SetBaud28800           = 0x98,
	ss_ctt_SetBaud57600           = 0x99,
	ss_ctt_SpeakerON              = 0x54,
	ss_ctt_SpeakerOFF             = 0x55,
	ss_ctt_RemissionMeas          = 0x9B,
	ss_ctt_TransmissionMeas       = 0x9C,
	ss_ctt_EmissionMeas           = 0x9D,
	ss_ctt_PhotometricAbsolute    = 0x9E,
	ss_ctt_PhotometricRelative    = 0x9F,
	ss_ctt_SetCustomFilter        = 0xA0,
	ss_ctt_ReleaseCustomFilter    = 0xA1
} ss_ctt;

/* Color Space Type */
typedef enum {
	ss_cst_XyY       = 0x00,
	ss_cst_Lab       = 0x01,
	ss_cst_LChab     = 0x02,
	ss_cst_Luv       = 0x03,
	ss_cst_XYZ       = 0x04,
	ss_cst_RxRyRz    = 0x05,
	ss_cst_HLab      = 0x06,
	ss_cst_LABmg     = 0x0B,
	ss_cst_LCHmg     = 0x0C,
	ss_cst_LCHuv     = 0x0D
} ss_cst;

/* Date Type */

/* Density Filter Spectral Array Type */

/* Density Filter Array Type */

/* Density Filter Type */
typedef enum {
	ss_dft_Db    = 0x00,
	ss_dft_Dc    = 0x01,
	ss_dft_Dm    = 0x02,
	ss_dft_Dy    = 0x03,
	ss_dft_Dmax  = 0x04,
	ss_dft_Dauto = 0x05
} ss_dft;

/* Device Name Type */

/* Device Number Type */
typedef enum {
	ss_dnot_SPM10       = 0x00,
	ss_dnot_SPM50       = 0x01,
	ss_dnot_SPM55       = 0x02,
	ss_dnot_SPM60       = 0x03,
	ss_dnot_SPM100      = 0x04,
	ss_dnot_SPM100IInl  = 0x05,
	ss_dnot_SPM100II    = 0x06,
	ss_dnot_D196        = 0x10,
	ss_dnot_D19C        = 0x11,
	ss_dnot_D118C       = 0x12,
	ss_dnot_DM620       = 0x13,
	ss_dnot_SPECTROLINO = 0x20,
	ss_dnot_VIDEOLINO   = 0x30,
	ss_dnot_SPECTROSCAN = 0x40
} ss_dnot;

/* Dmax OK Type */
typedef enum {
	ss_dmot_FALSE = 0x00,
	ss_dmot_TRUE  = 0x01
} ss_dmot;

/* Dmax Type */

/* Density Standard Type */
typedef enum {
	ss_dst_ANSIA   = 0x00,
	ss_dst_ANSIT   = 0x01,
	ss_dst_DIN     = 0x02,
	ss_dst_DINNB   = 0x03,
	ss_dst_DS1     = 0x08		/* User defined */
} ss_dst;

/* Communication Error Type */
typedef enum {
	ss_cet_NoError            = 0x00,
	ss_cet_StopButNoStart     = 0x01,
	ss_cet_IllegalCharInRec   = 0x02,
	ss_cet_IncorrectRecLen    = 0x03,
	ss_cet_IllegalRecType     = 0x04,
	ss_cet_NoTagField         = 0x06,
	ss_cet_ConvError          = 0x07,
	ss_cet_InvalidForEmission = 0x08,
	ss_cet_NoAccess           = 0x10,
} ss_cet;

/* Daylight Color Temperature Type */

/* Error Type + augmentation */
typedef enum {
	ss_et_NoError         = 0x00,
	ss_et_MemoryFailure   = 0x01,
	ss_et_PowerFailure    = 0x02,
	ss_et_LampFailure     = 0x04,
	ss_et_HardwareFailure = 0x05,
	ss_et_FilterOutOfPos  = 0x06,
	ss_et_SendTimeout     = 0x07,
	ss_et_DriveError      = 0x08,
	ss_et_MeasDisabled    = 0x09,
	ss_et_DensCalError    = 0x0A,
	ss_et_EPROMFailure    = 0x0D,
	ss_et_RemOverFlow     = 0x0E,
	ss_et_MemoryError     = 0x10,
	ss_et_FullMemory      = 0x11,
	ss_et_WhiteMeasOK     = 0x13,
	ss_et_NotReady        = 0x15,
	ss_et_WhiteMeasWarn   = 0x32,
	ss_et_ResetDone       = 0x33,
	ss_et_EmissionCalOK   = 0x34,
	ss_et_OnlyEmission    = 0x35,
	ss_et_CheckSumWrong   = 0x36,
	ss_et_NoValidMeas     = 0x37,
	ss_et_BackupError     = 0x38,
	ss_et_ProgramROMError = 0x3C,

	/* Incororate Remote Error Set values into snerr value */
	/* Since ss_res is a bit mask, we just prioritize the errors: */
	ss_et_NoValidDStd           = 0x41,
	ss_et_NoValidWhite          = 0x42,
	ss_et_NoValidIllum          = 0x43,
	ss_et_NoValidObserver       = 0x44,
	ss_et_NoValidMaxLambda      = 0x45,
	ss_et_NoValidSpect          = 0x46,
	ss_et_NoValidColSysOrIndex  = 0x47,
	ss_et_NoValidChar           = 0x48,
	ss_et_DorlOutOfRange        = 0x49,
	ss_et_ReflectanceOutOfRange = 0x4A,
	ss_et_Color1OutOfRange      = 0x4B,
	ss_et_Color2OutOfRange      = 0x4C,
	ss_et_Color3OutOfRange      = 0x4D,
	ss_et_NotAnSROrBoolean      = 0x4E,
	ss_et_NoValidValOrRef       = 0x4F,

	/* Incorporate scan error codes thus: */
	ss_et_DeviceIsOffline    = 0x61,
	ss_et_OutOfRange         = 0x62,
	ss_et_ProgrammingError   = 0x63,
	ss_et_NoUserAccess       = 0x64,
	ss_et_NoValidCommand     = 0x65,
	ss_et_NoDeviceFound      = 0x66,
	ss_et_MeasurementError   = 0x67,
	ss_et_NoTransmTable      = 0x68,
	ss_et_NotInTransmMode    = 0x69,
	ss_et_NotInReflectMode   = 0x6A,

	/* Incorporate communication errors thus: */
	ss_et_StopButNoStart     = 0x81,
	ss_et_IllegalCharInRec   = 0x82,
	ss_et_IncorrectRecLen    = 0x83,
	ss_et_IllegalRecType     = 0x84,
	ss_et_NoTagField         = 0x86,
	ss_et_ConvError          = 0x87,
	ss_et_InvalidForEmission = 0x88,
	ss_et_NoAccess           = 0x90,

	/* Add our own communication errors here too. */
	ss_et_SerialFail         = 0xF0,
	ss_et_SendBufferFull     = 0xF5,
	ss_et_RecBufferEmpty     = 0xF6,
	ss_et_BadAnsFormat       = 0xF7,
	ss_et_BadHexEncoding     = 0xF8,
	ss_et_RecBufferOverun    = 0xF9
} ss_et;

/* Handshake Type */
typedef enum {
	ss_hst_None     = 0x00,
	ss_hst_XonXOff  = 0x01,
	ss_hst_Hardware = 0x02,
} ss_hst;

/* Illuminant Type */
typedef enum {
	ss_ilt_A       = 0x00,
	ss_ilt_C       = 0x01,
	ss_ilt_D65     = 0x02,
	ss_ilt_D50     = 0x03,
	ss_ilt_1       = 0x08,		/* User defined */
	ss_ilt_Dxx     = 0x10,		/* Variable daylight table */
	ss_ilt_F1      = 0x18,
	ss_ilt_F2      = 0x19,
	ss_ilt_F3      = 0x1A,
	ss_ilt_F4      = 0x1B,
	ss_ilt_F5      = 0x1C,
	ss_ilt_F6      = 0x1D,
	ss_ilt_F7      = 0x1E,
	ss_ilt_F8      = 0x1F,
	ss_ilt_F9      = 0x20,
	ss_ilt_F10     = 0x21,
	ss_ilt_F11     = 0x22,
	ss_ilt_F12     = 0x23
} ss_ilt;

/* Keyset - treat as cardinal */
typedef enum {
	ss_ks_NoKey          = 0x0000,
	ss_ks_MeasurementKey = 0x0080
} ss_ks;

/* Lambda Type */

/* Light Level Type */
typedef enum {
	ss_llt_AllOff   = 0x00,		/* All lights off during standby */
	ss_llt_Standby1 = 0x01,		/* Surround is on but measurement lamp is off */
	ss_llt_Standby2 = 0x02,		/* Surround is on and measurement lamp is on low */
} ss_llt;

/* Measurement Mode Type */
typedef enum {
	ss_mmt_NormalMeas        = 0x00,
	ss_mmt_WhiteCalibration  = 0x01,
	ss_mmt_WhiteCalWithWarn  = 0x07,
	ss_mmt_EmissionCal       = 0x08
} ss_mmt;

/* New Key Type */
typedef enum {
	ss_nkt_False  = 0x00,
	ss_nkt_True   = 0x01
} ss_nkt;

/* New Measurement Type */
typedef enum {
	ss_nmt_NoNewMeas      = 0x00,
	ss_nmt_NewMeas        = 0x01,
	ss_nmt_NewWhiteCal    = 0x02,
	ss_nmt_NewWhiteCalWW  = 0x03,
	ss_nmt_NewEmissionCal = 0x04
} ss_nmt;

/* Observer Type */
typedef enum {
	ss_ot_TwoDeg   = 0x00,
	ss_ot_TenDeg   = 0x01
} ss_ot;

/* Original White Reference Type */
typedef enum {
	ss_owrt_OriginalWhiteRef     = 0x00,
	ss_owrt_OriginalUserWhiteRef = 0x01,
	ss_owrt_NotDefWhiteRef       = 0x02
} ss_owrt;


/* Output Set Type */
typedef enum {
	ss_ost_ParameterSet = 0x00,	/* To define measurement of parameters for output */
	ss_ost_SpectrumSet  = 0x01,	/* To define spectra for output */
	ss_ost_CMetry1Set   = 0x02,	/* To define colorimetry values for output */
	ss_ost_CMetry2Set   = 0x03,	/* To define colorimetry values for output */
	ss_ost_DensitySet   = 0x04,	/* To define densitometry values for output */
	ss_ost_ErrorType    = 0xFF	/* To get the error of the measurement */
} ss_ost;

/* Output Parameter Set - bit masks */
typedef enum {
	ss_ops_None         = 0x00,
	ss_ops_DStdType     = 0x01,
	ss_ops_WBase      	= 0x02,
	ss_ops_Illuminant   = 0x04,
	ss_ops_Observer  	= 0x08,
	ss_ops_ActualFilter = 0x10
} ss_ops;

/* Output Spectrum Set - bit masks */
typedef enum {
	ss_oss_None        = 0x00,
	ss_oss_Spectrum    = 0x01,
	ss_oss_Density     = 0x02
} ss_oss;

/* Output Colorimetry 1 Set - bit masks */
typedef enum {
	ss_oc1s_None      = 0x00,
	ss_oc1s_xyY       = 0x01,
	ss_oc1s_Lab       = 0x02,
	ss_oc1s_LChab     = 0x04,
	ss_oc1s_Luv       = 0x08,
	ss_oc1s_XYZ       = 0x10,
	ss_oc1s_RxRyRz    = 0x20,
	ss_oc1s_HLab      = 0x40
} ss_oc1s;

/* Output Colorimetry 2 Set - bit masks */
typedef enum {
	ss_oc2s_None      = 0x00,
	ss_oc2s_LABmg     = 0x01,
	ss_oc2s_LCHmg     = 0x02,
	ss_oc2s_LChuv     = 0x04
} ss_oc2s;

/* Output Density Set - bit masks */
typedef enum {
	ss_ods_None       = 0x00,
	ss_ods_Black      = 0x01,
	ss_ods_Cyan       = 0x02,
	ss_ods_Magenta    = 0x04,
	ss_ods_Yellow     = 0x08,
	ss_ods_Max        = 0x10,
	ss_ods_Auto       = 0x20
} ss_ods;

/* Type that is one of the above, depending on what ss_ost is selected */
typedef union {
	ss_ods  od;
	ss_oss  os;
	ss_oc1s oc1;
	ss_oc2s oc2;
	ss_ops  op;
	int     i;
} ss_os;


/* Press Time Type */
typedef enum {
	ss_ptt_Short        = 0x00,
	ss_ptt_Long         = 0x01
} ss_ptt;

/* Reference Type */
typedef enum {
	ss_rt_SensorRef    = 0x00,
	ss_rt_SightRef     = 0x01
} ss_rt;

/* Reference Valid Type */
typedef enum {
	ss_rvt_False       = 0x00,
	ss_rvt_True        = 0x01
} ss_rvt;

/* Remaining Positions Type */

/* Remote Error Set - bit mask - treat as cardinal */
typedef enum {
	ss_res_NoError               = 0x0000,
	ss_res_NoValidDStd           = 0x0001,
	ss_res_NoValidWhite          = 0x0002,
	ss_res_NoValidIllum          = 0x0004,
	ss_res_NoValidObserver       = 0x0008,
	ss_res_NoValidMaxLambda      = 0x0010,
	ss_res_NoValidSpect          = 0x0020,
	ss_res_NoValidColSysOrIndex  = 0x0040,
	ss_res_NoValidChar           = 0x0080,
	ss_res_SlopeOutOfRange       = 0x0100,
	ss_res_DorlOutOfRange        = 0x0200,
	ss_res_ReflectanceOutOfRange = 0x0400,
	ss_res_Color1OutOfRange      = 0x0800,
	ss_res_Color2OutOfRange      = 0x1000,
	ss_res_Color3OutOfRange      = 0x2000,
	ss_res_NotAnSROrBoolean      = 0x4000,
	ss_res_NoValidValOrRef       = 0x8000,
} ss_res;

/* Scan Error Type */
typedef enum {
	ss_set_NoError               = 0x00,
	ss_set_DeviceIsOffline       = 0x01,
	ss_set_OutOfRange            = 0x02,
	ss_set_ProgrammingError      = 0x03,
	ss_set_NoUserAccess          = 0x04,
	ss_set_NoValidCommand        = 0x05,
	ss_set_NoDeviceFound         = 0x06,
	ss_set_MeasurementError      = 0x07,
	ss_set_NoTransmTable         = 0x08,
	ss_set_NotInTransmMode       = 0x09,
	ss_set_NotInReflectMode      = 0x0A
} ss_set;

/* Scan Key Set - bit mask */
typedef enum {
	ss_sks_EnterKey          = 0x01,
	ss_sks_PaperKey          = 0x02,
	ss_sks_OnlineKey         = 0x04,
	ss_sks_UpDownKey         = 0x08,
	ss_sks_MoveRightKey      = 0x10,
	ss_sks_MoveLeftKey       = 0x20,
	ss_sks_MoveDownKey       = 0x40,
	ss_sks_MoveUpKey         = 0x80
} ss_sks;

/* Serial Number Type */

/* Special Status Set - bit mask */
typedef enum {
	ss_sss_HeadDwnOnMove     = 0x01,	/* Don't lift the head when moving */
	ss_sss_TableInTransMode  = 0x10,	/* Table is set to transmission mode */
	ss_sss_AllLightsOn       = 0x20		/* Surround light on + low measure light */
} ss_sss;

/* Spect. Type */
typedef enum {
	ss_st_LinearSpectrum      = 0x00,
	ss_st_DensitySpectrum     = 0x01
} ss_st;

/* Status Mode Type */
typedef enum {
	ss_smt_InitAll           = 0x01,
	ss_smt_InitWithoutRemote = 0x05
} ss_smt;

/* Status Set - bit mask */
typedef enum {
	ss_sts_EnterKeyPressed   = 0x01,
	ss_sts_DeviceIsOnline    = 0x10,
	ss_sts_DigitizingModeOn  = 0x20,
	ss_sts_KeyAckModeOn      = 0x40,
	ss_sts_PaperIsHeld       = 0x80
} ss_sts;

/* Standard Density Filter Type */
typedef enum {
	ss_sdft_Db    = 0x00,
	ss_sdft_Dc    = 0x01,
	ss_sdft_Dm    = 0x02,
	ss_sdft_Dy    = 0x03
} ss_sdft;

/* Table Mode Type */
typedef enum {
	ss_tmt_Reflectance  = 0x00,
	ss_tmt_Transmission = 0x01
} ss_tmt;

/* Table Value Type */
typedef enum {
	ss_tvt_vDxx1  = 0x60
} ss_tvt;

/* Target On Off Status Type (Enables/Disables measurement key ?) */
typedef enum {
	ss_toost_Activated   = 0x00,
	ss_toost_Deactivated = 0x01
} ss_toost;

/* Target Tech Type */
typedef enum {
	ss_ttt_SPM          = 0x00,
	ss_ttt_D190         = 0x01,
	ss_ttt_Spectrolino  = 0x02,
	ss_ttt_Videolino    = 0x03,
	ss_ttt_SpectroScan  = 0x04
} ss_ttt;

/* White Base Type */
typedef enum {
	ss_wbt_Pap = 0x00,
	ss_wbt_Abs = 0x01
} ss_wbt;

/* White Reference Position Type */
typedef enum {
	ss_wrpt_RefTile1 = 0x00,
	ss_wrpt_RefTile2 = 0x01
} ss_wrpt;

/* Zed Koordinate Type Type */
typedef enum {
	ss_zkt_SensorUp   = 0x00,
	ss_zkt_SensorDown = 0x01
} ss_zkt;

/* Spectrolino request and answer types */
typedef enum {
	ss_ParameterRequest          = 0x00,
	ss_ParameterAnswer           = 0x0B,
	ss_SlopeRequest              = 0x01,
	ss_SlopeAnswer               = 0x0C,
	ss_DensityRequest            = 0x03,
	ss_DensityAnswer             = 0x0E,
	ss_DmaxRequest               = 0x04,
	ss_DmaxAnswer                = 0x0F,
	ss_SpectrumRequest           = 0x05,
	ss_SpectrumAnswer            = 0x10,
	ss_CRequest                  = 0x06,
	ss_CAnswer                   = 0x11,
	ss_NewMeasureRequest         = 0x07,
	ss_NewMeasureAnswer          = 0x12,
	ss_NewKeyRequest             = 0x08,
	ss_NewKeyAnswer              = 0x13,
	ss_ParameterDownload         = 0x16,
	ss_SlopeDownload             = 0x17,
	ss_DownloadError             = 0x1F,
	ss_ExecMeasurement           = 0x20,
	ss_ExecWhiteMeasurement      = 0x21,
	ss_ExecRefMeasurement        = 0x22,
	ss_ExecError                 = 0x25,
	ss_ActErrorRequest           = 0x29,
	ss_ActErrorAnswer            = 0x2F,
	ss_TargetIdRequest           = 0x2B,
	ss_TargetIdAnswer            = 0x31,
	ss_TargetOnOffStDownload     = 0x33,
	ss_IllumTabRequest           = 0x38,
	ss_IllumTabAnswer            = 0x39,
	ss_IllumTabDownload          = 0x3A,
	ss_DensTabRequest            = 0x3B,
	ss_DensTabAnswer             = 0x3C,
	ss_DensTabDownload           = 0x3D,
	ss_GetValNr                  = 0x47,
	ss_ValNrAnswer               = 0x48,
	ss_SetValNr                  = 0x49,
	ss_ExecWhiteRefToOrigDat     = 0x4A,
	ss_MeasControlDownload       = 0x4D,
	ss_ResetStatusDownload       = 0x5A,
	ss_MeasControlRequest        = 0x5B,
	ss_MeasControlAnswer         = 0x5C,
	ss_SetMeasurementOutput      = 0xB1,
	ss_WhiteReferenceRequest     = 0xB3,
	ss_WhiteReferenceAnswer      = 0xB4,
	ss_DeviceDataRequest         = 0xB5,
	ss_DeviceDataAnswer          = 0xB6,
	ss_WhiteReferenceDownld      = 0xB7,
	ss_SpecParameterRequest      = 0xB8,
	ss_SpecParameterAnswer       = 0xB9,
	ss_CParameterRequest         = 0xBA,
	ss_CParameterAnswer          = 0xBB,
	ss_DensityParameterAnswer    = 0xBC,
	ss_DensityParameterRequest   = 0xBD,
	ss_Printout                  = 0xBE,
	ss_FloatRequest              = 0xC0,
	ss_FloatAnswer               = 0xC1,
	ss_FloatDownload             = 0xC2,
	ss_COMErr                    = 0x26
} ss_so_cat;

/* Spectroscan request and answer types */
typedef enum {
	ss_ReqPFX                    = 0xD0,		/* Prefix */
	ss_AnsPFX                    = 0xD1,		/* Prefix */
	ss_MoveAbsolut               = 0x00,
	ss_MoveRelative              = 0x01,
	ss_MoveHome                  = 0x02,
	ss_MoveUp                    = 0x03,
	ss_MoveDown                  = 0x04,
	ss_OutputActualPosition      = 0x05,
	ss_MoveToWhiteRefPos         = 0x06,
	ss_MoveAndMeasure            = 0x07,
	ss_InitializeDevice          = 0x0A,
	ss_ScanSpectrolino           = 0x0B,
	ss_InitMotorPosition         = 0x0C,
	ss_SetTableMode              = 0x0D,
	ss_SetLightLevel             = 0x0E,
	ss_SetTransmStandbyPos       = 0x0F,
	ss_SetDeviceOnline           = 0x10,
	ss_SetDeviceOffline          = 0x11,
	ss_HoldPaper                 = 0x12,
	ss_ReleasePaper              = 0x13,
	ss_SetDigitizingMode         = 0x14,
	ss_OutputDigitizingValues    = 0x15,
	ss_SetKeyAcknowlge           = 0x16,
	ss_ResetKeyAcknowlge         = 0x17,
	ss_ChangeBaudRate            = 0x20,
	ss_ChangeHandshake           = 0x21,
	ss_OutputActualKey           = 0x22,
	ss_OutputLastKey             = 0x23,
	ss_OutputStatus              = 0x24,
	ss_ClearStatus               = 0x25,
	ss_SetSpecialStatus          = 0x26,
	ss_ClearSpecialStatus        = 0x27,
	ss_OutputSpecialStatus       = 0x28,
	ss_OutputType                = 0x30,
	ss_OutputSerialNumber        = 0x31,
	ss_OutputArticleNumber       = 0x32,
	ss_OutputProductionDate      = 0x33,
	ss_OutputSoftwareVersion     = 0x34,
	ss_ErrorAnswer               = 0x80,
	ss_PositionAnswer            = 0x81,
	ss_KeyAnswer                 = 0x82,
	ss_StatusAnswer              = 0x83,
	ss_TypeAnswer                = 0x90,
	ss_SerialNumberAnswer        = 0x91,
	ss_ArticleNumberAnswer       = 0x92,
	ss_ProductionDateAnswer      = 0x93,
	ss_SoftwareVersionAnswer     = 0x94,
	ss_SSCOMErr                  = 0xA0
} ss_ss_cat;

/* -------------------------- */
/* Interface declarations */

struct _ss;

/* ------------------------------------------- */
/* Serialisation for different types functions */

/* QUERY: */
/* Reset send buffer, and init with start character */
void ss_init_send(struct _ss *p);

/* Reset send buffer, and add an Spectrolino Request enum */
void ss_add_soreq(struct _ss *p, int rq);

/* Reset send buffer, and add an SpectroScan Request enum */
void ss_add_ssreq(struct _ss *p, int rq);

/* Add an int/enum/char into one byte type */
void ss_add_1(struct _ss *p, int c);

/* Add an int/enum into two byte type */
void ss_add_2(struct _ss *p, int s);

/* Add an int/enum into four byte type */
void ss_add_4(struct _ss *p, int i);

/* Add a double into four byte type */
void ss_add_double(struct _ss *p, double d);

/* Add an  ASCII string into the send buffer. */
/* The string will be padded with nul's up to len. */
void ss_add_string(struct _ss *p, char *t, int len);

/* - - - - - - - - - - - - - - - - - - - - - */
/* ANSWER: */

/* Return the first enum from the recieve buffer without removing it. */
int ss_peek_ans(struct _ss *p);

/* Remove a Spectrolino answer enum from the revieve buffer, */
/* and check it is correct.  */
void ss_sub_soans(struct _ss *p, int cv);

/* Remove a SpectroScan Prefix and answer enum from the revieve buffer, */
/* and check it is correct.  */
void ss_sub_ssans(struct _ss *p, int cv);

/* Remove an int/enum/char into one byte type */
int ss_sub_1(struct _ss *p);

/* Remove an int/enum into two byte type */
int ss_sub_2(struct _ss *p);

/* Remove an int/enum into four byte type */
int ss_sub_4(struct _ss *p);

/* Remove a double into four byte type */
double ss_sub_double(struct _ss *p);

/* Remove an  ASCII string from the receive buffer. */
/* The string will be terminated with a nul, so a buffer */
/* of len+1 should be provided to return the string in. */
void ss_sub_string(struct _ss *p, char *t, int len);

/* - - - - - - - - - - - - - - - - - - - - - */
/* ERROR CODES: */

/* Convert an ss error into an inst_error */
inst_code ss_inst_err(struct _ss *p);

/* Incorporate a error into the snerr value */
void ss_incorp_err(struct _ss *p, ss_et se);

/* Incororate Remote Error Set values into snerr value */
/* Since ss_res is a bit mask, we just prioritize the errors. */
void ss_incorp_remerrset(struct _ss *p, ss_res es);

/* Incorporate a scan error into the snerr value */
void ss_incorp_scanerr(struct _ss *p, ss_set se);

/* Incorporate a device communication error into the snerr value */
void ss_incorp_comerr(struct _ss *p, ss_cet se);

/* - - - - - - - - - - - - - - - - - - - - - */
/* EXECUTION: */

/* Interpret an icoms error into a SS error */
int icoms2ss_err(int se);

/* Terminate the send buffer, and then do a */
/* send/receieve to the device. */
/* Leave any error in p->snerr */
void ss_command(struct _ss *p, double tmo);

/* - - - - - - - - - - - - - - - - - - - - */
/* Device Initialisation and configuration */

/* Reset instrument */
inst_code so_do_ResetStatusDownload(
struct _ss *p,
ss_smt sm		/* Init all or all except communications */
);

/* Load various parameters, such as: */
/* comm flow control, baud rate, speaker, */
/* reflective/tranmission/emmission mode, */
/* custom filter on/off */
inst_code so_do_MeasControlDownload(
struct _ss *p,
ss_ctt ct			/* Control */
);

/* Query various current parameters, such as: */
/* comm flow control, baud rate, speaker, */
/* reflective/tranmission/emmission mode, */
/* custom filter on/off. */
inst_code so_do_MeasControlRequest(
struct _ss *p,
ss_ctt ct,		/* Control to query  */
ss_ctt *rct		/* Return current state */
);

/* Queries specific device data */
inst_code so_do_DeviceDataRequest(
struct _ss *p,
char dn[19],		/* Return the device name */
ss_dnot *dno,		/* Return device number */
char pn[9],			/* Return the part number */
unsigned int *sn,	/* Return serial number */
char sv[13]			/* Return software version */
);

/* Query special device data */
inst_code so_do_TargetIdRequest(
struct _ss *p,
char dn[19],	/* Return Device Name */
int *sn,		/* Return Serial Number (1-65535) */
int *sr,		/* Return Software Release */
int *yp,		/* Return Year of production (e.g. 1996) */
int *mp,		/* Return Month of production (1-12) */
int *dp,		/* Return Day of production (1-31) */
int *hp,		/* Return Hour of production (0-23) */
int *np,		/* Return Minuite of production (0-59) */
ss_ttt *tt,		/* Return Target Tech Type (SPM/Spectrolino etc.) */
int *fswl,		/* Return First spectral wavelength (nm) */
int *nosw,		/* Return Number of spectral wavelengths */
int *dpsw		/* Return Distance between spectral wavelengths (nm) */
);

/* - - - - - - - - - - - - - */
/* Measurement configuration */

/* Query the standard or user definable densitometric tables */
inst_code so_do_DensTabRequest(
struct _ss *p,
ss_dst ds,			/* Density standard (ANSI/DIN/User etc.) */
ss_dst *rds,		/* Return Density standard (ANSI/DIN/User etc.) */
double sp[4][36]	/* Return 4 * 36 spectral weighting values */
);

/* Download user definable densitometric tables */
inst_code so_do_DensTabDownload(
struct _ss *p,
double sp[4][36]	/* 4 * 36 spectral weighting values */
);

/* Set slope values for densitometry */
inst_code so_do_SlopeDownload(
struct _ss *p,
double dv[4]	/* Db Dc Dm Dy density values */
);

/* Query slope values of densitometry */
inst_code so_do_SlopeRequest(
struct _ss *p,
double dv[4]	/* Return Db Dc Dm Dy density values */
);

/* Set the colorimetric parameters */
inst_code so_do_ParameterDownload(
struct _ss *p,
ss_dst ds,	/* Density standard (ANSI/DIN etc.) */
ss_wbt wb,	/* White base (Paper/Absolute) */
ss_ilt it,	/* Illuminant type (A/C/D65 etc.) */
ss_ot  ot	/* Observer type (2deg/10deg) */
);

/* Query colorimetric parameters */
inst_code so_do_ParameterRequest(
struct _ss *p,
ss_dst *ds,		/* Return Density Standard */
ss_wbt *wb,		/* Return White Base */
ss_ilt *it,		/* Return Illuminant type (A/C/D65/User etc.) */
ss_ot  *ot,		/* Return Observer type (2deg/10deg) */
ss_aft *af		/* Return Filter being used (None/Pol/D65/UV/custom */
);

/* Query the standard or user defined illuminant tables (Colorimetry) */
inst_code so_do_IllumTabRequest(
struct _ss *p,
ss_ilt it,		/* Illuminant type (A/C/D65/User etc.) */
ss_ilt *rit,	/* Return Illuminant type (A/C/D65/User etc.) */
double sp[36]	/* Return 36 spectral values */
);

/* Download user definable illuminant tables (Colorimetry) */
inst_code so_do_IllumTabDownload(
struct _ss *p,
double sp[36]	/* 36 spectral values to set */
);

/* Query for the color temperature of daylight illuminant Dxx */
inst_code so_do_GetValNr(
struct _ss *p,
int *ct		/* Return color temperature in deg K/100 */
);

/* Download user definable illuminant tables (Colorimetry) */
inst_code so_do_SetValNr(
struct _ss *p,
int ct		/* Color temperature to set for illuminant Dxx in deg K/100 */
);

/* Queries the spectra of the white reference for the desired filter */
inst_code so_do_WhiteReferenceRequest(
struct _ss *p,
ss_aft af,		/* Filter being queried (None/Pol/D65/UV/custom */
ss_aft *raf,	/* Return filter being queried (None/Pol/D65/UV/custom */
double sp[36],	/* Return 36 spectral values */
ss_owrt *owr,	/* Return original white reference */
char dtn[19]	/* Return name of data table */
);

/* Load spectra of a user defined white reference for the desired filter. */
/* A name can be given to the white reference. */
inst_code so_do_WhiteReferenceDownld(
struct _ss *p,
ss_aft af,		/* Filter being set (None/Pol/D65/UV/custom */
double sp[36],	/* 36 spectral values being set */
char dtn[19]	/* Name for data table */
);

/* Query the reference value for the relative photometric (emission) reference value */
inst_code so_do_FloatRequest(
struct _ss *p,
ss_comft comf,		/* Choose common float type (PhotometricYRef) */
ss_comft *rcomf,	/* Return common float type (PhotometricYRef) */
double *comfv		/* Return the reference value */
);

/* Set the reference value for the relative photometric (emission) reference value */
inst_code so_do_FloatDownload(
struct _ss *p,
ss_comft comf,		/* Choose common float type (PhotometricYRef) */
double comfv		/* The reference value */
);

/* - - - - - - */
/* Calibration */

/* Reset the spectra of the respective white reference to the original data */
inst_code so_do_ExecWhiteRefToOrigDat(
struct _ss *p
);


/* Perform a Reference Measurement */
inst_code so_do_ExecRefMeasurement(
struct _ss *p,
ss_mmt mm	/* Measurement Mode (Meas/Cal etc.) */
);

/* Perform a White Measurement - not recommended */
/* (ExecRefMeasuremen is preferred instead) */
inst_code so_do_ExecWhiteMeasurement(struct _ss *p);

/* - - - - - - - - - - - - */
/* Performing measurements */

/* Perform a Measurement */
inst_code so_do_ExecMeasurement(struct _ss *p);

/* Define automatic output after each measurement */
/* [Note that dealing with the resulting measurement replies */
/*  isn't directly supported, currently.] */
inst_code so_do_SetMeasurementOutput(
struct _ss *p,
ss_ost os,		/* Type of output to request */
ss_os o			/* bitmask of output */
);

/* - - - - - - - - */
/* Getting results */

/* Query Density measurement results and associated parameters */
inst_code so_do_DensityParameterRequest(
struct _ss *p,
ss_cst *rct,	/* Return Color Type (Lab/XYZ etc.) */
double dv[4],	/* Return Db Dc Dm Dy density values */
ss_sdft *sdf,	/* Return Standard Density Filter (Db/Dc/Dm/Dy) */
ss_rvt *rv,		/* Return Reference Valid Flag */
ss_aft *af,		/* Return filter being used (None/Pol/D65/UV/custom */
ss_wbt *wb,		/* Return white base (Paper/Absolute) */
ss_dst *ds,		/* Return Density standard (ANSI/DIN/User etc.) */
ss_ilt *rit,	/* Return Illuminant type (A/C/D65/User etc.) */
ss_ot  *ot		/* Return Observer type (2deg/10deg) */
);

/* Query Densitometric measurement values - not recommended */
/* (DensityParameterRequest is preferred instead) */
inst_code so_do_DensityRequest(
struct _ss *p,
double dv[4],	/* Return Db Dc Dm Dy density values */
ss_sdft *sdf,	/* Return Standard Density Filter (Db/Dc/Dm/Dy) */
ss_rvt  *rv		/* Return Reference Valid */
);

/* Query maximum density reading */
inst_code so_do_DmaxRequest(
struct _ss *p,
double *Dmax,	/* Return Value of Maximum Density */
int *lambda,	/* Return wavelength where maximum density was found */
ss_dmot *dmo,	/* Return Dmax OK flag. */
ss_rvt *rv		/* Return Reference Valid Flag */
);

/* Query Color measurement results and associated parameters */
inst_code so_do_CParameterRequest(
struct _ss *p,
ss_cst ct,		/* Choose Color Type (Lab/XYZ etc.) */
ss_cst *rct,	/* Return Color Type (Lab/XYZ etc.) */
double cv[3],	/* Return 3 color values */
ss_rvt *rv,		/* Return Reference Valid Flag */
ss_aft *af,		/* Return filter being used (None/Pol/D65/UV/custom) */
ss_wbt *wb,		/* Return white base (Paper/Absolute) */
ss_ilt *it,		/* Return Illuminant type (A/C/D65/User etc.) */
ss_ot  *ot		/* Return Observer type (2deg/10deg) */
);

/* Query Colorimetric measurement results - not recommended */
/* (CParameterRequest is prefered instead) */
inst_code so_do_CRequest(
struct _ss *p,
ss_cst *ct,		/* Return Color Type (Lab/XYZ etc.) */
double *cv[3],	/* Return 3 color values */
ss_rvt *rv		/* Return Reference Valid Flag */
);

/* Query Spectral measurement results and associated parameters */
inst_code so_do_SpecParameterRequest(
struct _ss *p,
ss_st st,		/* Choose Spectrum Type (Reflectance/Density) */
ss_st *rst,		/* Return Spectrum Type (Reflectance/Density) */
double sp[36],	/* Return 36 spectral values */
ss_rvt *rv,		/* Return Reference Valid Flag */
ss_aft *af,		/* Return filter being used (None/Pol/D65/UV/custom */
ss_wbt *wb		/* Return white base (Paper/Absolute) */
);

/* Query Spectral measurement results - not recommended */
/* (SpecParameterRequest is preferred instead) */
inst_code so_do_SpectrumRequest(
struct _ss *p,
ss_st *st,		/* Return Spectrum Type (Reflectance/Density) */
double sp[36],	/* Return 36 spectral values */
ss_rvt *rv		/* Return Reference Valid Flag */
);

/* - - - - - -  */
/* Miscelanious */

/* Query whether a new measurement was performed since the last accestruct _ss */
inst_code so_do_NewMeasureRequest(
struct _ss *p,
ss_nmt *nm		/* Return New Measurement (None/Meas/White etc.) */
);

/* Query whether a key was pressed since the last accestruct _ss */
inst_code so_do_NewKeyRequest(
struct _ss *p,
ss_nkt *nk,		/* Return whether a new key was pressed */
ss_ks *k		/* Return the key that was pressed (none/meas) */
);

/* Query for the general error status */
inst_code so_do_ActErrorRequest(
struct _ss *p
);

/* Set Target On/Off status (Enables/Disables measurement key ?) */
inst_code so_do_TargetOnOffStDownload(
struct _ss *p,
ss_toost oo		/* Activated/Deactivated */
);

/* =========================================== */
/* SpectroScan/T specific commands and queries */

/* - - - - - - - - - - - - - - - - - - - - */
/* Device Initialisation and configuration */

/* Initialise the device. Scans the Spectrolino */
/* (Doesn't work when device is offline, */
/* takes some seconds for the device to recover after reply.) */
inst_code ss_do_ScanInitializeDevice(struct _ss *p);

/* Establish communications between the SpectroScan and Spectrolino */
/* at the highest possible baud rate. */
/* (Doesn't work when device is offline ) */
inst_code ss_do_ScanSpectrolino(struct _ss *p);

/* Establish the zero position of the motors and set the position to 0,0 */ 
/* (Doesn't work when device is offline ) */
inst_code ss_do_InitMotorPosition(struct _ss *p);

/* Change the SpectroScan baud rate */
inst_code ss_do_ChangeBaudRate(
struct _ss *p,
ss_bt br		/* Baud rate (110 - 57600) */
);

/* Change the SpectroScan handshaking mode. */
inst_code ss_do_ChangeHandshake(
struct _ss *p,
ss_hst hs		/* Handshake type (None/XonXoff/HW) */
);

/* Query the type of XY table */
inst_code ss_do_OutputType(
struct _ss *p,
char dt[19]		/* Return Device Type ("SpectroScan " or "SpectroScanT") */
);

/* Query the serial number of the XY table */
inst_code ss_do_OutputSerialNumber(
struct _ss *p,
unsigned int *sn	/* Return serial number */
);

/* Query the part number of the XY table */
inst_code ss_do_OutputArticleNumber(
struct _ss *p,
char pn[9]		/* Return Part Number */
);

/* Query the production date of the XY table */
inst_code ss_do_OutputProductionDate(
struct _ss *p,
int *yp,	/* Return Year of production (e.g. 1996) */
int *mp,	/* Return Month of production (1-12) */
int *dp		/* Return Day of production (1-31) */
);

/* Query the Software Version of the XY table */
inst_code ss_do_OutputSoftwareVersion(
struct _ss *p,
char sv[13]		/* Return Software Version */
);

/* - - - - - - - - - - - - - */
/* Measurement configuration */

/* Set the SpectroScanT to reflectance or transmission. */
/* The Spectrolino is also automatically set to the corresponding mode. */
/* (Doesn't work when device is offline ) */
inst_code ss_do_SetTableMode(
struct _ss *p,
ss_tmt tm	/* Table mode (Reflectance/Transmission) */
);

/* - - - - - - - - - - - - - */
/* Table operation */

/* Set the SpectrScan to online. All moving keys are disabled. */
/* (Only valid when device is in reflectance mode.) */
inst_code ss_do_SetDeviceOnline(struct _ss *p);

/* Set the SpectrScan to offline. All moving keys are enabled. */
/* (Only valid when device is in reflectance mode.) */
/* (All remote commands to move the device will be ignored.) */
inst_code ss_do_SetDeviceOffline(struct _ss *p);

/* Enable electrostatic paper hold. */
/* (Not valid when device is offline) */
inst_code ss_do_HoldPaper(struct _ss *p);

/* Disable electrostatic paper hold. */
/* (Not valid when device is offline) */
inst_code ss_do_ReleasePaper(struct _ss *p);

/* - - - - - - */
/* Positioning */

/* Move either the sight or sensor to an absolute position. */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveAbsolut(
struct _ss *p,
ss_rt  r,	/* Reference (Sensor/Sight) */
double x,	/* X coord in mm, 0-310.0, accurate to 0.1mm */
double y	/* Y coord in mm, 0-230.0, accurate to 0.1mm */
);

/* Move relative to current position. */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveRelative(
struct _ss *p,
double x,	/* X distance in mm, 0-310.0, accurate to 0.1mm */
double y	/* Y distance in mm, 0-230.0, accurate to 0.1mm */
);

/* Move to the home position (== 0,0). */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveHome(
struct _ss *p
);

/* Move to the sensor up. */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveUp(
struct _ss *p
);

/* Move to the sensor down. */
/* (Doesn't work when device is offline or transmission mode.) */
inst_code ss_do_MoveDown(
struct _ss *p
);

/* Query the current absolute position of the sensor or sight. */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_OutputActualPosition(
struct _ss *p,
ss_rt  r,	/* Reference (Sensor/Sight) */
ss_rt  *rr,	/* Return reference (Sensor/Sight) */
double *x,	/* Return the X coord in mm, 0-310.0, accurate to 0.1mm */
double *y,	/* Return the Y coord in mm, 0-230.0, accurate to 0.1mm */
ss_zkt *zk	/* Return the Z coordinate (Up/Down) */
);

/* Move to the white reference position */
/* (Doesn't work when device is offline or transmissioin mode.) */
inst_code ss_do_MoveToWhiteRefPos(
struct _ss *p,
ss_wrpt wrp		/* White Reference Position (Tile1/Tile2) */
);

/* - - - - - - - - - - - - */
/* Performing measurements */

/* Move the sensor to an absolute position, move the */
/* sensor down, execute a measurement, move the head up, */
/* and return spectral measuring results. */
inst_code ss_do_MoveAndMeasure(
struct _ss *p,
double x,		/* X coord in mm, 0-310.0, accurate to 0.1mm */
double y,		/* Y coord in mm, 0-230.0, accurate to 0.1mm */
double sp[36],	/* Return 36 spectral values */
ss_rvt *rv		/* Return Reference Valid Flag */
);

/* - - - - - -  */
/* Miscelanious */

/* Set the SpectroScanT transmission light level during standby. */
/* (Only valid on SpectroScanT in transmission mode) */
inst_code ss_do_SetLightLevel(
struct _ss *p,
ss_llt ll	/* Transmission light level (Off/Surround/Low) */
);

/* Set tranmission standby position. */
/* (Only valid on SpectroScanT in transmission mode) */
inst_code ss_do_SetTransmStandbyPos(
struct _ss *p,
ss_rt  r,	/* Reference (Sensor/Sight) */
double x,	/* X coord in mm, 0-310.0, accurate to 0.1mm */
double y	/* Y coord in mm, 0-230.0, accurate to 0.1mm */
);

/* Set digitizing mode. Clears digitizing buffer, */
/* and puts the device offline. The user can move */
/* and enter positions. */
inst_code ss_do_SetDigitizingMode(struct _ss *p);

/* Get last digitized position from memory. */
inst_code ss_do_OutputDigitizingValues(
struct _ss *p,
ss_rt  r,	/* Reference (Sensor/Sight) */
ss_rt  *rr,	/* Return reference (Sensor/Sight) */
int    *nrp,/* Return the number of remaining positions in memory. */
double *x,	/* Return the X coord in mm, 0-310.0, accurate to 0.1mm */
double *y,	/* Return the Y coord in mm, 0-230.0, accurate to 0.1mm */
ss_zkt *zk	/* Return the Z coordinate (Up/Down) */
);

/* Turn on key aknowledge mode. Causes a KeyAnswer message */
/* to be generated whenever a key is pressed. */
/* (KetAnswer isn't well supported here ?) */
inst_code ss_do_SetKeyAcknowlge(struct _ss *p);

/* Turn off key aknowledge mode. */
inst_code ss_do_ResetKeyAcknowlge(struct _ss *p);

/* Query the keys that are currently pressed */
inst_code ss_do_OutputActualKey(
struct _ss *p,
ss_sks *sk,	/* Return Scan Key Set (Key bitmask) */
ss_ptt *pt	/* Return press time (Short/Long) */
);

/* Query the keys that were last pressed */
inst_code ss_do_OutputLastKey(
struct _ss *p,
ss_sks *sk,	/* Return Scan Key bitmask (Keys) */
ss_ptt *pt	/* Return press time (Short/Long) */
);

/* Query the status register */
inst_code ss_do_OutputStatus(
struct _ss *p,
ss_sts *st	/* Return status bitmask (Enter key/Online/Digitize/KeyAck/Paper) */
);

/* Clear the status register */
inst_code ss_do_ClearStatus(
struct _ss *p,
ss_sts st	/* Status to reset (Enter key/Online/Digitize/KeyAck/Paper) */
);

/* Set the special status register */
/* (Set to all 0 on reset) */
inst_code ss_do_SetSpecialStatus(
struct _ss *p,
ss_sss sss	/* Status bits to set (HeadDwnOnMv/TableInTransMode/AllLightsOn) */
);

/* Clear the special status register */
inst_code ss_do_ClearSpecialStatus(
struct _ss *p,
ss_sss sss	/* Status bits to clear (HeadDwnOnMv/TableInTransMode/AllLightsOn) */
);

/* Query the special status register */
inst_code ss_do_OutputSpecialStatus(
struct _ss *p,
ss_sss *sss	/* Return Special Status bits */
);

#ifdef __cplusplus
	}
#endif

#define SS_IMP_H
#endif /* SS_IMP_H */
