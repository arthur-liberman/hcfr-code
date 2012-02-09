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

#include "stdafx.h"
#include "ColorHCFR.h"
#include "LangSelection.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLangSelection dialog


CLangSelection::CLangSelection(CWnd* pParent /*=NULL*/)
	: CDialog(CLangSelection::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLangSelection)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CLangSelection::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLangSelection)
	DDX_Control(pDX, IDC_LISTBOX, m_Listbox);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLangSelection, CDialog)
	//{{AFX_MSG_MAP(CLangSelection)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLangSelection message handlers

BOOL CLangSelection::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	POSITION	pos;
	CString		strLang;

	pos = m_Languages.GetHeadPosition ();
	while ( pos )
	{
		strLang = m_Languages.GetNext ( pos );
		m_Listbox.AddString ( strLang );
	}

	if ( m_Selection.IsEmpty () )
		m_Listbox.SetCurSel ( 1 );
	else
		m_Listbox.SetCurSel ( m_Listbox.FindString ( -1, (LPCSTR) m_Selection ) );
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CLangSelection::OnOK() 
{
	// TODO: Add extra validation here
	m_Listbox.GetText ( m_Listbox.GetCurSel (), m_Selection );
	CDialog::OnOK();
}

void CLangSelection::OnCancel() 
{
	// TODO: Add extra validation here
	m_Listbox.GetText ( m_Listbox.GetCurSel (), m_Selection );
	CDialog::OnCancel();
}
