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
//	Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_RGBHISTOVIEW_H__0C087EB3_7C7F_4FC4_B2FA_7A5EB932EEF5__INCLUDED_)
#define AFX_RGBHISTOVIEW_H__0C087EB3_7C7F_4FC4_B2FA_7A5EB932EEF5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RGBHistoView.h : header file
//
#include "GraphControl.h"

/////////////////////////////////////////////////////////////////////////////
// CRGBHistoView form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDataSetDoc;

class CRGBGrapher
{
 public:
	CRGBGrapher ();

	CGraphControl m_graphCtrl;
	long m_redGraphID;
	long m_greenGraphID;
	long m_blueGraphID;
	long m_refGraphID;

	long m_redDataRefGraphID;	//Ki
	long m_greenDataRefGraphID;	//Ki
	long m_blueDataRefGraphID;	//Ki

	CGraphControl m_graphCtrl2;
	long m_deltaEGraphID;
	long m_deltaEDataRefGraphID;	//Ki
	long m_deltaEBetweenGraphID;

	// Updatable flags. Initialized with default values in constructor, can be changed before calling UpdateGraph
	BOOL m_showReference;
	BOOL m_showDeltaE;
	BOOL m_showDataRef;	//Ki
	UINT m_scaleYrgb;
	UINT m_scaleYdeltaE;

// Operations
	void UpdateGraph ( CDataSetDoc * pDoc );
};

class CRGBHistoView : public CSavingView
{
protected:
	CRGBHistoView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CRGBHistoView)

// Form Data
public:
	//{{AFX_DATA(CRGBHistoView)
	//}}AFX_DATA

// Attributes
public:
	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }
	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

	CRGBGrapher m_Grapher;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRGBHistoView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CRGBHistoView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CRGBHistoView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnRgbGraphReference();
	afx_msg void OnRgbGraphDeltaE();
	afx_msg void OnRgbGraphGamma();
	afx_msg void OnRgbGraphDataRef();	//Ki
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
	afx_msg void OnRGBGraphYScale1();
	afx_msg void OnRGBGraphYScale2();
	afx_msg void OnRGBGraphYScale3();
	afx_msg void OnDeltaEGraphYScale1();
	afx_msg void OnDeltaEGraphYScale2();
	afx_msg void OnDeltaEGraphYScale3();
	afx_msg void OnGraphScaleFit();
	afx_msg void OnGraphYScaleFit();
	afx_msg void OnGraphScaleCustom();
	afx_msg void OnGraphSave();
	afx_msg void OnDeltaEGraphYScaleFit();
	afx_msg void OnDeltaEGraphYShiftBottom();
	afx_msg void OnDeltaEGraphYShiftTop();
	afx_msg void OnDeltaEGraphYZoomIn();
	afx_msg void OnDeltaEGraphYZoomOut();
	afx_msg void OnDeltaEGraphScaleCustom();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RGBHISTOVIEW_H__0C087EB3_7C7F_4FC4_B2FA_7A5EB932EEF5__INCLUDED_)
