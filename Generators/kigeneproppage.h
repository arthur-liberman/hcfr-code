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

#if !defined(AFX_KIGENEPROPPAGE_H__7FCC9A7E_09E6_491C_8397_EEEB41D43DE8__INCLUDED_)
#define AFX_KIGENEPROPPAGE_H__7FCC9A7E_09E6_491C_8397_EEEB41D43DE8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// KiGenePropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKiGenePropPage dialog

#include "IRProfilePropPage.h"
	
class CKiGenePropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CKiGenePropPage)

// Construction
public:
	CKiGenePropPage();
	~CKiGenePropPage();

// Dialog Data
	//{{AFX_DATA(CKiGenePropPage)
	enum { IDD = IDD_GENERATOR_KI_PROP_PAGE };
	CListBox	m_IRProfileList;
	BOOL	m_showDialog;
	CString	m_comPort;
	BOOL	m_patternCheckOnGrayscale;
	BOOL	m_patternCheckOnPrimaries;
	BOOL	m_patternCheckOnSaturations;
	BOOL	m_patternCheckOnSecondaries;
	UINT	m_patternCheckMaxRetryBySeries;
	UINT	m_patternCheckMaxRetryByPattern;
	UINT	m_patternCheckMaxThreshold;
	//}}AFX_DATA

	CIRProfilePropPage m_IRProfilePropPage;
	CString m_IRProfileFileName;
	BOOL m_IRProfileFileIsModified;
	
	CKiGenerator *	m_pGenerator;
	
	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CKiGenePropPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CKiGenePropPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnNewIRProfile();
	afx_msg void OnModifyIRProfile();
	afx_msg void OnSelchangeIRProfileList();
	afx_msg void OnDeleteIRProfile();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KIGENEPROPPAGE_H__7FCC9A7E_09E6_491C_8397_EEEB41D43DE8__INCLUDED_)
