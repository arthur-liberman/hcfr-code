// HDRProvider.cpp : Defines the exported classes for the DLL application.
//

#include "HDRProvider.h"
#include "NvApiWrapper.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x)    \
   if(x != nullptr)        \
   {                       \
      x->Release();        \
      x = nullptr;         \
   }
#endif

#ifndef SAFE_FREE_LIB
#define SAFE_FREE_LIB(x)   \
	if (x != nullptr)	   \
	{					   \
		FreeLibrary(x);	   \
		x = nullptr;	   \
	}
#endif

#ifndef SAFE_ARRAY_FREE
#define SAFE_ARRAY_FREE(x) \
   if(x != NULL)           \
   {                       \
      free(x);             \
      x = NULL;            \
   }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(x)     \
   if(x != NULL)           \
   {                       \
      delete x;            \
      x = NULL;            \
   }
#endif

#ifndef INIT_STRUCT
#define INIT_STRUCT(x, size) \
	ZeroMemory(&x, sizeof(x)); \
	x.size = sizeof(x);
#endif

IHdrInterface LIBHDR_DLL_EXPORT *GetHdrInterface(HWND hWnd, HMONITOR hMonitor)
{
	static IHdrInterface *hdrInterface = nullptr;
	if (hdrInterface == nullptr)
		hdrInterface = new HDRProvider(hWnd, hMonitor);

	return hdrInterface;
}

HDRProvider::HDRProvider(HWND hWnd, HMONITOR hMonitor)
	: IHdrInterface(hWnd, hMonitor)
{
	m_LastError = S_OK;
	m_DxReady = false;
	m_Dxgi_Library = nullptr;
	m_D12_Library = nullptr;
	m_D3D12Device = nullptr;
	m_CommandQueue = nullptr;
	m_SwapChain4 = nullptr;
	m_ActiveAdapter = nullptr;
	m_ApiInterface = nullptr;

	m_hWnd = hWnd;
	m_hMonitor = hMonitor;
	ZeroMemory(&m_MonitorResolution, sizeof(RECT));
}

HDRProvider::~HDRProvider()
{
	LIBHDR_HDR_METADATA_HDR10 metaData = {0};
	SetHDR10Mode(false, metaData);
	cleanUpDirectX();
	SAFE_DELETE(m_ApiInterface);
}

HDR_TYPE HDRProvider::GetSupportedHDRModes()
{
	return m_ApiInterface ? m_ApiInterface->GetSupportedHDRModes() : HDR_TYPE_HDR10;
}

HDR_STATUS HDRProvider::SetHDR10Mode(bool enable, const LIBHDR_HDR_METADATA_HDR10 &metaData)
{
	HDR_STATUS ret = HDR_OK;

	ret = setDirectX(enable);
	if (SUCCEEDED(ret))
	{
		if (m_ApiInterface)
			ret = m_ApiInterface->SetHDR10Mode(enable, metaData);
		else
			ret = setDxHDRMode(enable, metaData);
	}

	return ret;
}

HDR_STATUS HDRProvider::setDirectX(bool hdrOn)
{
	HDR_STATUS ret = HDR_OK;

	if (!m_DxReady)
	{
		ret = initDxgiFactory();
		if (SUCCEEDED(ret))
			ret = initAdapters();
		if (SUCCEEDED(ret))
			ret = mapDisplayToAdapter();
		if (SUCCEEDED(ret))
			ret = initApiInterface();
		if (SUCCEEDED(ret))
			ret = initDx12();
	}

	if (SUCCEEDED(ret))
	{
		SAFE_RELEASE(m_SwapChain4);
		ret = initSwapChain(hdrOn);
		m_DxReady = SUCCEEDED(ret);
	}

	return ret;
}

HDR_STATUS HDRProvider::initDxgiFactory()
{
	HDR_STATUS ret = HDR_DXGI_NOT_FOUND;

	m_LastError = S_OK;
	m_Dxgi_Library = LoadLibrary(TEXT("dxgi.dll"));
	if (m_Dxgi_Library)
	{
		ret = HDR_CREATE_DXGI_FACTORY2_NOT_FOUND;
		pfn_CreateDXGIFactory2 = (PFN_CreateDXGIFactory2)GetProcAddress(m_Dxgi_Library, "CreateDXGIFactory2");
		if (pfn_CreateDXGIFactory2)
		{
			ret = HDR_CREATE_DXGI_FACTORY4_FAILED;
			m_LastError = pfn_CreateDXGIFactory2(0, IID_PPV_ARGS(&m_Factory4));
			if (SUCCEEDED(m_LastError))
			{
				ret = HDR_OK;
			}
		}
	}

	return ret;
}

HDR_STATUS HDRProvider::initAdapters()
{
	int i = 0;
	IDXGIAdapter1 *pAdapter;
	DXGI_ADAPTER_DESC desc;

	m_LastError = S_OK;
	while (m_Factory4->EnumAdapters1(i++, &pAdapter) != DXGI_ERROR_NOT_FOUND)
	{
		if (SUCCEEDED(pAdapter->GetDesc(&desc)))
		{
			// Ignore Microsoft Basic Display Adapter.
			if (!(desc.VendorId == 0x1414 && desc.DeviceId == 0x8c))
				m_Adapters.push_back(pAdapter);
			else
				pAdapter->Release();
		}
	}

	return m_Adapters.size() > 0 ? HDR_OK : HDR_ADAPTERS_NOT_FOUND;
}

HDR_STATUS HDRProvider::mapDisplayToAdapter()
{
	HDR_STATUS ret = HDR_DISPLAYS_NOT_FOUND;

	IDXGIOutput *pOutput;
	DXGI_OUTPUT_DESC outputDesc;
	m_LastError = S_OK;
	for (std::vector<IDXGIAdapter1 *>::iterator it = m_Adapters.begin(); it != m_Adapters.end(); it++)
	{
		int i = 0;
		while ((*it)->EnumOutputs(i++, &pOutput) != DXGI_ERROR_NOT_FOUND && ret != HDR_OK)
		{
			if (SUCCEEDED(pOutput->GetDesc(&outputDesc)) && outputDesc.Monitor == m_hMonitor)
			{
				m_ActiveAdapter = *it;
				m_OutputDesc = outputDesc;
				ret = initMonitor(outputDesc);
			}
			else
				SAFE_RELEASE(pOutput);
		}
	}

	return ret;
}

HDR_STATUS HDRProvider::initMonitor(const DXGI_OUTPUT_DESC &outputDesc)
{
	DISPLAY_DEVICE monitor;
	INIT_STRUCT(monitor, cb);
	HDR_STATUS ret = HDR_DISPLAY_INIT_FAILED;
	int length = WideCharToMultiByte(NULL, 0, outputDesc.DeviceName, -1, NULL, 0, NULL, NULL);
	char *deviceName = new char[length];
	if (WideCharToMultiByte(NULL, 0, outputDesc.DeviceName, -1, deviceName, length, NULL, NULL))
	{
		m_MonitorDeviceName = deviceName;
		RECT rect = { 0, 0, outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left,
			outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top };
		m_MonitorResolution = rect;
		ret = HDR_OK;
	}

	return ret;
}

HDR_STATUS HDRProvider::initApiInterface()
{
	SAFE_DELETE(m_ApiInterface);
	m_ApiInterface = new NvApi();
	if (!m_ApiInterface->MonitorExists(m_MonitorDeviceName.c_str()))
		SAFE_DELETE(m_ApiInterface);

	return HDR_OK;
}

HDR_STATUS HDRProvider::initDx12()
{
	HDR_STATUS ret = HDR_D3D12_RUNTIME_NOT_FOUND;

	m_LastError = S_OK;
	m_D12_Library = LoadLibrary(TEXT("D3D12.dll"));
	if (m_D12_Library)
	{
		ret = HDR_CREATE_D3D12_DEVICE_NOT_FOUND;
		pfn_D3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(m_D12_Library, "D3D12CreateDevice");
		if (pfn_D3D12CreateDevice)
		{
			ret = HDR_CREATE_D3D12_DEVICE_FAILED;
			m_LastError = pfn_D3D12CreateDevice(m_ActiveAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_D3D12Device));
			if (SUCCEEDED(m_LastError))
			{
				ret = HDR_CREATE_D3D12_COMMAND_QUEUE_FAILED;
				D3D12_COMMAND_QUEUE_DESC cmdQueueDesc;
				cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
				cmdQueueDesc.NodeMask = 0;
				m_LastError = m_D3D12Device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_CommandQueue));
				if (SUCCEEDED(m_LastError))
				{
					ret = HDR_OK;
				}
			}
		}
	}

	return ret;
}

HDR_STATUS HDRProvider::initSwapChain(bool hdrOn)
{
	HDR_STATUS ret = HDR_CREATE_SWAP_CHAIN_FAILED;

	IDXGISwapChain1 *swapChain1;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	swapChainDesc.Width = m_MonitorResolution.right;
	swapChainDesc.Height = m_MonitorResolution.bottom;
	swapChainDesc.Format = hdrOn ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Flags = 0;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	m_LastError = m_Factory4->CreateSwapChainForHwnd(m_CommandQueue, m_hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
	if (SUCCEEDED(m_LastError))
	{
		m_LastError = swapChain1->QueryInterface(IID_PPV_ARGS(&m_SwapChain4));
		if (SUCCEEDED(m_LastError))
		{
			ret = HDR_OK;
		}

		SAFE_RELEASE(swapChain1);
	}

	return ret;
}

HDR_STATUS HDRProvider::setDxHDRMode(bool hdrOn, const LIBHDR_HDR_METADATA_HDR10 &metaData)
{
	if (hdrOn)
		m_LastError = m_SwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(metaData), (void*)&metaData);
	else
		m_LastError = m_SwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, NULL);

	return SUCCEEDED(m_LastError) ? HDR_OK : HDR_SET_HDR_DX_FAILED;
}

void HDRProvider::cleanUpDirectX()
{
	SAFE_RELEASE(m_SwapChain4);
	SAFE_RELEASE(m_CommandQueue);
	SAFE_RELEASE(m_D3D12Device);
	for (std::vector<IDXGIAdapter1 *>::iterator it = m_Adapters.begin(); it != m_Adapters.end(); ++it)
	{
		SAFE_RELEASE((*it));
	}

	m_Adapters.clear();
	m_ActiveAdapter = nullptr;
	SAFE_RELEASE(m_Factory4);
	SAFE_FREE_LIB(m_D12_Library);
	SAFE_FREE_LIB(m_Dxgi_Library);
	m_DxReady = false;
	m_LastError = S_OK;
}