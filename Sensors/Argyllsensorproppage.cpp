/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
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
//    Georges GALLERAND
//    John Adcock
//    Ian C
/////////////////////////////////////////////////////////////////////////////

// ArgyllSensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ArgyllSensor.h"
#include "ArgyllSensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CArgyllSensorPropPage property page

IMPLEMENT_DYNCREATE(CArgyllSensorPropPage, CPropertyPageWithHelp)

CArgyllSensorPropPage::CArgyllSensorPropPage() : CPropertyPageWithHelp(CArgyllSensorPropPage::IDD)
{
    //{{AFX_DATA_INIT(CArgyllSensorPropPage)
    m_DisplayType = 0;
    m_SpectralType = "Default";
    m_intTime = 1;
    m_ReadingType = 0;
    m_MeterName = "";
    m_DebugMode = FALSE;
    m_HiRes = FALSE;
    m_Adapt = FALSE;
    //}}AFX_DATA_INIT
}

CArgyllSensorPropPage::~CArgyllSensorPropPage()
{
}

void CArgyllSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPageWithHelp::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_ARGYLLSENSOR_DISPLAYTYPE_COMBO, m_DisplayTypeCombo);
    DDX_Control(pDX, IDC_ARGYLLSENSOR_SPECTRALTYPE_COMBO, m_SpectralTypeCombo);
    DDX_Control(pDX, IDC_ARGYLLSENSOR_INTTIME_COMBO, m_IntTypeCombo);
    DDX_Control(pDX, IDC_ARGYLL_SENSOR_HIRES, m_HiResCheckBox);
//    DDX_Control(pDX, IDC_ARGYLL_SENSOR_ADAPT, m_AdaptCheckBox);

    if(m_DisplayTypeCombo.GetCount() == 0)
    {
        m_pSensor->FillDisplayTypeCombo(m_DisplayTypeCombo);
    }
    m_DisplayTypeCombo.EnableWindow((m_DisplayTypeCombo.GetCount() != 0)?TRUE:FALSE);
    m_SpectralTypeCombo.EnableWindow(m_obTypeEnabled);
    m_IntTypeCombo.EnableWindow(m_intTimeEnabled);

    m_HiResCheckBox.EnableWindow(m_HiResCheckBoxEnabled);
    m_AdaptCheckBox.EnableWindow(FALSE);
	m_HiRes = (m_HiResCheckBoxEnabled?m_HiRes:0);

    if ( m_DisplayTypeCombo.GetCount() == 0 && m_ReadingType == 0 && m_MeterName == "Xrite i1 DisplayPro, ColorMunki Display" )
    {
        m_ReadingType = 2;
        GetColorApp()->InMeasureMessageBox("Diffuser is deployed, switching to Ambient mode","Wrong mode!",MB_OK);
    }

    DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_DISPLAYTYPE_COMBO, m_DisplayType);
    DDX_CBString(pDX, IDC_ARGYLLSENSOR_SPECTRALTYPE_COMBO, m_SpectralType);
    DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_INTTIME_COMBO, m_intTime);
	DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_READINGTYPE_COMBO, m_ReadingType);
    DDX_Text(pDX, IDC_ARGYLLSENSOR_METER_NAME, m_MeterName);
    DDX_Check(pDX, IDC_ARGYLL_SENSOR_DEBUG_CB, m_DebugMode);
    DDX_Check(pDX, IDC_ARGYLL_SENSOR_HIRES, m_HiRes);
//    DDX_Check(pDX, IDC_ARGYLL_SENSOR_ADAPT, m_Adapt);
}


BEGIN_MESSAGE_MAP(CArgyllSensorPropPage, CPropertyPageWithHelp)
    //{{AFX_MSG_MAP(CArgyllSensorPropPage)
    ON_BN_CLICKED(IDC_ARGYLL_CALIBRATE, OnCalibrate)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CArgyllSensorPropPage message handlers
/////////////////////////////////////////////////////////////////////////////

void CArgyllSensorPropPage::OnCalibrate()
{
    UpdateData(TRUE);
    m_pSensor->GetPropertiesSheetValues();
    m_pSensor->Calibrate();
}


UINT CArgyllSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
    return HID_SENSOR_EYEONE;
}

BOOL CArgyllSensorPropPage::OnSetActive() 
{
    // TODO: Add your specialized code here and/or call the base class
    if (m_SpectralType == "")
        m_SpectralType = "Default";
    BOOL bRet = CPropertyPageWithHelp::OnSetActive();
    return bRet;
}
