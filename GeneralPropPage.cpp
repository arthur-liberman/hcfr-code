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

// GeneralPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GeneralPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGeneralPropPage property page

IMPLEMENT_DYNCREATE(CGeneralPropPage, CPropertyPageWithHelp)

CGeneralPropPage::CGeneralPropPage() : CPropertyPageWithHelp(CGeneralPropPage::IDD)
{
	//{{AFX_DATA_INIT(CGeneralPropPage)
	m_doMultipleInstance = FALSE;
	m_doUpdateCheck = FALSE;
	m_bDisplayTestColors = TRUE;
	m_latencyTime = 0;
	m_doSavePosition = FALSE;
	m_bContinuousMeasures = TRUE;
	m_bLatencyBeep = FALSE;
	bDisplayRT = TRUE;
	m_BWColorsToAdd = -1;
	m_bDetectPrimaries = TRUE;
	m_useHSV = FALSE;
	m_isSettling = FALSE;
	m_bUseRoundDown = FALSE;
	//}}AFX_DATA_INIT

	m_isModified = FALSE;
}

CGeneralPropPage::~CGeneralPropPage()
{
}

void CGeneralPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGeneralPropPage)
	DDX_Check(pDX, IDC_CHECK_MULTIPLEINSTANCE, m_doMultipleInstance);
	DDX_Check(pDX, IDC_CHECK_UPDATE, m_doUpdateCheck);
	DDX_Check(pDX, IDC_CHECK_COLORS, m_bDisplayTestColors);
	DDX_Text(pDX, IDC_EDIT_IRIS_TIME, m_latencyTime);
	DDV_MinMaxInt(pDX, m_latencyTime, 0, 30000);
	DDX_Check(pDX, IDC_CHECK_SAVEPOSITION, m_doSavePosition);
	DDX_Check(pDX, IDC_CHECK_CONTINUOUS, m_bContinuousMeasures);
	DDX_Check(pDX, IDC_CHECK_BEEP, m_bLatencyBeep);
	DDX_Check(pDX, IDC_CHECK_DISPLAYRT, bDisplayRT);
	DDX_Radio(pDX, IDC_RADIO1, m_BWColorsToAdd);
	DDX_Check(pDX, IDC_CHECK_DETECT_PRIMARIES, m_bDetectPrimaries);
	DDX_Check(pDX, IDC_IS_SETTLING, m_isSettling);
	DDX_Check(pDX, IDC_CHECK_USE_HSV, m_useHSV);
	DDX_Check(pDX, IDC_CHECK_USE_ROUNDDOWN, m_bUseRoundDown);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGeneralPropPage, CPropertyPageWithHelp)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_MULTIPLEINSTANCE, IDC_CHECK_MULTIPLEINSTANCE, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_MULTIPLEINSTANCE, IDC_CHECK_UPDATE, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_SAVEPOSITION, IDC_CHECK_SAVEPOSITION, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_COLORS, IDC_CHECK_COLORS, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_CONTINUOUS, IDC_CHECK_CONTINUOUS, OnControlClicked)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_EDIT_IRIS_TIME, IDC_EDIT_IRIS_TIME, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_RADIO1, IDC_RADIO3, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_BEEP, IDC_CHECK_BEEP, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_DISPLAYRT, IDC_CHECK_DISPLAYRT, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_DETECT_PRIMARIES, IDC_CHECK_DETECT_PRIMARIES, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_USE_HSV, IDC_CHECK_USE_HSV, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_IS_SETTLING, IDC_IS_SETTLING, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_USE_ROUNDDOWN, IDC_CHECK_USE_ROUNDDOWN, OnControlClicked)
	//{{AFX_MSG_MAP(CGeneralPropPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGeneralPropPage message handlers

void CGeneralPropPage::OnControlClicked(UINT nID) 
{
	// m_isModified becomes true only when Continuous Reading flag changes. This flag
	// allow parent dialog to send a WM_SYSCOLORCHANGE message to all DataSetView to change
	// measurement button look (camera or start icon).
	if ( nID == IDC_CHECK_CONTINUOUS || IDC_CHECK_USE_ROUNDDOWN )
    {
		m_isModified=TRUE;
    	SetModified(TRUE);
    }
	
}

BOOL CGeneralPropPage::OnApply() 
{
    GetConfig()->ApplySettings(FALSE);
	m_isModified=FALSE;
    return CPropertyPageWithHelp::OnApply();
}

UINT CGeneralPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_PREF_GENERAL;
}
