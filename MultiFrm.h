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

// MultiFrm.h : interface of the CMultiFrame class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_MULTIFRM_H__0FA24630_ABBF_472F_9EBC_142687A72E75__INCLUDED_)
#define AFX_MULTIFRM_H__0FA24630_ABBF_472F_9EBC_142687A72E75__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BtnST.h"

#include "Tools/CustomTabControl/ThemeUtil.h"
#include "Tools/CustomTabControl/CustomTabCtrl.h"

#define MAX_TABBED_VIEW		32

/////////////////////////////////////////////////////////////////////////////
// CRefCheckDlg dialog

class CRefCheckDlg : public CDialog
{
// Construction
public:
	CRefCheckDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CRefCheckDlg)
	enum { IDD = IDD_REF_CHECKBOX };
	CButtonST	m_ButtonMenu;
	CButton		m_RefCheck;
	CButton		m_XYZCheck;
	//}}AFX_DATA

	BOOL	m_bTop;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRefCheckDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CRefCheckDlg)
	afx_msg void OnPaint();
	afx_msg void OnCheckRef();
	afx_msg void OnCheckXYZ();
	afx_msg void OnButtonMenu();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class CMultiFrame : public CNewMDIChildWnd
{
	DECLARE_DYNCREATE(CMultiFrame)
public:
	CMultiFrame();

// Attributes
public:
	POINT			m_MinSize;
	POINT			m_MinSize2;
	BOOL			m_bUseMinSize2;
	CRefCheckDlg	m_RefCheckDlg;
	CCustomTabCtrl	m_TabCtrl;
	BOOL			m_bDisplayTab;
	BOOL			m_bTabUp;
	
	int				m_NbTabbedViews;
	CView *			m_pTabbedView [ MAX_TABBED_VIEW ];
	int				m_nTabbedViewIndex [ MAX_TABBED_VIEW ];

	BOOL			m_bDDERunning;

	CDataSetDoc *	GetDocument()	{ return (CDataSetDoc*) m_pDocument; }

// Internal flags to control new frame creation
private:
	CDocument *		m_pDocument;
	UINT			m_nCreateFrameStyle;
	UINT			m_nViewTypeDefinition;
	int				m_LastRightClickedTab;

// Operations
public:
	CView * CreateTabbedView ( CCreateContext * pContext, int nIDName, int nIDTooltipText, RECT Rect, BOOL bDisableInitialUpdate = FALSE );
	CView * CreateOrActivateView ( int nViewIndex, BOOL bForceCreate = FALSE, BOOL bDisableInitialUpdate = FALSE );
	void CreateNewFrameOnView ( int nViewIndex, BOOL bAlone );
	void ActivateViewOnAllFrames ( int nViewIndex, BOOL bExcludeMyself );
	void CloseAllThoseViews ( int nViewIndex );
	void PerformToolbarOperation ( int nViewIndex, int nOperation );
	int	GetViewTabIndex ( int nIDName );

	void OnChangeRef ( BOOL bSet );
	void OnChangeXYZ ( BOOL bSet );
	void OpenNewTabMenu ( POINT pt );

	void OnRightToolbarButton(WPARAM wParam);
	void OnMiddleToolbarButton(WPARAM wParam);

	void EnsureMinimumSize ();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMultiFrame)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext );
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMultiFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	BOOL DdeCmdExec ( CString & strCommand, BOOL bCanSendAckMsg, HWND hWndClient, LPARAM lParam, LPBOOL pbAckMsgSent );

// Generated message map functions
protected:
	//{{AFX_MSG(CMultiFrame)
	afx_msg void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTabChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRightClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnToggleTab();
	afx_msg void OnUpdateToggleTab(CCmdUI* pCmdUI);
	afx_msg void OnClickTabctrl(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPrevTab();
	afx_msg void OnNextTab();
	afx_msg void OnRefreshReference();
	afx_msg void OnViewDataSet();
	afx_msg void OnUpdateViewDataSet(CCmdUI* pCmdUI);
	afx_msg void OnViewCiechart();
	afx_msg void OnUpdateViewCiechart(CCmdUI* pCmdUI);
	afx_msg void OnViewColortemphisto();
	afx_msg void OnUpdateViewColortemphisto(CCmdUI* pCmdUI);
	afx_msg void OnViewLuminancehisto();
	afx_msg void OnUpdateViewLuminancehisto(CCmdUI* pCmdUI);
	afx_msg void OnViewGammahisto();
	afx_msg void OnUpdateViewGammahisto(CCmdUI* pCmdUI);
	afx_msg void OnViewNearBlackhisto();
	afx_msg void OnUpdateViewNearBlackhisto(CCmdUI* pCmdUI);
	afx_msg void OnViewNearWhitehisto();
	afx_msg void OnUpdateViewNearWhitehisto(CCmdUI* pCmdUI);
	afx_msg void OnViewRgbhisto();
	afx_msg void OnUpdateViewRgbhisto(CCmdUI* pCmdUI);
	afx_msg void OnViewSatLumhisto();
	afx_msg void OnUpdateViewSatLumhisto(CCmdUI* pCmdUI);
	afx_msg void OnViewSatLumshift();
	afx_msg void OnUpdateViewSatLumshift(CCmdUI* pCmdUI);
	afx_msg void OnViewMeasuresCombo();
	afx_msg void OnUpdateViewMeasuresCombo(CCmdUI* pCmdUI);
	afx_msg void OnActivateTab();
	afx_msg void OnActivateTabOnAll();
	afx_msg void OnDuplicateTab();
	afx_msg void OnCloseTab();
	afx_msg void OnOpenTabInNewWindow();
	//}}AFX_MSG
 public:
	afx_msg LRESULT OnDDEInitiate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDDEExecute(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDDERequest(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDDEPoke(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnDDETerminate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnAppCommand(WPARAM wParam, LPARAM lParam );
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MULTIFRM_H__0FA24630_ABBF_472F_9EBC_142687A72E75__INCLUDED_)
