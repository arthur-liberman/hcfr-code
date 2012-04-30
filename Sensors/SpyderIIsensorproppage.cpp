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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// SpyderIISensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "SpyderIISensor.h"
#include "SpyderIISensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSpyderIISensorPropPage property page

IMPLEMENT_DYNCREATE(CSpyderIISensorPropPage, CPropertyPageWithHelp)

CSpyderIISensorPropPage::CSpyderIISensorPropPage() : CPropertyPageWithHelp(CSpyderIISensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CSpyderIISensorPropPage)
	m_CalibrationMode = 2;
	m_ReadTime = 300;
	m_bAdjustTime = FALSE;
	m_bAverageReads = FALSE;
	m_debugMode = FALSE;
	//}}AFX_DATA_INIT
}

CSpyderIISensorPropPage::~CSpyderIISensorPropPage()
{
}

void CSpyderIISensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSpyderIISensorPropPage)
	DDX_CBIndex(pDX, IDC_S2SENSOR_MODE_COMBO, m_CalibrationMode);
	DDX_Text(pDX, IDC_READTIME_EDIT, m_ReadTime);
	DDV_MinMaxInt(pDX, m_ReadTime, 50, 10000);
	DDX_Check(pDX, IDC_CHECK_VARIABLE_READTIME, m_bAdjustTime);
	DDX_Check(pDX, IDC_CHECK_AVG_MEASURES, m_bAverageReads);
	DDX_Check(pDX, IDC_KISENSOR_DEBUG_CB, m_debugMode);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSpyderIISensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CSpyderIISensorPropPage)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSpyderIISensorPropPage message handlers
/////////////////////////////////////////////////////////////////////////////

UINT CSpyderIISensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_SPYDERII;
}

