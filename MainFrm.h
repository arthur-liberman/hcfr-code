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

// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MAINFRM_H__630B1B99_075B_426B_8F06_860B102D6825__INCLUDED_)
#define AFX_MAINFRM_H__630B1B99_075B_426B_8F06_860B102D6825__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "PPTooltip.h" 

#include "TestColorWnd.h"
#include "LuminanceWnd.h"

class CPatternDisplay;	//Pa

class CMDIClientWithLogo : public CWnd
{
public:
	CMDIClientWithLogo ();
	~CMDIClientWithLogo ();

private:
	CBitmap		m_LogoBitmap;

// Generated message map functions
protected:
	//{{AFX_MSG(CMDIClientWithLogo)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


class CMainFrame : public CNewMDIFrameWnd
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();


// Attributes
public:
	HICON		m_hIconControlledMode;
	CPPToolTip	m_tooltip;
	int m_mainToolbarID,m_viewToolbarID,m_measureToolbarID,m_measureexToolbarID,m_measuresatToolbarID;

	CMDIClientWithLogo	m_LogoWnd;

// Operations
public:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual BOOL DestroyWindow();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation

public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	BOOL LoadToolbars();
	BOOL VerifyBarState(LPCTSTR lpszProfileName);
	void DockControlBarNextTo(CControlBar* pBar,CControlBar* pTargetBar);

protected:  // control bar embedded members
	CNewMenuBar			m_wndMenuBar;
	CNewStatusBar		m_wndStatusBar;
	CNewToolBar			m_wndToolBar;
	CNewToolBar			m_wndToolBarViews;
	CNewToolBar			m_wndToolBarMeasures;
	CNewToolBar			m_wndToolBarMeasuresEx;
	CNewToolBar			m_wndToolBarMeasuresSat;
	CLuminanceWnd		m_wndLuminanceWnd;
	
public:
	CTestColorWnd		m_wndTestColorWnd;
	CPatternDisplay * 	p_wndPatternDisplay; //Pa

// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDuplicateDoc();
	afx_msg void OnViewViewBar();
	afx_msg void OnUpdateViewSensorBar(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewViewBar(CCmdUI* pCmdUI);
	afx_msg void OnViewToolbar();
	afx_msg void OnUpdateViewToolbar(CCmdUI* pCmdUI);
	afx_msg void OnAppearanceConfig();
	afx_msg void OnSysColorChange();
	afx_msg void OnViewTestColor();
	afx_msg void OnUpdateViewTestColor(CCmdUI* pCmdUI);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnGeneralConfig();
	afx_msg void OnHelp();
	afx_msg void OnHelpForum();
	afx_msg void OnHelpInstall();
	afx_msg void OnHelpSupport();
	afx_msg void OnLanguage();
	afx_msg void OnUpdateSoft();
	afx_msg void OnUpdateEtalons();
	afx_msg void OnPatternDisplay();
	afx_msg void OnRefreshLux();
	afx_msg void OnViewLuminance();
	afx_msg void OnUpdateViewLuminance(CCmdUI* pCmdUI);
	afx_msg void OnViewMeasureBar();
	afx_msg void OnUpdateViewMeasureBar(CCmdUI* pCmdUI);
	afx_msg void OnViewMeasureExBar();
	afx_msg void OnUpdateViewMeasureExBar(CCmdUI* pCmdUI);
	afx_msg void OnUpdateIRProfiles();
	afx_msg void OnUpdateControlMode(CCmdUI* pCmdUI);
	afx_msg void OnViewMeasureSatBar();
	afx_msg void OnUpdateViewMeasureSatBar(CCmdUI* pCmdUI);
	//}}AFX_MSG

	afx_msg LRESULT OnDDEInitiate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDDEExecute(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDDERequest(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDDEPoke(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDDETerminate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRightButton(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnMiddleButton(WPARAM wParam, LPARAM lParam);

	afx_msg LRESULT OnBkgndMeasureReady(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__630B1B99_075B_426B_8F06_860B102D6825__INCLUDED_)
