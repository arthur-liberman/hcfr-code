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

// GDIGenePropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GDIGenerator.h"
#include "DataSetDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CGDIGenePropPage property page

IMPLEMENT_DYNCREATE(CGDIGenePropPage, CPropertyPageWithHelp)

CGDIGenePropPage::CGDIGenePropPage() : CPropertyPageWithHelp(CGDIGenePropPage::IDD)
{
	//{{AFX_DATA_INIT(CGDIGenePropPage)
	m_rectSizePercent = 0;
	m_bgStimPercent = 0;
	m_Intensity = 0;
	//}}AFX_DATA_INIT
	m_activeMonitorNum = 0;
	m_nDisplayMode = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_GDI);
	m_b16_235 = FALSE;
	m_busePic = FALSE;
    m_madVR_3d = FALSE;
    m_madVR_vLUT = FALSE;
	m_madVR_OSD = FALSE;
}

CGDIGenePropPage::~CGDIGenePropPage()
{
}

void CGDIGenePropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGDIGenePropPage)
    DDX_Control(pDX, IDC_MADVR_3D, m_madVREdit);    
	DDX_Control(pDX, IDC_MONITOR_COMBO, m_monitorComboCtrl);
    DDX_Control(pDX, IDC_MADVR_3D2, m_madVREdit2);    
    DDX_Control(pDX, IDC_MADVR_OSD, m_madVREdit3);    
    DDX_Control(pDX, IDC_USEPIC, m_usePicEdit);    
	DDX_Text(pDX, IDC_PATTERNSIZE_EDIT, m_rectSizePercent);
	DDX_Text(pDX, IDC_BGSTIM_EDIT, m_bgStimPercent);
	DDX_Text(pDX, IDC_INTENSITY_EDIT, m_Intensity);
	DDV_MinMaxUInt(pDX, m_rectSizePercent, 1, 100);
	DDV_MinMaxUInt(pDX, m_bgStimPercent, 0, 100);
	DDV_MinMaxUInt(pDX, m_Intensity, 1, 100);
	DDX_Check(pDX, IDC_MADVR_3D, m_madVR_3d);
	DDX_Check(pDX, IDC_MADVR_3D2, m_madVR_vLUT);
	DDX_Check(pDX, IDC_MADVR_OSD, m_madVR_OSD);
	DDX_Check(pDX, IDC_USEPIC, m_busePic);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGDIGenePropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CGDIGenePropPage)
	ON_BN_CLICKED(IDC_OVERLAY, OnTestOverlay)
	ON_BN_CLICKED(IDC_RADIO1, OnClickmadVR)
	ON_BN_CLICKED(IDC_RADIO2, OnClickmadVR)
	ON_BN_CLICKED(IDC_RADIO3, OnClickmadVR)
	ON_BN_CLICKED(IDC_RADIO4, OnClickmadVR)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGDIGenePropPage message handlers

void CGDIGenePropPage::OnOK() 
{
	m_activeMonitorNum=m_monitorComboCtrl.GetCurSel();	
	if ( IsDlgButtonChecked ( IDC_RADIO2 ) )
		m_nDisplayMode = DISPLAY_OVERLAY;

	else if ( IsDlgButtonChecked ( IDC_RADIO3 ) )
		m_nDisplayMode = DISPLAY_madVR;

	else if ( IsDlgButtonChecked ( IDC_RADIO4 ) )
		m_nDisplayMode = DISPLAY_GDI_nBG;

	else
		m_nDisplayMode = DISPLAY_GDI;

	if ( IsDlgButtonChecked ( IDC_RGBLEVEL_RADIO2 ) )
		m_b16_235 = TRUE;
	else
		m_b16_235 = FALSE;

	CPropertyPageWithHelp::OnOK();
}

BOOL CGDIGenePropPage::OnSetActive() 
{
	// Init combo box with monitor list stored in array
	m_monitorComboCtrl.ResetContent();
	for(int i=0;i<m_monitorNameArray.GetSize();i++)
		m_monitorComboCtrl.AddString(m_monitorNameArray[i]);

	// Select correct monitor
	if( m_activeMonitorNum < m_monitorComboCtrl.GetCount() )
		m_monitorComboCtrl.SetCurSel(m_activeMonitorNum);
	else
		m_monitorComboCtrl.SetCurSel(0);
	
	CheckRadioButton ( IDC_RADIO1,  IDC_RADIO4 , IDC_RADIO1 + m_nDisplayMode );
	CheckRadioButton ( IDC_RGBLEVEL_RADIO1, IDC_RGBLEVEL_RADIO2, IDC_RGBLEVEL_RADIO1 + m_b16_235 );
    if (IsDlgButtonChecked ( IDC_RADIO3 ) )
    {
        m_madVREdit.EnableWindow(TRUE);
        m_madVREdit2.EnableWindow(TRUE);
        m_madVREdit3.EnableWindow(TRUE);
    }
    else
    {
        m_madVREdit.EnableWindow(FALSE);
        m_madVREdit2.EnableWindow(FALSE);
        m_madVREdit3.EnableWindow(FALSE);
    }

	return CPropertyPageWithHelp::OnSetActive();
}

BOOL CGDIGenePropPage::OnKillActive() 
{
	m_activeMonitorNum=m_monitorComboCtrl.GetCurSel();	
	
	if ( IsDlgButtonChecked ( IDC_RADIO4 ) )
	{
		m_nDisplayMode = DISPLAY_GDI_nBG;
	}
	else if ( IsDlgButtonChecked ( IDC_RADIO3 ) )
	{
		m_nDisplayMode = DISPLAY_madVR;
	}
	else if ( IsDlgButtonChecked ( IDC_RADIO2 ) )
	{
		m_nDisplayMode = DISPLAY_OVERLAY;
	}
	else
	{
		m_nDisplayMode = DISPLAY_GDI;
	}
	if ( IsDlgButtonChecked ( IDC_RGBLEVEL_RADIO2 ) )
	{
		m_b16_235 = TRUE;
	}
	else
	{
		m_b16_235 = FALSE;
	}

	return CPropertyPageWithHelp::OnKillActive();
}

void CGDIGenePropPage::OnTestOverlay() 
{
	CFullScreenWindow	OverlayWnd ( TRUE );

	m_activeMonitorNum=m_monitorComboCtrl.GetCurSel();	

	OverlayWnd.MoveToMonitor(m_monitorHandle[m_activeMonitorNum]);

	if ( OverlayWnd.m_nDisplayMode == DISPLAY_OVERLAY )
		OverlayWnd.DisplayRGBColor ( ColorRGBDisplay(0.5), TRUE );

	if ( OverlayWnd.m_nDisplayMode == DISPLAY_OVERLAY )
		MessageBox ( "Overlay window created (small gray rectangle on top-right). You can use advanced display properties to change settings.\r\nClick OK to close overlay window.", "Overlay", MB_OK | MB_ICONINFORMATION );
	else
		MessageBox ( "An error occured during Overlay creation.", "Overlay", MB_OK | MB_ICONHAND );
}

void CGDIGenePropPage::OnClickmadVR() 
{
    if (IsDlgButtonChecked ( IDC_RADIO3 ) )
    {
        m_madVREdit.EnableWindow(TRUE);
        m_madVREdit2.EnableWindow(TRUE);
        m_madVREdit3.EnableWindow(TRUE);
    }
    else
    {
        m_madVREdit.EnableWindow(FALSE);
        m_madVREdit2.EnableWindow(FALSE);
        m_madVREdit3.EnableWindow(FALSE);
    }
}

UINT CGDIGenePropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_GENERATOR_GDI;
}
