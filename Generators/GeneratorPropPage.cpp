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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// GeneratorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GeneratorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGeneratorPropPage property page

IMPLEMENT_DYNCREATE(CGeneratorPropPage, CPropertyPageWithHelp)

CGeneratorPropPage::CGeneratorPropPage() : CPropertyPageWithHelp(CGeneratorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CGeneratorPropPage)
	m_doScreenBlanking = FALSE;
	//}}AFX_DATA_INIT
}

CGeneratorPropPage::~CGeneratorPropPage()
{
}

void CGeneratorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGeneratorPropPage)
	DDX_Check(pDX, IDC_BLANKING_CHECK, m_doScreenBlanking);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGeneratorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CGeneratorPropPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGeneratorPropPage message handlers

UINT CGeneratorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_GENERATOR_BLANKING;
}
