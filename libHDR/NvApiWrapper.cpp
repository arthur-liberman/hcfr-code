#include "NvApiWrapper.h"

NvApi::NvApi()
{
	OutputDebugString(TEXT("Enter: NvApi::NvApi"));
	NvAPI_Initialize();
	m_DisplayId = 0;
	OutputDebugString(TEXT("Exit: NvApi::NvApi"));
}

NvApi::~NvApi()
{
	OutputDebugString(TEXT("Enter: NvApi::~NvApi"));
	NvAPI_Unload();
	OutputDebugString(TEXT("Exit: NvApi::~NvApi"));
}

bool NvApi::MonitorExists(const char *deviceName)
{
	OutputDebugString(TEXT("Enter: NvApi::MonitorExists"));
	NvU32 nvStatus = NvAPI_DISP_GetDisplayIdByDisplayName(deviceName, &m_DisplayId);
	bool ret = nvStatus == NVAPI_OK;

	OutputDebugString(TEXT("Exit: NvApi::MonitorExists"));
	return ret;
}

bool NvApi::Initialize()
{
	OutputDebugString(TEXT("Enter: NvApi::Initialize"));
	OutputDebugString(TEXT("Exit: NvApi::Initialize"));
	return true;
}

HDR_TYPE NvApi::GetSupportedHDRModes() const
{
	OutputDebugString(TEXT("Enter: NvApi::GetSupportedHDRModes"));
	OutputDebugString(TEXT("Exit: NvApi::GetSupportedHDRModes"));
	return HDR_TYPE_HDR10;
}

HDR_STATUS NvApi::SetHDR10Mode(bool hdrOn, const LIBHDR_HDR_METADATA_HDR10 &metaData)
{
	OutputDebugString(TEXT("Enter: NvApi::SetHDR10Mode"));
	HDR_STATUS ret = HDR_NV_DISPLAY_NOT_FOUND;

	if ((hdrOn && m_HdrMode == HDR_TYPE_HDR10) || (!hdrOn && m_HdrMode == HDR_TYPE_NONE))
		return HDR_OK;

	if (m_DisplayId)
	{
		NV_HDR_COLOR_DATA hdrColorData;
		convertMetaDataStruct(hdrColorData, metaData);
		hdrColorData.hdrMode = hdrOn ? NV_HDR_MODE_UHDA_PASSTHROUGH : NV_HDR_MODE_OFF;
		NvU32 nvStatus = NvAPI_Disp_HdrColorControl(m_DisplayId, &hdrColorData);
		ret = nvStatus == NVAPI_OK ? HDR_OK : HDR_SET_HDR_NV_FAILED;
		if (SUCCEEDED(ret))
			m_HdrMode = hdrOn ? HDR_TYPE_HDR10 : HDR_TYPE_NONE;
	}

	OutputDebugString(TEXT("Exit: NvApi::SetHDR10Mode"));
	return ret;
}

void NvApi::convertMetaDataStruct(NV_HDR_COLOR_DATA &destination, const LIBHDR_HDR_METADATA_HDR10 &source)
{
	OutputDebugString(TEXT("Enter: NvApi::convertMetaDataStruct"));
	destination.version = NV_HDR_COLOR_DATA_VER;
	destination.cmd = NV_HDR_CMD_SET;
	destination.hdrMode = NV_HDR_MODE_OFF;
	destination.static_metadata_descriptor_id = NV_STATIC_METADATA_TYPE_1;

	destination.mastering_display_data.displayPrimary_x0 = source.RedPrimary[0];
	destination.mastering_display_data.displayPrimary_y0 = source.RedPrimary[1];
	destination.mastering_display_data.displayPrimary_x1 = source.GreenPrimary[0];
	destination.mastering_display_data.displayPrimary_y1 = source.GreenPrimary[1];
	destination.mastering_display_data.displayPrimary_x2 = source.BluePrimary[0];
	destination.mastering_display_data.displayPrimary_y2 = source.BluePrimary[1];
	destination.mastering_display_data.displayWhitePoint_x = source.WhitePoint[0];
	destination.mastering_display_data.displayWhitePoint_y = source.WhitePoint[1];
	destination.mastering_display_data.max_display_mastering_luminance = (NvU16)(source.MaxMasteringLuminance / 10000);
	destination.mastering_display_data.min_display_mastering_luminance = (NvU16)source.MinMasteringLuminance;
	destination.mastering_display_data.max_content_light_level = source.MaxContentLightLevel;
	destination.mastering_display_data.max_frame_average_light_level = source.MaxFrameAverageLightLevel;
	OutputDebugString(TEXT("Exit: NvApi::convertMetaDataStruct"));
}