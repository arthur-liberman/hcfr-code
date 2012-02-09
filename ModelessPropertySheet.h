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

#if !defined(AFX_MODELESSPROPERTYSHEET_H__E881F0DD_F567_4884_9020_C04022E7DFA4__INCLUDED_)
#define AFX_MODELESSPROPERTYSHEET_H__E881F0DD_F567_4884_9020_C04022E7DFA4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ModelessPropertySheet.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CModelessPropertySheet

class CModelessPropertySheet : public CPropertySheet
{
	DECLARE_DYNAMIC(CModelessPropertySheet)

// Construction
public:
	CModelessPropertySheet(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
	CModelessPropertySheet(LPCTSTR pszCaption="", CWnd* pParentWnd = NULL, UINT iSelectPage = 0);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CModelessPropertySheet)
	public:
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CModelessPropertySheet();

	// Generated message map functions
protected:
	//{{AFX_MSG(CModelessPropertySheet)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MODELESSPROPERTYSHEET_H__E881F0DD_F567_4884_9020_C04022E7DFA4__INCLUDED_)
