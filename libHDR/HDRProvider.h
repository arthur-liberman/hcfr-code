#pragma once
#include "IApiInterface.h"
#include "win10sdk.h"
#include <string>

typedef HRESULT(WINAPI *PFN_CreateDXGIFactory2)(UINT Flags, REFIID riid, _COM_Outptr_ void **ppFactory);

class HDRProvider : public IHdrInterface
{
public:
	HDRProvider(HWND hWnd, HMONITOR hMonitor);
	virtual ~HDRProvider();
	virtual HDR_TYPE GetSupportedHDRModes() const;
	virtual HDR_TYPE GetCurrentHDRMode() const;
	virtual void SetWindowMonitor(HWND hWnd, HMONITOR hMonitor);
	virtual HDR_STATUS SetHDR10Mode(bool enable, const LIBHDR_HDR_METADATA_HDR10 &metaData);
	virtual int GetLastError() { return m_LastError; }

private:
	HDR_STATUS  setDirectX(bool hdrOn);
	void cleanUpDirectX();
	void fullCleanUp();

	HDR_STATUS initDxgiFactory();
	HDR_STATUS initAdapters();
	HDR_STATUS mapDisplayToAdapter();
	HDR_STATUS initMonitor(const DXGI_OUTPUT_DESC &outputDesc);
	HDR_STATUS initApiInterface();
	HDR_STATUS initDx12();
	HDR_STATUS initSwapChain(bool hdrOn);
	HDR_STATUS setDxHDRMode(bool hdrOn, const LIBHDR_HDR_METADATA_HDR10 &metaData);

	DXGI_OUTPUT_DESC m_OutputDesc;
	RECT m_MonitorResolution;
	std::string m_MonitorDeviceName;

	HMODULE m_Dxgi_Library;
	HMODULE m_D12_Library;

	IDXGIFactory4 *m_Factory4;

	std::vector<IDXGIAdapter1 *> m_Adapters;
	IDXGIAdapter1 *m_ActiveAdapter;
	ID3D12Device *m_D3D12Device;
	ID3D12CommandQueue *m_CommandQueue;
	IDXGISwapChain4 *m_SwapChain4;

	bool m_DxReady;
	int  m_LastError;

	PFN_CreateDXGIFactory2 pfn_CreateDXGIFactory2;
	PFN_D3D12_CREATE_DEVICE pfn_D3D12CreateDevice;

	IApiInterface *m_ApiInterface;
};