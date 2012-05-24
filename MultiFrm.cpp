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

// MultiFrm.cpp : implementation of the CMultiFrame class
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "MainView.h"
#include "MainFrm.h"

#include "MultiFrm.h"

#include "LuminanceHistoView.h"
#include "GammaHistoView.h"
#include "RGBHistoView.h"
#include "CIEChartView.h"
#include "ColorTempHistoView.h"
#include "NearBlackHistoView.h"
#include "NearWhiteHistoView.h"
#include "SatLumHistoView.h"
#include "SatLumShiftView.h"
#include "measureshistoview.h"

#include "OneDeviceSensor.h"
#include "KiSensor.h"
#include "GDIGenerator.h"

#include <dde.h>
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Tab control ID
#define IDC_TABCTRL		1

// Tab control height
#define TABCTRL_HEIGHT	20

CPtrList gOpenedFramesList;

typedef struct
{
	CRuntimeClass * pRunTime;
	int				nIDName;
	int				nIDTooltipText;
} HCFR_VIEW_TYPE;

static const HCFR_VIEW_TYPE g_ViewType [] =	
{
	{ RUNTIME_CLASS ( CMainView ), IDS_DATASETVIEW_NAME, IDS_DATASETVIEW_NAME },
	{ RUNTIME_CLASS ( CLuminanceHistoView ), IDS_LUMINANCE, IDS_LUMINANCEHISTOVIEW_NAME },
	{ RUNTIME_CLASS ( CGammaHistoView ), IDS_GAMMA, IDS_GAMMAHISTOVIEW_NAME },
	{ RUNTIME_CLASS ( CNearBlackHistoView ), IDS_NEARBLACK, IDS_NEARBLACKHISTOVIEW_NAME },
	{ RUNTIME_CLASS ( CNearWhiteHistoView ), IDS_NEARWHITE, IDS_NEARWHITEHISTOVIEW_NAME },
	{ RUNTIME_CLASS ( CRGBHistoView ), IDS_RGBLEVELS, IDS_RGBHISTOVIEW_NAME },
	{ RUNTIME_CLASS ( CColorTempHistoView ), IDS_COLORTEMP, IDS_COLORTEMPHISTOVIEW_NAME },
	{ RUNTIME_CLASS ( CCIEChartView ), IDS_CIECHARTVIEW_NAME, IDS_CIECHARTVIEW_NAME },
	{ RUNTIME_CLASS ( CSatLumHistoView ), IDS_SATLUM, IDS_SATLUMHISTOVIEW_NAME },
	{ RUNTIME_CLASS ( CSatLumShiftView ), IDS_SATLUMSHIFT, IDS_SATLUMSHIFTVIEW_NAME },
	{ RUNTIME_CLASS ( CMeasuresHistoView ), IDS_FREEMEASURES, IDS_MEASURESHISTOVIEW_NAME }
};

// Define view type indexes (indexes in array above)
#define VIEW_IDX_DATASET			0
#define VIEW_IDX_LUMINANCE			1
#define VIEW_IDX_GAMMA				2
#define VIEW_IDX_NEARBLACK			3
#define VIEW_IDX_NEARWHITE			4
#define VIEW_IDX_RGB				5
#define VIEW_IDX_COLORTEMP			6
#define VIEW_IDX_CIECHART			7
#define VIEW_IDX_SATLUM				8
#define VIEW_IDX_SATLUMSHIFT		9
#define VIEW_IDX_FREEMEASURES		10

// Tool function: retrieve an array index from name ID
static int GetViewTypeIndex ( int nIDName )
{
	int	i;

	for ( i = 0; i < sizeof(g_ViewType)/sizeof(g_ViewType[0]) ; i ++ )
		if ( g_ViewType [ i ].nIDName == nIDName )
			return i;

	return -1;
}


/////////////////////////////////////////////////////////////////////////////
// CMultiFrame

#ifndef WM_APPCOMMAND
#define WM_APPCOMMAND                   0x0319
#define APPCOMMAND_BROWSER_BACKWARD     1
#define APPCOMMAND_BROWSER_FORWARD      2

#define FAPPCOMMAND_MASK				0x8000
#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#endif

IMPLEMENT_DYNCREATE(CMultiFrame, CNewMDIChildWnd)

BEGIN_MESSAGE_MAP(CMultiFrame, CNewMDIChildWnd)
	//{{AFX_MSG_MAP(CMultiFrame)
	ON_WM_GETMINMAXINFO()
	ON_WM_DESTROY()
	ON_WM_SIZE()
    ON_MESSAGE(WM_APPCOMMAND, OnAppCommand)
	ON_NOTIFY(CTCN_SELCHANGE, IDC_TABCTRL, OnTabChanged)
	ON_NOTIFY(CTCN_RCLICK, IDC_TABCTRL, OnRightClick)
	ON_COMMAND(IDM_TOGGLE_TAB,OnToggleTab)
	ON_UPDATE_COMMAND_UI(IDM_TOGGLE_TAB,OnUpdateToggleTab)
	ON_NOTIFY(CTCN_CLICK, IDC_TABCTRL, OnClickTabctrl)
	ON_COMMAND(IDM_NEXT_TAB,OnNextTab)
	ON_COMMAND(IDM_PREV_TAB,OnPrevTab)
	ON_COMMAND(IDM_REFRESH_REFERENCE, OnRefreshReference)
	ON_COMMAND(IDM_VIEW_DATASET, OnViewDataSet)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_DATASET, OnUpdateViewDataSet)
	ON_COMMAND(IDM_VIEW_CIECHART, OnViewCiechart)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_CIECHART, OnUpdateViewCiechart)
	ON_COMMAND(IDM_VIEW_COLORTEMPHISTO, OnViewColortemphisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_COLORTEMPHISTO, OnUpdateViewColortemphisto)
	ON_COMMAND(IDM_VIEW_LUMINANCEHISTO, OnViewLuminancehisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_LUMINANCEHISTO, OnUpdateViewLuminancehisto)
	ON_COMMAND(IDM_VIEW_GAMMAHISTO, OnViewGammahisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_GAMMAHISTO, OnUpdateViewGammahisto)
	ON_COMMAND(IDM_VIEW_NEARBLACKHISTO, OnViewNearBlackhisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_NEARBLACKHISTO, OnUpdateViewNearBlackhisto)
	ON_COMMAND(IDM_VIEW_NEARWHITEHISTO, OnViewNearWhitehisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_NEARWHITEHISTO, OnUpdateViewNearWhitehisto)
	ON_COMMAND(IDM_VIEW_RGBHISTO, OnViewRgbhisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_RGBHISTO, OnUpdateViewRgbhisto)
	ON_COMMAND(IDM_VIEW_SATLUMHISTO, OnViewSatLumhisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_SATLUMHISTO, OnUpdateViewSatLumhisto)
	ON_COMMAND(IDM_VIEW_SATLUMSHIFT, OnViewSatLumshift)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_SATLUMSHIFT, OnUpdateViewSatLumshift)
	ON_COMMAND(IDM_VIEW_MEASURESCOMBO, OnViewMeasuresCombo)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_MEASURESCOMBO, OnUpdateViewMeasuresCombo)
	ON_COMMAND(IDM_TAB_ACTIVATE, OnActivateTab)
	ON_COMMAND(IDM_TAB_ACTIVATE_ON_ALL, OnActivateTabOnAll)
	ON_COMMAND(IDM_TAB_DUPLICATE, OnDuplicateTab)
	ON_COMMAND(IDM_TAB_CLOSE, OnCloseTab)
	ON_COMMAND(IDM_TAB_NEWFRAME, OnOpenTabInNewWindow)
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_DDE_INITIATE, OnDDEInitiate)
	ON_MESSAGE(WM_DDE_EXECUTE, OnDDEExecute)
	ON_MESSAGE(WM_DDE_REQUEST, OnDDERequest)
	ON_MESSAGE(WM_DDE_POKE, OnDDEPoke)
	ON_MESSAGE(WM_DDE_TERMINATE, OnDDETerminate)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMultiFrame construction/destruction

CMultiFrame::CMultiFrame()
{
	m_MinSize.x = 100;
	m_MinSize.y = 100;
	m_MinSize2.x = 784;//844;
	m_MinSize2.y = 464;//569;//320;
	m_bUseMinSize2 = FALSE;

	m_NbTabbedViews = 0;
	m_bDisplayTab = TRUE;
	m_bTabUp = GetConfig()->GetProfileInt("DataSet View","UpperTabControl",FALSE);
	m_RefCheckDlg.m_bTop = m_bTabUp;
	
	m_nCreateFrameStyle = 0;
	m_nViewTypeDefinition = 0;
	m_LastRightClickedTab = -1;

	// Avoid using MFC RecalcLayout.
	m_bInRecalcLayout = TRUE;

	gOpenedFramesList.AddTail ( this );
}
	
CMultiFrame::~CMultiFrame()
{
	POSITION	pos = gOpenedFramesList.Find ( this );
	gOpenedFramesList.RemoveAt ( pos );
}

/////////////////////////////////////////////////////////////////////////////
// CMultiFrame diagnostics

#ifdef _DEBUG
void CMultiFrame::AssertValid() const
{
	CMDIChildWnd::AssertValid();
}

void CMultiFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWnd::Dump(dc);
}

#endif //_DEBUG

void CMultiFrame::OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI) 
{
	int		nOffset;
	int		nMaxOffset = 0;
	CRect	Rect;

	CNewMDIChildWnd::OnGetMinMaxInfo(lpMMI);

	if ( m_bUseMinSize2 )
	{
		lpMMI->ptMinTrackSize.x=m_MinSize2.x;
		lpMMI->ptMinTrackSize.y=m_MinSize2.y + ( m_bDisplayTab ? TABCTRL_HEIGHT : 0 );

		for ( int i = 0; i < m_NbTabbedViews ; i++ )
		{
			if ( m_nTabbedViewIndex [ i ] == IDS_DATASETVIEW_NAME )
			{
		 		nOffset = ( (CMainView *) m_pTabbedView [ i ] ) -> m_nSizeOffset;
				if ( nOffset > nMaxOffset )
					nMaxOffset = nOffset;
			}
		}

		lpMMI->ptMinTrackSize.y += nMaxOffset;
	}
	else
	{
		lpMMI->ptMinTrackSize.x=m_MinSize.x;
		lpMMI->ptMinTrackSize.y=m_MinSize.y + ( m_bDisplayTab ? TABCTRL_HEIGHT : 0 );
	}
}

void CMultiFrame::OnDestroy() 
{
	CRect rect;
	GetWindowRect(&rect);

	GetConfig()->WriteProfileInt("Views Size","MultiFrame W", rect.Width());
	GetConfig()->WriteProfileInt("Views Size","MultiFrame H", rect.Height());
	
	CNewMDIChildWnd::OnDestroy();
}

BOOL CMultiFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	int width=GetConfig()->GetProfileInt("Views Size","MultiFrame W", 850);
	int height=GetConfig()->GetProfileInt("Views Size","MultiFrame H", 400);

	cs.cx=width;
	cs.cy=height;
	return CNewMDIChildWnd::PreCreateWindow(cs);
}

void CMultiFrame::OnSize(UINT nType, int cx, int cy) 
{
	int		nSelTab;
	DWORD	nSelView;
	RECT	Rect, Rect2, Rect3;

	GetClientRect ( & Rect );

	if ( m_bDisplayTab )
	{
		if ( m_bTabUp )
			Rect.bottom = Rect.top + TABCTRL_HEIGHT;
		else
			Rect.top = Rect.bottom - TABCTRL_HEIGHT;
		
		m_RefCheckDlg.GetWindowRect ( & Rect2 );
		Rect3.top = Rect.top;
		Rect3.bottom = Rect.bottom;
		Rect3.right = Rect.right;
		Rect3.left = Rect3.right - ( Rect2.right - Rect2.left );

		Rect.right = Rect3.left;
		
		GetClientRect ( & Rect2 );
		if ( m_bTabUp )
			Rect2.top += TABCTRL_HEIGHT;
		else
			Rect2.bottom -= TABCTRL_HEIGHT;

		m_TabCtrl.SetWindowPos ( NULL, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER );

		nSelTab = m_TabCtrl.GetCurSel ();
		if ( nSelTab >= 0 && nSelTab < m_NbTabbedViews )
		{
			m_TabCtrl.GetItemData ( nSelTab, nSelView );
			m_pTabbedView [ nSelView ] -> SetWindowPos ( NULL, Rect2.left, Rect2.top, Rect2.right - Rect2.left, Rect2.bottom - Rect2.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER );
		}

		m_RefCheckDlg.SetWindowPos ( NULL, Rect3.left, Rect3.top, Rect3.right - Rect3.left, Rect3.bottom - Rect3.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER );
	}
	else
	{
		if ( m_NbTabbedViews )
			m_pTabbedView [ 0 ] -> SetWindowPos ( NULL, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER );
	}

	CNewMDIChildWnd::OnSize ( nType, cx, cy );
}

BOOL CMultiFrame::OnCreateClient( LPCREATESTRUCT lpcs, CCreateContext* pContext )
{
	int					nViewIndex = VIEW_IDX_DATASET;
	BOOL				bDisableInitialUpdate = FALSE;
	BOOL				bMaximize = FALSE;
	BOOL				bMinimize = FALSE;
	DWORD				dwUserInfo = 0;
	RECT				Rect, Rect2, Rect3;
	CString				Msg;
	POSITION			pos = NULL;
	WINDOWPLACEMENT *	pWndPlacement;
	CDataSetViewInfo *	pViewInfo;

	m_pDocument = pContext -> m_pCurrentDoc;

	if ( GetDocument () -> m_pFramePosInfo )
	{
		// Create frame with window position and view(s) definition
		m_bDisplayTab = FALSE;

		pWndPlacement = & GetDocument () -> m_pFramePosInfo -> m_FramePlacement;
		if ( pWndPlacement -> showCmd == SW_SHOWMAXIMIZED )
		{
			pWndPlacement -> showCmd = SW_SHOW;
			bMaximize = TRUE;
		}
		else if ( pWndPlacement -> showCmd == SW_SHOWMINIMIZED )
		{
			pWndPlacement -> showCmd = SW_SHOW;
			bMinimize = TRUE;
		}
		
		SetWindowPlacement ( pWndPlacement );
		m_bDisplayTab = TRUE;
		
		pos = GetDocument () -> m_pFramePosInfo -> m_ViewInfoList.GetHeadPosition ();
		pViewInfo = ( CDataSetViewInfo * ) GetDocument () -> m_pFramePosInfo -> m_ViewInfoList.GetNext ( pos );
		nViewIndex = GetViewTypeIndex ( pViewInfo -> m_nViewIndex );
		dwUserInfo = pViewInfo -> m_dwUserInfo;

		// Do not call OnInitialUpdate during view creation, it will be called by the framework for all frame descendant windows
		bDisableInitialUpdate = TRUE;
	}
	else if ( pContext -> m_pCurrentFrame )
	{
		// Create this frame with a particular view, with or without tab control
		if ( ( (CMultiFrame *) pContext -> m_pCurrentFrame ) -> m_nCreateFrameStyle != 0 )
		{
			nViewIndex = ( (CMultiFrame *) pContext -> m_pCurrentFrame ) -> m_nViewTypeDefinition;
			if ( ( (CMultiFrame *) pContext -> m_pCurrentFrame ) -> m_nCreateFrameStyle > 1 )
				m_bDisplayTab = FALSE;
		}
	}
	
	GetClientRect ( & Rect );

	if ( m_bDisplayTab )
	{
		if ( m_bTabUp )
			Rect.bottom = Rect.top + TABCTRL_HEIGHT;
		else
			Rect.top = Rect.bottom - TABCTRL_HEIGHT;
		
		m_RefCheckDlg.Create (IDD_REF_CHECKBOX, this);

		m_RefCheckDlg.GetWindowRect ( & Rect2 );
		Rect3.top = Rect.top;
		Rect3.bottom = Rect.bottom;
		Rect3.right = Rect.right;
		Rect3.left = Rect3.right - ( Rect2.right - Rect2.left );

		Rect.right = Rect3.left;

		m_RefCheckDlg.SetWindowPos ( NULL, Rect3.left, Rect3.top, Rect3.right - Rect3.left, Rect3.bottom - Rect3.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER );
		m_RefCheckDlg.ShowWindow ( SW_SHOW );

		if ( GetDataRef () == GetDocument () )
			m_RefCheckDlg.m_RefCheck.SetCheck ( TRUE );

		m_TabCtrl.Create ( WS_CHILD | WS_VISIBLE | CTCS_TOOLTIPS | CTCS_CLOSEBUTTON | CTCS_DRAGMOVE | (m_bTabUp?CTCS_TOP:0), Rect, this, IDC_TABCTRL );

		m_TabCtrl.SetDragCursors(AfxGetApp()->LoadCursor(IDC_CURSORMOVE),NULL);
		
		Msg.LoadString(IDM_CLOSEVIEW);
		m_TabCtrl.SetItemTooltipText(CTCID_CLOSEBUTTON,Msg);
	}
	
	do
	{
		CView * pView = CreateOrActivateView ( nViewIndex, TRUE, bDisableInitialUpdate );
		if ( dwUserInfo != 0 )
		{
			if ( nViewIndex == VIEW_IDX_DATASET )
				( (CMainView *) pView ) -> SetUserInfo ( dwUserInfo );
			else
			{
				ASSERT ( pView -> IsKindOf ( RUNTIME_CLASS(CSavingView) ) );
				( (CSavingView *) pView ) -> SetUserInfo ( dwUserInfo );
			}
		}

		if ( ! pos )
			break;
		
		pViewInfo = ( CDataSetViewInfo * ) GetDocument () -> m_pFramePosInfo -> m_ViewInfoList.GetNext ( pos );
		nViewIndex = GetViewTypeIndex ( pViewInfo -> m_nViewIndex );
		dwUserInfo = pViewInfo -> m_dwUserInfo;
			
	} while ( TRUE );

	if ( bDisableInitialUpdate )
	{
		m_TabCtrl.SetCurSel ( GetDocument () -> m_pFramePosInfo -> m_nActiveView );
		OnTabChanged ( NULL, NULL );
	}

	if ( bMaximize )
		PostMessage ( WM_SYSCOMMAND, SC_MAXIMIZE );
	else if ( bMinimize )
		PostMessage ( WM_SYSCOMMAND, SC_MINIMIZE );

	return ( m_NbTabbedViews > 0 );
}

CView * CMultiFrame::CreateTabbedView ( CCreateContext * pContext, int nIDName, int nIDTooltipText, RECT Rect, BOOL bDisableInitialUpdate )
{
	CView *		pView = NULL;
	CString		Name, TooltipText;

	if ( m_NbTabbedViews < MAX_TABBED_VIEW )
		pView = (CView *) CreateView ( pContext, AFX_IDW_PANE_FIRST );

	if ( pView )
	{
		// Do not initialize first created view: it will be done by MFC Framework
		if ( m_NbTabbedViews > 0 && ! bDisableInitialUpdate )
			pView -> OnInitialUpdate();

		Name.LoadString ( nIDName );
		TooltipText.LoadString ( nIDTooltipText );
		
		pView -> SetWindowPos ( NULL, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOOWNERZORDER );
		m_pTabbedView [ m_NbTabbedViews ] = pView;
		m_nTabbedViewIndex [ m_NbTabbedViews ] = nIDName;
		if ( m_bDisplayTab )
		{
			m_TabCtrl.InsertItem ( m_NbTabbedViews, Name );
			m_TabCtrl.SetItemData ( m_NbTabbedViews, m_NbTabbedViews );
			m_TabCtrl.SetItemTooltipText ( m_NbTabbedViews, TooltipText );
		}
		else
		{
			pView -> ShowWindow ( SW_SHOW );
		}
		m_NbTabbedViews ++;
	}

	return pView;
}

CView * CMultiFrame::CreateOrActivateView ( int nViewIndex, BOOL bForceCreate, BOOL bDisableInitialUpdate ) 
{
	int		nIndex = GetViewTabIndex ( g_ViewType [ nViewIndex ].nIDName );
	RECT	Rect;
	CView *	pView = NULL;

	if ( nIndex >= 0 && ! bForceCreate )
	{
		// Activate existing view
		m_TabCtrl.SetCurSel ( nIndex );
		OnTabChanged ( NULL, NULL );
	}
	else
	{
		// Create view
		CCreateContext	context;
		POSITION		pos = AfxGetApp () -> GetFirstDocTemplatePosition ();

		context.m_pCurrentDoc = m_pDocument;
		context.m_pCurrentFrame = this;
		context.m_pLastView = ( m_NbTabbedViews ? m_pTabbedView [ m_NbTabbedViews - 1 ] : NULL );
		context.m_pNewDocTemplate = AfxGetApp () -> GetNextDocTemplate(pos);
		context.m_pNewViewClass = g_ViewType [ nViewIndex ].pRunTime;

		if ( nViewIndex == VIEW_IDX_DATASET )
			m_bUseMinSize2 = TRUE;

		GetClientRect ( & Rect );
		if ( m_bDisplayTab )
		{
			if ( m_bTabUp )
				Rect.bottom = Rect.top + TABCTRL_HEIGHT;
			else
				Rect.top = Rect.bottom - TABCTRL_HEIGHT;
		}

		pView = CreateTabbedView ( & context, g_ViewType [ nViewIndex ].nIDName, g_ViewType [ nViewIndex ].nIDTooltipText, Rect, bDisableInitialUpdate );
		
		if ( m_bDisplayTab && ! bDisableInitialUpdate )
		{
			// Select new view in Tab
			m_TabCtrl.SetCurSel ( m_NbTabbedViews - 1 );
			OnTabChanged ( NULL, NULL );
		}

		if ( nViewIndex == VIEW_IDX_DATASET && ! bDisableInitialUpdate )
		{
			// Ensure minimum frame window size
			EnsureMinimumSize ();
		}
	}
	
	return pView;
}

void CMultiFrame::EnsureMinimumSize ()
{
	RECT	Rect;

	GetWindowRect ( & Rect );
	SetWindowPos ( NULL, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE );
}

void CMultiFrame::CreateNewFrameOnView ( int nViewIndex, BOOL bAlone ) 
{
	// Put internal flags to force mode
	m_nCreateFrameStyle = ( bAlone ? 2 : 1 );
	m_nViewTypeDefinition = nViewIndex;

	CDocTemplate* pTemplate = m_pDocument->GetDocTemplate();
	CFrameWnd* pFrame = pTemplate->CreateNewFrame(m_pDocument, this);
	pTemplate->InitialUpdateFrame(pFrame, m_pDocument);

	// Restore internal flags
	m_nCreateFrameStyle = 0;
	m_nViewTypeDefinition = 0;
}

int	CMultiFrame::GetViewTabIndex ( int nIDName )
{
	int	i;

	for ( i = 0; i < m_NbTabbedViews; i ++ )
		if ( m_nTabbedViewIndex [ i ] == nIDName )
			return i;

	return -1;
}

void CMultiFrame::OnTabChanged(NMHDR* pNMHDR, LRESULT* pResult) 
{
	int		nCurTab;
	int		nSelTab = m_TabCtrl.GetCurSel ();
	DWORD	nSelView = 0xFFFFFFFF;
	RECT	Rect;

	m_TabCtrl.GetItemData ( nSelTab, nSelView );

	GetClientRect ( & Rect );

	if ( m_bTabUp )
		Rect.top += TABCTRL_HEIGHT;
	else
		Rect.bottom -= TABCTRL_HEIGHT;
	
	for ( nCurTab = 0; nCurTab < m_NbTabbedViews ; nCurTab ++ )
	{
		if ( nCurTab != nSelView && m_pTabbedView [ nCurTab ] -> IsWindowVisible () )
			m_pTabbedView [ nCurTab ] -> ShowWindow ( SW_HIDE );
	}

	m_pTabbedView [ nSelView ] -> SetWindowPos ( NULL, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOOWNERZORDER );
	m_pTabbedView [ nSelView ] -> ShowWindow ( SW_SHOW );
	SetActiveView ( m_pTabbedView [ nSelView ] );

	if ( pResult )
		*pResult = 0;
}

void CMultiFrame::OpenNewTabMenu ( POINT pt ) 
{
	CNewMenu menu;
	menu.LoadMenu(IDR_MESURETYPE);
	menu.LoadToolBar(IDR_MENUBARGRAPH);
	
	// Retrieve "Graphs" menu (must be at position 4)
	CMenu* pViewPopup = menu.GetSubMenu(4);
	ASSERT ( pViewPopup );

	if ( pViewPopup )
	{
		pViewPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, pt.x, pt.y, this);
	}
}

void CMultiFrame::OnRightClick(NMHDR* pNMHDR, LRESULT* pResult) 
{
	int		TabIndex = ((CTC_NMHDR*)pNMHDR)->nItem;
	POINT	pt;
	
	m_LastRightClickedTab = -1;
	if ( TabIndex == CTCHT_NOWHERE )
	{
		pt = ((CTC_NMHDR*)pNMHDR)->ptHitTest;
		m_TabCtrl.ClientToScreen ( & pt );
		OpenNewTabMenu ( pt );
	}
	else if ( TabIndex >= 0 )
	{
		m_LastRightClickedTab = TabIndex;

		CNewMenu menu;
		menu.LoadMenu(IDR_TAB_MENU);
		
		CMenu* pPopup = menu.GetSubMenu(0);
		ASSERT ( pPopup );
		
		if ( m_NbTabbedViews < 2 )
		{
			// Remove separator and close option
			pPopup->RemoveMenu(4,MF_BYPOSITION);
			pPopup->RemoveMenu(4,MF_BYPOSITION);
		}

		pt = ((CTC_NMHDR*)pNMHDR)->ptHitTest;
		m_TabCtrl.ClientToScreen ( & pt );
		pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
			pt.x, pt.y, this);
	}

	if ( pResult )
		*pResult = 0;
}

void CMultiFrame::OnToggleTab() 
{
	if ( m_bDisplayTab )
	{
		if ( m_bTabUp )
			m_TabCtrl.ModifyStyle(CTCS_TOP,0);
		else
			m_TabCtrl.ModifyStyle(0,CTCS_TOP);
		m_bTabUp = !m_bTabUp;
		m_RefCheckDlg.m_bTop = m_bTabUp;

		OnSize ( 0, 0, 0 );

		GetConfig()->WriteProfileInt("DataSet View","UpperTabControl",m_bTabUp);
	}
}

void CMultiFrame::OnUpdateToggleTab(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable ( m_bDisplayTab );
	pCmdUI->SetCheck ( m_bDisplayTab && m_bTabUp );
}

void CMultiFrame::OnClickTabctrl(NMHDR* pNMHDR, LRESULT* pResult) 
{
	int		nSelTab;
	int		nCurTab;
	int		TabIndex = ((CTC_NMHDR*)pNMHDR)->nItem;
	int		nViewIndex;
	DWORD	nSelView, nCurView;
	CView *	pView;

	if ( TabIndex == CTCHT_ONCLOSEBUTTON )
	{
		nSelTab = m_TabCtrl.GetCurSel ();

		if ( nSelTab >= 0 && nSelTab < m_NbTabbedViews && m_NbTabbedViews > 1 )
		{
			m_TabCtrl.GetItemData ( nSelTab, nSelView );
			
			pView = m_pTabbedView [ nSelView ];

			pView -> DestroyWindow ();
			pView = NULL;

			if ( nSelView < (DWORD)(m_NbTabbedViews - 1))
			{
				memmove ( & m_pTabbedView [ nSelView ], & m_pTabbedView [ nSelView + 1 ], ( m_NbTabbedViews - nSelView - 1 ) * sizeof ( m_pTabbedView [ 0 ] ) );
				memmove ( & m_nTabbedViewIndex [ nSelView ], & m_nTabbedViewIndex [ nSelView + 1 ], ( m_NbTabbedViews - nSelView - 1 ) * sizeof ( m_nTabbedViewIndex [ 0 ] ) );
			}
			m_NbTabbedViews --;

			m_TabCtrl.DeleteItem ( nSelTab );

			if ( nSelTab >= m_NbTabbedViews )
				nSelTab --;

			m_TabCtrl.SetCurSel ( nSelTab );
			
			for ( nCurTab = 0; nCurTab < m_NbTabbedViews ; nCurTab ++ )
			{
				m_TabCtrl.GetItemData ( nCurTab, nCurView );
				if ( nCurView > nSelView )
					m_TabCtrl.SetItemData ( nCurTab, nCurView - 1 );
			}
			
			OnTabChanged ( NULL, NULL );

			if ( GetViewTabIndex ( IDS_DATASETVIEW_NAME ) < 0 )
				m_bUseMinSize2 = FALSE;
		}
	}
	else if ( TabIndex >= 0 && TabIndex < m_NbTabbedViews )
	{
		// Activate this view on all frames when control of shift is down
		if ( GetKeyState ( VK_CONTROL ) < 0 || GetKeyState ( VK_SHIFT ) < 0 )
		{
			m_TabCtrl.GetItemData ( TabIndex, nSelView );
			
			nViewIndex = GetViewTypeIndex ( m_nTabbedViewIndex [ nSelView ] );
			if ( nViewIndex >= 0 )
			{
				ActivateViewOnAllFrames ( nViewIndex, TRUE );
			}
		}

	}
	*pResult = 0;
}

void CMultiFrame::CloseAllThoseViews ( int nViewIndex )
{
	int		nTab, nCurTab;
	BOOL	bFound = FALSE;
	BOOL	bCurrentClosed = FALSE;
	DWORD	nSelView, nCurView;
	CView *	pView;

	for ( nTab = 0; nTab < m_NbTabbedViews && m_NbTabbedViews > 1 ; nTab ++ )
	{
		m_TabCtrl.GetItemData ( nTab, nSelView );
		
		if ( m_nTabbedViewIndex [ nSelView ] == g_ViewType [ nViewIndex ].nIDName )
		{
			// Delete this view
			pView = m_pTabbedView [ nSelView ];

			pView -> DestroyWindow ();
			pView = NULL;

			if ( nSelView < (DWORD)(m_NbTabbedViews - 1))
			{
				memmove ( & m_pTabbedView [ nSelView ], & m_pTabbedView [ nSelView + 1 ], ( m_NbTabbedViews - nSelView - 1 ) * sizeof ( m_pTabbedView [ 0 ] ) );
				memmove ( & m_nTabbedViewIndex [ nSelView ], & m_nTabbedViewIndex [ nSelView + 1 ], ( m_NbTabbedViews - nSelView - 1 ) * sizeof ( m_nTabbedViewIndex [ 0 ] ) );
			}
			m_NbTabbedViews --;

			if ( m_TabCtrl.GetCurSel () == nTab )
				bCurrentClosed = TRUE;

			m_TabCtrl.DeleteItem ( nTab );

			for ( nCurTab = 0; nCurTab < m_NbTabbedViews ; nCurTab ++ )
			{
				m_TabCtrl.GetItemData ( nCurTab, nCurView );
				if ( nCurView > nSelView )
					m_TabCtrl.SetItemData ( nCurTab, nCurView - 1 );
			}
			
			nTab --;
		}
	}

	if ( GetViewTabIndex ( IDS_DATASETVIEW_NAME ) < 0 )
		m_bUseMinSize2 = FALSE;

	if ( bCurrentClosed )
	{
		m_TabCtrl.SetCurSel ( 0 );
		OnTabChanged ( NULL, NULL );
	}
}

void CMultiFrame::ActivateViewOnAllFrames ( int nViewIndex, BOOL bExcludeMyself )
{
	CMultiFrame *	pOtherFrame;
	POSITION		pos;

	pos = gOpenedFramesList.GetHeadPosition ();
	while ( pos )
	{
		pOtherFrame = (CMultiFrame *) gOpenedFramesList.GetNext ( pos );
		if ( ! bExcludeMyself || pOtherFrame != this )
			pOtherFrame -> CreateOrActivateView ( nViewIndex );
	}
}

void CMultiFrame::OnViewDataSet() 
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_DATASET );
}

void CMultiFrame::OnUpdateViewDataSet(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_DATASETVIEW_NAME ) >= 0 );
}

void CMultiFrame::OnViewCiechart() 
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_CIECHART );
}

void CMultiFrame::OnUpdateViewCiechart(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_CIECHARTVIEW_NAME ) >= 0 );
}

void CMultiFrame::OnViewColortemphisto()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_COLORTEMP );
}

void CMultiFrame::OnUpdateViewColortemphisto(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_COLORTEMP ) >= 0 );
}

void CMultiFrame::OnViewLuminancehisto()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_LUMINANCE );
}

void CMultiFrame::OnUpdateViewLuminancehisto(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_LUMINANCE ) >= 0 );
}

void CMultiFrame::OnViewGammahisto()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_GAMMA );
}

void CMultiFrame::OnUpdateViewGammahisto(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_GAMMA ) >= 0 );
}

void CMultiFrame::OnViewNearBlackhisto()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_NEARBLACK );
}

void CMultiFrame::OnUpdateViewNearBlackhisto(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_NEARBLACK ) >= 0 );
}

void CMultiFrame::OnViewNearWhitehisto()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_NEARWHITE );
}

void CMultiFrame::OnUpdateViewNearWhitehisto(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_NEARWHITE ) >= 0 );
}

void CMultiFrame::OnViewRgbhisto()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_RGB );
}

void CMultiFrame::OnUpdateViewRgbhisto(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_RGBLEVELS ) >= 0 );
}

void CMultiFrame::OnViewSatLumhisto()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_SATLUM );
}

void CMultiFrame::OnUpdateViewSatLumhisto(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_SATLUM ) >= 0 );
}

void CMultiFrame::OnViewSatLumshift()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_SATLUMSHIFT );
}

void CMultiFrame::OnUpdateViewSatLumshift(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_SATLUMSHIFT ) >= 0 );
}

void CMultiFrame::OnViewMeasuresCombo()
{
	if ( m_bDisplayTab )
		CreateOrActivateView ( VIEW_IDX_FREEMEASURES );
}

void CMultiFrame::OnUpdateViewMeasuresCombo(CCmdUI* pCmdUI)
{
	pCmdUI -> Enable ( m_bDisplayTab );
	pCmdUI -> SetCheck ( GetViewTabIndex ( IDS_FREEMEASURES ) >= 0 );
}

void CMultiFrame::OnActivateTab()
{
	if ( m_LastRightClickedTab >= 0 && m_LastRightClickedTab < m_NbTabbedViews )
	{
		m_TabCtrl.SetCurSel ( m_LastRightClickedTab );
		OnTabChanged ( NULL, NULL );
	}
}

void CMultiFrame::OnActivateTabOnAll()
{
	int		nViewIndex;
	DWORD	nSelView;

	if ( m_LastRightClickedTab >= 0 && m_LastRightClickedTab < m_NbTabbedViews )
	{
		m_TabCtrl.SetCurSel ( m_LastRightClickedTab );
		OnTabChanged ( NULL, NULL );

		m_TabCtrl.GetItemData ( m_LastRightClickedTab, nSelView );
		
		nViewIndex = GetViewTypeIndex ( m_nTabbedViewIndex [ nSelView ] );
		if ( nViewIndex >= 0 )
		{
			CMultiFrame *	pOtherFrame;
			POSITION		pos;

			pos = gOpenedFramesList.GetHeadPosition ();
			while ( pos )
			{
				pOtherFrame = (CMultiFrame *) gOpenedFramesList.GetNext ( pos );
				if ( pOtherFrame != this )
					pOtherFrame -> CreateOrActivateView ( nViewIndex );
			}
		}
	}
}

void CMultiFrame::OnDuplicateTab()
{
	int		nViewIndex;
	DWORD	nSelView;

	if ( m_LastRightClickedTab >= 0 && m_LastRightClickedTab < m_NbTabbedViews )
	{
		m_TabCtrl.GetItemData ( m_LastRightClickedTab, nSelView );
		nViewIndex = GetViewTypeIndex ( m_nTabbedViewIndex [ nSelView ] );
		if ( nViewIndex >= 0 )
			CreateOrActivateView ( nViewIndex, TRUE );
	}
}

void CMultiFrame::OnCloseTab()
{
	DWORD	nSelView, nCurView;
	int		nCurTab;

	if ( m_LastRightClickedTab >= 0 && m_LastRightClickedTab < m_NbTabbedViews && m_NbTabbedViews > 1 )
	{
		m_TabCtrl.GetItemData ( m_LastRightClickedTab, nSelView );
		
		m_pTabbedView [ nSelView ] -> DestroyWindow ();

		if ( nSelView < (DWORD)(m_NbTabbedViews - 1))
		{
			memmove ( & m_pTabbedView [ nSelView ], & m_pTabbedView [ nSelView + 1 ], ( m_NbTabbedViews - nSelView - 1 ) * sizeof ( m_pTabbedView [ 0 ] ) );
			memmove ( & m_nTabbedViewIndex [ nSelView ], & m_nTabbedViewIndex [ nSelView + 1 ], ( m_NbTabbedViews - nSelView - 1 ) * sizeof ( m_nTabbedViewIndex [ 0 ] ) );
		}
		m_NbTabbedViews --;

		if ( m_TabCtrl.GetCurSel () == m_LastRightClickedTab )
		{
			m_TabCtrl.DeleteItem ( m_LastRightClickedTab );

			if ( m_LastRightClickedTab >= m_NbTabbedViews )
				m_LastRightClickedTab --;

			m_TabCtrl.SetCurSel ( m_LastRightClickedTab );
			OnTabChanged ( NULL, NULL );
		}
		else
		{
			m_TabCtrl.DeleteItem ( m_LastRightClickedTab );
		}
		
		for ( nCurTab = 0; nCurTab < m_NbTabbedViews ; nCurTab ++ )
		{
			m_TabCtrl.GetItemData ( nCurTab, nCurView );
			if ( nCurView > nSelView )
				m_TabCtrl.SetItemData ( nCurTab, nCurView - 1 );
		}

		if ( GetViewTabIndex ( IDS_DATASETVIEW_NAME ) < 0 )
			m_bUseMinSize2 = FALSE;
	}
}

void CMultiFrame::OnOpenTabInNewWindow()
{
	int		nViewIndex;
	DWORD	nSelView;

	if ( m_LastRightClickedTab >= 0 && m_LastRightClickedTab < m_NbTabbedViews )
	{
		m_TabCtrl.GetItemData ( m_LastRightClickedTab, nSelView );
		nViewIndex = GetViewTypeIndex ( m_nTabbedViewIndex [ nSelView ] );
		if ( nViewIndex >= 0 )
			CreateNewFrameOnView ( nViewIndex, FALSE );
	}
}

void CMultiFrame::OnChangeRef ( BOOL bSet )
{
	UpdateDataRef(bSet, GetDocument());

	AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
}
 
LRESULT CMultiFrame::OnAppCommand(WPARAM /*wParam*/, LPARAM lParam )
{
    LRESULT lResult = 0;

    switch ( GET_APPCOMMAND_LPARAM(lParam) )
	{
		case APPCOMMAND_BROWSER_BACKWARD:
			 OnPrevTab ();
			 lResult = TRUE;
			 break;

		case APPCOMMAND_BROWSER_FORWARD:
			 OnNextTab ();
			 lResult = TRUE;
			 break;
	}

    return lResult;
}

void CMultiFrame::OnNextTab() 
{
	int		nSelTab;

	nSelTab = m_TabCtrl.GetCurSel ();
	if ( nSelTab < m_NbTabbedViews - 1 )
		m_TabCtrl.SetCurSel ( nSelTab + 1 );
	else 
		m_TabCtrl.SetCurSel ( 0 );

	OnTabChanged ( NULL, NULL );
}

void CMultiFrame::OnPrevTab() 
{
	int		nSelTab;

	nSelTab = m_TabCtrl.GetCurSel ();
	if ( nSelTab > 0 )
		m_TabCtrl.SetCurSel ( nSelTab - 1 );
	else 
		m_TabCtrl.SetCurSel ( m_NbTabbedViews - 1 );

	OnTabChanged ( NULL, NULL );
}

void CMultiFrame::OnRefreshReference() 
{
	if ( m_bDisplayTab )
	{
		m_RefCheckDlg.m_RefCheck.SetCheck ( GetDataRef () == GetDocument () );
	}
}

static int GetViewIndexFromMenuCommand ( WPARAM wParam )
{
	int	nViewIndex = -1;

	switch (wParam)
	{
		case IDM_VIEW_DATASET:
			 nViewIndex = VIEW_IDX_DATASET;
			 break;

		case IDM_VIEW_LUMINANCEHISTO:
			 nViewIndex = VIEW_IDX_LUMINANCE;
			 break;

		case IDM_VIEW_GAMMAHISTO:
			 nViewIndex = VIEW_IDX_GAMMA;
			 break;

		case IDM_VIEW_RGBHISTO:
			 nViewIndex = VIEW_IDX_RGB;
			 break;

		case IDM_VIEW_COLORTEMPHISTO:
			 nViewIndex = VIEW_IDX_COLORTEMP;
			 break;

		case IDM_VIEW_SATLUMHISTO:
			 nViewIndex = VIEW_IDX_SATLUM;
			 break;

		case IDM_VIEW_SATLUMSHIFT:
			 nViewIndex = VIEW_IDX_SATLUMSHIFT;
			 break;

		case IDM_VIEW_CIECHART:
			 nViewIndex = VIEW_IDX_CIECHART;
			 break;

		case IDM_VIEW_NEARBLACKHISTO:
			 nViewIndex = VIEW_IDX_NEARBLACK;
			 break;

		case IDM_VIEW_NEARWHITEHISTO:
			 nViewIndex = VIEW_IDX_NEARWHITE;
			 break;

		case IDM_VIEW_MEASURESCOMBO:
			 nViewIndex = VIEW_IDX_FREEMEASURES;
			 break;
	}

	return nViewIndex;
}

void CMultiFrame::PerformToolbarOperation ( int nViewIndex, int nOperation )
{
	switch ( nOperation )
	{
		case 0:	// Open view in new window
			 CreateNewFrameOnView ( nViewIndex, FALSE );
			 break;

		case 1: // Activate view in all opened windows
			 ActivateViewOnAllFrames ( nViewIndex, FALSE );
			 break;

		case 2:	// Duplicate view
			 if ( m_bDisplayTab )
				CreateOrActivateView ( nViewIndex, TRUE );
			 break;

		case 3:	// Close view
			 CloseAllThoseViews ( nViewIndex );
			 break;
	}
}

void CMultiFrame::OnRightToolbarButton(WPARAM wParam)
{
	int	nViewIndex = GetViewIndexFromMenuCommand ( wParam );

	if ( nViewIndex >= 0 )
		PerformToolbarOperation ( nViewIndex, GetConfig()->m_TBViewsRightClickMode );
}

void CMultiFrame::OnMiddleToolbarButton(WPARAM wParam)
{
	int	nViewIndex = GetViewIndexFromMenuCommand ( wParam );

	if ( nViewIndex >= 0 )
		PerformToolbarOperation ( nViewIndex, GetConfig()->m_TBViewsMiddleClickMode );
}

void DdeParseString ( LPCSTR lpszString, CString & strCmd, CStringList & CmdParams )
{
	int		nNbPar;
	char	c;
	LPSTR	lpStr;
	LPSTR	lpStart;
	CString	strTemp;

	CmdParams.RemoveAll ();

	while ( lpszString [ 0 ] == ' ' || lpszString [ 0 ] == '\t' )
		lpszString ++;

	lpStr = strchr ( (LPSTR)lpszString, (int) '(' );
	if ( lpStr )
	{	
		lpStr [ 0 ] = '\0';
		strCmd = lpszString;
		lpStr [ 0 ] = '(';

		lpStart = lpStr + 1;
		if ( lpStart [ 0 ] != ')' )
		{
			do
			{
				while ( lpStart [ 0 ] == ' ' || lpStart [ 0 ] == '\t' )
					lpStart ++;
				
				nNbPar = 0;
				lpStr = lpStart;
				while ( (c=lpStr[0]) != '\0' )
				{
					switch ( c )
					{
						case ',':
							 if ( nNbPar == 0 )
								nNbPar = -1;
							 break;

						case '(':
							 nNbPar ++;
							 break;

						case ')':
							 nNbPar --;
							 break;
					}

					if ( nNbPar < 0 )
						break;

					lpStr ++;
				}
				
				switch ( c )
				{
					case '\0':
					case ')':
						// Last parameter
						lpStr [ 0 ] = '\0';
						strTemp = lpStart;
						lpStr [ 0 ] = ')';

						lpStart = NULL;
						break;

					case ',':
						lpStr [ 0 ] = '\0';
						strTemp = lpStart;
						lpStr [ 0 ] = ',';
						
						lpStart = lpStr + 1;
						break;
				}
				
				strTemp.TrimRight ();
				CmdParams.AddTail ( strTemp );

			} while ( lpStart );
		}
	}
	else
	{
		strCmd = lpszString;
		strCmd.TrimRight ();
	}
}

static const char * g_szDDETopicName = "ColorDataSet:";

LRESULT CMultiFrame::OnDDEInitiate(WPARAM wParam, LPARAM lParam)
{
	BOOL		bOk = FALSE;
	LRESULT		lResult = -1;
	HWND		hWndClient = (HWND) wParam;
	ATOM		nAtomAppli = (ATOM) LOWORD(lParam);
	ATOM		nAtomTopic = (ATOM) HIWORD(lParam);
	ATOM		nAtomSrvAppli;
	ATOM		nAtomSrvTopic;
	char		szBuf [ 256 ];
	char		szTopic [ 512 ];

	// Only the first document frame accepts DDE dialog
	if ( m_nWindow <= 1 )
	{
		// Accept conversation only when one document is opened, and when sensor is calibrated
		strcpy ( szTopic, g_szDDETopicName );
		strcat ( szTopic, (LPCSTR) GetDocument () -> GetTitle () );

		if ( nAtomAppli )
		{
			if ( nAtomAppli == AfxGetApp () -> m_atomApp )
			{
				if ( nAtomTopic )
				{
					GlobalGetAtomName ( nAtomTopic, szBuf, sizeof ( szBuf ) );
					if ( _stricmp ( szBuf, szTopic ) == 0 )
						bOk = TRUE;
				}
				else
				{
					bOk = TRUE;
				}
			}
		}

		if ( bOk )
		{
			// Accept DDE conversation
			nAtomSrvAppli = AfxGetApp () -> m_atomApp;
			nAtomSrvTopic = GlobalAddAtom ( szTopic );
			lParam = MAKELPARAM ( nAtomSrvAppli, nAtomSrvTopic );
			::SendMessage ( hWndClient, WM_DDE_ACK, (WPARAM) m_hWnd, lParam );

			m_bDDERunning = TRUE;
		}
	}

	return lResult;
}

LRESULT CMultiFrame::OnDDEExecute(WPARAM wParam, LPARAM lParam)
{
	int			i;
	char		c;
	BOOL		bOk = FALSE;
	LRESULT		lResult = 0;
	HWND		hWndClient = (HWND) wParam;
	HGLOBAL		hMem = (HGLOBAL) lParam;
	LPCSTR		lpszCommand;
	LPCSTR		lpStr;
	LPCSTR		lpStart = NULL;
	char		szBuf [ 256 ];
	CString		strCmd;
	CStringList	CmdList;
	POSITION	pos;
	DDEACK		Ack;

	if ( m_bDDERunning )
	{
		// Parse commands
		lpszCommand = (LPCSTR) GlobalLock ( hMem );
		lpStr = lpszCommand;
		while ( (c=lpStr[0]) != '\0' )
		{
			if ( lpStart == NULL )
			{
				if ( c == '[' )
				{
					lpStr ++;
					if ( lpStr [ 0 ] != '[' )
						lpStart = lpStr;
				}
			}
			else
			{
				if ( c == ']' )
				{
					if ( lpStr [ 1 ] == ']' )
						lpStr ++;
					else
					{
						i = lpStr - lpStart;
						strncpy ( szBuf, lpStart, i );
						szBuf [ i ] = '\0';
						CmdList.AddTail ( szBuf );
						bOk = TRUE;
						lpStart = NULL;
					}
				}
			}
			lpStr ++;
		}
		GlobalUnlock ( hMem );
	}

	if ( bOk )
	{
		BOOL	bAckSent = FALSE;

		pos = CmdList.GetHeadPosition ();
		while (pos)
		{
			strCmd = CmdList.GetNext ( pos );
			bOk &= DdeCmdExec ( strCmd, (pos == NULL), hWndClient, lParam, & bAckSent );
		}

		if ( ! bAckSent )
		{
			Ack.bAppReturnCode = 0;
			Ack.fAck = bOk;
			Ack.fBusy = FALSE;

			lParam = PackDDElParam ( WM_DDE_ACK, *(UINT*)(&Ack), lParam );
			::PostMessage ( hWndClient, WM_DDE_ACK, (WPARAM) m_hWnd, lParam );
		}

		return lResult;
	}
	else
		return -1;
}

LRESULT CMultiFrame::OnDDERequest(WPARAM wParam, LPARAM lParam)
{
	int			i, j;
	int			nNbParms = 0;
	int			nFormat;
	BOOL		bOk = FALSE;
	BOOL		bIsMine = FALSE;
	LRESULT		lResult = 0;
	HWND		hWndClient = (HWND) wParam;
	WORD		wFormat = LOWORD(lParam);
	ATOM		nAtomData = (ATOM) HIWORD(lParam);
	char		szBuf [ 256 ];
	CString		strData;
	CString		strTemp;
	CString		strCmd;
	CStringList	CmdParams;
	CColor		ReqColor=noDataColor;
	DDEACK		Ack;
	HGLOBAL		hMem = NULL;
	DDEDATA *	pDdeData;

	if ( wFormat == CF_TEXT && m_bDDERunning )
	{
		bOk = TRUE;
		bIsMine = TRUE;

		GlobalGetAtomName ( nAtomData, szBuf, sizeof ( szBuf ) );
		DdeParseString ( szBuf, strCmd, CmdParams ); 

		if ( _stricmp ( (LPCSTR) strCmd, "MeasureSizes" ) == 0 )
		{
			// Array sizes
			strData.Format("%d,%d,%d,%d", GetDocument()->GetMeasure()->GetGrayScaleSize()-1, GetDocument()->GetMeasure()->GetNearBlackScaleSize()-1, GetDocument()->GetMeasure()->GetNearWhiteScaleSize()-1, GetDocument()->GetMeasure()->GetSaturationSize()-1 );
		}
		else
		{
			// Color values
			nNbParms = 1;
			if ( _stricmp ( (LPCSTR) strCmd, "red" ) == 0 )
				ReqColor = GetDocument()->GetMeasure()->GetPrimary(0);
			else if ( _stricmp ( (LPCSTR) strCmd, "green" ) == 0 )
				ReqColor = GetDocument()->GetMeasure()->GetPrimary(1);
			else if ( _stricmp ( (LPCSTR) strCmd, "blue" ) == 0 )
				ReqColor = GetDocument()->GetMeasure()->GetPrimary(2);
			else if ( _stricmp ( (LPCSTR) strCmd, "yellow" ) == 0 )
				ReqColor = GetDocument()->GetMeasure()->GetSecondary(0);
			else if ( _stricmp ( (LPCSTR) strCmd, "cyan" ) == 0 )
				ReqColor = GetDocument()->GetMeasure()->GetSecondary(1);
			else if ( _stricmp ( (LPCSTR) strCmd, "magenta" ) == 0 )
				ReqColor = GetDocument()->GetMeasure()->GetSecondary(2);
			else if ( _stricmp ( (LPCSTR) strCmd, "free" ) == 0 )
			{
				if ( GetDocument()->GetMeasure()->GetMeasurementsSize() > 0 )
					ReqColor = GetDocument()->GetMeasure()->GetMeasurement(0);
				else
					ReqColor = noDataColor;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "IRE" ) == 0 )
			{
				nNbParms = 2;
				if ( ! CmdParams.IsEmpty () )
				{
					strTemp = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strTemp );
					if ( i < 0 )
						i = 0;
					else if ( i > 100 )
						i = 100;
					j = GetDocument()->GetMeasure()->GetGrayScaleSize() - 1;

					ReqColor = GetDocument()->GetMeasure()->GetGray(i*j/100);
				}
				else
				{
					bOk = FALSE;
				}
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "NearBlack" ) == 0 )
			{
				nNbParms = 2;
				if ( ! CmdParams.IsEmpty () )
				{
					strTemp = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strTemp );
					j = GetDocument()->GetMeasure()->GetNearBlackScaleSize() - 1;
					if ( i < 0 )
						i = 0;
					else if ( i > j )
						i = j;
					ReqColor = GetDocument()->GetMeasure()->GetNearBlack(i);
				}
				else
				{
					bOk = FALSE;
				}
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "NearWhite" ) == 0 )
			{
				nNbParms = 2;
				if ( ! CmdParams.IsEmpty () )
				{
					strTemp = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strTemp );
					j = GetDocument()->GetMeasure()->GetNearWhiteScaleSize() - 1;
					if ( i < 100-j )
						i = 100-j;
					else if ( i > 100 )
						i = 100;
					ReqColor = GetDocument()->GetMeasure()->GetNearWhite(i-100+j);
				}
				else
				{
					bOk = FALSE;
				}
			}
			else
			{
				// Not a recognized request
				bOk = FALSE;
				bIsMine = FALSE;
			}

			if ( bOk )
			{
				// Color format
				if ( CmdParams.GetCount () <= nNbParms )
				{
					if ( CmdParams.GetCount () == nNbParms )
					{
						strTemp = CmdParams.GetTail ();

						if ( _stricmp ( (LPCSTR) strTemp, "XYZ" ) == 0 )
							nFormat = HCFR_XYZ_VIEW;
						else if ( _stricmp ( (LPCSTR) strTemp, "SENSOR" ) == 0 )
							nFormat = HCFR_SENSORRGB_VIEW;
						else if ( _stricmp ( (LPCSTR) strTemp, "RGB" ) == 0 )
							nFormat = HCFR_RGB_VIEW;
						else if ( _stricmp ( (LPCSTR) strTemp, "xyz2" ) == 0 )
							nFormat = HCFR_xyz2_VIEW;
						else if ( _stricmp ( (LPCSTR) strTemp, "xyY" ) == 0 )
							nFormat = HCFR_xyY_VIEW;
						else if ( _stricmp ( (LPCSTR) strTemp, "SPECTRUM" ) == 0 )
							nFormat = -1;
						else
							bOk = FALSE;
					}
					else
					{
						// Use default
						nFormat = HCFR_XYZ_VIEW;
					}
				}
			}

			if ( bOk )
			{
				if ( !ReqColor.isValid() )
				{
					strData = "-1,-1,-1";
				}
				else
				{
					switch(nFormat)
					{
						case -1:
							if ( ReqColor.HasSpectrum () )
							{
								CString strTmp;
								CSpectrum Spectrum = ReqColor.GetSpectrum ();

								strData.Format("%d,%d,%d,%d|",Spectrum.GetRows(),Spectrum.m_WaveLengthMin,Spectrum.m_WaveLengthMax,Spectrum.m_BandWidth);
								for ( int i = 0; i < Spectrum.GetRows(); i ++ )
								{
									if ( i > 0 )
										strData+=",";
									strTmp.Format("%.4f",Spectrum[i]);
									strData+=strTmp;
								}
							}
							else
							{
								strData="0,0,0,0|";
							}
							break;
						case HCFR_XYZ_VIEW:
						case HCFR_SENSORRGB_VIEW:
                            {
							    ColorXYZ aColor=ReqColor.GetXYZValue();
							    strData.Format("%.3f,%.3f,%.3f",aColor[0],aColor[1],aColor[2]);
                            }
							break;
						case HCFR_RGB_VIEW:
                            {
							    ColorRGB aColor=ReqColor.GetRGBValue(GetColorReference());
							    strData.Format("%.3f,%.3f,%.3f",aColor[0],aColor[1],aColor[2]);
                            }
							break;
						case HCFR_xyz2_VIEW:
                            {
							    Colorxyz aColor=ReqColor.GetxyzValue();
							    strData.Format("%.3f,%.3f,%.3f",aColor[0],aColor[1],aColor[2]);
                            }
							break;
						case HCFR_xyY_VIEW:
                            {
							    ColorxyY aColor=ReqColor.GetxyYValue();
							    strData.Format("%.3f,%.3f,%.3f",aColor[0],aColor[1],aColor[2]);
                            }
							break;
					}
				}
			}
		}
	}

	if ( bIsMine )
	{
		if ( bOk )
		{
			hMem = GlobalAlloc ( GHND | GMEM_DDESHARE, sizeof ( DDEDATA ) + strData.GetLength () + 1 );
			pDdeData = (DDEDATA*) GlobalLock ( hMem );

			pDdeData -> cfFormat = CF_TEXT;
			pDdeData -> fAckReq = 0;
			pDdeData -> fRelease = 1;
			pDdeData -> fResponse = 1;
			strcpy ( (LPSTR) pDdeData -> Value, (LPCSTR) strData );

			GlobalUnlock ( hMem );

			lParam = PackDDElParam ( WM_DDE_DATA, (UINT) hMem, nAtomData );
			::PostMessage ( hWndClient, WM_DDE_DATA, (WPARAM) m_hWnd, lParam );
		}
		else
		{
			Ack.bAppReturnCode = 0;
			Ack.fAck = FALSE;
			Ack.fBusy = FALSE;

			lParam = PackDDElParam ( WM_DDE_ACK, *(UINT*)(&Ack), nAtomData );
			::PostMessage ( hWndClient, WM_DDE_ACK, (WPARAM) m_hWnd, lParam );
		}
	}
	else
	{
		lResult = -1;
	}

	return lResult;
}

LRESULT CMultiFrame::OnDDEPoke(WPARAM wParam, LPARAM lParam)
{
	int			i, j;
	int			nNbParms = 0;
	int			nFormat;
	double		a = 0.0, b = 0.0, c = 0.0;
	BOOL		bOk = FALSE;
	BOOL		bIsMine = FALSE;
	BOOL		bRelease = FALSE;
	LRESULT		lResult = 0;
	LPARAM		lHint = UPD_EVERYTHING;
	HWND		hWndClient = (HWND) wParam;
	HGLOBAL		hMem;
	ATOM		nAtom;
	DDEPOKE *	lpDDEPoke;
	DDEACK		Ack;
	char		szBuf [ 256 ];
	CString		strTemp;
	CString		strCmd;
	CStringList	CmdParams;
	CColor		ReceivedColor=noDataColor;

	UnpackDDElParam ( WM_DDE_POKE, lParam, (unsigned int *) & hMem, (unsigned int *) & nAtom );
	FreeDDElParam ( WM_DDE_POKE, lParam );

	lpDDEPoke = (DDEPOKE *) GlobalLock ( hMem ); 
	
	if ( lpDDEPoke && m_bDDERunning )
	{
		if ( lpDDEPoke -> cfFormat == CF_TEXT )
		{	
			bOk = TRUE;

			GlobalGetAtomName ( nAtom, szBuf, sizeof ( szBuf ) );
			DdeParseString ( szBuf, strCmd, CmdParams ); 

			if ( ! CmdParams.IsEmpty () )
			{
				bIsMine = TRUE;
				strTemp = CmdParams.GetTail ();

				// Default to "XYZ"
				nFormat = HCFR_XYZ_VIEW;

				if ( _stricmp ( (LPCSTR) strTemp, "SENSOR" ) == 0 )
					nFormat = HCFR_SENSORRGB_VIEW;
				else if ( _stricmp ( (LPCSTR) strTemp, "RGB" ) == 0 )
					nFormat = HCFR_RGB_VIEW;
				else if ( _stricmp ( (LPCSTR) strTemp, "xyY" ) == 0 )
					nFormat = HCFR_xyY_VIEW;
			}

			sscanf ( (LPCSTR) lpDDEPoke -> Value, "%lf,%lf,%lf", & a, & b, & c );

			if ( a >= 0.0 && b >= 0.0 && c >= 0.0 )
			{

				// Convert back color data to XYZ
				switch(nFormat)
				{
					case HCFR_XYZ_VIEW:
						ReceivedColor.SetXYZValue(ColorXYZ(a, b, c));
						break;
					case HCFR_SENSORRGB_VIEW:
						ReceivedColor = ColorXYZ(a, b, c);
						break;
					case HCFR_RGB_VIEW:
						ReceivedColor.SetRGBValue(ColorRGB(a, b, c), GetColorReference());
						break;
					case HCFR_xyY_VIEW:
						ReceivedColor.SetxyYValue(a, b, c);
						break;
				}
			}
			
			if ( _stricmp ( (LPCSTR) strCmd, "red" ) == 0 )
			{
				GetDocument()->GetMeasure()->SetRedPrimary(ReceivedColor);
				lHint = UPD_PRIMARIES;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "green" ) == 0 )
			{
				GetDocument()->GetMeasure()->SetGreenPrimary(ReceivedColor);
				lHint = UPD_PRIMARIES;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "blue" ) == 0 )
			{
				GetDocument()->GetMeasure()->SetBluePrimary(ReceivedColor);
				lHint = UPD_PRIMARIES;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "yellow" ) == 0 )
			{
				GetDocument()->GetMeasure()->SetYellowSecondary(ReceivedColor);
				lHint = UPD_SECONDARIES;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "cyan" ) == 0 )
			{
				GetDocument()->GetMeasure()->SetCyanSecondary(ReceivedColor);
				lHint = UPD_SECONDARIES;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "magenta" ) == 0 )
			{
				GetDocument()->GetMeasure()->SetMagentaSecondary(ReceivedColor);
				lHint = UPD_SECONDARIES;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "free" ) == 0 )
			{
				GetDocument()->GetMeasure()->InsertMeasurement(0,ReceivedColor);
				lHint = UPD_FREEMEASURES;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "IRE" ) == 0 )
			{
				lHint = UPD_GRAYSCALE;
				if ( ! CmdParams.IsEmpty () )
				{
					strTemp = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strTemp );
					if ( i < 0 )
						i = 0;
					else if ( i > 100 )
						i = 100;
					j = GetDocument()->GetMeasure()->GetGrayScaleSize() - 1;

					GetDocument()->GetMeasure()->SetGray(i*j/100, ReceivedColor);
				}
				else
				{
					bOk = FALSE;
				}
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "NearBlack" ) == 0 )
			{
				lHint = UPD_NEARBLACK;
				if ( ! CmdParams.IsEmpty () )
				{
					strTemp = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strTemp );
					j = GetDocument()->GetMeasure()->GetNearBlackScaleSize() - 1;
					if ( i < 0 )
						i = 0;
					else if ( i > j )
						i = j;

					GetDocument()->GetMeasure()->SetNearBlack(i, ReceivedColor);
				}
				else
				{
					bOk = FALSE;
				}
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "NearWhite" ) == 0 )
			{
				lHint = UPD_NEARWHITE;
				if ( ! CmdParams.IsEmpty () )
				{
					strTemp = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strTemp );
					j = GetDocument()->GetMeasure()->GetNearWhiteScaleSize() - 1;
					if ( i < 100-j )
						i = 100-j;
					else if ( i > 100 )
						i = 100;

					GetDocument()->GetMeasure()->SetNearWhite(i-100+j, ReceivedColor);
				}
				else
				{
					bOk = FALSE;
				}
			}
			else
			{
				// Not a recognized request
				bOk = FALSE;
				bIsMine = FALSE;
			}
		}

		bRelease = lpDDEPoke -> fRelease;
		GlobalUnlock ( hMem );
	}
	else
	{
		// Invalid parameters or DDE not initialized
		bOk = FALSE;
		bIsMine = FALSE;
	}

	if ( bIsMine )
	{
		if ( bOk )
		{
			if ( bRelease )
				GlobalFree ( hMem );

			// Update document and views
			GetDocument()->SetModifiedFlag(GetDocument()->m_measure.IsModified());
			GetDocument()->UpdateAllViews(NULL, lHint);

			GetDocument () -> SetSelectedColor ( noDataColor );

			Ack.bAppReturnCode = 0;
			Ack.fAck = TRUE;
			Ack.fBusy = FALSE;

			lParam = PackDDElParam ( WM_DDE_ACK, *(UINT*)(&Ack), nAtom );
			::PostMessage ( hWndClient, WM_DDE_ACK, (WPARAM) m_hWnd, lParam );
		}
		else
		{
			Ack.bAppReturnCode = 0;
			Ack.fAck = FALSE;
			Ack.fBusy = FALSE;

			lParam = PackDDElParam ( WM_DDE_ACK, *(UINT*)(&Ack), nAtom );
			::PostMessage ( hWndClient, WM_DDE_ACK, (WPARAM) m_hWnd, lParam );
		}
	}
	else
	{
		lResult = -1;
	}

	return lResult;
}

LRESULT CMultiFrame::OnDDETerminate(WPARAM wParam, LPARAM lParam)
{
	HWND		hWndClient = (HWND) wParam;

	if ( m_bDDERunning )
	{
		m_bDDERunning = FALSE;
		::PostMessage ( hWndClient, WM_DDE_TERMINATE, (WPARAM) m_hWnd, 0L );
		return 0;
	}
	else
	{
		return -1;
	}
}

#define CLRCAT_NONE			0
#define CLRCAT_IRE			1
#define CLRCAT_NEARBLACK	2
#define CLRCAT_NEARWHITE	3
#define CLRCAT_PRIMARY		4
#define CLRCAT_SECONDARY	5
#define CLRCAT_FREE			6
#define CLRCAT_SAT_RED		7
#define CLRCAT_SAT_GREEN	8
#define CLRCAT_SAT_BLUE		9
#define CLRCAT_SAT_YELLOW	10
#define CLRCAT_SAT_CYAN		11
#define CLRCAT_SAT_MAGENTA	12

BOOL CMultiFrame::DdeCmdExec ( CString & strCommand, BOOL bCanSendAckMsg, HWND hWndClient, LPARAM lParam, LPBOOL pbAckMsgSent )
{
	int			i, j;
	int			ire = 0;
	int			r = 0, g = 0, b = 0;
	int			nCount;
	int			nRow, nCol;
	int			nCategory = CLRCAT_NONE;
	int			nClrIndex = 0;
	BOOL		bOk = FALSE;
	BOOL		bDisplay = FALSE;
	BOOL		bNoWait = FALSE;
	BOOL		bUpdateName = FALSE;
	BOOL		bSensorCalibrated = GetDocument() -> m_pSensor -> IsCalibrated ();
	LPARAM		lHint = UPD_EVERYTHING;
	Matrix		SensorMatrix ( 0.0, 3, 3 );
	Matrix		WhiteMatrix ( 0.0, 3, 1 );
	CString		str;
	CString		strCmd;
	CString		strParam;
	CStringList	CmdParams;
	POSITION	pos;
	CColor		MeasuredColor = noDataColor;
	COLORREF	clrref;	

	DdeParseString ( (LPCSTR) strCommand, strCmd, CmdParams );

	if ( _stricmp ( (LPCSTR) strCmd, "Save" ) == 0 )
	{
		if ( CmdParams.IsEmpty () )
		{
			// Save using current name
			bOk = GetDocument () -> DoSave ( NULL, TRUE );
		}
		else
		{
			// Save using name in parameter
			if ( CmdParams.GetCount () == 2 )
			{
				strParam = CmdParams.GetTail ();
				if ( _strnicmp ( (LPCSTR) strParam, "Yes", 3 ) == 0 )
					bUpdateName = TRUE;
			}
			strParam = CmdParams.GetHead ();
			bOk = GetDocument () -> DoSave ( (LPCSTR) strParam, bUpdateName );
		}
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "Close" ) == 0 )
	{
		GetParent () -> PostMessage ( WM_CLOSE );
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "SetSensorMatrix" ) == 0 )
	{
		nCount = CmdParams.GetCount ();
		
		if ( nCount == 9 )
		{
			bOk = TRUE;

			// Retrieve Matrix
			pos = CmdParams.GetHeadPosition ();
			for ( nRow = 0; nRow < 3 && pos ; nRow ++ )
			{
				for ( nCol = 0; nCol < 3 && pos ; nCol ++ )
				{
					strParam = CmdParams.GetNext ( pos );
					SensorMatrix [nRow] [nCol] = atof ( (LPCSTR) strParam );
				}
			}

			if ( nRow == 3 && nCol == 3 && pos == NULL )
			{
				GetDocument () -> m_pSensor -> SetSensorMatrix ( SensorMatrix );
			}
			else
			{
				bOk = FALSE;
			}
		}
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "SetPrimariesChromacities" ) == 0 )
	{
		nCount = CmdParams.GetCount ();
		
		if ( nCount == 9 )
		{
			bOk = TRUE;

			// Retrieve Matrix
			pos = CmdParams.GetHeadPosition ();
			for ( nRow = 0; nRow < 3 && pos ; nRow ++ )
			{
				for ( nCol = 0; nCol < 3 && pos ; nCol ++ )
				{
					strParam = CmdParams.GetNext ( pos );
					SensorMatrix [nRow] [nCol] = atof ( (LPCSTR) strParam );
				}
			}

			if ( nRow == 3 && nCol == 3 && pos == NULL )
			{
				( ( COneDeviceSensor * ) GetDocument () -> m_pSensor ) -> m_primariesChromacities = SensorMatrix;
			}
			else
			{
				bOk = FALSE;
			}
		}
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "SetWhiteChromacity" ) == 0 )
	{
		nCount = CmdParams.GetCount ();
		
		if ( nCount == 3 )
		{
			bOk = TRUE;

			// Retrieve Matrix
			pos = CmdParams.GetHeadPosition ();
			for ( nRow = 0; nRow < 3 && pos ; nRow ++ )
			{
				strParam = CmdParams.GetNext ( pos );
				WhiteMatrix [nRow] [0] = atof ( (LPCSTR) strParam );
			}

			if ( nRow == 3 && pos == NULL )
			{
				( ( COneDeviceSensor * ) GetDocument () -> m_pSensor ) -> m_whiteChromacity = WhiteMatrix;
			}
			else
			{
				bOk = FALSE;
			}
		}
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "SetInfoText" ) == 0 )
	{
		nCount = CmdParams.GetCount ();
		
		if ( nCount == 1 )
		{
			bOk = TRUE;
			CString str = CmdParams.GetHead ();
			GetDocument()->GetMeasure()->SetInfoString(str);
			GetDlgItem ( IDC_INFOS_EDIT ) -> SetWindowText ( (LPCSTR) str );
		}
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "SetHCFRSensorParams" ) == 0 )
	{
		str.LoadString(IDS_KISENSOR_NAME);
		if ( GetDocument() -> m_pSensor -> GetName () == str )
		{
			CKiSensor * pSensor = ( CKiSensor * ) GetDocument() -> m_pSensor;
			nCount = CmdParams.GetCount ();
			
			if ( nCount > 0 && nCount < 8 )
			{
				bOk = TRUE;

				pos = CmdParams.GetHeadPosition ();
				
				// Retrieve COM port
				strParam = CmdParams.GetNext ( pos );
				pSensor -> m_comPort = strParam;

				if ( pos )
				{
					// Debug mode
					strParam = CmdParams.GetNext ( pos );
					pSensor -> m_debugMode = ( _strnicmp ( (LPCSTR) strParam, "Yes", 3 ) == 0 ? TRUE : FALSE );
				}

				if ( pos )
				{
					// Timeout
					strParam = CmdParams.GetNext ( pos );
					pSensor -> m_timeoutMesure = atoi ( (LPCSTR) strParam );
				}

				if ( pos )
				{
					// Measure white
					strParam = CmdParams.GetNext ( pos );
					pSensor -> m_bMeasureWhite = ( _strnicmp ( (LPCSTR) strParam, "Yes", 3 ) == 0 ? TRUE : FALSE );
				}

				if ( pos )
				{
					// Sensor used
					strParam = CmdParams.GetNext ( pos );
					pSensor -> m_nSensorsUsed = atoi ( (LPCSTR) strParam );
					if ( pSensor -> m_nSensorsUsed < 0 || pSensor -> m_nSensorsUsed > 2 )
						pSensor -> m_nSensorsUsed = 0;
				}

				if ( pos )
				{
					// Interlace mode
					strParam = CmdParams.GetNext ( pos );
					pSensor -> m_nInterlaceMode = atoi ( (LPCSTR) strParam );
					if ( pSensor -> m_nInterlaceMode < 0 || pSensor -> m_nInterlaceMode > 2 )
						pSensor -> m_nInterlaceMode = 0;
				}

				if ( pos )
				{
					// Fast measure
					strParam = CmdParams.GetNext ( pos );
					pSensor -> m_bFastMeasure = ( _strnicmp ( (LPCSTR) strParam, "Yes", 3 ) == 0 ? TRUE : FALSE );
				}
			}
		}
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "SetMeasureSizes" ) == 0 )
	{
		nCount = CmdParams.GetCount ();
		
		if ( nCount >= 0 && nCount <= 4 )
		{
			bOk = TRUE;

			pos = CmdParams.GetHeadPosition ();
			
			// Retrieve Gray Scale size
			if ( pos )
			{
				strParam = CmdParams.GetNext ( pos );
				i = atoi ( (LPCSTR) strParam );
				if ( i <= 0 || i > 255 )
					i = 10;
			}
			else
			{
				// Default
				i = 10;
			}
			GetDocument()->GetMeasure()->SetGrayScaleSize(i+1);

			// Near black size
			if ( pos )
			{
				strParam = CmdParams.GetNext ( pos );
				i = atoi ( (LPCSTR) strParam );
				if ( i <= 0 || i > 20 )
					i = 4;
			}
			else
			{
				// Default
				i = 4;
			}
			GetDocument()->GetMeasure()->SetNearBlackScaleSize(i+1);

			// Near white size
			if ( pos )
			{
				strParam = CmdParams.GetNext ( pos );
				i = atoi ( (LPCSTR) strParam );
				if ( i <= 0 || i > 20 )
					i = 4;
			}
			else
			{
				// Default
				i = 4;
			}
			GetDocument()->GetMeasure()->SetNearWhiteScaleSize(i+1);

			// Saturation size
			if ( pos )
			{
				strParam = CmdParams.GetNext ( pos );
				i = atoi ( (LPCSTR) strParam );
				if ( i <= 0 || i > 20 )
					i = 4;
			}
			else
			{
				// Default
				i = 4;
			}
			GetDocument()->GetMeasure()->SetSaturationSize(i+1);

			// Update document and views
			GetDocument()->SetModifiedFlag(GetDocument()->m_measure.IsModified());
			GetDocument()->UpdateAllViews(NULL, UPD_ARRAYSIZES);

			GetDocument () -> SetSelectedColor ( noDataColor );
		}
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "SaveCalibrationFile" ) == 0 && bSensorCalibrated )
	{
		nCount = CmdParams.GetCount ();
		
		if ( nCount == 1 )
		{
			strParam = CmdParams.GetHead ();

			COneDeviceSensor * pSensor = ( COneDeviceSensor * ) GetDocument() -> m_pSensor;

			if (pSensor -> GetStandardSubDir () [ 0 ] != '\0' )
			{
				bOk = TRUE;
				
				LPCSTR lpStr = strrchr ( (LPCSTR) strParam, '.' );
				if ( lpStr )
				{
					if ( _stricmp ( lpStr, ".thc" ) != 0 )
						bOk = FALSE;
				}
				else
				{
					strParam += ".thc";
				}

				if ( bOk )
				{
					if ( strParam.Find ( '\\' ) < 0 )
					{
						// Build full calibration file name
						str = GetConfig () -> m_ApplicationPath;
						str += pSensor -> GetStandardSubDir ();
						
						GetConfig () -> EnsurePathExists ( str );
						
						str += '\\';
						str += strParam;
					}
					else
					{
						// Name already qualified with a path
						str = strParam;
					}

					CFile ThcFile ( str, CFile::modeCreate | CFile::modeWrite );
					CArchive ar ( & ThcFile, CArchive::store );
					pSensor -> Serialize(ar);
				}
			}
		}
	}
	else if ( _stricmp ( (LPCSTR) strCmd, "measure" ) == 0 && bSensorCalibrated )
	{
		if ( CmdParams.IsEmpty () )
		{
			// Default values
			bOk = TRUE;
			bDisplay = FALSE;
			bNoWait = FALSE;
			nCategory = CLRCAT_FREE;
			nClrIndex = 0;
		}
		else
		{
			nCount = CmdParams.GetCount ();
			
			if ( nCount >= 1 && nCount <= 3 )
			{
				bOk = TRUE;

				pos = CmdParams.GetTailPosition ();
				if ( nCount == 3 )
				{
					strParam = CmdParams.GetPrev ( pos );

					if ( _stricmp ( (LPCSTR) strParam, "no_wait" ) == 0 )
						bNoWait = TRUE;
					else if ( _stricmp ( (LPCSTR) strParam, "wait" ) == 0 || strParam.IsEmpty () )
						bNoWait = FALSE;
					else
						bOk = FALSE;
				}
				if ( nCount >= 2 )
				{
					// Second param is "display" or "no_display"
					strParam = CmdParams.GetPrev ( pos );

					if ( _stricmp ( (LPCSTR) strParam, "display" ) == 0 )
						bDisplay = TRUE;
					else if ( _stricmp ( (LPCSTR) strParam, "no_display" ) == 0 || strParam.IsEmpty () )
						bDisplay = FALSE;
					else
						bOk = FALSE;
				}
				else
				{
					// No display by default
					bDisplay = FALSE;
				}
			}

			if ( bOk )
			{
				// First param is the color to measure (must be analyzed)
				strParam = CmdParams.GetHead ();
				DdeParseString ( (LPCSTR) strParam, strCmd, CmdParams );

				nCount = CmdParams.GetCount ();

				if ( _stricmp ( (LPCSTR) strCmd, "red" ) == 0 && nCount == 0 )
				{
					lHint = UPD_PRIMARIES;
					nCategory = CLRCAT_PRIMARY;
					nClrIndex = 0;
					clrref = RGB(255,0,0);
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "green" ) == 0 && nCount == 0 )
				{
					lHint = UPD_PRIMARIES;
					nCategory = CLRCAT_PRIMARY;
					nClrIndex = 1;
					clrref = RGB(0,255,0);
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "blue" ) == 0 && nCount == 0 )
				{
					lHint = UPD_PRIMARIES;
					nCategory = CLRCAT_PRIMARY;
					nClrIndex = 2;
					clrref = RGB(0,0,255);
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "yellow" ) == 0 && nCount == 0 )
				{
					lHint = UPD_SECONDARIES;
					nCategory = CLRCAT_SECONDARY;
					nClrIndex = 0;
					clrref = RGB(255,255,0);
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "cyan" ) == 0 && nCount == 0 )
				{
					lHint = UPD_SECONDARIES;
					nCategory = CLRCAT_SECONDARY;
					nClrIndex = 1;
					clrref = RGB(0,255,255);
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "magenta" ) == 0 && nCount == 0 )
				{
					lHint = UPD_SECONDARIES;
					nCategory = CLRCAT_SECONDARY;
					nClrIndex = 2;
					clrref = RGB(255,0,255);
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "IRE" ) == 0 && nCount == 1 )
				{
					lHint = UPD_GRAYSCALE;
					nCategory = CLRCAT_IRE;
					
					strParam = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strParam );
					if ( i < 0 )
						i = 0;
					else if ( i > 100 )
						i = 100;
					
					ire = i;
					
					j = GetDocument()->GetMeasure()->GetGrayScaleSize() - 1;

					nClrIndex = i*j/100;
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "NearBlack" ) == 0 && nCount == 1 )
				{
					lHint = UPD_NEARBLACK;
					nCategory = CLRCAT_NEARBLACK;
					
					strParam = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strParam );
					j = GetDocument()->GetMeasure()->GetNearBlackScaleSize() - 1;
					if ( i < 0 )
						i = 0;
					else if ( i > j )
						i = j;
					
					ire = i;
					nClrIndex = i;
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "NearWhite" ) == 0 && nCount == 1 )
				{
					lHint = UPD_NEARWHITE;
					nCategory = CLRCAT_NEARWHITE;
					
					strParam = CmdParams.GetHead ();
					i = atoi ( (LPCSTR) strParam );
					j = GetDocument()->GetMeasure()->GetNearWhiteScaleSize() - 1;
					if ( i < 100-j )
						i = 100-j;
					else if ( i > 100 )
						i = 100;
					
					ire = i;
					nClrIndex = i-100+j;
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "saturation" ) == 0 && nCount == 2 )
				{
					strParam = CmdParams.GetHead ();
					if ( _stricmp ( (LPCSTR) strParam, "red" ) == 0 )
					{
						lHint = UPD_REDSAT;
						nCategory = CLRCAT_SAT_RED;
					}
					else if ( _stricmp ( (LPCSTR) strParam, "green" ) == 0 )
					{
						lHint = UPD_GREENSAT;
						nCategory = CLRCAT_SAT_GREEN;
					}
					else if ( _stricmp ( (LPCSTR) strParam, "blue" ) == 0 )
					{
						lHint = UPD_BLUESAT;
						nCategory = CLRCAT_SAT_BLUE;
					}
					else if ( _stricmp ( (LPCSTR) strParam, "yellow" ) == 0 )
					{
						lHint = UPD_YELLOWSAT;
						nCategory = CLRCAT_SAT_YELLOW;
					}
					else if ( _stricmp ( (LPCSTR) strParam, "cyan" ) == 0 )
					{
						lHint = UPD_CYANSAT;
						nCategory = CLRCAT_SAT_CYAN;
					}
					else if ( _stricmp ( (LPCSTR) strParam, "magenta" ) == 0 )
					{
						lHint = UPD_MAGENTASAT;
						nCategory = CLRCAT_SAT_MAGENTA;
					}
					
					strParam = CmdParams.GetTail ();
					i = atoi ( (LPCSTR) strParam );
					if ( i < 0 )
						i = 0;
					else if ( i > 100 )
						i = 100;

					ire = i;

					j = GetDocument()->GetMeasure()->GetSaturationSize() - 1;

					nClrIndex = i*j/100;
				}
				else if ( _stricmp ( (LPCSTR) strCmd, "free" ) == 0 && ( nCount == 0 || nCount == 3 ) )
				{
					lHint = UPD_FREEMEASURES;
					nCategory = CLRCAT_FREE;
					nClrIndex = 0;

					r = g = b = 0;

					if ( nCount == 3 )
					{
						pos = CmdParams.GetHeadPosition ();
						
						strParam = CmdParams.GetNext ( pos );
						r = atoi ( (LPCSTR) strParam );
						if ( r < 0 )
							r = 0;
						else if ( r > 255 )
							r = 255;
						
						strParam = CmdParams.GetNext ( pos );
						g = atoi ( (LPCSTR) strParam );
						if ( g < 0 )
							g = 0;
						else if ( g > 255 )
							g = 255;

						strParam = CmdParams.GetNext ( pos );
						b = atoi ( (LPCSTR) strParam );
						if ( b < 0 )
							b = 0;
						else if ( b > 255 )
							b = 255;
					}

					clrref = RGB(r,g,b);
				}
				else
				{
					// Not a recognized color code
					bOk = FALSE;
				}
			}
		}

		if ( bOk )
		{
			// Perform measure
			if ( bDisplay )
			{
				if ( GetDocument () -> m_pGenerator -> Init() != TRUE )
				{
					bOk = FALSE;
				}
			}

			if ( bOk )
			{
				if ( GetDocument () -> m_pSensor -> Init(FALSE) != TRUE )
				{
					if ( bDisplay )
						GetDocument () -> m_pGenerator->Release();
					bOk = FALSE;
				}
			}

			if ( bOk )
			{
				if ( bCanSendAckMsg && bNoWait )
				{
					// Send DDE acknowledgement message and reply immediately
					DDEACK		Ack;
					
					Ack.bAppReturnCode = 0;
					Ack.fAck = bOk;
					Ack.fBusy = FALSE;

					lParam = PackDDElParam ( WM_DDE_ACK, *(UINT*)(&Ack), lParam );
					::PostMessage ( hWndClient, WM_DDE_ACK, (WPARAM) m_hWnd, lParam );

					// Replay to message to allow calling application to continue
					ReplyMessage ( 0 );

					// Inform caller not to send a second acknoledgement
					* pbAckMsgSent = TRUE;
				}

				switch ( nCategory )
				{
					case CLRCAT_PRIMARY:
					case CLRCAT_SECONDARY:
					case CLRCAT_FREE:
						 if ( bDisplay )
						 {
							if ( GetDocument()->m_pGenerator->DisplayRGBColor( ColorRGBDisplay(clrref),CGenerator::MT_UNKNOWN) )
								GetDocument()->m_measure.WaitForDynamicIris ( TRUE );
							else
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							MeasuredColor = GetDocument()->m_pSensor->MeasureColor ( ColorRGBDisplay(clrref) );
							if ( ! GetDocument()->m_pSensor->IsMeasureValid() )
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							switch ( nCategory )
							{
								case CLRCAT_PRIMARY:
									GetDocument()->m_measure.SetPrimary ( nClrIndex, MeasuredColor);
									break;

								case CLRCAT_SECONDARY:
									GetDocument()->m_measure.SetSecondary ( nClrIndex, MeasuredColor);
									break;

								case CLRCAT_FREE:
									GetDocument()->m_measure.SetMeasurements ( nClrIndex, MeasuredColor);
									break;
							}
						 }
						 break;

					case CLRCAT_IRE:
						 if ( bDisplay )
						 {
							if ( GetDocument()->m_pGenerator->DisplayGray(ire,CGenerator::MT_IRE) )
								GetDocument()->m_measure.WaitForDynamicIris ( TRUE );
							else
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							MeasuredColor = GetDocument()->m_pSensor->MeasureGray (ire);
							if ( ! GetDocument()->m_pSensor->IsMeasureValid() )
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							GetDocument()->m_measure.SetGray ( nClrIndex, MeasuredColor);
						 }
						 break;

					case CLRCAT_NEARBLACK:
						 if ( bDisplay )
						 {
							if ( GetDocument()->m_pGenerator->DisplayGray(ire,CGenerator::MT_NEARBLACK) )
								GetDocument()->m_measure.WaitForDynamicIris ( TRUE );
							else
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							MeasuredColor = GetDocument()->m_pSensor->MeasureGray (ire);
							if ( ! GetDocument()->m_pSensor->IsMeasureValid() )
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							GetDocument()->m_measure.SetNearBlack ( nClrIndex, MeasuredColor);
						 }
						 break;

					case CLRCAT_NEARWHITE:
						 if ( bDisplay )
						 {
							if ( GetDocument()->m_pGenerator->DisplayGray(ire,CGenerator::MT_NEARWHITE) )
								GetDocument()->m_measure.WaitForDynamicIris ( TRUE );
							else
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							MeasuredColor = GetDocument()->m_pSensor->MeasureGray (ire);
							if ( ! GetDocument()->m_pSensor->IsMeasureValid() )
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							GetDocument()->m_measure.SetNearWhite ( nClrIndex, MeasuredColor);
						 }
						 break;

					case CLRCAT_SAT_RED:
					case CLRCAT_SAT_GREEN:
					case CLRCAT_SAT_BLUE:
					case CLRCAT_SAT_YELLOW:
					case CLRCAT_SAT_CYAN:
					case CLRCAT_SAT_MAGENTA:
						 // GGA: TODO. Manque le calcul de clrref pour la saturation ire
						 bOk = FALSE;
/*
						 if ( bDisplay )
						 {
							if ( GetDocument()->m_pGenerator->DisplayRGBColor(clrref) )
								GetDocument()->m_measure.WaitForDynamicIris ();
							else
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							MeasuredColor = GetDocument()->m_pSensor->MeasureColor ( clrref );
							if ( ! GetDocument()->m_pSensor->IsMeasureValid() )
								bOk = FALSE;
						 }
						 
						 if ( bOk )
						 {
							switch ( nCategory )
							{
								case CLRCAT_SAT_RED:
									GetDocument()->m_measure.SetMeasuredRedSat ( nClrIndex, MeasuredColor, GetDocument()->m_pSensor->GetSensorMatrix());
									break;
								case CLRCAT_SAT_GREEN:
									GetDocument()->m_measure.SetMeasuredGreenSat ( nClrIndex, MeasuredColor, GetDocument()->m_pSensor->GetSensorMatrix());
									break;
								case CLRCAT_SAT_BLUE:
									GetDocument()->m_measure.SetMeasuredBlueSat ( nClrIndex, MeasuredColor, GetDocument()->m_pSensor->GetSensorMatrix());
									break;
								case CLRCAT_SAT_YELLOW:
									GetDocument()->m_measure.SetMeasuredYellowSat ( nClrIndex, MeasuredColor, GetDocument()->m_pSensor->GetSensorMatrix());
									break;
								case CLRCAT_SAT_CYAN:
									GetDocument()->m_measure.SetMeasuredCyanSat ( nClrIndex, MeasuredColor, GetDocument()->m_pSensor->GetSensorMatrix());
									break;
								case CLRCAT_SAT_MAGENTA:
									GetDocument()->m_measure.SetMeasuredMagentaSat ( nClrIndex, MeasuredColor, GetDocument()->m_pSensor->GetSensorMatrix());
									break;
							}
						 }
*/
						 break;
				}
				
				GetDocument()->m_pSensor->Release();
				GetDocument()->m_pGenerator->Release();
			}
		}

		if ( bOk )
		{
			// Update document and views
			GetDocument()->SetModifiedFlag(GetDocument()->m_measure.IsModified());
			GetDocument()->UpdateAllViews(NULL, lHint);

			GetDocument () -> SetSelectedColor ( noDataColor );
		}
	}

	return bOk;
}

/////////////////////////////////////////////////////////////////////////////
// CRefCheckDlg dialog


CRefCheckDlg::CRefCheckDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRefCheckDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRefCheckDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_bTop = FALSE;
}


void CRefCheckDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRefCheckDlg)
	DDX_Control(pDX, IDC_BUTTON_MENU, m_ButtonMenu);
	DDX_Control(pDX, IDC_CHECK_REF, m_RefCheck);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CRefCheckDlg, CDialog)
	//{{AFX_MSG_MAP(CRefCheckDlg)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_CHECK_REF, OnCheckRef)
	ON_BN_CLICKED(IDC_BUTTON_MENU, OnButtonMenu)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRefCheckDlg message handlers

void CRefCheckDlg::OnPaint() 
{
	RECT		Rect;
	CPen		blackPen(PS_SOLID, 1, GetSysColor(COLOR_BTNTEXT));
	CPaintDC	dc(this); // device context for painting
	
	GetClientRect ( & Rect );
	CPen* pOldPen = dc.SelectObject ( & blackPen );
	if ( m_bTop )
	{
		dc.MoveTo ( 0, Rect.bottom - 2 );
		dc.LineTo ( Rect.right, Rect.bottom - 2 );
	}
	else
	{
		dc.MoveTo ( 0, 1 );
		dc.LineTo ( Rect.right, 1 );
	}
	dc.SelectObject ( pOldPen );

	// Do not call CDialog::OnPaint() for painting messages
}

void CRefCheckDlg::OnCheckRef() 
{
	( (CMultiFrame *) GetParent () ) -> OnChangeRef ( m_RefCheck.GetCheck () );
}

void CRefCheckDlg::OnButtonMenu() 
{
	RECT	Rect;
	POINT	pt = { 16, 0 };

	ClientToScreen ( & pt );
	m_ButtonMenu.GetClientRect(&Rect);
	
	( (CMultiFrame *) GetParent () ) -> OpenNewTabMenu ( pt );
}

BOOL CRefCheckDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	CString		Msg;

	Msg.LoadString ( IDS_OPENNEWTAB );
	m_ButtonMenu.EnableBalloonTooltip();
	m_ButtonMenu.SetTooltipText(Msg);
	m_ButtonMenu.SetIcon ( IDI_NEWTAB_ICON, 16, 16 );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
