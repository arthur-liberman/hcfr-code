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

// ModelessPropertySheet.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ModelessPropertySheet.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CModelessPropertySheet

IMPLEMENT_DYNAMIC(CModelessPropertySheet, CPropertySheet)

CModelessPropertySheet::CModelessPropertySheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CModelessPropertySheet::CModelessPropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
}

CModelessPropertySheet::~CModelessPropertySheet()
{
}


BEGIN_MESSAGE_MAP(CModelessPropertySheet, CPropertySheet)
	//{{AFX_MSG_MAP(CModelessPropertySheet)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CModelessPropertySheet message handlers

BOOL CModelessPropertySheet::OnInitDialog() 
{
   m_bModeless = FALSE;
   m_nFlags |= WF_CONTINUEMODAL;

   BOOL bResult = CPropertySheet::OnInitDialog();

   m_bModeless = TRUE;
   m_nFlags &= ~WF_CONTINUEMODAL;
   return bResult;
}
