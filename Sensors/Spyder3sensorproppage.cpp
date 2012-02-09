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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// Spyder3SensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "Spyder3Sensor.h"
#include "Spyder3SensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSpyder3SensorPropPage property page

IMPLEMENT_DYNCREATE(CSpyder3SensorPropPage, CPropertyPageWithHelp)

CSpyder3SensorPropPage::CSpyder3SensorPropPage() : CPropertyPageWithHelp(CSpyder3SensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CSpyder3SensorPropPage)
	m_ReadTime = 3000;
	m_bAdjustTime = FALSE;
	m_bAverageReads = FALSE;
	m_debugMode = FALSE;
	//}}AFX_DATA_INIT
}

CSpyder3SensorPropPage::~CSpyder3SensorPropPage()
{
}

void CSpyder3SensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSpyder3SensorPropPage)
	DDX_Text(pDX, IDC_READTIME_EDIT, m_ReadTime);
	DDV_MinMaxInt(pDX, m_ReadTime, 50, 30000);
	DDX_Check(pDX, IDC_CHECK_VARIABLE_READTIME, m_bAdjustTime);
	DDX_Check(pDX, IDC_CHECK_AVG_MEASURES, m_bAverageReads);
	DDX_Check(pDX, IDC_KISENSOR_DEBUG_CB, m_debugMode);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSpyder3SensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CSpyder3SensorPropPage)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSpyder3SensorPropPage message handlers
/////////////////////////////////////////////////////////////////////////////

UINT CSpyder3SensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_SPYDER3;
}

