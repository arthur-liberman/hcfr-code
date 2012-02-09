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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_TARGETWND_H__8F910E63_5CB4_4C82_B6B5_6E26B64A7A85__INCLUDED_)
#define AFX_TARGETWND_H__8F910E63_5CB4_4C82_B6B5_6E26B64A7A85__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TargetWnd.h : header file
//

#include "PPTooltip.h" 

/////////////////////////////////////////////////////////////////////////////
// CTargetWnd window

class CTargetWnd : public CWnd
{
// Construction
public:
	CTargetWnd();

// Attributes
public:
	CBitmap m_bgBitmap;
	CBitmap m_pointBitmap;
	CBitmap m_scaledPointBitmap;

	double		m_deltax;
	double		m_deltay;
	COLORREF	m_clr;
	int m_marginInPercent;
	int m_targetRectInPercent;
	int m_pointSizeInPercent;

	CPPToolTip m_tooltip;
	CString *	pTooltipText;

	CColor *	m_pRefColor;

// Operations
public:
	void Refresh();

protected:
	int m_prev_cx; 
	int m_prev_cy; 

	void MakeBgBitmap();
	void UpdateScaledBitmap();
	double GetZoomFactor();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTargetWnd)
	public:
	virtual BOOL Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext = NULL);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTargetWnd();

	// Generated message map functions
protected:
	afx_msg void NotifyDisplayTooltip(NMHDR * pNMHDR, LRESULT * result);
	//{{AFX_MSG(CTargetWnd)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TARGETWND_H__8F910E63_5CB4_4C82_B6B5_6E26B64A7A85__INCLUDED_)
