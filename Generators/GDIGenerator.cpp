/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//	François-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// GDIGenerator.cpp: implementation of the CGDIGenerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GDIGenerator.h"
#include "madTPG.h"
#include "../PatternDisplay.h"
#include "../libnum/numsup.h"
#include "../libconv/conv.h"
#include "../libccast/ccmdns.h"
#include "../libccast/ccwin.h"
#include "../libccast/ccast.h"
#include "../MainFrm.h"

#include <string>
#include <float.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// we use multimon stubs to
// allow backwards compatibilty and
// this doesn't get defined
#if (WINVER < 0x0500)
#define EDD_GET_DEVICE_INTERFACE_NAME 0x00000001
#endif

BOOL CALLBACK MonitorEnumProc( HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData )
{
	CGDIGenerator *pClass=(CGDIGenerator *)dwData;

	pClass->m_hMonitor[pClass->m_monitorNb]=hMonitor;
	pClass->m_monitorNb++;
	return ( pClass->m_monitorNb < MAX_MONITOR_NB );
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CGDIGenerator,CGenerator,1) ;

CGDIGenerator::CGDIGenerator()
{
	m_bBlankingCanceled = FALSE;
	m_displayWindow.m_rectSizePercent=GetConfig()->GetProfileInt("GDIGenerator","SizePercent",10);
	m_displayWindow.m_offsetx = GetConfig()->GetProfileInt("GDIGenerator","XOffset",0);
	m_displayWindow.m_offsety = GetConfig()->GetProfileInt("GDIGenerator","YOffset",0);
	m_displayWindow.m_bgStimPercent=GetConfig()->GetProfileInt("GDIGenerator","bgStimPercent",0);
	m_displayWindow.m_Intensity=GetConfig()->GetProfileInt("GDIGenerator","Intensity",100);
	m_displayWindow.m_busePic=GetConfig()->GetProfileInt("GDIGenerator","USEPIC",0);
	m_displayWindow.m_bdispTrip=GetConfig()->GetProfileInt("GDIGenerator","DISPLAYTRIPLETS",1);
	m_displayWindow.m_brPi_user=GetConfig()->GetProfileInt("GDIGenerator","DISPLAYRPIUSER",0);
	m_displayWindow.m_bLinear=GetConfig()->GetProfileInt("GDIGenerator","LOADLINEAR",1);
	m_rectSizePercent = m_displayWindow.m_rectSizePercent;
	m_bgStimPercent = m_displayWindow.m_bgStimPercent;
	m_offsetx = m_displayWindow.m_offsetx;
	m_offsety = m_displayWindow.m_offsety;
	m_Intensity = m_displayWindow.m_Intensity;
	m_patternDGenerator=NULL;
	m_HdrInterface=NULL;

	GetMonitorList();
	m_activeMonitorNum = m_monitorNb-1;

	m_nDisplayMode = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE);
	m_b16_235 = GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",1);
	m_busePic = GetConfig()->GetProfileInt("GDIGenerator","USEPIC",0);
	m_bLinear = GetConfig()->GetProfileInt("GDIGenerator","LOADLINEAR",1);
	m_bHdr10 = GetConfig()->GetProfileInt("GDIGenerator","EnableHDR10",0);
	m_bdispTrip = GetConfig()->GetProfileInt("GDIGenerator","DISPLAYTRIPLETS",1);
	m_brPi_user = GetConfig()->GetProfileInt("GDIGenerator","DISPLAYRPIUSER",0);
    m_madVR_3d = GetConfig()->GetProfileInt("GDIGenerator","MADVR3D",1);
    m_madVR_vLUT = GetConfig()->GetProfileInt("GDIGenerator","MADVRvLUT",1);
    m_madVR_HDR = GetConfig()->GetProfileInt("GDIGenerator","MADVRHDR",0);
    m_madVR_OSD = GetConfig()->GetProfileInt("GDIGenerator","MADVROSD",0);
	m_displayWindow.SetDisplayMode(m_nDisplayMode);	
//	m_displayWindow.SetDisplayMode();	
	m_bisInited = FALSE;

	CString str;
	str.LoadString(IDS_GDIGENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str); 
	m_propertySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	AddPropertyPage(&m_GDIGenePropertiesPage);

	str.LoadString(IDS_GDIGENERATOR_NAME);
	SetName(str);
	m_bConnect = FALSE;
}

CGDIGenerator::CGDIGenerator(int nDisplayMode, BOOL b16_235)
{
	// This constructor is used by pattern generator: no screen blanking
	m_bBlankingCanceled = FALSE;
	m_doScreenBlanking = FALSE;
	m_displayWindow.m_rectSizePercent=100;
	m_displayWindow.m_bgStimPercent=0;
	m_displayWindow.m_offsetx = 0;
	m_displayWindow.m_offsety = 0;
	m_displayWindow.m_Intensity=100;
	m_displayWindow.m_busePic=FALSE;
	m_displayWindow.m_brPi_user = FALSE;
	m_displayWindow.m_bdispTrip=FALSE;
	m_displayWindow.m_bLinear=FALSE;
	m_displayWindow.m_bHdr10=FALSE;
	m_HdrInterface=NULL;
	m_nPat = 0;

	GetMonitorList();
	m_activeMonitorNum = m_monitorNb-1;

	m_nDisplayMode = nDisplayMode;
	m_b16_235 = b16_235;
	m_displayWindow.SetDisplayMode(nDisplayMode);

	CString str;
	str.LoadString(IDS_GDIGENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str); 
	m_propertySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	AddPropertyPage(&m_GDIGenePropertiesPage);

	str.LoadString(IDS_GDIGENERATOR_NAME);
	SetName(str);
	m_bConnect = FALSE;
	Init();
}

CGDIGenerator::~CGDIGenerator()
{
	if (m_HdrInterface)
	{
		OutputDebugString("Destroy existing HdrInterface");
		delete m_HdrInterface;
	}
} 

void CGDIGenerator::Copy(CGenerator * p)
{
	CGenerator::Copy(p);

	m_activeMonitorNum = ((CGDIGenerator*)p)->m_activeMonitorNum;
	m_nDisplayMode = ((CGDIGenerator*)p)->m_nDisplayMode;

}

void CGDIGenerator::GetMonitorList()
{
	m_monitorNb=0;
	// Get monitors handles and nb 
	EnumDisplayMonitors(NULL,NULL,MonitorEnumProc,(LPARAM)this);
	
	MONITORINFOEX	mi;
	mi.cbSize = sizeof ( mi );

	// Fill monitor string array used for combo box in properties sheet
	m_GDIGenePropertiesPage.m_monitorNameArray.RemoveAll();
	for(UINT i=0; i<m_monitorNb; i++)
	{
		GetMonitorInfo ( m_hMonitor[i], & mi );
		std::string sMonitor = GetMonitorName(&mi);
		m_GDIGenePropertiesPage.m_monitorNameArray.Add(sMonitor.c_str());		
		m_GDIGenePropertiesPage.m_monitorHandle [ i ] = m_hMonitor [ i ];
	}
}

std::string CGDIGenerator::GetMonitorName(const MONITORINFOEX *m) const
{
	// Windows is a little ugly about how it handles monitors. ColorHCFR uses a HMONITOR in operations to write to the
	// screen. However, the EnumDisplayMonitors() function that returns the HMONITOR handle does not return a friendly name
	// for the monitor. The function that DOES return the friendly name for the monitor, EnumDisplayDevices() on the 
	// other hand, does not return a HMONITOR. So we need to call first EnumDisplayMonitors() to get the handle, and then call
	// EnumDisplayDevices() to find the friendly monitor name.

	DISPLAY_DEVICE  dd, dm;
    
	SecureZeroMemory(&dd, sizeof(dd));
	dd.cb = sizeof(dd);

	SecureZeroMemory(&dm, sizeof(dm));
	dm.cb = sizeof(dm);   

	std::string sMonitor;

	for (DWORD numAdapter = 0; m && EnumDisplayDevices(NULL, numAdapter, &dd, EDD_GET_DEVICE_INTERFACE_NAME); numAdapter++)
	{
		if (!(dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) 
		{
			; // silently ignore as it's disabled or virtual
		}
		else if (strcmp(dd.DeviceName, m->szDevice))
		{
			; // silently ignore as it's not the right monitor
		}
		else if (EnumDisplayDevices(dd.DeviceName, 0, &dm, 0))
		{			
			sMonitor = dm.DeviceString;
		}
	}
	return sMonitor;
}

BOOL CGDIGenerator::IsOnOtherMonitor ()
{
	BOOL		bResult = FALSE;
	MONITORINFO	mi;
	RECT		Rect, Rect2;

	if ( m_monitorNb > 1 && m_hMonitor[m_activeMonitorNum] != NULL )
	{
		mi.cbSize = sizeof ( mi );
		GetMonitorInfo ( m_hMonitor[m_activeMonitorNum], & mi );

		AfxGetMainWnd () -> GetWindowRect ( & Rect );
		if ( ! IntersectRect ( & Rect2, & Rect, & mi.rcMonitor ) )
			bResult = TRUE;
	}

	return bResult;
}

void CGDIGenerator::Serialize(CArchive& archive)
{
	CGenerator::Serialize(archive);

	if (archive.IsStoring())
	{
		int version=3;
		archive << version;
		archive << m_displayWindow.m_offsetx;
		archive << m_displayWindow.m_offsety;
		archive << m_displayWindow.m_rectSizePercent;
		archive << m_displayWindow.m_bgStimPercent;
		archive << m_displayWindow.m_Intensity;
		archive << m_activeMonitorNum;
		archive << m_nDisplayMode;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 3 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		if ( version > 2)
		{
			archive >> m_displayWindow.m_offsetx;
			archive >> m_displayWindow.m_offsety;
		}
		archive >> m_displayWindow.m_rectSizePercent;
		archive >> m_displayWindow.m_bgStimPercent;
		archive >> m_displayWindow.m_Intensity;
		archive >> m_activeMonitorNum;
		if ( version > 1 )
		{
			int m_cDisplayMode = m_nDisplayMode;
			archive >> m_nDisplayMode;
			if (m_nDisplayMode != m_cDisplayMode)
			{
				GetColorApp()->InMeasureMessageBox("Restoring generator setting from save file...", "Generator Change", MB_OK);
				GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode", m_nDisplayMode);
				SetPropertiesSheetValues();
			}
		}
		else
			m_nDisplayMode = DISPLAY_DEFAULT_MODE;
	}
}

void CGDIGenerator::SetPropertiesSheetValues()
{
	CGenerator::SetPropertiesSheetValues();

	m_GDIGenePropertiesPage.m_rectSizePercent=m_displayWindow.m_rectSizePercent;
	m_GDIGenePropertiesPage.m_bgStimPercent=m_displayWindow.m_bgStimPercent;
	m_GDIGenePropertiesPage.m_Intensity=m_displayWindow.m_Intensity;
	m_GDIGenePropertiesPage.m_offsetx=m_displayWindow.m_offsetx;
	m_GDIGenePropertiesPage.m_offsety=m_displayWindow.m_offsety;
	m_GDIGenePropertiesPage.m_activeMonitorNum=m_activeMonitorNum;
	m_GDIGenePropertiesPage.m_nDisplayMode=m_nDisplayMode;
	m_GDIGenePropertiesPage.m_b16_235=m_b16_235;
	m_GDIGenePropertiesPage.m_busePic=m_busePic;
	m_GDIGenePropertiesPage.m_bLinear=m_bLinear;
	m_GDIGenePropertiesPage.m_bdispTrip=m_bdispTrip;
	m_GDIGenePropertiesPage.m_brPi_user=m_brPi_user;
	m_GDIGenePropertiesPage.m_madVR_3d=m_madVR_3d;
	m_GDIGenePropertiesPage.m_madVR_vLUT=m_madVR_vLUT;
	m_GDIGenePropertiesPage.m_madVR_HDR=m_madVR_HDR;
	m_GDIGenePropertiesPage.m_madVR_OSD=m_madVR_OSD;
}

void CGDIGenerator::GetPropertiesSheetValues()
{
	CGenerator::GetPropertiesSheetValues();

	if( m_displayWindow.m_rectSizePercent != m_GDIGenePropertiesPage.m_rectSizePercent )
	{
		m_displayWindow.m_rectSizePercent=m_GDIGenePropertiesPage.m_rectSizePercent;
		GetConfig()->WriteProfileInt("GDIGenerator","SizePercent",m_displayWindow.m_rectSizePercent);
		SetModifiedFlag(TRUE);
	}

	if( m_displayWindow.m_bgStimPercent != m_GDIGenePropertiesPage.m_bgStimPercent )
	{
		m_displayWindow.m_bgStimPercent=m_GDIGenePropertiesPage.m_bgStimPercent;
		GetConfig()->WriteProfileInt("GDIGenerator","bgStimPercent",m_displayWindow.m_bgStimPercent);
		SetModifiedFlag(TRUE);
	}

	if( m_displayWindow.m_Intensity != m_GDIGenePropertiesPage.m_Intensity )
	{
		m_displayWindow.m_Intensity=m_GDIGenePropertiesPage.m_Intensity;
		GetConfig()->WriteProfileInt("GDIGenerator","Intensity",m_displayWindow.m_Intensity);
		SetModifiedFlag(TRUE);
	}

	if( m_displayWindow.m_offsetx != m_GDIGenePropertiesPage.m_offsetx )
	{
		m_displayWindow.m_offsetx=m_GDIGenePropertiesPage.m_offsetx;
		GetConfig()->WriteProfileInt("GDIGenerator","XOffset",m_displayWindow.m_offsetx);
		SetModifiedFlag(TRUE);
	}

	if( m_displayWindow.m_offsety != m_GDIGenePropertiesPage.m_offsety )
	{
		m_displayWindow.m_offsety=m_GDIGenePropertiesPage.m_offsety;
		GetConfig()->WriteProfileInt("GDIGenerator","YOffset",m_displayWindow.m_offsety);
		SetModifiedFlag(TRUE);
	}

	if( m_activeMonitorNum!=m_GDIGenePropertiesPage.m_activeMonitorNum )
	{
		m_activeMonitorNum=m_GDIGenePropertiesPage.m_activeMonitorNum;
		SetModifiedFlag(TRUE);
	}

	if( m_nDisplayMode!=m_GDIGenePropertiesPage.m_nDisplayMode )
	{
		m_nDisplayMode=m_GDIGenePropertiesPage.m_nDisplayMode;
		GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode",m_nDisplayMode);
		SetModifiedFlag(TRUE);
	}

	if ( m_b16_235!=m_GDIGenePropertiesPage.m_b16_235 )
	{
		m_b16_235=m_GDIGenePropertiesPage.m_b16_235;
		GetConfig()->WriteProfileInt("GDIGenerator","RGB_16_235",m_b16_235);
		SetModifiedFlag(TRUE);
	}

	if ( m_busePic!=m_GDIGenePropertiesPage.m_busePic )
	{
		m_busePic=m_GDIGenePropertiesPage.m_busePic;
		GetConfig()->WriteProfileInt("GDIGenerator","USEPIC",m_busePic);
		SetModifiedFlag(TRUE);
	}

	if ( m_bLinear!=m_GDIGenePropertiesPage.m_bLinear )
	{
		m_bLinear=m_GDIGenePropertiesPage.m_bLinear;
		GetConfig()->WriteProfileInt("GDIGenerator","LOADLINEAR",m_bLinear);
		SetModifiedFlag(TRUE);
	}

	if ( m_bHdr10!=m_GDIGenePropertiesPage.m_bHdr10 )
	{
		m_bHdr10=m_GDIGenePropertiesPage.m_bHdr10;
		GetConfig()->WriteProfileInt("GDIGenerator","EnableHDR10",m_bHdr10);
		SetModifiedFlag(TRUE);
	}

	if ( m_bdispTrip!=m_GDIGenePropertiesPage.m_bdispTrip )
	{
		m_bdispTrip=m_GDIGenePropertiesPage.m_bdispTrip;
		GetConfig()->WriteProfileInt("GDIGenerator","DISPLAYTRIPLETS",m_bdispTrip);
		SetModifiedFlag(TRUE);
	}

	if ( m_brPi_user!=m_GDIGenePropertiesPage.m_brPi_user )
	{
		m_brPi_user=m_GDIGenePropertiesPage.m_brPi_user;
		GetConfig()->WriteProfileInt("GDIGenerator","DISPLAYRPIUSER",m_brPi_user);
		SetModifiedFlag(TRUE);
	}

    if ( m_madVR_3d!=m_GDIGenePropertiesPage.m_madVR_3d )
	{
		m_madVR_3d=m_GDIGenePropertiesPage.m_madVR_3d;
		GetConfig()->WriteProfileInt("GDIGenerator","MADVR3D",m_madVR_3d);
		SetModifiedFlag(TRUE);
	}

    if ( m_madVR_vLUT!=m_GDIGenePropertiesPage.m_madVR_vLUT )
	{
		m_madVR_vLUT=m_GDIGenePropertiesPage.m_madVR_vLUT;
		GetConfig()->WriteProfileInt("GDIGenerator","MADVRvLUT",m_madVR_vLUT);
		SetModifiedFlag(TRUE);
	}

    if ( m_madVR_HDR!=m_GDIGenePropertiesPage.m_madVR_HDR )
	{
		m_madVR_HDR=m_GDIGenePropertiesPage.m_madVR_HDR;
		GetConfig()->WriteProfileInt("GDIGenerator","MADVRHDR",m_madVR_HDR);
		SetModifiedFlag(TRUE);
	}

	if ( m_madVR_OSD!=m_GDIGenePropertiesPage.m_madVR_OSD )
	{
		m_madVR_OSD=m_GDIGenePropertiesPage.m_madVR_OSD;
		GetConfig()->WriteProfileInt("GDIGenerator","MADVROSD",m_madVR_OSD);
		SetModifiedFlag(TRUE);
	}
}

BOOL CGDIGenerator::Init(UINT nbMeasure, bool isSpecial)
{
//	GetColorApp()->InMeasureMessageBox( "    ** GDI Generator Init **", "Error", MB_ICONINFORMATION);
	BOOL	bOk, bOnOtherMonitor;
	GetMonitorList();
	m_nDisplayMode =  		GetConfig()->GetProfileInt("GDIGenerator","DisplayMode", DISPLAY_DEFAULT_MODE);
	if (!m_bConnect && m_bLinear) //linear gamma tables
	{
		char arg[255];
		CString str = GetConfig () -> m_ApplicationPath;
		CString str1 = GetConfig () -> m_ApplicationPath;
		str += "\\tools\\dispwin.exe";
		str1 += "\\tools\\current.cal";
		sprintf(arg," -d%d -s " + str1, m_activeMonitorNum+1);
		ShellExecute(NULL, "open", str, arg, NULL, SW_HIDE);
		Sleep(100);
		sprintf(arg," -d%d -c", m_activeMonitorNum+1);
		ShellExecute(NULL, "open", str, arg, NULL, SW_HIDE);
		m_bConnect = TRUE;
	}

	m_displayWindow.SetDisplayMode(m_nDisplayMode);
	m_displayWindow.SetRGBScale(m_b16_235);
	m_displayWindow.MoveToMonitor(m_hMonitor[m_activeMonitorNum]);

	if (m_nDisplayMode == DISPLAY_GDI || m_nDisplayMode == DISPLAY_OVERLAY || m_nDisplayMode == DISPLAY_GDI_nBG ||
		m_nDisplayMode == DISPLAY_GDI_Hide || m_nDisplayMode == DISPLAY_VMR9 || isSpecial)
	{
		if (!m_HdrInterface)
		{
			OutputDebugString("Create HdrInterface\n");
			m_HdrInterface = GetNewHdrInterface(m_displayWindow.hWnd, m_hMonitor[m_activeMonitorNum]);
		}
		else
		{
			OutputDebugString("Set HdrInterface's hMonitor and hWnd\n");
			m_HdrInterface->SetWindowMonitor(m_displayWindow.hWnd, m_hMonitor[m_activeMonitorNum]);
		}
		if (m_HdrInterface)
		{
			OutputDebugString("HdrInterface exists\n");
			OutputDebugString(m_bHdr10 ? "HDR10 enabled\n":"HDR10 disabled\n");
			LIBHDR_HDR_METADATA_HDR10 metaData = {0};
			if (m_bHdr10)
			{
				// Dump values to debug.
				char buf[1024];
				OutputDebugString("HDR10 Metadata to be used:\n");
				sprintf_s(buf, "Red:   X = %.4f, Y = %.4f\n", GetConfig()->m_manualRedx, GetConfig()->m_manualRedy);
				OutputDebugString(buf);
				sprintf_s(buf, "Green: X = %.4f, Y = %.4f\n", GetConfig()->m_manualGreenx, GetConfig()->m_manualGreeny);
				OutputDebugString(buf);
				sprintf_s(buf, "Blue:  X = %.4f, Y = %.4f\n", GetConfig()->m_manualBluex, GetConfig()->m_manualBluey);
				OutputDebugString(buf);
				sprintf_s(buf, "White: X = %.4f, Y = %.4f\n", GetConfig()->m_manualWhitex, GetConfig()->m_manualWhitey);
				OutputDebugString(buf);
				sprintf_s(buf, "Master Max = %.2f, Master Min = %.4f\n", GetConfig()->m_MasterMaxL, GetConfig()->m_MasterMinL);
				OutputDebugString(buf);
				sprintf_s(buf, "Content Max = %.2f, Frame Avg. Max = %.2f\n", GetConfig()->m_ContentMaxL, GetConfig()->m_FrameAvgMaxL);
				OutputDebugString(buf);

				// Default to DCI/P3 primaries
				metaData.RedPrimary[0] = UINT16(GetConfig()->m_manualRedx * 50000.0);
				metaData.RedPrimary[1] = UINT16(GetConfig()->m_manualRedy * 50000.0);
				metaData.GreenPrimary[0] = UINT16(GetConfig()->m_manualGreenx * 50000.0);
				metaData.GreenPrimary[1] = UINT16(GetConfig()->m_manualGreeny * 50000.0);
				metaData.BluePrimary[0] = UINT16(GetConfig()->m_manualBluex * 50000.0);
				metaData.BluePrimary[1] = UINT16(GetConfig()->m_manualBluey * 50000.0);
				metaData.WhitePoint[0] = UINT16(GetConfig()->m_manualWhitex * 50000.0);
				metaData.WhitePoint[1] = UINT16(GetConfig()->m_manualWhitey * 50000.0);
				// Default luminosity levels.
				metaData.MaxMasteringLuminance = UINT(GetConfig()->m_MasterMaxL * 10000.0);
				metaData.MinMasteringLuminance = UINT(GetConfig()->m_MasterMinL * 10000.0);
				metaData.MaxContentLightLevel = USHORT(GetConfig()->m_ContentMaxL);
				metaData.MaxFrameAverageLightLevel = USHORT(GetConfig()->m_FrameAvgMaxL);
			}
			HDR_STATUS hdrStat = m_HdrInterface->SetHDR10Mode(m_bHdr10, metaData);
			if (SUCCEEDED(hdrStat))
				OutputDebugString("HDR mode switch successful\n");
			else
			{
				char buffer[1024];
				sprintf_s(buffer, "HDR mode switch failed, error number %d\n", (int)hdrStat);
				OutputDebugString(buffer);
			}
		}
		else
			OutputDebugString("HdrInterface doesn't exist\n");
	}

	if (m_nDisplayMode == DISPLAY_GDI || m_nDisplayMode == DISPLAY_GDI_nBG || isSpecial )
		m_displayWindow.ShowWindow(SW_SHOWMAXIMIZED);
	if (m_nDisplayMode == DISPLAY_GDI_Hide && !isSpecial) //to use test colour window instead
		m_displayWindow.ShowWindow(SW_HIDE);
	
	bOnOtherMonitor = IsOnOtherMonitor ();

	if ( ! bOnOtherMonitor )
	{
		// Deactivate blanking when generator window is on the same monitor
		m_bBlankingCanceled = m_doScreenBlanking;
		m_doScreenBlanking = FALSE;
	}
	
	bOk = CGenerator::Init (nbMeasure );

	if ( ! bOnOtherMonitor )
	{
		GetColorApp() -> SetPatternWindow ( & m_displayWindow );
		GetColorApp() -> EndMeasureCursor ();
	}
	else
	{
		GetColorApp() -> RestoreMeasureCursor ();
	}
	m_bisInited = TRUE;

	return bOk;
}

BOOL CGDIGenerator::DisplayRGBColormadVR( const ColorRGBDisplay& clr, bool first, UINT nPattern )
{
	//init done in generator.cpp 
      int blackLevel, whiteLevel;
      double r, g, b;
      int rT,gT,bT;
      madVR_GetBlackAndWhiteLevel ( &blackLevel, &whiteLevel);
      // if madvr is sending video levels then no dithering and targets are fine, if madvr is sending full (or custom) range then you should dither
      // and again targets will be fine.
      r = (clr[0] / 100. );
      g = (clr[1] / 100. );
      b = (clr[2] / 100. );
	  //What rounded int level will be sent by madVR if dithering is turned off 
      rT = (int) (r * (whiteLevel - blackLevel) + blackLevel + 0.5);
      gT = (int) (g * (whiteLevel - blackLevel) + blackLevel + 0.5);
      bT = (int) (b * (whiteLevel - blackLevel) + blackLevel + 0.5);
      char aBuf[128];
	  madVR_SetDisableOsdButton(!m_madVR_OSD);
	  madVR_SetHdrButton(m_madVR_HDR);
	  if (m_madVR_HDR)
		  madVR_SetHdrMetadata(GetConfig()->m_manualRedx, GetConfig()->m_manualRedy, GetConfig()->m_manualGreenx, GetConfig()->m_manualGreeny, GetConfig()->m_manualBluex, GetConfig()->m_manualBluey, GetConfig()->m_manualWhitex, GetConfig()->m_manualWhitey, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_ContentMaxL, GetConfig()->m_FrameAvgMaxL);
	  CGDIGenerator Cgen;
	  double bgstim = Cgen.m_bgStimPercent / 100.;
	  madVR_SetPatternConfig(Cgen.m_rectSizePercent, int (bgstim * 100), -1, 20);
	  if (first)
	  {
    	  sprintf(aBuf,"%s","Display settling, please wait...");
	      const CString s(aBuf);
	      madVR_SetOsdText(CT2CW(s));
	      if (!madVR_ShowRGB(.75, .75, .75))
		  {
			MessageBox(0, "Test pattern failure.", "Error", MB_ICONERROR);
			return false;
		  }
		  for (int i=0;i<=24;i++)
		  {
			  madVR_ShowRGB(double(i) * 10.0 / 255.0,double(i) * 10.0 / 255.0,double(i) * 10.0 / 255.0);
			  Sleep(33);
		  }
	  }
      if (m_madVR_3d)
    	  sprintf(aBuf,"HCFR is measuring display, pleaset wait...%d:%d:%d[3dlut disabled]",rT,gT,bT);
      else
    	  sprintf(aBuf,"HCFR is measuring display, please wait...%d:%d:%d",rT,gT,bT);
      const CString s2(aBuf);
	  madVR_SetOsdText(CT2CW(s2));

	m_nPat++;
	if ( (m_nPat % GetConfig()->m_ablFreq == 0) && GetConfig()->m_bABL )
	{
		double lvl = m_b16_235 ? 2.19 * GetConfig()->m_ablLevel + 16 : 2.55 * GetConfig()->m_ablLevel;
		//sleep prevention every 40 patterns for longer sequences
		madVR_SetPatternConfig(100, 0, -1, 0);
		if (!madVR_ShowRGB(lvl, lvl, lvl))
		{
			MessageBox(0, "Test pattern failure.", "Error", MB_ICONERROR);
			return false;
		}	 
//		madVR_ShowRGB(.4, .4 , .4);
		Sleep(GetConfig()->m_ablDuration);
		madVR_SetPatternConfig(Cgen.m_rectSizePercent, int (bgstim * 100), -1, 20);
	}

	if (!madVR_ShowRGB(r, g, b))
      {
        MessageBox(0, "Test pattern failure.", "Error", MB_ICONERROR);
		return false;
      } 

	// Sleep 80 ms while dispatching messages to ensure window is really displayed
		MSG		Msg;
		HWND	hEscapeWnd = NULL;
		DWORD	dwWait = GetConfig () -> GetProfileInt ( "Debug", "WaitAfterDisplayPattern", 80 );
		DWORD	dwStart = GetTickCount();
		DWORD	dwNow = dwStart;
		
		// Wait until dwWait time is expired, but ensures all posted messages are treated even if wait time is zero
		while((dwNow - dwStart) < dwWait)
		{
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				if ( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP ) && Msg.wParam == VK_ESCAPE )
				{
					// Do not treat this message, store it for later use
					hEscapeWnd = Msg.hwnd;
				}
				else
				{
					TranslateMessage ( & Msg );
					DispatchMessage ( & Msg );
				}
				Sleep(0);
			}
			dwNow = GetTickCount();
		}
		if ( hEscapeWnd )
		{
			// Escape key detected and stored during above loop: put it again in message loop to allow detection
			::PostMessage ( hEscapeWnd, WM_KEYDOWN, VK_ESCAPE, NULL );
			::PostMessage ( hEscapeWnd, WM_KEYUP, VK_ESCAPE, NULL );
		}

return TRUE;
}

BOOL CGDIGenerator::DisplayRGBCCast( const ColorRGBDisplay& clr, bool first, UINT nPattern )
{
	//init done in generator.cpp 
    double r, g, b;
	CGDIGenerator Cgen;
	dispwin *ccwin=Cgen.ccwin;
	double bgstim = Cgen.m_bgStimPercent / 100.;
	//Chromecast needs full range RGB
    r = ((clr[0]) / 100. );
	g = ((clr[1]) / 100. );
    b = ((clr[2]) / 100. );

	double R1=0.,G1=0.,B1=0.;
	//subtract window area for APL
	if (Cgen.m_rectSizePercent < 100)
	{
		R1 = max(0,(bgstim - r*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		G1 = max(0,(bgstim - g*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		B1 = max(0,(bgstim - b*Cgen.m_rectSizePercent/100.))/(1-Cgen.m_rectSizePercent/100. );
		R1 = min(R1, 1);
		G1 = min(G1, 1);
		B1 = min(B1, 1);
	}

	if (ccwin->height == 0) 
	{
		MessageBox(0, "Test pattern failure.", "Error", MB_ICONERROR);
		return false;
	} 

	if (ccwin->set_bg(ccwin,(R1+G1+B1)/3.) != 0)
	{
		MessageBox(0, "CCast Test pattern failure.", "set_bg", MB_ICONERROR);
		return false;
	} 

	if (first)
	{
		  if (ccwin->set_color(ccwin,0.75,0.75,0.75) != 0)
		  {
	        MessageBox(0, "CCast Test pattern failure.", "set_bg", MB_ICONERROR);
			return false;
		  }
		  for (int i=0;i<=24;i++)
		  {
			  ccwin->set_color(ccwin,double(i) * 10.0 /  255.0,double(i) * 10.0 / 255.0,double(i) * 10.0 / 255.0);
			  Sleep(50);
		  }
	}
	
	m_nPat++;
	if ( (m_nPat % GetConfig()->m_ablFreq == 0) && GetConfig()->m_bABL)
	{
		double lvl = m_b16_235 ? 2.19 * GetConfig()->m_ablLevel + 16 : 2.55 * GetConfig()->m_ablLevel;
		//sleep prevention
		ccwin->set_bg(ccwin,0);
		if (ccwin->set_color(ccwin, lvl, lvl, lvl) != 0 )
		{
	        MessageBox(0, "CCast Test pattern failure.", "set_color", MB_ICONERROR);
			return false;
		}
		Sleep(GetConfig()->m_ablDuration);

		ccwin->set_bg(ccwin,bgstim);
	}

		if (ccwin->set_color(ccwin,r,g,b) != 0 )
		{
	        MessageBox(0, "CCast Test pattern failure.", "set_color", MB_ICONERROR);
			return false;
		} 
	  
	// Sleep 80 ms while dispatching messages to ensure window is really displayed
		MSG		Msg;
		HWND	hEscapeWnd = NULL;
		DWORD	dwWait = GetConfig () -> GetProfileInt ( "Debug", "WaitAfterDisplayPattern", 80 );
		DWORD	dwStart = GetTickCount();
		DWORD	dwNow = dwStart;
		
		// Wait until dwWait time is expired, but ensures all posted messages are treated even if wait time is zero
		while((dwNow - dwStart) < dwWait)
		{
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				if ( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP ) && Msg.wParam == VK_ESCAPE )
				{
					// Do not treat this message, store it for later use
					hEscapeWnd = Msg.hwnd;
				}
				else
				{
					TranslateMessage ( & Msg );
					DispatchMessage ( & Msg );
				}
				Sleep(0);
			}
			dwNow = GetTickCount();
		}
		if ( hEscapeWnd )
		{
			// Escape key detected and stored during above loop: put it again in message loop to allow detection
			::PostMessage ( hEscapeWnd, WM_KEYDOWN, VK_ESCAPE, NULL );
			::PostMessage ( hEscapeWnd, WM_KEYUP, VK_ESCAPE, NULL );
		}

return TRUE;
}

BOOL CGDIGenerator::DisplayRGBColorrPI( const ColorRGBDisplay& clr, bool first, UINT nPattern )
{
	//init done in generator.cpp 
	int r=0, g=0, b=0;
	char CPat[256];
	CGDIGenerator Cgen;
	double bgstim = Cgen.m_bgStimPercent / 100.;
	double R1=0.,G1=0.,B1=0.;
	//subtract window area for APL
	if (Cgen.m_rectSizePercent < 100)
	{
		R1 = max(0,(bgstim*255 - clr[0]*Cgen.m_rectSizePercent/100.))/(1-m_rectSizePercent/100. );
		G1 = max(0,(bgstim*255 - clr[1]*Cgen.m_rectSizePercent/100.))/(1-m_rectSizePercent/100. );
		B1 = max(0,(bgstim*255 - clr[2]*Cgen.m_rectSizePercent/100.))/(1-m_rectSizePercent/100. );
		R1 = min(R1, 255);
		G1 = min(G1, 255);
		B1 = min(B1, 255);
	}

	if (m_b16_235)
	{
		r = floor((clr[0]) / 100. * 219.0 + 16.5);
		g = floor((clr[1]) / 100. * 219.0 + 16.5);
		b = floor((clr[2]) / 100. * 219.0 + 16.5);
		r=min(max(r,0),235);
		g=min(max(g,0),235);
		b=min(max(b,0),235);
		R1 = floor(R1/255. * 219. + 16.5);
		G1 = floor(G1/255. * 219. + 16.5);
		B1 = floor(B1/255. * 219. + 16.5);
	}
	else
	{
		r = floor((clr[0]) / 100. * 255.0 + 0.5);
		g = floor((clr[1]) / 100. * 255.0 + 0.5);
		b = floor((clr[2]) / 100. * 255.0 + 0.5);
		r=min(max(r,0),255);
		g=min(max(g,0),255);
		b=min(max(b,0),255);
	}
	
	m_nPat++;

	int x2 = Cgen.m_offsetx;
	int y2 = Cgen.m_offsety;
	if (x2 > 0)
		x2 = min(x2, rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2. );
	else
		x2 = max(x2, -1*(rPi_xWidth / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_xWidth / 2.) );
	if (y2 > 0)
		y2 = min(y2, rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.);
	else
		y2 = max(y2, -1*(rPi_yHeight / 2. - pow(Cgen.m_rectSizePercent/100.0,0.5) * rPi_yHeight / 2.) );

	if ( (m_nPat % GetConfig()->m_ablFreq == 0) && GetConfig()->m_bABL)
	{
		BYTE lvl = m_b16_235 ? (BYTE)(2.19 * GetConfig()->m_ablLevel + 16) : (BYTE)(2.55 * GetConfig()->m_ablLevel);
		sprintf_s(CPat,"RGB=RECTANGLE;%d,%d;100;%d,%d,%d;%d,%d,%d;-1,-1", (int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_xWidth),(int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_yHeight),lvl,lvl,lvl,0,0,0);
		if (CGenerator::_RB8PG_send && CPat)
			CGenerator::_RB8PG_send(sock,CPat);
		else
			GetColorApp()->InMeasureMessageBox( "Error communicating with rPI", "Error", MB_ICONINFORMATION);

		Sleep(GetConfig()->m_ablDuration);
	}

		if (m_brPi_user) //user background
			sprintf_s(CPat, "TESTTEMPLATEDISK:PatternDynamic:%d,%d,%d",r,g,b);
		else if (!m_bdispTrip)
			sprintf_s(CPat,"RGB=RECTANGLE;%d,%d;100;%d,%d,%d;%d,%d,%d;-1,-1,%d,%d", (int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_xWidth),(int)(pow((double)(Cgen.m_rectSizePercent)/100.0,0.5) * rPi_yHeight),r,g,b,(int)R1,(int)G1,(int)B1,x2,y2);
		else
			sprintf_s(CPat, "TESTTEMPLATERAMDISK:HCFR:%d,%d,%d;%d,%d,%d",r,g,b,(int)R1,(int)G1,(int)B1);

	CString debug=_T(CPat);

		if (CGenerator::_RB8PG_send && CPat)
			CGenerator::_RB8PG_send(sock,CPat);
		else
			GetColorApp()->InMeasureMessageBox( "Error communicating with rPI", "Error", MB_ICONINFORMATION);

	// Sleep 80 ms while dispatching messages to ensure window is really displayed
		MSG		Msg;
		HWND	hEscapeWnd = NULL;
		DWORD	dwWait = GetConfig () -> GetProfileInt ( "Debug", "WaitAfterDisplayPattern", 80 );
		DWORD	dwStart = GetTickCount();
		DWORD	dwNow = dwStart;
		
		// Wait until dwWait time is expired, but ensures all posted messages are treated even if wait time is zero
		while((dwNow - dwStart) < dwWait)
		{
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				if ( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP ) && Msg.wParam == VK_ESCAPE )
				{
					// Do not treat this message, store it for later use
					hEscapeWnd = Msg.hwnd;
				}
				else
				{
					TranslateMessage ( & Msg );
					DispatchMessage ( & Msg );
				}
				Sleep(0);
			}
			dwNow = GetTickCount();
		}
		if ( hEscapeWnd )
		{
			// Escape key detected and stored during above loop: put it again in message loop to allow detection
			::PostMessage ( hEscapeWnd, WM_KEYDOWN, VK_ESCAPE, NULL );
			::PostMessage ( hEscapeWnd, WM_KEYUP, VK_ESCAPE, NULL );
		}

return TRUE;

}

BOOL CGDIGenerator::DisplayRGBColor( const ColorRGBDisplay& clr , MeasureType nPatternType , UINT nPatternInfo , BOOL bChangePattern, BOOL bSilentMode)
{
	ColorRGBDisplay p_clr;
	BOOL do_Intensity=false;
	if ( nPatternType == MT_PRIMARY || nPatternType == MT_SECONDARY || nPatternType == MT_SAT_RED || nPatternType == MT_SAT_GREEN || nPatternType == MT_SAT_BLUE || nPatternType == MT_SAT_YELLOW || nPatternType == MT_SAT_CYAN || nPatternType == MT_SAT_MAGENTA || nPatternType == MT_ACTUAL)
		do_Intensity = true;

	p_clr[0] = clr[0] * m_displayWindow.m_Intensity / 100;
	p_clr[1] = clr[1] * m_displayWindow.m_Intensity / 100;
	p_clr[2] = clr[2] * m_displayWindow.m_Intensity / 100;

	//see if we need to reconnect generator
	if (!this->m_bisInited)
		Init();

	if (m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_GDI_Hide && nPatternType != MT_SPECIAL && nPatternType != MT_CONTRAST)
	{
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.ShowWindow(SW_SHOW);
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> EnableWindow (TRUE);
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetForegroundWindow();
//static pattern
		m_nPat++;
		if ((m_nPat % GetConfig()->m_ablFreq == 0)  && GetConfig()->m_bABL)
		{
			BYTE lvl = m_b16_235 ? (BYTE)(2.19 * GetConfig()->m_ablLevel + 16) : (BYTE)(2.55 * GetConfig()->m_ablLevel);
			WINDOWPLACEMENT wp;
			CRect rect;
			::GetWindowRect ( ::GetDesktopWindow (), & rect );
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.GetWindowPlacement(&wp);			
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetWindowPos(&CWnd::wndTop,0,0,rect.Width(),rect.Height(),SWP_SHOWWINDOW);

			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.SetColor ( RGB(lvl, lvl, lvl) );
			
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.RedrawWindow ();
			
			Sleep(GetConfig()->m_ablDuration);
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetWindowPlacement(&wp);			
		}
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.SetForegroundWindow();
		( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) -> m_wndTestColorWnd.RedrawWindow ();
	} else
	{
		if ( m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_madVR)
			DisplayRGBColormadVR (do_Intensity?p_clr:clr, GetConfig()->m_isSettling, nPatternInfo);
		else if ( m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_ccast)
			DisplayRGBCCast (do_Intensity?p_clr:clr, GetConfig()->m_isSettling, nPatternInfo );
		else if ( m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_rPI)
			DisplayRGBColorrPI (do_Intensity?p_clr:clr, GetConfig()->m_isSettling, nPatternInfo );
		else
			m_displayWindow.DisplayRGBColor(do_Intensity?p_clr:clr, nPatternInfo);
	}

	return TRUE;
}


BOOL CGDIGenerator::CanDisplayAnsiBWRects()
{
	return ((m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_madVR));
}

BOOL CGDIGenerator::CanDisplayAnimatedPatterns(BOOL isSpecialty)
{
	if (isSpecialty && m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_madVR)
		return TRUE;

	return ((m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_madVR) && (m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_ccast) && (m_GDIGenePropertiesPage.m_nDisplayMode != DISPLAY_rPI) ? TRUE:FALSE);
}

BOOL CGDIGenerator::DisplayAnsiBWRects(BOOL bInvert)
{
	m_displayWindow.DisplayAnsiBWRects(bInvert);
	return TRUE;
}

BOOL CGDIGenerator::DisplayAnimatedBlack()
{
	m_displayWindow.DisplayAnimatedBlack();
	return TRUE;
}

BOOL CGDIGenerator::DisplayAnimatedWhite()
{
	m_displayWindow.DisplayAnimatedWhite();
	return TRUE;
}

BOOL CGDIGenerator::DisplayGradient()
{
	m_displayWindow.DisplayGradient();
	return TRUE;
}

BOOL CGDIGenerator::DisplayRG()
{
	m_displayWindow.DisplayRG();
	return TRUE;
}
BOOL CGDIGenerator::DisplayRB()
{
	m_displayWindow.DisplayRB();
	return TRUE;
}
BOOL CGDIGenerator::DisplayGB()
{
	m_displayWindow.DisplayGB();
	return TRUE;
}
BOOL CGDIGenerator::DisplayRGd()
{
	m_displayWindow.DisplayRGd();
	return TRUE;
}
BOOL CGDIGenerator::DisplayRBd()
{
	m_displayWindow.DisplayRBd();
	return TRUE;
}
BOOL CGDIGenerator::DisplayGBd()
{
	m_displayWindow.DisplayGBd();
	return TRUE;
}

BOOL CGDIGenerator::DisplayGradient2()
{
	m_displayWindow.DisplayGradient2();
	return TRUE;
}

BOOL CGDIGenerator::DisplayLramp()
{
	m_displayWindow.DisplayLramp();
	return TRUE;
}

BOOL CGDIGenerator::DisplayGranger()
{
	m_displayWindow.DisplayGranger();
	return TRUE;
}

BOOL CGDIGenerator::Display80()
{
	m_displayWindow.Display80();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTV()
{
	m_displayWindow.DisplayTV();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTV2()
{
	m_displayWindow.DisplayTV2();
	return TRUE;
}

BOOL CGDIGenerator::DisplaySpectrum()
{
	m_displayWindow.DisplaySpectrum();
	return TRUE;
}

BOOL CGDIGenerator::DisplaySramp()
{
	m_displayWindow.DisplaySramp();
	return TRUE;
}

BOOL CGDIGenerator::DisplayVSMPTE()
{
	m_displayWindow.DisplayVSMPTE();
	return TRUE;
}

BOOL CGDIGenerator::DisplayEramp()
{
	m_displayWindow.DisplayEramp();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC0()
{
	m_displayWindow.DisplayTC0();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC1()
{
	m_displayWindow.DisplayTC1();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC2()
{
	m_displayWindow.DisplayTC2();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC3()
{
	m_displayWindow.DisplayTC3();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC4()
{
	m_displayWindow.DisplayTC4();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTC5()
{
	m_displayWindow.DisplayTC5();
	return TRUE;
}

BOOL CGDIGenerator::DisplayBN()
{
	m_displayWindow.DisplayBN();
	return TRUE;
}

BOOL CGDIGenerator::DisplayDR0()
{
	m_displayWindow.DisplayDR0();
	return TRUE;
}

BOOL CGDIGenerator::DisplayDR1()
{
	m_displayWindow.DisplayDR1();
	return TRUE;
}

BOOL CGDIGenerator::DisplayDR2()
{
	m_displayWindow.DisplayDR2();
	return TRUE;
}


BOOL CGDIGenerator::DisplayAlign()
{
	m_displayWindow.DisplayAlign();
	return TRUE;
}

BOOL CGDIGenerator::DisplayAlign2()
{
	m_displayWindow.DisplayAlign2();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser1()
{
	m_displayWindow.DisplayUser1();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser2()
{
	m_displayWindow.DisplayUser2();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser3()
{
	m_displayWindow.DisplayUser3();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser4()
{
	m_displayWindow.DisplayUser4();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser5()
{
	m_displayWindow.DisplayUser5();
	return TRUE;
}

BOOL CGDIGenerator::DisplayUser6()
{
	m_displayWindow.DisplayUser6();
	return TRUE;
}

BOOL CGDIGenerator::DisplaySharp()
{
	m_displayWindow.DisplaySharp();
	return TRUE;
}

BOOL CGDIGenerator::DisplayClipH()
{
	m_displayWindow.DisplayClipH();
	return TRUE;
}

BOOL CGDIGenerator::DisplayClipHO()
{
	m_displayWindow.DisplayClipHO();
	return TRUE;
}

BOOL CGDIGenerator::DisplayNBO()
{
	m_displayWindow.DisplayNBO();
	return TRUE;
}

BOOL CGDIGenerator::DisplayClipL()
{
	m_displayWindow.DisplayClipL();
	return TRUE;
}

BOOL CGDIGenerator::DisplayClipLO()
{
	m_displayWindow.DisplayClipLO();
	return TRUE;
}

BOOL CGDIGenerator::DisplayTestimg()
{
	m_displayWindow.DisplayTestimg();
	return TRUE;
}

BOOL CGDIGenerator::DisplayISO12233()
{
	m_displayWindow.DisplayISO12233();
	return TRUE;
}

BOOL CGDIGenerator::DisplayNB()
{
	m_displayWindow.DisplayNB();
	return TRUE;
}

BOOL CGDIGenerator::DisplayBBCHD()
{
	m_displayWindow.DisplayBBCHD();
	return TRUE;
}

BOOL CGDIGenerator::DisplayCROSSl()
{
	m_displayWindow.DisplayCROSSl();
	return TRUE;
}

BOOL CGDIGenerator::DisplayCROSSd()
{
	m_displayWindow.DisplayCROSSd();
	return TRUE;
}

BOOL CGDIGenerator::DisplayPM5644()
{
	m_displayWindow.DisplayPM5644();
	return TRUE;
}

BOOL CGDIGenerator::DisplayZONE()
{
	m_displayWindow.DisplayZONE();
	return TRUE;
}

BOOL CGDIGenerator::DisplayDotPattern( const ColorRGBDisplay& clr , BOOL dot2, UINT nPads)
{
	m_displayWindow.DisplayDotPattern(clr, dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayHVLinesPattern( const ColorRGBDisplay& clr , BOOL dot2, BOOL vLines)
{
	m_displayWindow.DisplayHVLinesPattern(clr, dot2, vLines);
	return TRUE;
}

BOOL CGDIGenerator::DisplayColorLevelPattern( INT clrLevel , BOOL dot2, UINT nPads)
{
	m_displayWindow.DisplayColorLevelPattern(clrLevel, dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayGeomPattern( BOOL dot2, UINT nPads)
{
	m_displayWindow.DisplayGeomPattern(dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayConvPattern( BOOL dot2, UINT nPads)
{
	m_displayWindow.DisplayConvPattern(dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayColorPattern( BOOL dot2)
{
	m_displayWindow.DisplayColorPattern(dot2);
	return TRUE;
}

BOOL CGDIGenerator::DisplayPatternPicture(HMODULE hInst, UINT nIDResource, BOOL bResizePict)
{
	m_displayWindow.DisplayPatternPicture(hInst,nIDResource,bResizePict);
	return TRUE;
}


BOOL CGDIGenerator::Release(INT nbNext)
{
	GetColorApp() -> SetPatternWindow ( NULL );
	m_displayWindow.Hide();
	m_displayWindow.SetDisplayMode(GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE));
	m_displayWindow.m_nPat = 0;

	if (m_madVR_HDR && m_GDIGenePropertiesPage.m_nDisplayMode == DISPLAY_madVR)
		  madVR_SetHdrButton(FALSE);

	BOOL bOk = CGenerator::Release(nbNext);
	m_nPat = 0;
	if ( m_bBlankingCanceled )
	{
		m_doScreenBlanking = TRUE;
		m_bBlankingCanceled = FALSE;
	}

	//restore gamma tables
	if (m_bConnect && m_bLinear)
	{
		char arg[255];
		CString str = GetConfig () -> m_ApplicationPath;
		CString str1 = GetConfig () -> m_ApplicationPath;
		str += "\\tools\\dispwin.exe";
		str1 += "\\tools\\current.cal";
		sprintf(arg," -d%d " + str1, m_activeMonitorNum+1);
		ShellExecute(NULL, "open", str, arg, NULL, SW_HIDE);
		m_bConnect = FALSE;
	}

	return bOk;
}

CString CGDIGenerator::GetActiveDisplayName() 
{ 
	return m_GDIGenePropertiesPage.m_monitorNameArray[m_activeMonitorNum];
}