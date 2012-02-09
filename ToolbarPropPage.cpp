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

// ToolbarPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ToolbarPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CToolbarPropPage property page

IMPLEMENT_DYNCREATE(CToolbarPropPage, CPropertyPageWithHelp)

CToolbarPropPage::CToolbarPropPage() : CPropertyPageWithHelp(CToolbarPropPage::IDD)
{
	//{{AFX_DATA_INIT(CToolbarPropPage)
	m_TBViewsRightClickMode = 0;
	m_TBViewsMiddleClickMode = 1;
	//}}AFX_DATA_INIT
}

CToolbarPropPage::~CToolbarPropPage()
{
}

void CToolbarPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CToolbarPropPage)
	DDX_Radio(pDX, IDC_RADIO_TBV_1, m_TBViewsRightClickMode);
	DDX_Radio(pDX, IDC_RADIO_TBV_5, m_TBViewsMiddleClickMode);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CToolbarPropPage, CPropertyPageWithHelp)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_RADIO_TBV_1, IDC_RADIO_TBV_4, OnControlClicked)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_RADIO_TBV_5, IDC_RADIO_TBV_8, OnControlClicked)
	//{{AFX_MSG_MAP(CToolbarPropPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CToolbarPropPage message handlers

void CToolbarPropPage::OnControlClicked(UINT nID) 
{
	SetModified(TRUE);	
}

BOOL CToolbarPropPage::OnApply() 
{
	GetConfig()->ApplySettings(FALSE);
	return CPropertyPageWithHelp::OnApply();
}

UINT CToolbarPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_PREF_TOOLBAR;
}
