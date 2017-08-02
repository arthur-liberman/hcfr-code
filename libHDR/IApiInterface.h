#pragma once
#include "IHdrInterface.h"

class IApiInterface : public IHdrInterface
{
public:
	IApiInterface() : IHdrInterface(NULL, NULL) { }
	virtual inline ~IApiInterface() { }

	virtual bool MonitorExists(const char *deviceName) = 0;
	virtual bool Initialize() = 0;
	virtual HDR_TYPE GetSupportedHDRModes() = 0;
	virtual HDR_STATUS SetHDR10Mode(bool enable, const LIBHDR_HDR_METADATA_HDR10 &metaData) = 0;
	virtual int GetLastError() = 0;
};