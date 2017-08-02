#pragma once
#include "IApiInterface.h"
#include "win10sdk.h"
#include <nvapi.h>

class NvApi : public IApiInterface
{
public:
	NvApi();
	virtual ~NvApi();

	bool MonitorExists(const char *deviceName);
	bool Initialize();
	HDR_TYPE GetSupportedHDRModes();
	HDR_STATUS SetHDR10Mode(bool enable, const LIBHDR_HDR_METADATA_HDR10 &metaData);
	inline int GetLastError() { return m_LastError; }

private:
	DXGI_OUTPUT_DESC m_OutputDesc;
	NvU32 m_DisplayId;
	int  m_LastError;

	void convertMetaDataStruct(NV_HDR_COLOR_DATA &destination, const LIBHDR_HDR_METADATA_HDR10 &source);
};

