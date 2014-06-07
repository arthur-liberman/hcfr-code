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

#if !defined(AFX_SATLUMSHIFTVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_)
#define AFX_SATLUMSHIFTVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SatLumShiftView.h : header file
//

#include "GraphControl.h"

/////////////////////////////////////////////////////////////////////////////
// CLuminanceShiftView form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDataSetDoc;

class CSatLumShiftGrapher
{
 public:
	CSatLumShiftGrapher ();

	CGraphControl m_graphCtrl;
	CGraphControl m_graphCtrl2;
	
	long m_redSatGraphID;
	long m_greenSatGraphID;
	long m_blueSatGraphID;
	long m_yellowSatGraphID;
	long m_cyanSatGraphID;
	long m_magentaSatGraphID;
	long m_redColorGraphID;
	long m_greenColorGraphID;
	long m_blueColorGraphID;
	long m_yellowColorGraphID;
	long m_cyanColorGraphID;
	long m_magentaColorGraphID;
	long m_refGraphID;

	long m_redSatDataRefGraphID;
	long m_greenSatDataRefGraphID;
	long m_blueSatDataRefGraphID;
	long m_yellowSatDataRefGraphID;
	long m_cyanSatDataRefGraphID;
	long m_magentaSatDataRefGraphID;
	long m_redColorDataRefGraphID;
	long m_greenColorDataRefGraphID;
	long m_blueColorDataRefGraphID;
	long m_yellowColorDataRefGraphID;
	long m_cyanColorDataRefGraphID;
	long m_magentaColorDataRefGraphID;

	// Updatable flags. Initialized with default values in constructor, can be changed before calling UpdateGraph
	BOOL m_showReference;
	BOOL m_showRed;
	BOOL m_showGreen;
	BOOL m_showBlue;
	BOOL m_showYellow;
	BOOL m_showCyan;
	BOOL m_showMagenta;
	BOOL m_showDataRef;

// Operations
public:
	void GetEndPoint ( double & xend, double & yend, CColor & SaturatedColor, const ColorRGB& ClrRGB );
	void GetSatShift ( double & satshift, double & deltaE, const CColor& SatColor, int num, int count, double xstart, double ystart, double xend, double yend, double gamma, double luma, double YWhite, CDataSetDoc * pDoc );
	void UpdateGraph ( CDataSetDoc * pDoc );
};


class CSatLumShiftView : public CSavingView
{
protected:
	CSatLumShiftView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CSatLumShiftView)

// Form Data
public:
	//{{AFX_DATA(CSatLumShiftView)
	//}}AFX_DATA

// Attributes
public:
	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }
	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

	CSatLumShiftGrapher m_Grapher;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSatLumShiftView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CSatLumShiftView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CSatLumShiftView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnSatGraphRed();
	afx_msg void OnSatGraphGreen();
	afx_msg void OnSatGraphBlue();
	afx_msg void OnSatGraphYellow();
	afx_msg void OnSatGraphCyan();
	afx_msg void OnSatGraphMagenta();
	afx_msg void OnSatGraphShowRef();
	afx_msg void OnSatGraphShowDataRef();
	afx_msg void OnGraphSettings();
	afx_msg void OnGraphSave();
	afx_msg void OnGraphYShiftBottom();
	afx_msg void OnGraphYShiftTop();
	afx_msg void OnGraphYZoomIn();
	afx_msg void OnGraphYZoomOut();
	afx_msg void OnLuminanceGraphYScale1();
	afx_msg void OnLuminanceGraphYScale2();
	afx_msg void OnGraphScaleFit();
	afx_msg void OnGraphYScaleFit();
	afx_msg void OnGraphScaleCustom();
	afx_msg void OnDeltaEGraphYScale1();
	afx_msg void OnDeltaEGraphYScale2();
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

#endif // !defined(AFX_SATLUMSHIFTVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_)
