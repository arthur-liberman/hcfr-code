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
    m_ReadingType = 0;
    m_PortNumber = 1;
    m_DebugMode = FALSE;
    //}}AFX_DATA_INIT
}

CArgyllSensorPropPage::~CArgyllSensorPropPage()
{
}

void CArgyllSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPageWithHelp::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CArgyllSensorPropPage)
    DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_DISPLAYTYPE_COMBO, m_DisplayType);
    DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_READINGTYPE_COMBO, m_ReadingType);
    DDX_CBIndex(pDX, IDC_ARGYLLSENSOR_PORTNUMBER_COMBO, m_PortNumber);
    DDX_Check(pDX, IDC_ARGYLL_SENSOR_DEBUG_CB, m_DebugMode);
    //}}AFX_DATA_MAP
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
    BOOL bRet = CPropertyPageWithHelp::OnSetActive();
    return bRet;
}
