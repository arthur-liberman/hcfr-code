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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_LUMINANCEWND_H__3D79CDDF_163C_496E_AEE0_551554699ACD__INCLUDED_)
#define AFX_LUMINANCEWND_H__3D79CDDF_163C_496E_AEE0_551554699ACD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LuminanceWnd.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLuminanceWnd window

class CLuminanceWnd : public CWnd
{
// Construction
public:
	CLuminanceWnd();

// Attributes
public:
	CString m_valueStr;
	CBitmap m_bgBitmap;
	COLORREF m_textColor;
	int m_marginInPercent;
	LOGFONT m_logFont;

// Operations
public:
	void Refresh ( LPCSTR lpStr );

protected:
	void MakeBgBitmap();
	void ComputeFontSize();

public:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLuminanceWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CLuminanceWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CLuminanceWnd)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClose();
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LUMINANCEWND_H__3D79CDDF_163C_496E_AEE0_551554699ACD__INCLUDED_)
