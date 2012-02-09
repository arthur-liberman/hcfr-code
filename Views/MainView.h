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
//	Fran�ois-Xavier CHABOUD
//	Georges GALLERAND
//	Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// MainView.h : interface of the CMainView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINVIEW_H__F4DFD748_954B_43EB_9050_4CC1C81FA527__INCLUDED_)
#define AFX_MAINVIEW_H__F4DFD748_954B_43EB_9050_4CC1C81FA527__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define HCFR_XYZ_VIEW		0
#define HCFR_SENSORRGB_VIEW	1
#define HCFR_RGB_VIEW		2
#define HCFR_xyz2_VIEW		3
#define HCFR_xyY_VIEW		4

class CGridCtrl;
#include "BtnST.h"
#include "ShadeButtonST.h"
#include "XPGroupBox.h"
#include "RGBLevelWnd.h"
#include "TargetWnd.h"

class CMainView : public CFormView
{
protected: // create from serialization only
	CMainView();
	DECLARE_DYNCREATE(CMainView)

public:
	//{{AFX_DATA(CMainView)
	enum { IDD = IDD_MAINVIEW_FORM };
	CComboBox	m_comboMode;
	CXPGroupBox	m_grayScaleGroup;
	CXPGroupBox	m_sensorGroup;
	CXPGroupBox	m_generatorGroup;
	CXPGroupBox	m_datarefGroup;
	CXPGroupBox	m_displayGroup;
	CXPGroupBox	m_paramGroup;
	CXPGroupBox	m_selectGroup;
	CXPGroupBox	m_viewGroup;
	CButton		m_editCheckButton;
	BOOL		m_datarefCheckButton;
	CButton		m_AdjustXYZCheckButton;
	CButtonST	m_grayScaleButton;
	CButtonST	m_grayScaleDeleteButton;
	CButtonST	m_configSensorButton;
	CButtonST	m_configGeneratorButton;
	CStatic	m_valuesStatic;
	CStatic	m_colordataStatic;
	CString	m_generatorName;
	CString	m_sensorName;
	CStatic	m_TargetStatic;
	CStatic	m_RGBLevelsStatic;
	CComboBox	m_comboDisplay;
	CButtonST	m_testAnsiPatternButton;
	//}}AFX_DATA

private:
	BOOL		m_bPositionsInit;
	POINT		m_InitialWindowSize;
	RECT		m_OriginalRect;
	CPtrList	m_CtrlInitPos;

	DWORD		m_dwInitialUserInfo;

	// Information windows
	CWnd *		m_pInfoWnd;

// Attributes
public:
	int m_displayMode;
	int m_infoDisplay;
	int m_displayType;
	int	m_nSizeOffset;
	CBrush *m_pBgBrush;

	CDataSetDoc* GetDocument();

	CTargetWnd		m_Target;
	CRGBLevelWnd	m_RGBLevels;

	CColor		m_SelectedColor;

	void SetSelectedColor ( CColor & clr )	{ m_SelectedColor = clr; GetDocument () -> SetSelectedColor ( clr ); RefreshSelection (); }

	void RefreshSelection ();

// Operations
public:
	void UpdateAllGrids();
	DWORD	GetUserInfo ();
	void	SetUserInfo ( DWORD dwUserInfo );

protected:
	void InitGrid();
	void InitSelectedColorGrid();
	void InitButtons();
	void InitGroups();

	void UpdateGrid();
	void UpdateContrastValuesInGrid ();

	CString GetItemText(CColor & aMeasure, double YWhite, CColor & aReference, CColor & aRefDocColor, double YWhiteRefDoc, int aComponentNum, int nCol, double Offset);
	LPSTR GetGridRowLabel(int aComponentNum);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainView)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnInitialUpdate(); // called first time after construct
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnPrint(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	CGridCtrl* m_pGrayScaleGrid;
	CGridCtrl* m_pSelectedColorGrid;

	void OnGrayScaleGridBeginEdit(NMHDR *pNotifyStruct,LRESULT* pResult);
	void OnGrayScaleGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult);
	void OnGrayScaleGridEndSelChange(NMHDR *pNotifyStruct,LRESULT* pResult);

public:
	void UpdateMeasurementsAfterBkgndMeasure ();

// Generated message map functions
protected:
	//{{AFX_MSG(CMainView)
	afx_msg void OnXyzRadio();
	afx_msg void OnSensorrgbRadio();
	afx_msg void OnRgbRadio();
	afx_msg void OnXyz2Radio();
	afx_msg void OnxyYRadio();
	afx_msg void OnSelchangeComboMode();
	afx_msg void OnEditgridCheck();
	afx_msg void OnDatarefCheck();
	afx_msg void OnAdjustXYZCheck();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSysColorChange();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnMeasureGrayScale();
	afx_msg void OnDeleteGrayscale();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSelchangeInfoDisplay();
	afx_msg void OnChangeInfosEdit();
	afx_msg void OnHelp();
	afx_msg void OnEditCopy();
	afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
	afx_msg void OnEditCut();
	afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
	afx_msg void OnEditPaste();
	afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
	afx_msg void OnEditUndo();
	afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
	afx_msg void OnDeltaposSpinView(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnAnsiContrastPatternTestButton();
	//}}AFX_MSG
	afx_msg LRESULT OnSetUserInfoPostInitialUpdate(WPARAM wParam, LPARAM lParam);
 public:
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in ColorHCFRView.cpp
inline CDataSetDoc* CMainView::GetDocument()
   { return (CDataSetDoc*)m_pDocument; }
#endif


/////////////////////////////////////////////////////////////////////////////
// CSubFrame: transparent frame window which contains sub views inside CMainView

class CSubFrame : public CFrameWnd
{
public:
	CSubFrame();

	DECLARE_DYNAMIC(CSubFrame)

// Generated message map functions
public:
	//{{AFX_MSG(CSubFrame)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnDestroy();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINVIEW_H__F4DFD748_954B_43EB_9050_4CC1C81FA527__INCLUDED_)
