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
	m_GammaRef = 2.2;
	m_GammaAvg = 2.2;
	m_changeWhiteCheck = FALSE;
	m_useMeasuredGamma = FALSE;
	m_GammaOffsetType = 1;
	m_manualGOffset = 0.099;
    m_manualWhitex = 0.3127;
    m_manualWhitey = 0.3290;
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
	DDX_Control(pDX, IDC_EDIT_GAMMA_REL, m_GammaRelEdit);
	DDX_Control(pDX, IDC_EDIT_SPLIT, m_SplitEdit);
	DDX_Control(pDX, IDC_WHITETARGET_COMBO, m_whiteTargetCombo);
	DDX_CBIndex(pDX, IDC_WHITETARGET_COMBO, m_whiteTarget);
	DDX_CBIndex(pDX, IDC_COLORREF_COMBO, m_colorStandard);
	DDX_CBIndex(pDX, IDC_CCMODE_COMBO, m_CCMode);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_REF, m_GammaRef);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_REL, m_GammaRel);
  	DDX_Text(pDX, IDC_EDIT_SPLIT, m_Split);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_AVERAGE, m_GammaAvg);
	DDV_MinMaxDouble(pDX, m_GammaRef, 1., 5.);
	DDV_MinMaxDouble(pDX, m_GammaAvg, 1., 5.);
	DDV_MinMaxDouble(pDX, m_GammaRel, 0., 5.);
	DDV_MinMaxDouble(pDX, m_Split, 0., 100.);
	DDX_Check(pDX, IDC_CHANGEWHITE_CHECK, m_changeWhiteCheck);
	DDX_Check(pDX, IDC_USE_MEASURED_GAMMA, m_useMeasuredGamma);
	DDX_Control(pDX, IDC_USE_MEASURED_GAMMA, m_eMeasuredGamma);
	DDX_Radio(pDX, IDC_GAMMA_OFFSET_RADIO1, m_GammaOffsetType);
	DDX_Text(pDX, IDC_EDIT_MANUAL_GOFFSET, m_manualGOffset);
	DDX_Text(pDX, IDC_WHITE_X, m_manualWhitex);
	DDX_Text(pDX, IDC_WHITE_Y, m_manualWhitey);
	DDX_Text(pDX, IDC_RED_X, m_manualRedx);
	DDX_Text(pDX, IDC_RED_Y, m_manualRedy);
	DDX_Text(pDX, IDC_GREEN_X, m_manualGreenx);
	DDX_Text(pDX, IDC_GREEN_Y, m_manualGreeny);
	DDX_Text(pDX, IDC_BLUE_X, m_manualBluex);
	DDX_Text(pDX, IDC_BLUE_Y, m_manualBluey);
	DDV_MinMaxDouble(pDX, m_manualWhitex, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualWhitey, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualRedx, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualRedy, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualGreenx, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualGreeny, .1, .9);
	DDV_MinMaxDouble(pDX, m_manualBluex, .001, .9);
	DDV_MinMaxDouble(pDX, m_manualBluey, .001, .9);
	DDX_Control(pDX, IDC_WHITE_X, m_manualWhitexedit);
	DDX_Control(pDX, IDC_WHITE_Y, m_manualWhiteyedit);
	DDX_Control(pDX, IDC_RED_X, m_manualRedxedit);
	DDX_Control(pDX, IDC_RED_Y, m_manualRedyedit);
	DDX_Control(pDX, IDC_GREEN_X, m_manualGreenxedit);
	DDX_Control(pDX, IDC_GREEN_Y, m_manualGreenyedit);
	DDX_Control(pDX, IDC_BLUE_X, m_manualBluexedit);
	DDX_Control(pDX, IDC_BLUE_Y, m_manualBlueyedit);
	DDV_MinMaxDouble(pDX, m_manualGOffset, 0., 0.2);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CReferencesPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CReferencesPropPage)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_GAMMA_OFFSET_RADIO1, IDC_GAMMA_OFFSET_RADIO7, OnControlClicked)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_WHITETARGET_COMBO, IDC_WHITETARGET_COMBO, OnControlClicked)
	ON_BN_CLICKED(IDC_CHECK_COLORS, OnCheckColors)
	ON_EN_CHANGE(IDC_EDIT_IRIS_TIME, OnChangeEditIrisTime)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_REF, OnChangeEditGammaRef)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_REL, OnChangeEditGammaRel)
	ON_EN_CHANGE(IDC_EDIT_SPLIT, OnChangeEditGammaRel)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_AVERAGE, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_WHITE_X, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_WHITE_Y, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_RED_X, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_RED_Y, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_GREEN_X, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_GREEN_Y, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_BLUE_X, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_BLUE_Y, OnChangeEditGammaAvg)
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
	if (m_GammaOffsetType >= 4 || GetConfig()->m_colorStandard == sRGB)
    {
  	  m_GammaRefEdit.EnableWindow (FALSE);
      m_eMeasuredGamma.EnableWindow (FALSE);
    }
	else
    {
  	  m_GammaRefEdit.EnableWindow (TRUE);
      m_eMeasuredGamma.EnableWindow (TRUE);
    }
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
    if (m_whiteTarget == DCUST)
    {
        m_manualWhitexedit.EnableWindow (TRUE);
        m_manualWhiteyedit.EnableWindow (TRUE);
    }
    else
    {
        m_manualWhitexedit.EnableWindow (FALSE);
        m_manualWhiteyedit.EnableWindow (FALSE);
    }
    if (m_colorStandard == CUSTOM)
    {
        m_manualRedxedit.EnableWindow (TRUE);
        m_manualRedyedit.EnableWindow (TRUE);
        m_manualGreenxedit.EnableWindow (TRUE);
        m_manualGreenyedit.EnableWindow (TRUE);
        m_manualBluexedit.EnableWindow (TRUE);
        m_manualBlueyedit.EnableWindow (TRUE);
    }
    else
    {
        m_manualRedxedit.EnableWindow (FALSE);
        m_manualRedyedit.EnableWindow (FALSE);
        m_manualGreenxedit.EnableWindow (FALSE);
        m_manualGreenyedit.EnableWindow (FALSE);
        m_manualBluexedit.EnableWindow (FALSE);
        m_manualBlueyedit.EnableWindow (FALSE);
    }
	if (GetConfig ()->m_GammaOffsetType >= 4)
    {
  	  m_GammaRefEdit.EnableWindow (FALSE);
  	  m_eMeasuredGamma.EnableWindow (FALSE);
    }
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
	if (m_colorStandard == sRGB)
	{
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO1)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO2)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO3)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO4)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO5)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO6)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO7)->EnableWindow(FALSE);
		m_GammaRefEdit.EnableWindow (FALSE);
		m_eMeasuredGamma.EnableWindow (FALSE);
 		m_manualGOffset = 0.055;
	}
	m_GammaAvgEdit.EnableWindow(FALSE);
	m_changeWhiteCheck = (m_whiteTarget!=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white));
	if(m_changeWhiteCheck)
	{
		CheckRadioButton ( IDC_CHANGEWHITE_CHECK, IDC_CHANGEWHITE_CHECK, IDC_CHANGEWHITE_CHECK );
		m_whiteTargetCombo.EnableWindow (TRUE);
	}
    else
    {
        m_whiteTargetCombo.EnableWindow (FALSE);
        m_manualWhitexedit.EnableWindow (FALSE);
        m_manualWhiteyedit.EnableWindow (FALSE);        
    }
    if (m_colorStandard == CUSTOM)
    {
        m_manualRedxedit.EnableWindow (TRUE);
        m_manualRedyedit.EnableWindow (TRUE);
        m_manualGreenxedit.EnableWindow (TRUE);
        m_manualGreenyedit.EnableWindow (TRUE);
        m_manualBluexedit.EnableWindow (TRUE);
        m_manualBlueyedit.EnableWindow (TRUE);
    }
    else
    {
        m_manualRedxedit.EnableWindow (FALSE);
        m_manualRedyedit.EnableWindow (FALSE);
        m_manualGreenxedit.EnableWindow (FALSE);
        m_manualGreenyedit.EnableWindow (FALSE);
        m_manualBluexedit.EnableWindow (FALSE);
        m_manualBlueyedit.EnableWindow (FALSE);
    }
	if (m_useMeasuredGamma)
		CheckRadioButton ( IDC_USE_MEASURED_GAMMA, IDC_USE_MEASURED_GAMMA, IDC_USE_MEASURED_GAMMA );
	if (GetConfig ()->m_GammaOffsetType >= 4)
    {
  	  m_GammaRefEdit.EnableWindow (FALSE);
  	  m_eMeasuredGamma.EnableWindow (FALSE);
    }

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CReferencesPropPage::OnChangeEditGammaRef() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditGammaRel() 
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
	{
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO1)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO2)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO3)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO4)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO5)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO6)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO7)->EnableWindow(FALSE);
		m_GammaRefEdit.EnableWindow (FALSE);
		m_eMeasuredGamma.EnableWindow (FALSE);
 		m_manualGOffset = 0.055;
	}
	else
	{
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO1)->EnableWindow(TRUE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO2)->EnableWindow(TRUE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO3)->EnableWindow(TRUE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO4)->EnableWindow(TRUE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO5)->EnableWindow(TRUE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO6)->EnableWindow(TRUE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO7)->EnableWindow(TRUE);
		m_GammaRefEdit.EnableWindow (TRUE);
		m_eMeasuredGamma.EnableWindow (TRUE);
		m_manualGOffset = 0.099;
	}
	if (m_colorStandard == CC6)
		m_CCMode = GCD;
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
	UpdateData(FALSE);
}

void CReferencesPropPage::OnChangeEditGammaOffset() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
}
