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
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////
#if !defined(AFX_IRPROFILEPROPPAGE_H__A5A040BD_0B50_44A0_90AC_1F85895B18BB__INCLUDED_)
#define AFX_IRPROFILEPROPPAGE_H__A5A040BD_0B50_44A0_90AC_1F85895B18BB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// IRProfilePropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CIRProfilePropPage dialog
#include "IRProfile.h"

class CKiGenerator;

class CIRProfilePropPage : public CDialog
{
	DECLARE_DYNCREATE(CIRProfilePropPage)

// Construction
public:
	CIRProfilePropPage();
	~CIRProfilePropPage();

// Dialog Data
	//{{AFX_DATA(CIRProfilePropPage)
	enum { IDD = IDD_IRPROFILE_PROP_PAGE };
	CEdit	m_Edit_IR;
	CComboBox	m_ComboCodeName;
	int		m_LatencyNav;
	int		m_LatencyNext;
	int		m_LatencyValid;
	CString	m_Name;
	//}}AFX_DATA

	int		m_nCurrentSel;
	CString	m_CodeNext;
	CString	m_CodeOk;
	CString	m_CodeDown;
	CString	m_CodeRight;
	CKiGenerator *	m_pGenerator;
	CString m_comPort;
	CString m_fileName;
	
	BOOL	m_bUpdateExisting;
	CString	m_OriginalName;


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CIRProfilePropPage)
	public:
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CIRProfile	m_IRProfile;
	// Generated message map functions
	//{{AFX_MSG(CIRProfilePropPage)
	afx_msg void OnAcquireIRCode();
	afx_msg void OnChangeEditCodeIr();
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeComboCodeName();
	afx_msg void OnSendIrCode();
	afx_msg void OnTestIrValid();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IRPROFILEPROPPAGE_H__A5A040BD_0B50_44A0_90AC_1F85895B18BB__INCLUDED_)
