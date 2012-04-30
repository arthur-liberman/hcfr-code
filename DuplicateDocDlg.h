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
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_DUPLICATEDOCDLG_H__FD8D2769_2CA7_45A0_97BD_9AD665F5CE4E__INCLUDED_)
#define AFX_DUPLICATEDOCDLG_H__FD8D2769_2CA7_45A0_97BD_9AD665F5CE4E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DuplicateDocDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDuplicateDocDlg dialog

class CDuplicateDocDlg : public CDialog
{
// Construction
public:
	CDuplicateDocDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDuplicateDocDlg)
	enum { IDD = IDD_DUPLICATEDOC_DIALOG };
	BOOL	m_DuplContrastCheck;
	BOOL	m_DuplGrayLevelCheck;
	BOOL	m_DuplNearBlackCheck;
	BOOL	m_DuplNearWhiteCheck;
	BOOL	m_DuplPrimariesSatCheck;
	BOOL	m_DuplSecondariesSatCheck;
	BOOL	m_DuplPrimariesColCheck;
	BOOL	m_DuplSecondariesColCheck;
	BOOL	m_DuplInfoCheck;
	BOOL	m_DuplXYZCheck;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDuplicateDocDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDuplicateDocDlg)
	afx_msg void OnInvertButton();
	afx_msg void OnClearButton();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DUPLICATEDOCDLG_H__FD8D2769_2CA7_45A0_97BD_9AD665F5CE4E__INCLUDED_)
