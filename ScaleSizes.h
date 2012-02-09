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

#if !defined(AFX_SCALESIZES_H__7A35AAA7_7214_4290_ABE3_E1D8C2C3BC4C__INCLUDED_)
#define AFX_SCALESIZES_H__7A35AAA7_7214_4290_ABE3_E1D8C2C3BC4C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ScaleSizes.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CScaleSizes dialog

class CScaleSizes : public CDialog
{
// Construction
public:
	CScaleSizes(CDataSetDoc * pDoc, CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CScaleSizes)
	enum { IDD = IDD_SCALESIZES };
	CComboBox	m_ComboEditGrays;
	CComboBox	m_ComboGrays;
	int		m_NbNearBlack;
	int		m_NbNearWhite;
	int		m_NbSat;
	BOOL	m_bIRE;
	//}}AFX_DATA

	int		m_NbGrays;
	CDataSetDoc *	m_pDoc;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CScaleSizes)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CScaleSizes)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	afx_msg void OnCheckIre();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCALESIZES_H__7A35AAA7_7214_4290_ABE3_E1D8C2C3BC4C__INCLUDED_)
