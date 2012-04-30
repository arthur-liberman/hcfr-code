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

#if !defined(AFX_SATLUMHISTOVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_)
#define AFX_SATLUMHISTOVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SatLumHistoView.h : header file
//

#include "GraphControl.h"

/////////////////////////////////////////////////////////////////////////////
// CLuminanceHistoView form view

#ifndef __AFXEXT_H__
#include <afxext.h>
#endif

class CDataSetDoc;

class CSatLumGrapher
{
 public:
	CSatLumGrapher ();

	CGraphControl m_graphCtrl;
	long m_redLumGraphID;
	long m_greenLumGraphID;
	long m_blueLumGraphID;
	long m_yellowLumGraphID;
	long m_cyanLumGraphID;
	long m_magentaLumGraphID;

	long m_redLumDataRefGraphID;
	long m_greenLumDataRefGraphID;
	long m_blueLumDataRefGraphID;
	long m_yellowLumDataRefGraphID;
	long m_cyanLumDataRefGraphID;
	long m_magentaLumDataRefGraphID;
	
	long m_ref_redLumGraphID;
	long m_ref_greenLumGraphID;
	long m_ref_blueLumGraphID;
	long m_ref_yellowLumGraphID;
	long m_ref_cyanLumGraphID;
	long m_ref_magentaLumGraphID;

	// Updatable flags. Initialized with default values in constructor, can be changed before calling UpdateGraph
	BOOL m_showReferences;
	BOOL m_showPrimaries;
	BOOL m_showSecondaries;
	BOOL m_showDataRef;

// Operations
	void UpdateGraph ( CDataSetDoc * pDoc );
};

class CSatLumHistoView : public CSavingView
{
protected:
	CSatLumHistoView();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CSatLumHistoView)

// Form Data
public:
	//{{AFX_DATA(CSatLumHistoView)
	//}}AFX_DATA

// Attributes
public:
	CDataSetDoc * GetDocument() const { return (CDataSetDoc *) CView::GetDocument (); }
	virtual DWORD	GetUserInfo ();
	virtual void	SetUserInfo ( DWORD dwUserInfo );

	CSatLumGrapher m_Grapher;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSatLumHistoView)
	public:
	virtual void OnInitialUpdate();
	protected:
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnDraw(CDC* pDC);
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CSatLumHistoView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
	//{{AFX_MSG(CSatLumHistoView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnLumGraphPrimaries();
	afx_msg void OnLumGraphSecondaries();
	afx_msg void OnLumGraphShowRef();
	afx_msg void OnLumGraphShowDataRef();
	afx_msg void OnGraphSettings();
	afx_msg void OnGraphSave();
	afx_msg void OnGraphScaleFit();
	afx_msg void OnLuminanceGraphYScale1();
	afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SATLUMHISTOVIEW_H__3F1AD8CE_2332_4DA0_8450_C30E81805BAB__INCLUDED_)
