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
	m_nDisplayMode = GetConfig()->GetProfileInt("GDIGenerator","DisplayMode",DISPLAY_DEFAULT_MODE);
	m_b16_235 = FALSE;
	m_busePic = FALSE;
    m_madVR_3d = FALSE;
    m_madVR_vLUT = FALSE;
	m_madVR_HDR = FALSE;
	m_madVR_OSD = FALSE;
	m_bdispTrip = FALSE;
	m_bLinear = FALSE;
	m_bHdr10 = GetConfig()->GetProfileInt("GDIGenerator","EnableHDR10",0);
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
	DDX_Control(pDX, IDC_CCAST_COMBO, m_cCastComboCtrl);
    DDX_Control(pDX, IDC_MADVR_3D2, m_madVREdit2);    
    DDX_Control(pDX, IDC_MADVR_HDR, m_madVREdit4);    
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
	DDX_Check(pDX, IDC_MADVR_HDR, m_madVR_HDR);
	DDX_Check(pDX, IDC_MADVR_OSD, m_madVR_OSD);
	DDX_Check(pDX, IDC_USEPIC, m_busePic);
	DDX_Check(pDX, IDC_DISP_TRIP, m_bdispTrip);
	DDX_Check(pDX, IDC_DISP_TRIP2, m_bLinear);
	DDX_Check(pDX, IDC_ENBL_HDR, m_bHdr10);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGDIGenePropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CGDIGenePropPage)
	ON_BN_CLICKED(IDC_OVERLAY, OnTestOverlay)
	ON_BN_CLICKED(IDC_RADIO1, OnClickmadVR)
	ON_BN_CLICKED(IDC_RADIO2, OnClickmadVR)
	ON_BN_CLICKED(IDC_RADIO3, OnClickmadVR)
	ON_BN_CLICKED(IDC_RADIO4, OnClickmadVR)
	ON_BN_CLICKED(IDC_RADIO5, OnClickmadVR)
	ON_BN_CLICKED(IDC_RADIO6, OnClickmadVR)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGDIGenePropPage message handlers

void CGDIGenePropPage::OnOK() 
{
	m_activeMonitorNum=m_monitorComboCtrl.GetCurSel();	
	m_selectedGcastNum = m_cCastComboCtrl.GetCurSel();
	if ( IsDlgButtonChecked ( IDC_RADIO2 ) )
		m_nDisplayMode = DISPLAY_OVERLAY;
	else if ( IsDlgButtonChecked ( IDC_RADIO3 ) )
		m_nDisplayMode = DISPLAY_madVR;
	else if ( IsDlgButtonChecked ( IDC_RADIO4 ) )
		m_nDisplayMode = DISPLAY_GDI_nBG;
	else if ( IsDlgButtonChecked ( IDC_RADIO5 ) )
	{
		m_nDisplayMode = DISPLAY_ccast;
		m_b16_235 = FALSE;
	}
	else if ( IsDlgButtonChecked ( IDC_RADIO6 ) )
		m_nDisplayMode = DISPLAY_GDI_Hide;
	else
		m_nDisplayMode = DISPLAY_GDI;

	if (m_nDisplayMode == DISPLAY_madVR || m_nDisplayMode == DISPLAY_ccast )
	{
		GetDlgItem(IDC_RGBLEVEL_RADIO1)->EnableWindow(FALSE);
		GetDlgItem(IDC_RGBLEVEL_RADIO2)->EnableWindow(FALSE);
		GetDlgItem(IDC_DISP_TRIP2)->EnableWindow(FALSE);
	} else
	{

		GetDlgItem(IDC_RGBLEVEL_RADIO1)->EnableWindow(TRUE);
		GetDlgItem(IDC_RGBLEVEL_RADIO2)->EnableWindow(TRUE);
		GetDlgItem(IDC_DISP_TRIP2)->EnableWindow(TRUE);
		if ( IsDlgButtonChecked ( IDC_RGBLEVEL_RADIO2 ) )
			m_b16_235 = TRUE;
		else
			m_b16_235 = FALSE;
	}

	if (m_nDisplayMode == DISPLAY_GDI || m_nDisplayMode == DISPLAY_GDI_Hide || m_nDisplayMode == DISPLAY_GDI_nBG)
	{
		GetDlgItem(IDC_ENBL_HDR)->EnableWindow(TRUE);
		if ( IsDlgButtonChecked ( IDC_ENBL_HDR ) )
			m_bHdr10 = TRUE;
		else
			m_bHdr10 = FALSE;
	}
	else
	{
		GetDlgItem(IDC_ENBL_HDR)->EnableWindow(FALSE);
	}

	CheckRadioButton ( IDC_RGBLEVEL_RADIO1, IDC_RGBLEVEL_RADIO2, IDC_RGBLEVEL_RADIO1 + m_b16_235 );
	CheckRadioButton ( IDC_RADIO1,  IDC_RADIO1 + m_nDisplayMode , IDC_RADIO1 + m_nDisplayMode );

	CGDIGenerator m_pGenerator;
	m_pGenerator.m_nDisplayMode = m_nDisplayMode;
	GetConfig()->WriteProfileInt("GDIGenerator","DisplayMode",m_nDisplayMode);
	GetConfig()->WriteProfileInt("GDIGenerator","RGB_16_235",m_b16_235);
	GetConfig()->WriteProfileInt("GDIGenerator","EnableHDR10",m_bHdr10);
	if (m_nDisplayMode == DISPLAY_ccast && m_GCast.getCount() > 0)
	{
		char nameBuf[1024];
		m_cCastComboCtrl.GetWindowTextA(nameBuf, 1024);
		GetConfig()->WriteProfileInt("GDIGenerator","CCastIp",m_GCast.getCcastIpAddress(m_GCast[nameBuf]));
	}

	CPropertyPageWithHelp::OnOK();
}

BOOL CGDIGenePropPage::OnSetActive() 
{
	// Init combo box with monitor list stored in array
	m_monitorComboCtrl.ResetContent();
	m_cCastComboCtrl.ResetContent();
	for(int i=0;i<m_monitorNameArray.GetSize();i++)
		m_monitorComboCtrl.AddString(m_monitorNameArray[i]);

	// Select correct monitor
	if( m_activeMonitorNum < m_monitorComboCtrl.GetCount() )
		m_monitorComboCtrl.SetCurSel(m_activeMonitorNum);
	else
		m_monitorComboCtrl.SetCurSel(0);
	
	CheckRadioButton ( IDC_RADIO1,  IDC_RADIO1 + m_nDisplayMode , IDC_RADIO1 + m_nDisplayMode );
	CheckRadioButton ( IDC_RGBLEVEL_RADIO1, IDC_RGBLEVEL_RADIO2, IDC_RGBLEVEL_RADIO1 + m_b16_235 );

	if (IsDlgButtonChecked ( IDC_RADIO3 ) )
    {
        m_madVREdit.EnableWindow(TRUE);
        m_madVREdit2.EnableWindow(TRUE);
        m_madVREdit3.EnableWindow(TRUE);
        m_madVREdit4.EnableWindow(TRUE);
    }
    else
    {
        m_madVREdit.EnableWindow(FALSE);
        m_madVREdit2.EnableWindow(FALSE);
        m_madVREdit3.EnableWindow(FALSE);
        m_madVREdit4.EnableWindow(FALSE);
    }

	if ( IsDlgButtonChecked ( IDC_RADIO2 ) )
		m_nDisplayMode = DISPLAY_OVERLAY;
	else if ( IsDlgButtonChecked ( IDC_RADIO3 ) )
		m_nDisplayMode = DISPLAY_madVR;
	else if ( IsDlgButtonChecked ( IDC_RADIO4 ) )
		m_nDisplayMode = DISPLAY_GDI_nBG;
	else if ( IsDlgButtonChecked ( IDC_RADIO5 ) )
	{
		m_nDisplayMode = DISPLAY_ccast;
		m_b16_235 = FALSE;
		GetDlgItem(IDC_CCAST_COMBO)->EnableWindow(TRUE);
		CheckRadioButton ( IDC_RGBLEVEL_RADIO1, IDC_RGBLEVEL_RADIO2, IDC_RGBLEVEL_RADIO1 + m_b16_235 );
		m_GCast.RefreshList();
		unsigned int ccastIp = GetConfig()->GetProfileInt("GDIGenerator", "CCastIp", 0);
		for (int i = 0 ; i < m_GCast.getCount(); i++)
		{
			if (m_GCast[i]->typ != cctyp_Audio)
				m_cCastComboCtrl.AddString(m_GCast[i]->name);

			if (ccastIp && m_cCastComboCtrl.GetCurSel() < 0 && m_GCast.getCcastIpAddress(m_GCast[i]) == ccastIp)
				m_cCastComboCtrl.SetCurSel(i);
		}
		if (m_cCastComboCtrl.GetCurSel() < 0)
			m_cCastComboCtrl.SetCurSel(0);
	}
	else if ( IsDlgButtonChecked ( IDC_RADIO6 ) )
		m_nDisplayMode = DISPLAY_GDI_Hide;
	else
		m_nDisplayMode = DISPLAY_GDI;

	if (m_nDisplayMode == DISPLAY_madVR || m_nDisplayMode == DISPLAY_ccast )
	{
		GetDlgItem(IDC_RGBLEVEL_RADIO1)->EnableWindow(FALSE);
		GetDlgItem(IDC_RGBLEVEL_RADIO2)->EnableWindow(FALSE);
		GetDlgItem(IDC_DISP_TRIP2)->EnableWindow(FALSE);
	} else
	{
		GetDlgItem(IDC_RGBLEVEL_RADIO1)->EnableWindow(TRUE);
		GetDlgItem(IDC_RGBLEVEL_RADIO2)->EnableWindow(TRUE);
		GetDlgItem(IDC_DISP_TRIP2)->EnableWindow(TRUE);
		if ( IsDlgButtonChecked ( IDC_RGBLEVEL_RADIO2 ) )
			m_b16_235 = TRUE;
		else
			m_b16_235 = FALSE;
	}

	if (m_nDisplayMode == DISPLAY_GDI || m_nDisplayMode == DISPLAY_GDI_Hide || m_nDisplayMode == DISPLAY_GDI_nBG)
	{
		GetDlgItem(IDC_ENBL_HDR)->EnableWindow(TRUE);
		if ( IsDlgButtonChecked ( IDC_ENBL_HDR ) )
			m_bHdr10 = TRUE;
		else
			m_bHdr10 = FALSE;
	}
	else
	{
		GetDlgItem(IDC_ENBL_HDR)->EnableWindow(FALSE);
	}

	return CPropertyPageWithHelp::OnSetActive();
}

BOOL CGDIGenePropPage::OnKillActive() 
{
	m_activeMonitorNum=m_monitorComboCtrl.GetCurSel();	
	if ( IsDlgButtonChecked ( IDC_RADIO6 ) )
	{
		m_nDisplayMode = DISPLAY_GDI_Hide;
	} else if ( IsDlgButtonChecked ( IDC_RADIO5 ) )
	{
		m_nDisplayMode = DISPLAY_ccast;
	}	
	else if ( IsDlgButtonChecked ( IDC_RADIO4 ) )
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

	if ( IsDlgButtonChecked ( IDC_ENBL_HDR ) )
	{
		m_bHdr10 = TRUE;
	}
	else
	{
		m_bHdr10 = FALSE;
	}

	if ( IsDlgButtonChecked ( IDC_MADVR_HDR ) )
	{
		m_madVR_HDR = TRUE;
	}
	else
	{
		m_madVR_HDR = FALSE;
	}

	CheckRadioButton ( IDC_RGBLEVEL_RADIO1, IDC_RGBLEVEL_RADIO2, IDC_RGBLEVEL_RADIO1 + m_b16_235 );
	CheckRadioButton ( IDC_RADIO1,  IDC_RADIO1 + m_nDisplayMode , IDC_RADIO1 + m_nDisplayMode );

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
        m_madVREdit4.EnableWindow(TRUE);
    }
    else
    {
        m_madVREdit.EnableWindow(FALSE);
        m_madVREdit2.EnableWindow(FALSE);
        m_madVREdit3.EnableWindow(FALSE);
        m_madVREdit4.EnableWindow(FALSE);
    }

	if (IsDlgButtonChecked ( IDC_RADIO5 ) )
	{
		GetDlgItem(IDC_CCAST_COMBO)->EnableWindow(TRUE);
		m_cCastComboCtrl.ResetContent();
		m_GCast.RefreshList();
		unsigned int ccastIp = GetConfig()->GetProfileInt("GDIGenerator", "CCastIp", 0);
		for (int i = 0 ; i < m_GCast.getCount(); i++)
		{
			if (m_GCast[i]->typ != cctyp_Audio)
				m_cCastComboCtrl.AddString(m_GCast[i]->name);

			if (ccastIp && m_cCastComboCtrl.GetCurSel() < 0 && m_GCast.getCcastIpAddress(m_GCast[i]) == ccastIp)
				m_cCastComboCtrl.SetCurSel(i);
		}
		if (m_cCastComboCtrl.GetCurSel() < 0)
			m_cCastComboCtrl.SetCurSel(0);
	}
	else
		GetDlgItem(IDC_CCAST_COMBO)->EnableWindow(FALSE);


	m_activeMonitorNum=m_monitorComboCtrl.GetCurSel();	
	if ( IsDlgButtonChecked ( IDC_RADIO2 ) )
		m_nDisplayMode = DISPLAY_OVERLAY;
	else if ( IsDlgButtonChecked ( IDC_RADIO3 ) )
		m_nDisplayMode = DISPLAY_madVR;
	else if ( IsDlgButtonChecked ( IDC_RADIO4 ) )
		m_nDisplayMode = DISPLAY_GDI_nBG;
	else if ( IsDlgButtonChecked ( IDC_RADIO5 ) )
	{
		m_nDisplayMode = DISPLAY_ccast;
		m_b16_235 = FALSE;
		CheckRadioButton ( IDC_RGBLEVEL_RADIO1, IDC_RGBLEVEL_RADIO2, IDC_RGBLEVEL_RADIO1 + m_b16_235 );
	}
	else if ( IsDlgButtonChecked ( IDC_RADIO6 ) )
		m_nDisplayMode = DISPLAY_GDI_Hide;
	else
		m_nDisplayMode = DISPLAY_GDI;

	if (m_nDisplayMode == DISPLAY_madVR || m_nDisplayMode == DISPLAY_ccast )
	{
		GetDlgItem(IDC_RGBLEVEL_RADIO1)->EnableWindow(FALSE);
		GetDlgItem(IDC_RGBLEVEL_RADIO2)->EnableWindow(FALSE);
		GetDlgItem(IDC_DISP_TRIP2)->EnableWindow(FALSE);
	} else
	{

		GetDlgItem(IDC_RGBLEVEL_RADIO1)->EnableWindow(TRUE);
		GetDlgItem(IDC_RGBLEVEL_RADIO2)->EnableWindow(TRUE);
		GetDlgItem(IDC_DISP_TRIP2)->EnableWindow(TRUE);
		if ( IsDlgButtonChecked ( IDC_RGBLEVEL_RADIO2 ) )
			m_b16_235 = TRUE;
		else
			m_b16_235 = FALSE;
	}

	if (m_nDisplayMode == DISPLAY_GDI || m_nDisplayMode == DISPLAY_GDI_Hide || m_nDisplayMode == DISPLAY_GDI_nBG)
	{
		GetDlgItem(IDC_ENBL_HDR)->EnableWindow(TRUE);
		if ( IsDlgButtonChecked ( IDC_ENBL_HDR ) )
			m_bHdr10 = TRUE;
		else
			m_bHdr10 = FALSE;
	}
	else
	{
		GetDlgItem(IDC_ENBL_HDR)->EnableWindow(FALSE);
	}
}

UINT CGDIGenePropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_GENERATOR_GDI;
}
