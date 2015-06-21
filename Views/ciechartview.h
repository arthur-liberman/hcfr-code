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

#if !defined(AFX_CIECHARTVIEW_H__FA46DF20_DBB5_4B59_8EBE_85FAE9A931A4__INCLUDED_)
#define AFX_CIECHARTVIEW_H__FA46DF20_DBB5_4B59_8EBE_85FAE9A931A4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CIEChartView.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CCIEChartView form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDataSetDoc;

#include "PPTooltip.h" 

class CCIEGraphPoint
{
// Operations
public:
	CCIEGraphPoint(const ColorXYZ& color, double WhiteYRef, CString aName, BOOL bConvertCIEuv);
	int GetGraphX(CRect rect) const;
	int GetGraphY(CRect rect) const;
	CPoint GetGraphPoint(CRect rect) const;
    ColorXYZ GetNormalizedColor() const {return m_color;}

// Attributes
public:
	double	x;
	double	y;
	double	L;
	CString name;
	BOOL	bCIEuv;
	BOOL	bCIELab;
    ColorXYZ m_color;
};

class CCIEChartGrapher
{
 public:
	CCIEChartGrapher ();

	CBitmap m_bgBitmap;
	CBitmap m_drawBitmap;
	CBitmap m_gamutBitmap;
	CBitmap m_refRedPrimaryBitmap;
	CBitmap m_refGreenPrimaryBitmap;
	CBitmap m_refBluePrimaryBitmap;
	CBitmap m_refYellowSecondaryBitmap;
	CBitmap m_refCyanSecondaryBitmap;
	CBitmap m_refMagentaSecondaryBitmap;
	CBitmap m_illuminantPointBitmap;
	CBitmap m_colorTempPointBitmap;
	CBitmap m_redPrimaryBitmap;
	CBitmap m_greenPrimaryBitmap;
	CBitmap m_bluePrimaryBitmap;
	CBitmap m_yellowSecondaryBitmap;
	CBitmap m_cyanSecondaryBitmap;
	CBitmap m_magentaSecondaryBitmap;
	CBitmap m_redSatRefBitmap;
	CBitmap m_greenSatRefBitmap;
	CBitmap m_blueSatRefBitmap;
	CBitmap m_yellowSatRefBitmap;
	CBitmap m_cyanSatRefBitmap;
	CBitmap m_magentaSatRefBitmap;
	CBitmap m_cc24SatRefBitmap;
	CBitmap m_grayPlotBitmap;
	CBitmap m_measurePlotBitmap;
	CBitmap m_selectedPlotBitmap;
	CBitmap	m_datarefRedBitmap;
	CBitmap	m_datarefGreenBitmap;
	CBitmap	m_datarefBlueBitmap;
	CBitmap	m_datarefYellowBitmap;
	CBitmap	m_datarefCyanBitmap;
	CBitmap	m_datarefMagentaBitmap;

	// Updatable flags. Initialized with default values in constructor, can be changed before calling drawing functions
	BOOL m_doDisplayBackground;
	BOOL m_doDisplayDeltaERef;
	BOOL m_doShowReferences;
	BOOL m_doShowDataRef;
	BOOL m_doShowGrayScale;
	BOOL m_doShowSaturationScale;
	BOOL m_doShowSaturationScaleTarg;
	BOOL m_doShowCCScale;
	BOOL m_doShowCCScaleTarg;
	BOOL m_doShowMeasurements;
	BOOL m_bCIEuv;
	BOOL m_bCIEab;
	BOOL m_bdE10;
	double dE10;

	int		m_ttID; //tooltip index

	// Zoom handling, for window mode
	UINT	m_ZoomFactor;	// Zoom factor = 1000 for 1:1 scale, 2000 for 2x zoom, and so on
	int		m_DeltaX;		// When zoom active, delta values for picture scrolling in pixels
	int		m_DeltaY;

	// Operations
	void MakeBgBitmap(CRect rect,BOOL bWhiteBkgnd);
	void DrawAlphaBitmap(CDC *pDC, const CCIEGraphPoint& aGraphPoint, CBitmap *pBitmap, CRect rect, CPPToolTip * pTooltip, CWnd * pWnd, CCIEGraphPoint * pRefPoint = NULL, bool isSelected = FALSE, double dE10=100.0);
	void DrawChart(CDataSetDoc * pDoc, CDC* pDC, CRect rect, CPPToolTip * pTooltip, CWnd * pWnd);
	
	void SaveGraphFile ( CDataSetDoc * pDoc, CSize ImageSize, LPCSTR lpszPathName, int ImageFormat = 0, int ImageQuality = 95, bool PDF=FALSE );
};

class CCIEChartView : public CSavingView
{
protected:
	CCIEChartView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CCIEChartView)

// Form Data
public:
	//{{AFX_DATA(CCIEChartView)
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

protected:
// Attributes
	BOOL	m_bDelayedUpdate;

	CCIEChartGrapher m_Grapher;

	double	m_refDeltaE;

	CPPToolTip m_tooltip;

	CPoint	m_CurMousePoint;

public:
	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }
	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

// Operations
public:
	void	UpdateTestColor ( CPoint point );
	void	GetReferenceRect ( LPRECT lpRect );		// Returns client rect with size increased regarding zoom factor

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCIEChartView)
	public:
	virtual void OnInitialUpdate();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void OnDraw(CDC* pDC);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	//}}AFX_VIRTUAL

// Implementation
protected:

	void SaveChart();

	virtual ~CCIEChartView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CCIEChartView)
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnUpdateCieShowbackground(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowDeltaE(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowreferences(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieGraphShowDataRef(CCmdUI* pCmdUI);
	afx_msg void OnCieShowreferences();
	afx_msg void OnCieGraphShowDataRef();
	afx_msg void OnCieShowbackground();
	afx_msg void OnCieShowDeltaE();
	afx_msg void OnCieShowGrayScale();
	afx_msg void OnCieShowSaturationScale();
	afx_msg void OnCieShowSaturationScaleTarg();
	afx_msg void OnCieShowCCScale();
	afx_msg void OnCieShowdE10();
	afx_msg void OnCieShowCCScaleTarg();
	afx_msg void OnCieShowMeasurements();
	afx_msg void OnGraphZoomIn();
	afx_msg void OnGraphZoomOut();
	afx_msg void OnUpdateCieShowMeasurements(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowGrayScale(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowSaturationScale(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowSaturationScaleTarg(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowCCScale(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowdE10(CCmdUI* pCmdUI);
	afx_msg void OnUpdateCieShowCCScaleTarg(CCmdUI* pCmdUI);
	afx_msg void OnCieSavechart();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnHelp();
	afx_msg void OnCieUv();
	afx_msg void OnUpdateCieUv(CCmdUI* pCmdUI);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void NotifyDisplayTooltip(NMHDR * pNMHDR, LRESULT * result);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CIECHARTVIEW_H__FA46DF20_DBB5_4B59_8EBE_85FAE9A931A4__INCLUDED_)
