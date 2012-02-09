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

// Signature.cpp : implementation file
//

#include "stdafx.h"
#include "colorhcfr.h"
#include "Signature.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSignature dialog


CSignature::CSignature(CWnd* pParent /*=NULL*/)
	: CDialog(CSignature::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSignature)
	m_Signature = _T("");
	//}}AFX_DATA_INIT
}


void CSignature::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSignature)
	DDX_Text(pDX, IDC_EDIT_NAME, m_Signature);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSignature, CDialog)
	//{{AFX_MSG_MAP(CSignature)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSignature message handlers

BOOL CSignature::OnInitDialog() 
{
	m_Signature = GetConfig () -> GetProfileString ( "Settings", "Signature", "" );
	
	CDialog::OnInitDialog();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CSignature::OnOK() 
{
	CDialog::OnOK();

	GetConfig () -> WriteProfileString ( "Settings", "Signature", m_Signature );
}

void CSignature::OnCancel() 
{
	// Do not cancel: always validate.
	OnOK();
}

void CSignature::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_SIGNATURE, NULL );
}

BOOL CSignature::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	OnHelp ();
	return TRUE;
}


