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

// ReferencesPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ReferencesPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CReferencesPropPage property page

IMPLEMENT_DYNCREATE(CReferencesPropPage, CPropertyPageWithHelp)

CReferencesPropPage::CReferencesPropPage() : CPropertyPageWithHelp(CReferencesPropPage::IDD)
{
	//{{AFX_DATA_INIT(CReferencesPropPage)
	m_whiteTarget = 0;
	m_colorStandard = 1;
	m_CCMode = GCD;
	m_GammaRef = 2.22;
	m_GammaAvg = 2.22;
	m_changeWhiteCheck = FALSE;
	m_useMeasuredGamma = FALSE;
	m_GammaOffsetType = 1;
	m_manualGOffset = 0.099;
	//}}AFX_DATA_INIT

	m_isModified=FALSE;
}

CReferencesPropPage::~CReferencesPropPage()
{
}

void CReferencesPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CReferencesPropPage)
	DDX_Control(pDX, IDC_EDIT_GAMMA_REF, m_GammaRefEdit);
	DDX_Control(pDX, IDC_EDIT_GAMMA_AVERAGE, m_GammaAvgEdit);
	DDX_Control(pDX, IDC_WHITETARGET_COMBO, m_whiteTargetCombo);
	DDX_CBIndex(pDX, IDC_WHITETARGET_COMBO, m_whiteTarget);
	DDX_CBIndex(pDX, IDC_COLORREF_COMBO, m_colorStandard);
	DDX_CBIndex(pDX, IDC_CCMODE_COMBO, m_CCMode);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_REF, m_GammaRef);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_AVERAGE, m_GammaAvg);
	DDV_MinMaxDouble(pDX, m_GammaRef, 1., 5.);
	DDV_MinMaxDouble(pDX, m_GammaAvg, .1, 50.);
	DDX_Check(pDX, IDC_CHANGEWHITE_CHECK, m_changeWhiteCheck);
	DDX_Check(pDX, IDC_USE_MEASURED_GAMMA, m_useMeasuredGamma);
	DDX_Radio(pDX, IDC_GAMMA_OFFSET_RADIO1, m_GammaOffsetType);
	DDX_Text(pDX, IDC_EDIT_MANUAL_GOFFSET, m_manualGOffset);
	DDV_MinMaxDouble(pDX, m_manualGOffset, 0., 0.2);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CReferencesPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CReferencesPropPage)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_GAMMA_OFFSET_RADIO1, IDC_GAMMA_OFFSET_RADIO5, OnControlClicked)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_WHITETARGET_COMBO, IDC_WHITETARGET_COMBO, OnControlClicked)
	ON_BN_CLICKED(IDC_CHECK_COLORS, OnCheckColors)
	ON_EN_CHANGE(IDC_EDIT_IRIS_TIME, OnChangeEditIrisTime)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_REF, OnChangeEditGammaRef)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_AVERAGE, OnChangeEditGammaAvg)
	ON_BN_CLICKED(IDC_CHANGEWHITE_CHECK, OnChangeWhiteCheck)
	ON_BN_CLICKED(IDC_USE_MEASURED_GAMMA, OnUseMeasuredGammaCheck)
	ON_CBN_SELCHANGE(IDC_COLORREF_COMBO, OnSelchangeColorrefCombo)
	ON_CBN_SELCHANGE(IDC_CCMODE_COMBO, OnSelchangeCCmodeCombo)
	ON_EN_CHANGE(IDC_EDIT_MANUAL_GOFFSET, OnChangeEditManualGOffset)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CReferencesPropPage message handlers

void CReferencesPropPage::OnControlClicked(UINT nID) 
{
	UpdateData(TRUE);
	if (m_GammaOffsetType == 4)
  	  m_GammaRefEdit.EnableWindow (FALSE);
	else
  	  m_GammaRefEdit.EnableWindow (TRUE);
	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(FALSE);
}

void CReferencesPropPage::OnCheckColors() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
}

BOOL CReferencesPropPage::OnApply() 
{
	GetConfig()->ApplySettings(FALSE);
	m_isModified=FALSE;
	return CPropertyPageWithHelp::OnApply();
}


void CReferencesPropPage::OnChangeEditIrisTime() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
}

BOOL CReferencesPropPage::OnInitDialog() 
{
	CPropertyPageWithHelp::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_changeWhiteCheck = (m_whiteTarget!=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white));
	if(m_changeWhiteCheck)
	{
		CheckRadioButton ( IDC_CHANGEWHITE_CHECK, IDC_CHANGEWHITE_CHECK, IDC_CHANGEWHITE_CHECK );
		m_whiteTargetCombo.EnableWindow (TRUE);
	}
	if (m_useMeasuredGamma)
		CheckRadioButton ( IDC_USE_MEASURED_GAMMA, IDC_USE_MEASURED_GAMMA, IDC_USE_MEASURED_GAMMA );
	if (GetConfig ()->m_GammaOffsetType == 4)
  	  m_GammaRefEdit.EnableWindow (FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CReferencesPropPage::OnChangeEditGammaRef() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditGammaAvg() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditManualGOffset() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
}

UINT CReferencesPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_PREF_REFERENCES;
}

void CReferencesPropPage::OnChangeWhiteCheck() 
{
	UpdateData(TRUE);
	if(!m_changeWhiteCheck)	// Restore default white
	{
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);
		m_isModified=TRUE;
		SetModified(TRUE);
		UpdateData(FALSE);	
	}
	m_whiteTargetCombo.EnableWindow (m_changeWhiteCheck);
}

void CReferencesPropPage::OnUseMeasuredGammaCheck() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(TRUE);
}

void CReferencesPropPage::OnSelchangeColorrefCombo() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
	UpdateData(TRUE);	
	if (m_colorStandard == sRGB)
		m_manualGOffset = 0.055;
	else
		m_manualGOffset = 0.099;
	if (m_colorStandard == CC6)
		m_CCMode = GCD;
	if (m_colorStandard == CC6a)
		m_CCMode = MCD;	
	if(!m_changeWhiteCheck) // Restore default white
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);
	UpdateData(FALSE);	
}

void CReferencesPropPage::OnSelchangeCCmodeCombo() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
	UpdateData(TRUE);
	if (m_colorStandard == CC6)
		m_CCMode = GCD;
	if (m_colorStandard == CC6a)
		m_CCMode = MCD;	
	UpdateData(FALSE);
}

void CReferencesPropPage::OnChangeEditGammaOffset() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
}
