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
//	Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_NEARBLACKHISTOVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_)
#define AFX_NEARBLACKHISTOVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LuminanceHistoView.h : header file
//

#include "GraphControl.h"

/////////////////////////////////////////////////////////////////////////////
// CNearBlackHistoView form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDataSetDoc;

class CNearBlackGrapher
{
 public:
	CNearBlackGrapher ();

	CGraphControl m_graphCtrl;
	long m_luminanceGraphID;
	long m_luxmeterGraphID;
	long m_redLumGraphID;
	long m_greenLumGraphID;
	long m_blueLumGraphID;
	long m_refGraphID;

	long m_luminanceDataRefGraphID;	//Ki
	long m_luxmeterDataRefGraphID;
	long m_redLumDataRefGraphID;	//Ki
	long m_greenLumDataRefGraphID;	//Ki
	long m_blueLumDataRefGraphID;	//Ki

	CGraphControl m_logGraphCtrl;
	long m_luminanceLogGraphID;
	long m_luxmeterLogGraphID;
	long m_redLumLogGraphID;
	long m_greenLumLogGraphID;
	long m_blueLumLogGraphID;
	long m_refLogGraphID;

	long m_luminanceDataRefLogGraphID;	//Ki
	long m_luxmeterDataRefLogGraphID;
	long m_redLumDataRefLogGraphID;		//Ki
	long m_greenLumDataRefLogGraphID;	//Ki
	long m_blueLumDataRefLogGraphID;	//Ki
	
	// Updatable flags. Initialized with default values in constructor, can be changed before calling UpdateGraph
	BOOL m_showReference;
	BOOL m_doLogMode;
	BOOL m_showYLum;
	BOOL m_showL;
	BOOL m_showRedLum;
	BOOL m_showGreenLum;
	BOOL m_showBlueLum;
	BOOL m_showDataRef;	//Ki

// Operations
	void UpdateGraph ( CDataSetDoc * pDoc );

protected:
	void AddPointtoLumGraph(int ColorSpace,int ColorIndex,int Size,int PointIndex,CDataSetDoc *pDataSet,long GraphID,long LogGraphID,double Valformax);
};

class CNearBlackHistoView : public CSavingView
{
protected:
	CNearBlackHistoView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CNearBlackHistoView)

// Form Data
public:
	//{{AFX_DATA(CNearBlackHistoView)
	//}}AFX_DATA

// Attributes
public:
	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }
	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

	CNearBlackGrapher m_Grapher;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNearBlackHistoView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CNearBlackHistoView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CNearBlackHistoView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnLumGraphLogMode();
	afx_msg void OnLumGraphBlueLum();
	afx_msg void OnLumGraphGreenLum();
	afx_msg void OnLumGraphRedLum();
	afx_msg void OnLumGraphShowRef();
	afx_msg void OnLumGraphYLum();
	afx_msg void OnLumGraphL();
	afx_msg void OnLumGraphShowDataRef(); //Ki
	afx_msg void OnGraphSettings();
	afx_msg void OnGraphYShiftBottom();
	afx_msg void OnGraphYShiftTop();
	afx_msg void OnGraphYZoomIn();
	afx_msg void OnGraphYZoomOut();
	afx_msg void OnLuminanceGraphYScale1();
	afx_msg void OnGraphScaleFit();
	afx_msg void OnGraphYScaleFit();
	afx_msg void OnGammaGraphYScale1();
	afx_msg void OnGraphScaleCustom();
	afx_msg void OnGraphSave();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NEARBLACKHISTOVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_)
