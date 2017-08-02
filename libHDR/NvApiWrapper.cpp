#include "NvApiWrapper.h"

NvApi::NvApi()
{
	NvAPI_Initialize();
	m_DisplayId = 0;
}

NvApi::~NvApi()
{
	NvAPI_Unload();
}

bool NvApi::MonitorExists(const char *deviceName)
{
	NvU32 nvStatus = NvAPI_DISP_GetDisplayIdByDisplayName(deviceName, &m_DisplayId);
	bool ret = nvStatus == NVAPI_OK;

	return ret;
}

bool NvApi::Initialize()
{
	return true;
}

HDR_TYPE NvApi::GetSupportedHDRModes()
{
	return HDR_TYPE_HDR10;
}

HDR_STATUS NvApi::SetHDR10Mode(bool hdrOn, const LIBHDR_HDR_METADATA_HDR10 &metaData)
{
	HDR_STATUS ret = HDR_NV_DISPLAY_NOT_FOUND;

	if (m_DisplayId)
	{
		NV_HDR_COLOR_DATA hdrColorData;
		convertMetaDataStruct(hdrColorData, metaData);
		hdrColorData.hdrMode = hdrOn ? NV_HDR_MODE_UHDA_PASSTHROUGH : NV_HDR_MODE_OFF;
		NvU32 nvStatus = NvAPI_Disp_HdrColorControl(m_DisplayId, &hdrColorData);
		ret = nvStatus == NVAPI_OK ? HDR_OK : HDR_SET_HDR_NV_FAILED;
	}

	return ret;
}

void NvApi::convertMetaDataStruct(NV_HDR_COLOR_DATA &destination, const LIBHDR_HDR_METADATA_HDR10 &source)
{
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
}