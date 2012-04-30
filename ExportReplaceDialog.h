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
//	François-Xavier CHABOUD
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_EXPORTREPLACEDIALOG_H__3D14F383_C418_441A_AEFF_2CBABEB77B40__INCLUDED_)
#define AFX_EXPORTREPLACEDIALOG_H__3D14F383_C418_441A_AEFF_2CBABEB77B40__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ExportReplaceDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CExportReplaceDialog dialog

class CExportReplaceDialog : public CDialog
{
// Construction
public:
	CExportReplaceDialog(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CExportReplaceDialog)
	enum { IDD = IDD_EXPORTXLS_DIALOG };
	CButton	m_okButton;
	CListBox	m_replaceMeasureListBox;
	int		m_appendRadioChecked;
	//}}AFX_DATA
	CStringArray m_replaceMeasureList;
	int m_selectedMeasure;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CExportReplaceDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

public:
	void AddMeasure(CString aMeasure);
	int GetSelectedMeasure() {return m_selectedMeasure; }
	bool IsReplaceRadioChecked() {return (m_appendRadioChecked==1);}

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CExportReplaceDialog)
	afx_msg void OnAppendRadio();
	afx_msg void OnReplaceRadio();
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeReplaceMeasureList();
	afx_msg void OnDblclkReplaceMeasureList();
	virtual void OnOK();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo ( HELPINFO * pHelpInfo );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EXPORTREPLACEDIALOG_H__3D14F383_C418_441A_AEFF_2CBABEB77B40__INCLUDED_)
