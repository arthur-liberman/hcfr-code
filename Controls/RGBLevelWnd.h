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

#if !defined(AFX_RGBLEVELWND_H__FEB71004_E020_433C_9438_377256C45149__INCLUDED_)
#define AFX_RGBLEVELWND_H__FEB71004_E020_433C_9438_377256C45149__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RGBLevelWnd.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CRGBLevelWnd window

class CRGBLevelWnd : public CWnd
{
// Construction
public:
	CRGBLevelWnd();

// Attributes
public:
	int				m_redValue;
	int				m_greenValue;
	int				m_blueValue;
	BOOL			m_bLumaMode;

	CColor *		m_pRefColor;
	CDataSetDoc *	m_pDocument;

// Operations
public:
	void Refresh();

protected:
	void DrawGradientBar(CDC *pDc,COLORREF aColor, int aX, int aY, int aWidth, int aHeight);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRGBLevelWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CRGBLevelWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CRGBLevelWnd)
	afx_msg void OnPaint();
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RGBLEVELWND_H__FEB71004_E020_433C_9438_377256C45149__INCLUDED_)
