#pragma once
#include <Windows.h>

#ifdef LIBHDR_EXPORTS
#define LIBHDR_DLL_EXPORT __declspec(dllexport)
#else
#define LIBHDR_DLL_EXPORT
#endif

typedef enum
{
	HDR_OK, HDR_SET_FAILED = -100,
	HDR_DXGI_NOT_FOUND, HDR_CREATE_DXGI_FACTORY2_NOT_FOUND, HDR_CREATE_DXGI_FACTORY4_FAILED, HDR_CREATE_SWAP_CHAIN_FAILED,
	HDR_ADAPTERS_NOT_FOUND, HDR_DISPLAYS_NOT_FOUND, HDR_DISPLAY_INIT_FAILED, HDR_NV_DISPLAY_NOT_FOUND,
	HDR_D3D12_RUNTIME_NOT_FOUND, HDR_CREATE_D3D12_DEVICE_NOT_FOUND, HDR_CREATE_D3D12_DEVICE_FAILED, HDR_CREATE_D3D12_COMMAND_QUEUE_FAILED,
	HDR_SET_HDR_DX_FAILED, HDR_SET_HDR_NV_FAILED, HDR_SET_HDR_AMD_FAILED,
} HDR_STATUS;

typedef enum
{
	HDR_TYPE_NONE = 0x00, HDR_TYPE_HDR10 = 0x01, HDR_TYPE_HDR10_PLUS = 0x02, HDR_TYPE_DOLBY_VISION = 0x04, HDR_TYPE_HLG = 0x08,
} HDR_TYPE;

// Identical to DXGI_HDR_METADATA_HDR10, see MSDN for instructions.
typedef struct LIBHDR_HDR_METADATA_HDR10
{
	unsigned short	RedPrimary[ 2 ];
	unsigned short	GreenPrimary[ 2 ];
	unsigned short	BluePrimary[ 2 ];
	unsigned short	WhitePoint[ 2 ];
	unsigned int	MaxMasteringLuminance;
	unsigned int	MinMasteringLuminance;
	unsigned short	MaxContentLightLevel;
	unsigned short	MaxFrameAverageLightLevel;
} 	LIBHDR_HDR_METADATA_HDR10;

class IHdrInterface
{
public:
	IHdrInterface(HWND hWnd, HMONITOR hMonitor) : m_HdrMode(HDR_TYPE_NONE), m_hMonitor(hMonitor), m_hWnd(hWnd) { }
	virtual ~IHdrInterface() {}
	virtual HDR_TYPE GetSupportedHDRModes() const = 0;
	virtual HDR_TYPE GetCurrentHDRMode() const { return m_HdrMode; }
	virtual HMONITOR GetCurrentMonitor() const { return m_hMonitor; }
	virtual HWND GetCurrentWindow() const { return m_hWnd; }
	virtual void SetWindowMonitor(HWND hWnd, HMONITOR hMonitor) { m_hWnd = hWnd;  m_hMonitor = hMonitor; }
	virtual HDR_STATUS SetHDR10Mode(bool enable, const LIBHDR_HDR_METADATA_HDR10 &metaData) = 0;
	virtual int GetLastError() = 0;

protected:
	HWND m_hWnd;
	HMONITOR m_hMonitor;
	HDR_TYPE m_HdrMode;
};

IHdrInterface LIBHDR_DLL_EXPORT *GetNewHdrInterface(HWND hWnd, HMONITOR hMonitor);