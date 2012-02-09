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

#if !defined(AFX_SAVEGRAPHDIALOG_H__4AC1D0EA_093F_4A8D_A9A8_068371604994__INCLUDED_)
#define AFX_SAVEGRAPHDIALOG_H__4AC1D0EA_093F_4A8D_A9A8_068371604994__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// savegraphdialog.h : header file
//
 
/////////////////////////////////////////////////////////////////////////////
// CSaveGraphDialog dialog

class CSaveGraphDialog : public CDialog
{
// Construction
public:
	CSaveGraphDialog(CWnd* pParent = NULL);   // standard constructor
	~CSaveGraphDialog();

// Dialog Data
	//{{AFX_DATA(CSaveGraphDialog)
	enum { IDD = IDD_GRAPH_SAVE_DIALOG };
	int		m_fileType;
	UINT	m_saveHeight;
	UINT	m_saveWidth;
	int		m_sizeType;
	UINT	m_jpegQuality;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSaveGraphDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSaveGraphDialog)
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo ( HELPINFO * pHelpInfo );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SAVEGRAPHDIALOG_H__4AC1D0EA_093F_4A8D_A9A8_068371604994__INCLUDED_)
