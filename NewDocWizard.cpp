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
//	François-Xavier CHABOUD
/////////////////////////////////////////////////////////////////////////////

// NewDocWizard.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "NewDocWizard.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNewDocWizard

IMPLEMENT_DYNAMIC(CNewDocWizard, CPropertySheetWithHelp)

CNewDocWizard::CNewDocWizard(CWnd* pWndParent)
	 : CPropertySheetWithHelp(IDS_PROPSHT_CAPTION, pWndParent)
{
	// Add all of the property pages here.  Note that
	// the order that they appear in here will be
	// the order they appear in on screen.  By default,
	// the first page of the set is the active one.
	// One way to make a different property page the 
	// active one is to call SetActivePage().

	AddPage(&m_Page1);
	AddPage(&m_Page2);

	SetWizardMode();
}

CNewDocWizard::~CNewDocWizard()
{
}


BEGIN_MESSAGE_MAP(CNewDocWizard, CPropertySheetWithHelp)
	//{{AFX_MSG_MAP(CNewDocWizard)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CNewDocWizard message handlers

BOOL CNewDocWizard::OnInitDialog()
{
	BOOL bResult = CPropertySheetWithHelp::OnInitDialog();

	// add a preview window to the property sheet.
/*	CRect rectWnd;
	GetWindowRect(rectWnd);
	SetWindowPos(NULL, 0, 0,
		rectWnd.Width() + 100,
		rectWnd.Height(),
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	CRect rectPreview(rectWnd.Width() + 25, 25,
		rectWnd.Width()+75, 75);

	m_wndPreview.Create(NULL, NULL, WS_CHILD|WS_VISIBLE, rectPreview, this, 0x1000); */

	CenterWindow();
	return bResult;
}


