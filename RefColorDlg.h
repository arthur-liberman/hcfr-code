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

#if !defined(AFX_REFCOLORDLG_H__57A1C708_3586_413E_9E83_9CEB069BE573__INCLUDED_)
#define AFX_REFCOLORDLG_H__57A1C708_3586_413E_9E83_9CEB069BE573__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RefColorDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CRefColorDlg dialog

class CRefColorDlg : public CDialog
{
// Construction
public:
	CRefColorDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CRefColorDlg)
	enum { IDD = IDD_REF_COLOR_VALUES };
	CStatic	m_label1;
	CStatic	m_label2;
	CStatic	m_label3;
	double	m_red1;
	double	m_red2;
	double	m_red3;
	double	m_green1;
	double	m_green2;
	double	m_green3;
	double	m_blue1;
	double	m_blue2;
	double	m_blue3;
	double	m_white1;
	double	m_white2;
	double	m_white3;
	//}}AFX_DATA

	CColor	m_RedColor;
	CColor	m_GreenColor;
	CColor	m_BlueColor;
	CColor	m_WhiteColor;
	BOOL	m_bxyY;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRefColorDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CRefColorDlg)
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnOK();
	afx_msg void OnHelp();
	afx_msg void OnRadio1();
	afx_msg void OnRadio2();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REFCOLORDLG_H__57A1C708_3586_413E_9E83_9CEB069BE573__INCLUDED_)
