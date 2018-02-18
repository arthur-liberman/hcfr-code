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
	m_userBlack = FALSE;
	m_bOverRideTargs = FALSE;
	m_useToneMap = FALSE;
	m_ManualBlack = 0.0;
	m_useMeasuredGamma = FALSE;
	m_GammaOffsetType = 4;
	m_manualGOffset = 0.099;
    m_manualWhitex = 0.3127;
    m_manualWhitey = 0.3290;
	m_MasterMinL = 0.0;
	m_MasterMaxL = 4000.0;
	m_ContentMaxL = 4000.0;
	m_FrameAvgMaxL = 400.0;
	m_TargetMinL = 0.00;
	m_TargetSysGamma = 1.20;
	m_BT2390_BS = 1.0;
	m_BT2390_WS = 0.0;
	m_BT2390_WS1 = 25;
	m_TargetSysGammaUser = m_TargetSysGamma;
	m_BT2390_BSUser = m_BT2390_BS;
	m_BT2390_WS1User = m_BT2390_WS1;
	m_BT2390_WSUser = m_BT2390_WS;
	m_TargetMinLUser = m_TargetMinL;
	m_TargetMaxL = 700.;
	m_TargetMaxLUser = m_TargetMaxL;
	m_DiffuseL = 94.37844;
	m_DiffuseLUser = m_DiffuseL;
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
	DDX_Control(pDX, IDC_EDIT_MANUAL_BLACK, m_ManualBlackEdit);
	DDX_Control(pDX, IDC_EDIT_SPLIT, m_SplitEdit);
	DDX_Control(pDX, IDC_WHITETARGET_COMBO, m_whiteTargetCombo);
	DDX_CBIndex(pDX, IDC_WHITETARGET_COMBO, m_whiteTarget);
	DDX_CBIndex(pDX, IDC_COLORREF_COMBO, m_colorStandard);
	DDX_CBIndex(pDX, IDC_CCMODE_COMBO, m_CCMode);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_REF, m_GammaRef);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_REL, m_GammaRel);
  	DDX_Text(pDX, IDC_EDIT_MANUAL_BLACK, m_ManualBlack);
  	DDX_Text(pDX, IDC_EDIT_SPLIT, m_Split);
  	DDX_Text(pDX, IDC_EDIT_GAMMA_AVERAGE, m_GammaAvg);
  	DDX_Text(pDX, IDC_EDIT_DIFFUSE_WHITE, m_DiffuseL);
  	DDX_Control(pDX, IDC_EDIT_DIFFUSE_WHITE, m_DiffuseLCtrl);
	DDV_MinMaxDouble(pDX, m_DiffuseL, 1., 200.);
	DDX_Text(pDX, IDC_EDIT_MASTER_MINL, m_MasterMinL);
	DDX_Control(pDX, IDC_EDIT_MASTER_MINL, m_MasterMinLCtrl);
	DDV_MinMaxDouble(pDX, m_MasterMinL, 0., 0.5);
	DDX_Text(pDX, IDC_EDIT_MASTER_MAXL, m_MasterMaxL);
	DDX_Control(pDX, IDC_EDIT_MASTER_MAXL, m_MasterMaxLCtrl);
	DDV_MinMaxDouble(pDX, m_MasterMaxL, 100., 10000.);
	DDX_Text(pDX, IDC_EDIT_CONTENT_MAXL, m_ContentMaxL);
	DDX_Control(pDX, IDC_EDIT_CONTENT_MAXL, m_ContentMaxLCtrl);
	DDV_MinMaxDouble(pDX, m_ContentMaxL, 100., 10000.);
	DDX_Control(pDX, IDC_EDIT_FRAME_AVG_MAXL, m_FrameAvgMaxLCtrl);
	DDX_Text(pDX, IDC_EDIT_FRAME_AVG_MAXL, m_FrameAvgMaxL);
	DDV_MinMaxDouble(pDX, m_FrameAvgMaxL, 100., 10000.);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MINL, m_TargetMinL);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MINL, m_TargetMinLCtrl);
	DDV_MinMaxDouble(pDX, m_TargetMinL, 0., 100.);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL, m_TargetMaxL);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL, m_TargetMaxLCtrl);
	DDV_MinMaxDouble(pDX, m_TargetMaxL, 1., 10000.);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL2, m_TargetSysGamma);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL2, m_TargetSysGammaCtrl);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL3, m_BT2390_BS);
	DDV_MinMaxDouble(pDX, m_BT2390_BS, 0.1, 3.0);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL3, m_BT2390_BSCtrl);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL4, m_BT2390_WS);
	DDV_MinMaxDouble(pDX, m_BT2390_WS, -4.5, 4.5);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL4, m_BT2390_WSCtrl);
  	DDX_Text(pDX, IDC_EDIT_TARGET_MAXL5, m_BT2390_WS1);
	DDV_MinMaxDouble(pDX, m_BT2390_WS1, 0, 50);
  	DDX_Control(pDX, IDC_EDIT_TARGET_MAXL5, m_BT2390_WS1Ctrl);
	DDV_MinMaxDouble(pDX, m_TargetSysGamma, 0.1, 2.0);
	DDV_MinMaxDouble(pDX, m_GammaRef, 1., 5.);
	DDV_MinMaxDouble(pDX, m_GammaRel, 0., 5.);
	DDV_MinMaxDouble(pDX, m_Split, 0., 100.);
	DDV_MinMaxDouble(pDX, m_ManualBlack, 0., 1.);
	DDX_Check(pDX, IDC_CHANGEWHITE_CHECK, m_changeWhiteCheck);
	DDX_Control(pDX, IDC_CHANGEWHITE_CHECK, m_changeWhiteCheckCtrl);
	DDX_Check(pDX, IDC_USE_MEASURED_GAMMA, m_useMeasuredGamma);
	DDX_Check(pDX, IDC_USER_BLACK, m_userBlack);
	DDX_Control(pDX, IDC_USER_OVERRIDE_TARGS, m_bOverRideTargsCtrl);
	DDX_Check(pDX, IDC_USER_OVERRIDE_TARGS, m_bOverRideTargs);
	DDX_Control(pDX, IDC_USE_TONEMAP, m_useToneMapCtrl);
	DDX_Check(pDX, IDC_USE_TONEMAP, m_useToneMap);
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
    ON_CONTROL_RANGE(BN_CLICKED, IDC_GAMMA_OFFSET_RADIO1, IDC_GAMMA_OFFSET_RADIO10, OnControlClicked)
	ON_BN_CLICKED(IDC_CHECK_COLORS, OnCheckColors)
	ON_EN_CHANGE(IDC_EDIT_IRIS_TIME, OnChangeEditIrisTime)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_REF, OnChangeEditGammaRef)
	ON_EN_CHANGE(IDC_EDIT_GAMMA_REL, OnChangeEditGammaRel)
	ON_EN_CHANGE(IDC_EDIT_MANUAL_BLACK, OnChangeEditManualBlack)
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
	ON_EN_CHANGE(IDC_EDIT_DIFFUSE_WHITE, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_MASTER_MINL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_MASTER_MAXL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_CONTENT_MAXL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_FRAME_AVG_MAXL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MINL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL2, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL3, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL4, OnChangeEditGammaAvg)
	ON_EN_CHANGE(IDC_EDIT_TARGET_MAXL5, OnChangeEditGammaAvg)
	ON_BN_CLICKED(IDC_CHANGEWHITE_CHECK, OnChangeWhiteCheck)
	ON_BN_CLICKED(IDC_USE_MEASURED_GAMMA, OnUseMeasuredGammaCheck)
	ON_BN_CLICKED(IDC_USER_BLACK, OnUserBlackCheck)
	ON_BN_CLICKED(IDC_USER_OVERRIDE_TARGS, OnUserOverRideTargsCheck)
	ON_BN_CLICKED(IDC_USE_TONEMAP, OnUserBlackCheck)
	ON_CBN_SELCHANGE(IDC_COLORREF_COMBO, OnSelchangeColorrefCombo)
	ON_CBN_SELCHANGE(IDC_WHITETARGET_COMBO, OnSelchangeWhiteCombo)
	ON_CBN_SELCHANGE(IDC_CCMODE_COMBO, OnSelchangeCCmodeCombo)
	ON_EN_CHANGE(IDC_EDIT_MANUAL_GOFFSET, OnChangeEditManualGOffset)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CReferencesPropPage message handlers

void CReferencesPropPage::OnControlClicked(UINT nID) 
{
	m_bSave = TRUE;
	UpdateData(TRUE);

	if (m_GammaOffsetType >= 4 || GetConfig()->m_colorStandard == sRGB)
    {
  	  m_GammaRefEdit.EnableWindow (FALSE);
      m_eMeasuredGamma.EnableWindow (FALSE);
	  m_useMeasuredGamma = FALSE;
    }
	else
    {
  	  m_GammaRefEdit.EnableWindow (TRUE);
      m_eMeasuredGamma.EnableWindow (TRUE);
    }

	if (m_GammaOffsetType == 5 || m_GammaOffsetType == 7 )
	{
		if (m_bOverRideTargs)
		{
			m_TargetMinLCtrl.EnableWindow (TRUE);
  			m_TargetMaxLCtrl.EnableWindow (TRUE);
  			m_DiffuseLCtrl.EnableWindow (TRUE);
			m_TargetSysGammaCtrl.EnableWindow (TRUE);
			m_BT2390_BSCtrl.EnableWindow (TRUE);
			m_BT2390_WSCtrl.EnableWindow (TRUE);
			m_BT2390_WS1Ctrl.EnableWindow (TRUE);
		}
  		m_MasterMinLCtrl.EnableWindow (TRUE);
  		m_MasterMaxLCtrl.EnableWindow (TRUE);
  		m_ContentMaxLCtrl.EnableWindow (TRUE);
  		m_FrameAvgMaxLCtrl.EnableWindow (TRUE);
  		m_bOverRideTargsCtrl.EnableWindow (TRUE);
  		m_useToneMapCtrl.EnableWindow (TRUE);
	}
	else
	{
		m_TargetMinLCtrl.EnableWindow (FALSE);
  		m_TargetMaxLCtrl.EnableWindow (FALSE);
  		m_MasterMinLCtrl.EnableWindow (FALSE);
  		m_MasterMaxLCtrl.EnableWindow (FALSE);
  		m_ContentMaxLCtrl.EnableWindow (FALSE);
  		m_FrameAvgMaxLCtrl.EnableWindow (FALSE);
  		m_bOverRideTargsCtrl.EnableWindow (FALSE);
  		m_useToneMapCtrl.EnableWindow (FALSE);
  		m_DiffuseLCtrl.EnableWindow (FALSE);
		m_TargetSysGammaCtrl.EnableWindow (FALSE);
		m_BT2390_BSCtrl.EnableWindow (FALSE);
		m_BT2390_WSCtrl.EnableWindow (FALSE);
		m_BT2390_WS1Ctrl.EnableWindow (FALSE);
		m_TargetSysGamma = floor( (1.2 + 0.42 * log10(GetConfig()->m_TargetMaxL / 1000.))*100.+0.5)/100.;
	}

	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(TRUE);
}

void CReferencesPropPage::OnCheckColors() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
}

BOOL CReferencesPropPage::OnApply() 
{
	if (m_TargetMinL >= m_TargetMaxL)
	{
		m_TargetMinL = 0;
		GetConfig()->m_TargetMinL = 0;
		GetColorApp()->InMeasureMessageBox("Minumum Target Luminance must be greater than Maximum, setting to 0.",MB_OK); 
	}

	GetConfig()->	WriteProfileDouble("References","Manual Black Level",m_ManualBlack);
	GetConfig()->	WriteProfileDouble("References","Use Black Level",m_userBlack);
	GetConfig()->	WriteProfileDouble("References","Override Targets",m_bOverRideTargs);
	GetConfig()->	WriteProfileDouble("References","DiffuseLValue",m_DiffuseLUser);
	GetConfig()->	WriteProfileDouble("References","TargetMinLValue",m_TargetMinLUser);
	GetConfig()->	WriteProfileDouble("References","TargetMaxLValue",m_TargetMaxLUser);
	GetConfig()->	WriteProfileDouble("References","TargetSysGamma",m_TargetSysGamma);
	GetConfig()->	WriteProfileDouble("References","BT2390BlackSlopeFactor",m_BT2390_BS);
	GetConfig()->	WriteProfileDouble("References","BT2390WhiteSlopeFactor",m_BT2390_WS);
	GetConfig()->	WriteProfileDouble("References","BT2390WhiteSlopeFactor1",m_BT2390_WS1);
	GetConfig()->	WriteProfileDouble("References","TargetSysGammaUser",m_TargetSysGammaUser);
	if (m_colorStandard == HDTVa || m_colorStandard == HDTVb || m_colorStandard == UHDTV3 || m_colorStandard == UHDTV4 || m_colorStandard == CC6) 
	{
		m_changeWhiteCheckCtrl.EnableWindow(FALSE);
		m_changeWhiteCheck = FALSE;
        m_whiteTargetCombo.EnableWindow (FALSE);
        m_manualWhitexedit.EnableWindow (FALSE);
        m_manualWhiteyedit.EnableWindow (FALSE);        
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);	
	} 
	else
		m_changeWhiteCheckCtrl.EnableWindow(TRUE);

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
	  m_useMeasuredGamma = FALSE;
    }
	if (m_userBlack)
		m_ManualBlackEdit.EnableWindow(TRUE);
	else
		m_ManualBlackEdit.EnableWindow(FALSE);

	m_isModified=TRUE;
	GetConfig()->ApplySettings(FALSE);
	m_isModified=FALSE;

	if  ( (m_manualRedx != m_manualRedxold) || (m_manualRedy != m_manualRedyold) || (m_manualBluex != m_manualBluexold)
		|| (m_manualGreenx != m_manualGreenxold) || (m_manualGreeny != m_manualGreenyold) || (m_manualBluey != m_manualBlueyold)
		|| (m_manualWhitex != m_manualWhitexold) || (m_manualWhitey != m_manualWhiteyold) )
		m_bSave = TRUE;

	return CPropertyPageWithHelp::OnApply();
}

void CReferencesPropPage::OnOK() 
{
	GetConfig()->ApplySettings(FALSE);
	if  ( (m_manualRedx != m_manualRedxold) || (m_manualRedy != m_manualRedyold) || (m_manualBluex != m_manualBluexold)
		|| (m_manualGreenx != m_manualGreenxold) || (m_manualGreeny != m_manualGreenyold) || (m_manualBluey != m_manualBlueyold)
		|| (m_manualWhitex != m_manualWhitexold) || (m_manualWhitey != m_manualWhiteyold) )
		m_bSave = TRUE;

	CPropertyPageWithHelp::OnOK();
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
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO8)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO9)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO10)->EnableWindow(FALSE);
		m_GammaRefEdit.EnableWindow (FALSE);
		m_eMeasuredGamma.EnableWindow (FALSE);
 		m_manualGOffset = 0.055;
	}
	if (m_colorStandard == HDTVa || m_colorStandard == HDTVb)
	{
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO6)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO8)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO9)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO10)->EnableWindow(FALSE);
	}
	m_GammaAvgEdit.EnableWindow(FALSE);
	if (m_colorStandard == HDTVa || m_colorStandard == HDTVb || m_colorStandard == UHDTV3 || m_colorStandard == UHDTV4 || m_colorStandard == CC6) 
	{
		m_changeWhiteCheckCtrl.EnableWindow(FALSE);
		m_changeWhiteCheck = FALSE;
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);	}
	else
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
	  m_useMeasuredGamma = FALSE;
    }

	m_ManualBlackEdit.EnableWindow(m_userBlack);
	m_bSave = GetConfig()->m_bSave;
	m_manualRedxold = m_manualRedx;
	m_manualRedyold = m_manualRedy;
	m_manualBluexold = m_manualBluex;
	m_manualBlueyold = m_manualBluey;
	m_manualGreenxold = m_manualGreenx;
	m_manualGreenyold = m_manualGreeny;
	m_manualWhitexold = m_manualWhitex;
	m_manualWhiteyold = m_manualWhitey;
	m_TargetMinLCtrl.EnableWindow(m_bOverRideTargs);
	m_TargetMaxLCtrl.EnableWindow(m_bOverRideTargs);
	m_TargetSysGammaCtrl.EnableWindow(m_bOverRideTargs);
	m_BT2390_BSCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_WSCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_WS1Ctrl.EnableWindow (m_bOverRideTargs);
	if (!m_bOverRideTargs)
	{
		m_DiffuseL = 94.37844;
		m_TargetMinL = 0.0;
		m_TargetSysGamma = floor( (1.2 + 0.42 * log10(GetConfig()->m_TargetMaxL / 1000.))*100.+0.5)/100.;
		m_BT2390_BS = 1.0;
		m_BT2390_WS = 0.0;
		m_BT2390_WS1 = 25;
	}
	else
	{
		GetConfig()->m_DiffuseL = m_DiffuseL;
		GetConfig()->m_TargetMinL = m_TargetMinL;
		GetConfig()->m_TargetMaxL = m_TargetMaxL;
	}
	m_DiffuseLCtrl.EnableWindow(m_bOverRideTargs);
	GetConfig()->m_TargetSysGamma = m_TargetSysGamma;
	GetConfig()->m_BT2390_BS = m_BT2390_BS;
	GetConfig()->m_BT2390_WS = m_BT2390_WS;
	GetConfig()->m_BT2390_WS1 = m_BT2390_WS1;
	
	if (m_GammaOffsetType == 5 || m_GammaOffsetType == 7)
	{
		if (m_bOverRideTargs)
		{
			m_TargetMinLCtrl.EnableWindow (TRUE);
  			m_TargetMaxLCtrl.EnableWindow (TRUE);
  			m_DiffuseLCtrl.EnableWindow (TRUE);
			m_TargetSysGammaCtrl.EnableWindow (TRUE);
			m_BT2390_BSCtrl.EnableWindow (TRUE);
			m_BT2390_WSCtrl.EnableWindow (TRUE);
			m_BT2390_WS1Ctrl.EnableWindow (TRUE);
		}
  		m_MasterMinLCtrl.EnableWindow (TRUE);
  		m_MasterMaxLCtrl.EnableWindow (TRUE);
  		m_ContentMaxLCtrl.EnableWindow (TRUE);
  		m_FrameAvgMaxLCtrl.EnableWindow (TRUE);
  		m_bOverRideTargsCtrl.EnableWindow (TRUE);
  		m_useToneMapCtrl.EnableWindow (TRUE);
	}
	else
	{
		m_TargetMinLCtrl.EnableWindow (FALSE);
  		m_TargetMaxLCtrl.EnableWindow (FALSE);
  		m_MasterMinLCtrl.EnableWindow (FALSE);
  		m_MasterMaxLCtrl.EnableWindow (FALSE);
  		m_ContentMaxLCtrl.EnableWindow (FALSE);
  		m_FrameAvgMaxLCtrl.EnableWindow (FALSE);
  		m_bOverRideTargsCtrl.EnableWindow (FALSE);
  		m_useToneMapCtrl.EnableWindow (FALSE);
  		m_DiffuseLCtrl.EnableWindow (FALSE);
		m_TargetSysGammaCtrl.EnableWindow (FALSE);
		m_BT2390_BSCtrl.EnableWindow (FALSE);
		m_BT2390_WSCtrl.EnableWindow (FALSE);
		m_BT2390_WS1Ctrl.EnableWindow (FALSE);
	}
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CReferencesPropPage::OnChangeEditGammaRef() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditGammaRel() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditManualBlack() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(TRUE);
	m_bSave = TRUE;
}

void CReferencesPropPage::OnChangeEditGammaAvg() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::OnChangeEditManualGOffset() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

UINT CReferencesPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_PREF_REFERENCES;
}

void CReferencesPropPage::OnChangeWhiteCheck() 
{
	UpdateData(TRUE);
	m_bSave = TRUE;
	if(!m_changeWhiteCheck)	// Restore default white
	{
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);
		m_isModified=TRUE;
		SetModified(TRUE);
		UpdateData(FALSE);	
		OnSelchangeWhiteCombo();
	}
	m_whiteTargetCombo.EnableWindow (m_changeWhiteCheck);
}

void CReferencesPropPage::OnUseMeasuredGammaCheck() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	m_bSave = TRUE;
	UpdateData(TRUE);
}

void CReferencesPropPage::OnUserBlackCheck() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(TRUE);
	m_bSave = TRUE;
	m_ManualBlackEdit.EnableWindow(m_userBlack);
}

void CReferencesPropPage::OnUserOverRideTargsCheck() 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
	UpdateData(TRUE);
	m_bSave = TRUE;
	m_DiffuseLCtrl.EnableWindow(m_bOverRideTargs);
	if (!m_bOverRideTargs)
	{
		m_DiffuseLUser = m_DiffuseL;
		m_DiffuseL = 94.37844;
		m_TargetMinLUser = m_TargetMinL;
		m_TargetMinL = 0.0;
		m_TargetMaxLUser = m_TargetMaxL;
		m_TargetSysGammaUser = m_TargetSysGamma;
		m_TargetSysGamma = floor( (1.2 + 0.42 * log10(GetConfig()->m_TargetMaxL / 1000.))*100.0 + 0.5) / 100.0;
		m_BT2390_BSUser = m_BT2390_BS;
		m_BT2390_WSUser = m_BT2390_WS;
		m_BT2390_WS1User = m_BT2390_WS1;
		m_TargetSysGamma = 1.20;
	}
	else
	{
		m_DiffuseL = m_DiffuseLUser;
		m_TargetMinL = m_TargetMinLUser;
		m_TargetMaxL = m_TargetMaxLUser;
		m_TargetSysGamma = m_TargetSysGammaUser;
		m_BT2390_BS = m_BT2390_BSUser;
		m_BT2390_WS = m_BT2390_WSUser;
		m_BT2390_WS1 = m_BT2390_WS1User;
	}

	GetConfig()->m_DiffuseL = m_DiffuseL;
	m_TargetMinLCtrl.EnableWindow(m_bOverRideTargs);
	m_TargetMaxLCtrl.EnableWindow(m_bOverRideTargs);
	m_TargetSysGammaCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_BSCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_WSCtrl.EnableWindow (m_bOverRideTargs);
	m_BT2390_WS1Ctrl.EnableWindow (m_bOverRideTargs);
	GetConfig()->m_TargetSysGamma = m_TargetSysGamma;
	GetConfig()->m_BT2390_BS = m_BT2390_BS;
	GetConfig()->m_BT2390_WS = m_BT2390_WS;
	GetConfig()->m_BT2390_WS1 = m_BT2390_WS1;

	UpdateData(FALSE);
}

void CReferencesPropPage::OnSelchangeWhiteCombo()
{
	m_isModified=TRUE;
	SetModified(TRUE);
	m_bSave = TRUE;
	UpdateData(TRUE);
	BOOL enableEditControls = m_whiteTarget == DCUST ? TRUE : FALSE;
	m_manualWhitexedit.EnableWindow (enableEditControls);
	m_manualWhiteyedit.EnableWindow (enableEditControls);
	UpdateColorSpaceValues();
	UpdateData(FALSE);
}

void CReferencesPropPage::OnSelchangeColorrefCombo() 
{
	m_isModified=TRUE;
	SetModified(TRUE);
	m_bSave = TRUE;
	UpdateData(TRUE);
	UpdateColorSpaceValues();
	if (m_colorStandard == sRGB)
	{
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO1)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO2)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO3)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO4)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO5)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO6)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO7)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO8)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO9)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO10)->EnableWindow(FALSE);
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
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO8)->EnableWindow(TRUE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO9)->EnableWindow(TRUE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO10)->EnableWindow(TRUE);
		m_GammaRefEdit.EnableWindow (TRUE);
		if (m_GammaOffsetType < 4)
			m_eMeasuredGamma.EnableWindow (TRUE);
		else
		{
			m_eMeasuredGamma.EnableWindow (FALSE);
	 	    m_useMeasuredGamma = FALSE;
		}
		m_manualGOffset = 0.099;
	}
	if ( !(m_colorStandard == UHDTV || m_colorStandard == UHDTV2 || m_colorStandard == UHDTV3 || m_colorStandard == UHDTV4 || m_colorStandard == HDTV)  )
	{
		int bID = GetCheckedRadioButton(IDC_GAMMA_OFFSET_RADIO6, IDC_GAMMA_OFFSET_RADIO10);
		if (bID == IDC_GAMMA_OFFSET_RADIO6 || bID == IDC_GAMMA_OFFSET_RADIO8 || bID == IDC_GAMMA_OFFSET_RADIO9 || bID == IDC_GAMMA_OFFSET_RADIO10 )
		{
			m_GammaOffsetType = 4;
			CheckRadioButton(IDC_GAMMA_OFFSET_RADIO1,IDC_GAMMA_OFFSET_RADIO1,IDC_GAMMA_OFFSET_RADIO1);
		}
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO6)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO8)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO9)->EnableWindow(FALSE);
		GetDlgItem(IDC_GAMMA_OFFSET_RADIO10)->EnableWindow(FALSE);
	}
	if (m_colorStandard == CC6)
		m_CCMode = GCD;
	if(!m_changeWhiteCheck) // Restore default white
		m_whiteTarget=(int)(GetStandardColorReference((ColorStandard)(m_colorStandard)).m_white);
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

	UpdateData(FALSE);	
}

void CReferencesPropPage::OnSelchangeCCmodeCombo() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
	UpdateData(TRUE);
	if (m_colorStandard == CC6)
		m_CCMode = GCD;
	GetConfig()->ApplySettings(false);
	UpdateData(FALSE);
}

void CReferencesPropPage::OnChangeEditGammaOffset() 
{
	m_isModified=TRUE;
	m_bSave = TRUE;
	SetModified(TRUE);
}

void CReferencesPropPage::UpdateColorSpaceValues()
{
	CColorReference colorRef((ColorStandard)m_colorStandard, (WhiteTarget)m_whiteTarget);

	if (m_whiteTarget != DCUST)
	{
		CColor whiteColor(colorRef.GetWhite());
		m_manualWhitex = whiteColor.GetxyYValue()[0];
		m_manualWhitey = whiteColor.GetxyYValue()[1];
	}
	if (m_colorStandard != CUSTOM)
	{
		CColor redColor(colorRef.GetRed());
		m_manualRedx   = redColor.GetxyYValue()[0];
		m_manualRedy   = redColor.GetxyYValue()[1];

		CColor greenColor(colorRef.GetGreen());
		m_manualGreenx = greenColor.GetxyYValue()[0];
		m_manualGreeny = greenColor.GetxyYValue()[1];

		CColor blueColor(colorRef.GetBlue());
		m_manualBluex  = blueColor.GetxyYValue()[0];
		m_manualBluey  = blueColor.GetxyYValue()[1];
	}
}