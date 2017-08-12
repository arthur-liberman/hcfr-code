#pragma once
#include "IHdrInterface.h"

class IApiInterface : public IHdrInterface
{
public:
	IApiInterface() : IHdrInterface(NULL, NULL) { }
	virtual ~IApiInterface() { }

	virtual bool MonitorExists(const char *deviceName) = 0;
	virtual bool Initialize() = 0;
};