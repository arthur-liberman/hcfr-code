// ColorHCFR Device Interface DLL
// This DLL is NOT under GNU License, and its source code is private.
// This is the needed interface for Spyder III sensor.


#ifdef CHCFRDI4_EXPORTS
#define CHCFRDI4_API __declspec(dllexport)
#else
#define CHCFRDI4_API __declspec(dllimport)
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#define DI4ERR_SUCCESS		0
#define DI4ERR_NODLL		1
#define DI4ERR_INVALIDDLL	2
#define DI4ERR_SPYDER		3
#define DI4ERR_UNKNOWN		4

CHCFRDI4_API int InitDevice4(BOOL bStopLED);
CHCFRDI4_API BOOL ReleaseDevice4();
CHCFRDI4_API BOOL GetValuesDevice4(UINT nReadTime, double * pX, double * pY, double * pZ);

#ifdef __cplusplus
	} // extern "C"
#endif

