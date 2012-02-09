// ColorHCFR Device Interface DLL
// This DLL is NOT under GNU License, and its source code is private.
// This is the needed interface for Mazet MTCS-C2 sensor.


#ifdef CHCFRDI3_EXPORTS
#define CHCFRDI3_API __declspec(dllexport)
#else
#define CHCFRDI3_API __declspec(dllimport)
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#define DI3ERR_SUCCESS						0
#define DI3ERR_NODLL						1
#define DI3ERR_INVALIDDLL					2
#define DI3ERR_MTCS							3
#define DI3ERR_NOTCONNECTED					4
#define DI3ERR_UNKNOWN						5

// Offsets for user memory access
#define DI3_USERMEMOFFSET_SHIFT				0
#define DI3_USERMEMOFFSET_TOLERANCE			1
#define DI3_USERMEMOFFSET_BLACK_OFFSETS		2
#define DI3_USERMEMOFFSET_BLACK_R_OFFSET	2
#define DI3_USERMEMOFFSET_BLACK_G_OFFSET	3
#define DI3_USERMEMOFFSET_BLACK_B_OFFSET	4

CHCFRDI3_API int InitDevice3();
CHCFRDI3_API BOOL ReleaseDevice3();
CHCFRDI3_API BOOL GetValuesDevice3(UINT nMeasureMode, UINT nAmpMode, UINT nShift, UINT nMilliseconds, DWORD * pAmpFactor, DWORD * pR, DWORD * pG, DWORD * pB, UINT * pNbTicks);
CHCFRDI3_API void DisconnectDevice3();
CHCFRDI3_API BOOL GetDevice3Matrix(short * pMatrixValues);
CHCFRDI3_API BOOL SetDevice3Matrix(const short * pMatrixValues);
CHCFRDI3_API BOOL GetDevice3BlackLevel(short * pBlackLevelValues);
CHCFRDI3_API BOOL SetDevice3BlackLevel(const short * pBlackLevelValues);
CHCFRDI3_API BOOL GetDevice3AmpLevel(short * pAmpLevel);
CHCFRDI3_API BOOL SetDevice3AmpLevel(const short * pAmpLevel);
CHCFRDI3_API BOOL ReadDevice3UserMemory(short * pData, int nOffset, int NbValues);
CHCFRDI3_API BOOL WriteDevice3UserMemory(const short * pData, int nOffset, int NbValues);

#ifdef __cplusplus
	} // extern "C"
#endif

