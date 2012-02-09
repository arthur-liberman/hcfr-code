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

// LangSelection.cpp : implementation file
//

// LuxScaleAdvisor.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "LuxScaleAdvisor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLuxScaleAdvisor dialog


CLuxScaleAdvisor::CLuxScaleAdvisor(CWnd* pParent /*=NULL*/)
	: CDialog(CLuxScaleAdvisor::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLuxScaleAdvisor)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_bContinue = FALSE;
	m_bCancel = FALSE;
}


void CLuxScaleAdvisor::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLuxScaleAdvisor)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLuxScaleAdvisor, CDialog)
	//{{AFX_MSG_MAP(CLuxScaleAdvisor)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLuxScaleAdvisor message handlers

void CLuxScaleAdvisor::OnCancel() 
{
	// TODO: Add extra cleanup here
	m_bCancel = TRUE;
	DestroyWindow ();
}

void CLuxScaleAdvisor::OnOK() 
{
	// TODO: Add extra validation here
	m_bContinue = TRUE;
	DestroyWindow ();
}
