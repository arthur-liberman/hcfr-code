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


#include <string>

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
	m_displayWindow.m_rectSizePercent=GetConfig()->GetProfileInt("GDIGenerator","SizePercent",100);

	GetMonitorList();
	m_activeMonitorNum = m_monitorNb-1;

	m_nDisplayMode = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_GDI);
	m_b16_235 = GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",0);

	m_displayWindow.SetDisplayMode();	// Always init in GDI mode during init

	CString str;
	str.LoadString(IDS_GDIGENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str); 
	m_propertySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	AddPropertyPage(&m_GDIGenePropertiesPage);

	str.LoadString(IDS_GDIGENERATOR_NAME);
	SetName(str);
}

CGDIGenerator::CGDIGenerator(int nDisplayMode, BOOL b16_235)
{
	// This constructor is used by pattern generator: no screen blanking
	m_bBlankingCanceled = FALSE;
	m_doScreenBlanking = FALSE;
	m_displayWindow.m_rectSizePercent=100;

	GetMonitorList();
	m_activeMonitorNum = m_monitorNb-1;

	m_nDisplayMode = nDisplayMode;
	m_b16_235 = b16_235;
	m_displayWindow.SetDisplayMode();

	CString str;
	str.LoadString(IDS_GDIGENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str); 
	m_propertySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	AddPropertyPage(&m_GDIGenePropertiesPage);

	str.LoadString(IDS_GDIGENERATOR_NAME);
	SetName(str);
}

CGDIGenerator::~CGDIGenerator()
{
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
		int version=2;
		archive << version;
		archive << m_displayWindow.m_rectSizePercent;
		archive << m_activeMonitorNum;
		archive << m_nDisplayMode;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 2 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_displayWindow.m_rectSizePercent;
		archive >> m_activeMonitorNum;
		if ( version >= 2 )
			archive >> m_nDisplayMode;
		else
			m_nDisplayMode = DISPLAY_GDI;
	}
}

void CGDIGenerator::SetPropertiesSheetValues()
{
	CGenerator::SetPropertiesSheetValues();

	m_GDIGenePropertiesPage.m_rectSizePercent=m_displayWindow.m_rectSizePercent;
	m_GDIGenePropertiesPage.m_activeMonitorNum=m_activeMonitorNum;
	m_GDIGenePropertiesPage.m_nDisplayMode=m_nDisplayMode;
	m_GDIGenePropertiesPage.m_b16_235=m_b16_235;
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
}

BOOL CGDIGenerator::Init(UINT nbMeasure)
{
	BOOL	bOk, bOnOtherMonitor;

	m_displayWindow.SetDisplayMode(m_nDisplayMode);
	m_displayWindow.SetRGBScale(m_b16_235);
	m_displayWindow.MoveToMonitor(m_hMonitor[m_activeMonitorNum]);
	
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

	return bOk;
}

BOOL CGDIGenerator::DisplayRGBColor( COLORREF clr , MeasureType nPatternType , UINT nPatternInfo , BOOL bChangePattern,BOOL bSilentMode)
{
	m_displayWindow.DisplayRGBColor(clr);

	return TRUE;
}

BOOL CGDIGenerator::CanDisplayAnsiBWRects()
{
	return TRUE;	
}

BOOL CGDIGenerator::CanDisplayAnimatedPatterns()
{
	return TRUE;	
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

BOOL CGDIGenerator::DisplayDotPattern( COLORREF clr , BOOL dot2, UINT nPads)
{
	m_displayWindow.DisplayDotPattern(clr, dot2, nPads);
	return TRUE;
}

BOOL CGDIGenerator::DisplayHVLinesPattern( COLORREF clr , BOOL dot2, BOOL vLines)
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
	m_displayWindow.SetDisplayMode();
	
	BOOL bOk = CGenerator::Release();

	if ( m_bBlankingCanceled )
	{
		m_doScreenBlanking = TRUE;
		m_bBlankingCanceled = FALSE;
	}

	return bOk;
}
