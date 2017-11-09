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

//{{AFX_INCLUDES()
//}}AFX_INCLUDES
#if !defined(AFX_MEASURESHISTOVIEW_H__47A36C74_44E9_42CC_8E0B_1944C3606C31__INCLUDED_)
#define AFX_MEASURESHISTOVIEW_H__47A36C74_44E9_42CC_8E0B_1944C3606C31__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MeasuresHistoView.h : header file
//

#include "GraphControl.h"

/////////////////////////////////////////////////////////////////////////////
// CMeasuresHistoView form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDataSetDoc;

class CMeasuresHistoView : public CSavingView
{
protected:
	CMeasuresHistoView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CMeasuresHistoView)

	double m_LumaMaxY;

// Form Data
public:
	//{{AFX_DATA(CMeasuresHistoView)
	//}}AFX_DATA

// Attributes
public:
	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }
	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

	CGraphControl m_graphCtrl;
	long m_luminanceGraphID;
	long m_redLumGraphID;
	long m_greenLumGraphID;
	long m_blueLumGraphID;

	CGraphControl m_graphCtrl1;
	long m_redGraphID;
	long m_greenGraphID;
	long m_blueGraphID;
	long m_refGraphID;

	CGraphControl m_graphCtrl2;
	long m_deltaEGraphID;

	CGraphControl m_graphCtrl3;
	long m_ColorTempGraphID;
	long m_refGraphID3;

	BOOL m_showReference;
	BOOL m_showLuminance;
	BOOL m_showDeltaE;
	BOOL m_showColorTemp;
	int	m_XScale;
	int l_Display;
	int l_nCol;
	int l_nSize;
// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMeasuresHistoView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CMeasuresHistoView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CMeasuresHistoView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnColorTempGraphShowRef();
	afx_msg void OnGraphShowLuminance();
	afx_msg void OnGraphShowDeltaE();
	afx_msg void OnGraphShowColorTemp();
	afx_msg void OnRGBGraphYScale1();
	afx_msg void OnRGBGraphYScale2();
	afx_msg void OnRGBGraphYScale3();
	afx_msg void OnDeltaEGraphYScale1();
	afx_msg void OnDeltaEGraphYScale2();
	afx_msg void OnColortempGraphYScale1();
	afx_msg void OnColortempGraphYScale2();
	afx_msg void OnGraphXScale10();
	afx_msg void OnGraphXScale20();
	afx_msg void OnGraphXScale50();
	afx_msg void OnGraphSettings();
	afx_msg void OnGraphSave();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MEASURESHISTOVIEW_H__47A36C74_44E9_42CC_8E0B_1944C3606C31__INCLUDED_)
