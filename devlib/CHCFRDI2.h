// ColorHCFR Device Interface DLL
// This DLL is NOT under GNU License, and its source code is private.
// This is the needed interface for Eye One sensor.


#ifdef CHCFRDI2_EXPORTS
#define CHCFRDI2_API __declspec(dllexport)
#else
#define CHCFRDI2_API __declspec(dllimport)
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#define DI2ERR_SUCCESS		0
#define DI2ERR_NODLL		1
#define DI2ERR_INVALIDDLL	2
#define DI2ERR_EYEONE		3
#define DI2ERR_NOTCONNECTED	4
#define DI2ERR_UNKNOWN		5

CHCFRDI2_API int InitDevice2(UINT nCalibrationMode, BOOL bAmbientLight, LPSTR lpszErrorMsg);
CHCFRDI2_API BOOL ReleaseDevice2();
CHCFRDI2_API BOOL GetValuesDevice2(double * pX, double * pY, double * pZ, double * pSpectrum, BOOL * pbSpectrumOk);
CHCFRDI2_API BOOL CalibrateDevice2(LPSTR lpszErrorMsg);
CHCFRDI2_API BOOL Device2NeedCalibration();

#ifdef __cplusplus
	} // extern "C"
#endif

