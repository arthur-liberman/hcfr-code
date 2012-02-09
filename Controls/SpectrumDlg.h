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

#if !defined(AFX_SPECTRUMDLG_H__295C9F90_BFC2_4B87_8C8D_888EFCA24642__INCLUDED_)
#define AFX_SPECTRUMDLG_H__295C9F90_BFC2_4B87_8C8D_888EFCA24642__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SpectrumDlg.h : header file
//

#include "..\Views\GraphControl.h"


/////////////////////////////////////////////////////////////////////////////
// CSpectrumWnd Window

class CSpectrumWnd : public CWnd
{
// Construction
public:
	CSpectrumWnd();   // standard constructor

	CGraphControl	m_graphCtrl;
	long			m_SpectrumGraphID;
	CColor *		m_pRefColor;

	void Refresh ();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSpectrumWnd)
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSpectrumWnd)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SPECTRUMDLG_H__295C9F90_BFC2_4B87_8C8D_888EFCA24642__INCLUDED_)
