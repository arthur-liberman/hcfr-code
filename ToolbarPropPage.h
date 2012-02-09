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

#if !defined(AFX_TOOLBARPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_)
#define AFX_TOOLBARPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ToolbarPropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CToolbarPropPage dialog

class CToolbarPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CToolbarPropPage)

// Construction
public:
	CToolbarPropPage();
	~CToolbarPropPage();

// Dialog Data
	//{{AFX_DATA(CToolbarPropPage)
	enum { IDD = IDD_TOOLBAR_PROP_PAGE };
	int		m_TBViewsRightClickMode;
	int		m_TBViewsMiddleClickMode;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CToolbarPropPage)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CToolbarPropPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	afx_msg void OnControlClicked(UINT nID);
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TOOLBARPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_)
