// ColorHCFR Device Interface DLL
// This DLL is NOT under GNU License, and its source code is private.
// This is the needed interface for Spyder II sensor.


#ifdef CHCFRDI1_EXPORTS
#define CHCFRDI1_API __declspec(dllexport)
#else
#define CHCFRDI1_API __declspec(dllimport)
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#define DI1ERR_SUCCESS		0
#define DI1ERR_NODLL		1
#define DI1ERR_INVALIDDLL	2
#define DI1ERR_SPYDER		3
#define DI1ERR_UNKNOWN		4

CHCFRDI1_API int InitDevice1(UINT nCalibrationMode);
CHCFRDI1_API BOOL ReleaseDevice1();
CHCFRDI1_API BOOL GetValuesDevice1(UINT nReadTime, double * pX, double * pY, double * pZ);

#ifdef __cplusplus
	} // extern "C"
#endif

