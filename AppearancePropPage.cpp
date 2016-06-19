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
/////////////////////////////////////////////////////////////////////////////

// AppearancePropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "AppearancePropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAppearancePropPage property page

IMPLEMENT_DYNCREATE(CAppearancePropPage, CPropertyPageWithHelp)

CAppearancePropPage::CAppearancePropPage() : CPropertyPageWithHelp(CAppearancePropPage::IDD)
{
	//{{AFX_DATA_INIT(CAppearancePropPage)
	m_themeComboIndex = -1;
	m_drawMenuborder = FALSE;
	m_useCustomColor = -1;
	m_doSelectDisabledItem = FALSE;
	m_doGlooming = FALSE;
	m_doXpBlending = FALSE;
	m_bWhiteBkgndOnScreen = FALSE;
	m_bWhiteBkgndOnFile = TRUE;
	m_bmoveMessage = FALSE;
	//}}AFX_DATA_INIT
	
	m_isModified=FALSE;
	m_isWhiteModified=FALSE;
}

CAppearancePropPage::~CAppearancePropPage()
{
}

void CAppearancePropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAppearancePropPage)
	DDX_Control(pDX, IDC_WINDOWCOLOR_BUTTON, m_colorWindowButton);
	DDX_Control(pDX, IDC_MENUCOLOR_BUTTON, m_colorMenuButton);
	DDX_Control(pDX, IDC_SELECTIONCOLOR_BUTTON, m_colorSelectionButton);
	DDX_Control(pDX, IDC_TEXTCOLOR_BUTTON, m_colorTextButton);
	DDX_CBIndex(pDX, IDC_THEME_COMBO, m_themeComboIndex);
	DDX_Check(pDX, IDC_MENUBORDER_CHECK, m_drawMenuborder);
	DDX_Radio(pDX, IDC_CUSTOMCOLOR_RADIO, m_useCustomColor);
	DDX_Check(pDX, IDC_DISABLE_CHECK, m_doSelectDisabledItem);
	DDX_Check(pDX, IDC_GLOOM_CHECK, m_doGlooming);
	DDX_Check(pDX, IDC_XPBLENDING_CHECK, m_doXpBlending);
	DDX_Check(pDX, IDC_BK_WHITE_SCREEN, m_bWhiteBkgndOnScreen);
	DDX_Check(pDX, IDC_BK_WHITE_FILE, m_bWhiteBkgndOnFile);
	DDX_Check(pDX, IDC_MOVE_MESSAGE, m_bmoveMessage);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAppearancePropPage, CPropertyPageWithHelp)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_THEME_COMBO, IDC_RADIO7, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_BK_WHITE_SCREEN, IDC_MOVE_MESSAGE, OnWhiteCheckClicked)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_THEME_COMBO, IDC_RADIO7, OnControlClicked)
    ON_MESSAGE(CPN_SELENDOK, OnColorChanged)
	//{{AFX_MSG_MAP(CAppearancePropPage)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAppearancePropPage message handlers

void CAppearancePropPage::OnControlClicked(UINT nID) 
{
	m_isModified=TRUE;
	SetModified(TRUE);	
}

void CAppearancePropPage::OnWhiteCheckClicked(UINT nID) 
{
	m_isWhiteModified=TRUE;
	SetModified(TRUE);	
}

LRESULT CAppearancePropPage::OnColorChanged(WPARAM wParam, LPARAM lParam)
{
	m_useCustomColor=TRUE;	// Force custom color selection
	UpdateData(FALSE);		// to update radio button state
	m_isModified=TRUE;
	SetModified(TRUE);
	return 0;
}

BOOL CAppearancePropPage::OnApply() 
{
	GetConfig()->ApplySettings(FALSE);
	m_isWhiteModified=FALSE;
	return CPropertyPageWithHelp::OnApply();
}

UINT CAppearancePropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_PREF_APPEARANCE;
}
