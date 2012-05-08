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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// LangSelection.cpp : implementation file
//

#if !defined(AFX_LANGSELECTION_H__8DB9B463_5328_4AA8_A42F_EF0FF098EC65__INCLUDED_)
#define AFX_LANGSELECTION_H__8DB9B463_5328_4AA8_A42F_EF0FF098EC65__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LangSelection.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLangSelection dialog

class CLangSelection : public CDialog
{
// Construction
public:
	CLangSelection(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CLangSelection)
	enum { IDD = IDD_LANGSELECTION };
	CListBox	m_Listbox;
	//}}AFX_DATA

	CStringList	m_Languages;
	CString		m_Selection;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLangSelection)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLangSelection)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LANGSELECTION_H__8DB9B463_5328_4AA8_A42F_EF0FF098EC65__INCLUDED_)
