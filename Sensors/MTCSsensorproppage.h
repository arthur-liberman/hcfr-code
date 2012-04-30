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

#if !defined(AFX_MTCSSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_)
#define AFX_MTCSSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MTCSSensorPropPage.h : header file
//

class CMTCSSensor;

/////////////////////////////////////////////////////////////////////////////
// CMTCSSensorPropPage dialog

class CMTCSSensorPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CMTCSSensorPropPage)
 
// Construction
public:
	CMTCSSensorPropPage();
	~CMTCSSensorPropPage();

// Dialog Data
	//{{AFX_DATA(CMTCSSensorPropPage)
	enum { IDD = IDD_MTCSSENSOR_PROP_PAGE };
	CEdit	m_edit_black_3;
	CEdit	m_edit_black_2;
	CEdit	m_edit_black_1;
	UINT	m_ReadTime;
	BOOL	m_bAdjustTime;
	BOOL	m_debugMode;
	CStatic	m_matrixStatic;
	//}}AFX_DATA

	CMTCSSensor *	m_pSensor;

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CMTCSSensorPropPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CGridCtrl* m_pGrid;
	Matrix m_matrix;

	// Generated message map functions
	//{{AFX_MSG(CMTCSSensorPropPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnDisplayMatrix();
	afx_msg void OnWriteDeviceMatrix();
	afx_msg void OnWriteDefaultMatrix();
	afx_msg void OnDisplayBlack();
	afx_msg void OnWriteDeviceBlackLevel();
	afx_msg void OnMeasureBlackLevel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MTCSSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_)
