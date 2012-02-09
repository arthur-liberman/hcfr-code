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

#if !defined(AFX_LUXSCALEADVISOR_H__480F350D_F3F1_4C83_BF4E_1BEBD6AB39E6__INCLUDED_)
#define AFX_LUXSCALEADVISOR_H__480F350D_F3F1_4C83_BF4E_1BEBD6AB39E6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LuxScaleAdvisor.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLuxScaleAdvisor dialog

class CLuxScaleAdvisor : public CDialog
{
// Construction
public:
	CLuxScaleAdvisor(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CLuxScaleAdvisor)
	enum { IDD = IDD_LUXMETER_SCALE };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	BOOL	m_bContinue;
	BOOL	m_bCancel;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLuxScaleAdvisor)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLuxScaleAdvisor)
	virtual void OnCancel();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LUXSCALEADVISOR_H__480F350D_F3F1_4C83_BF4E_1BEBD6AB39E6__INCLUDED_)
