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

// AdvancedPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "AdvancedPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPropPage property page

IMPLEMENT_DYNCREATE(CAdvancedPropPage, CPropertyPageWithHelp)

CAdvancedPropPage::CAdvancedPropPage() : CPropertyPageWithHelp(CAdvancedPropPage::IDD)
{
	//{{AFX_DATA_INIT(CAdvancedPropPage)
	m_bConfirmMeasures = TRUE;
	m_comPort = _T("");
	m_bUseOnlyPrimaries = FALSE;
	m_bUseImperialUnits = FALSE;
	m_nLuminanceCurveMode = 0;
	m_bPreferLuxmeter = FALSE;
	m_dE_form = 5;
	m_dE_gray = 2;
    gw_Weight = 0;
    doHighlight = TRUE;
	//}}AFX_DATA_INIT

	m_isModified = FALSE;
}

CAdvancedPropPage::~CAdvancedPropPage()
{
}

void CAdvancedPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAdvancedPropPage)
	DDX_Control(pDX, IDC_COMBO_dE_WEIGHT, m_gwWeightEdit);
	DDX_Control(pDX, IDC_COMBO_dE_GRAY, m_dEgrayEdit);
	DDX_Control(pDX, IDC_COMBO_dE, m_dEform);
	DDX_Check(pDX, IDC_CHECK_CONFIRM, m_bConfirmMeasures);
	DDX_CBString(pDX, IDC_LUXMETER_COM_COMBO, m_comPort);
	DDX_CBIndex(pDX, IDC_COMBO_dE, m_dE_form);
	DDX_CBIndex(pDX, IDC_COMBO_dE_GRAY, m_dE_gray);
	DDX_CBIndex(pDX, IDC_COMBO_dE_WEIGHT, gw_Weight);
	DDX_Check(pDX, IDC_CHECK_CALIBRATION_OLD, m_bUseOnlyPrimaries);
	DDX_Check(pDX, IDC_HIGHLIGHT, doHighlight);
	DDX_Check(pDX, IDC_CHECK_IMPERIAL, m_bUseImperialUnits);
	DDX_Radio(pDX, IDC_RADIO1, m_nLuminanceCurveMode);
	DDX_Check(pDX, IDC_CHECK_PREFER_LUXMETER, m_bPreferLuxmeter);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAdvancedPropPage, CPropertyPageWithHelp)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_CONFIRM, IDC_CHECK_CONFIRM, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_IMPERIAL, IDC_CHECK_IMPERIAL, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_PREFER_LUXMETER, IDC_CHECK_PREFER_LUXMETER, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CHECK_DELTAE_GRAY_LUMA, IDC_CHECK_DELTAE_GRAY_LUMA, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_RADIO1, IDC_RADIO3, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_HIGHLIGHT, IDC_HIGHLIGHT, OnControlClicked)

	//{{AFX_MSG_MAP(CAdvancedPropPage)
	ON_CBN_SELCHANGE(IDC_LUXMETER_COM_COMBO, OnSelchangeLuxmeterComCombo)
	ON_CBN_SELCHANGE(IDC_COMBO_dE, OnSelchangedECombo)
	ON_CBN_SELCHANGE(IDC_COMBO_dE_GRAY, OnSelchangedECombo)
	ON_CBN_SELCHANGE(IDC_COMBO_dE_WEIGHT, OnSelchangedECombo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPropPage message handlers

void CAdvancedPropPage::OnControlClicked(UINT nID) 
{
	// m_isModified becomes true only when imperial units or luminance curve display flag changes. This flag
	// allow parent dialog to refresh all views to change data displayed
	if ( nID == IDC_CHECK_IMPERIAL || nID == IDC_CHECK_PREFER_LUXMETER || nID == IDC_RADIO1 || nID == IDC_RADIO2 || nID == IDC_RADIO3 || nID == IDC_CHECK_DELTAE_GRAY_LUMA || nID == IDC_HIGHLIGHT )
		m_isModified=TRUE;
	SetModified(TRUE);	
}

void CAdvancedPropPage::OnSelchangeLuxmeterComCombo() 
{
	SetModified(TRUE);	
}

void CAdvancedPropPage::OnSelchangedECombo() 
{
	if (m_dEform.GetCurSel() == 5)
	{
		m_dE_gray = 2;
		m_dEgrayEdit.EnableWindow(FALSE);
		((CComboBox&)m_dEgrayEdit).SetCurSel(m_dE_gray);
		gw_Weight = 0;
		m_gwWeightEdit.EnableWindow(FALSE);
		((CComboBox&)m_gwWeightEdit).SetCurSel(gw_Weight);
	}
	else if (((CComboBox&)m_dEgrayEdit).GetCurSel() == 0)
	{
		gw_Weight = 0;
		m_gwWeightEdit.EnableWindow(FALSE);
		((CComboBox&)m_gwWeightEdit).SetCurSel(gw_Weight);
	}
	else
	{
		m_dEgrayEdit.EnableWindow(TRUE);
		m_gwWeightEdit.EnableWindow(TRUE);
	}

	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

BOOL CAdvancedPropPage::OnApply() 
{
    m_gwWeightEdit.EnableWindow(TRUE);
    m_dEgrayEdit.EnableWindow(TRUE);
    if (m_dE_form == 5)
    {
        m_dE_gray = 2;
        m_dEgrayEdit.EnableWindow(FALSE);
        gw_Weight = 0;
        m_gwWeightEdit.EnableWindow(FALSE);
    }
    else if (m_dE_gray == 0)
    {
        gw_Weight = 0;
        m_gwWeightEdit.EnableWindow(FALSE);
    }
	m_isModified=TRUE;
	GetConfig()->ApplySettings(FALSE);
	m_isModified = FALSE;
	m_bSave = TRUE;
	return CPropertyPageWithHelp::OnApply();
}

UINT CAdvancedPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_PREF_ADVANCED;
}


BOOL CAdvancedPropPage::OnSetActive() 
{
	BOOL	bOk = CPropertyPageWithHelp::OnSetActive();
    m_gwWeightEdit.EnableWindow(TRUE);
    m_dEgrayEdit.EnableWindow(TRUE);
    if (m_dE_form == 5)
    {
        m_dE_gray = 2;
        m_dEgrayEdit.EnableWindow(FALSE);
        gw_Weight = 0;
        m_gwWeightEdit.EnableWindow(FALSE);
    }
    else if (m_dE_gray == 0)
    {
        gw_Weight = 0;
        m_gwWeightEdit.EnableWindow(FALSE);
    }
	m_bSave = GetConfig()->m_bSave2;
	GetConfig()->ApplySettings(FALSE);
	m_isModified=FALSE;
	SetModified(FALSE);	
	return bOk;
}
