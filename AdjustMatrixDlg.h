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

#if !defined(AFX_ADJUSTMATRIXDLG_H__D59751E9_AAC9_495B_8F8C_CE9E81F2AA21__INCLUDED_)
#define AFX_ADJUSTMATRIXDLG_H__D59751E9_AAC9_495B_8F8C_CE9E81F2AA21__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AdjustMatrixDlg.h : header file
//

#include "Tools/Matrix.h"
#include "Tools/GridCtrl/GridCtrl.h"

/////////////////////////////////////////////////////////////////////////////
// CAdjustMatrixDlg dialog

class CAdjustMatrixDlg : public CDialog
{
// Construction
public:
	CAdjustMatrixDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAdjustMatrixDlg)
	enum { IDD = IDD_CONVERSION_MATRIX };
	CStatic	m_matrixStatic;
	CEdit	m_Comment;
	//}}AFX_DATA

	CGridCtrl* m_pGrid;

	CString	m_strComment;
	Matrix	m_matrix;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAdjustMatrixDlg)
	protected:
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void OnGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult);

	// Generated message map functions
	//{{AFX_MSG(CAdjustMatrixDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnLoad();
	afx_msg void OnSave();
	virtual void OnCancel();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADJUSTMATRIXDLG_H__D59751E9_AAC9_495B_8F8C_CE9E81F2AA21__INCLUDED_)
