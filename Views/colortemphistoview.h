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
//	Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

//{{AFX_INCLUDES()
//}}AFX_INCLUDES
#if !defined(AFX_COLORTEMPHISTOVIEW_H__47A36C74_44E9_42CC_8E0B_1944C3606C31__INCLUDED_)
#define AFX_COLORTEMPHISTOVIEW_H__47A36C74_44E9_42CC_8E0B_1944C3606C31__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ColorTempHistoView.h : header file
//

#include "GraphControl.h"

/////////////////////////////////////////////////////////////////////////////
// CColorTempHistoView form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDataSetDoc;

class CColorTempGrapher
{
 public:
	CColorTempGrapher();

	CGraphControl m_graphCtrl;
	long m_ColorTempGraphID;
	long m_refGraphID;

	long m_ColorTempDataRefGraphID;	//Ki

	// Updatable flags. Initialized with default values in constructor, can be changed before calling UpdateGraph
	BOOL m_showReference;
	BOOL m_showDataRef;	//Ki

// Operations
	void UpdateGraph ( CDataSetDoc * pDoc );
};


class CColorTempHistoView : public CSavingView
{
protected:
	CColorTempHistoView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CColorTempHistoView)

// Form Data
public:
	//{{AFX_DATA(CColorTempHistoView)
	//}}AFX_DATA

// Attributes
public:
	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }
	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

	CColorTempGrapher m_Grapher;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CColorTempHistoView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CColorTempHistoView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CColorTempHistoView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnColorTempGraphShowRef();
	afx_msg void OnColorTempGraphShowDataRef();
	afx_msg void OnGraphSettings();
	afx_msg void OnGraphXScaleFit();
	afx_msg void OnGraphXZoomIn();
	afx_msg void OnGraphXZoomOut();
	afx_msg void OnGraphXShiftLeft();
	afx_msg void OnGraphXShiftRight();
	afx_msg void OnGraphXScale1();
	afx_msg void OnGraphXScale2();
	afx_msg void OnGraphYShiftBottom();
	afx_msg void OnGraphYShiftTop();
	afx_msg void OnGraphYZoomIn();
	afx_msg void OnGraphYZoomOut();
	afx_msg void OnColortempGraphYScale1();
	afx_msg void OnColortempGraphYScale2();
	afx_msg void OnGraphScaleFit();
	afx_msg void OnGraphYScaleFit();
	afx_msg void OnGraphScaleCustom();
	afx_msg void OnGraphSave();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COLORTEMPHISTOVIEW_H__47A36C74_44E9_42CC_8E0B_1944C3606C31__INCLUDED_)
