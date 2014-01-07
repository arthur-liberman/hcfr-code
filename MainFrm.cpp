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

// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "MainView.h"

#include "MainFrm.h"
#include "MultiFrm.h"
#include "NewDocPropertyPages.h"
#include "LangSelection.h"

#include "PatternDisplay.h"

#include "WebUpdate.h"

#include <dde.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// This define must be the same than in ColorHCFRConfig.cpp
#define LANG_PREFIX "CHCFR21_"

extern CPtrList gOpenedFramesList;	// Implemented in MultiFrm.cpp

extern BOOL g_bNewDocUseDDEInfo;	// Implemented in Datasetdoc.cpp
extern CString g_NewDocName;
extern CString g_ThcFileName;
extern int g_NewDocGeneratorID;
extern int g_NewDocSensorID;
extern Matrix g_NewDocSensorMatrix;

extern BOOL	g_bNewDocIsDuplication;	// Implemented in Datasetdoc.cpp
extern CDataSetDoc * g_DocToDuplicate;		// Implemented in Datasetdoc.cpp

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CNewMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CNewMDIFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_COMMAND(IDM_DUPLICATEDOC, OnDuplicateDoc)
	ON_COMMAND(ID_VIEW_VIEW_BAR, OnViewViewBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_VIEW_BAR, OnUpdateViewViewBar)
	ON_COMMAND(ID_VIEW_TOOLBAR, OnViewToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR, OnUpdateViewToolbar)
	ON_WM_SYSCOLORCHANGE()
	ON_COMMAND(IDM_VIEW_TESTCOLOR, OnViewTestColor)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_TESTCOLOR, OnUpdateViewTestColor)
	ON_WM_DROPFILES()
	ON_COMMAND(IDM_GENERAL_CONFIG, OnGeneralConfig)
	ON_COMMAND(ID_HELP, OnHelp)
	ON_COMMAND(IDM_HELP_FORUM, OnHelpForum)
	ON_COMMAND(IDM_HELP_INSTALL, OnHelpInstall)
	ON_COMMAND(IDM_HELP_SUPPORT, OnHelpSupport)
	ON_COMMAND(IDM_LANGUAGE, OnLanguage)
	ON_COMMAND(IDM_UPDATE_SOFT, OnUpdateSoft)
	ON_COMMAND(IDM_UPDATE_ETALONS, OnUpdateEtalons)
	ON_COMMAND(IDM_PATTERN_DISP_ROOT, OnPatternDisplay)
	ON_COMMAND(IDM_REFRESH_LUX, OnRefreshLux)
	ON_COMMAND(IDM_VIEW_LUMINANCE, OnViewLuminance)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_LUMINANCE, OnUpdateViewLuminance)
	ON_COMMAND(ID_VIEW_MEASURE_BAR, OnViewMeasureBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MEASURE_BAR, OnUpdateViewMeasureBar)
	ON_COMMAND(ID_VIEW_MEASURE_EXT_BAR, OnViewMeasureExBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MEASURE_EXT_BAR, OnUpdateViewMeasureExBar)
	ON_COMMAND(IDM_UPDATE_IRPROFILES, OnUpdateIRProfiles)
	ON_COMMAND(IDM_HELP, OnHelp)
	ON_COMMAND(IDM_PATTERN_DISPLAY, OnPatternDisplay)
	ON_COMMAND(ID_VIEW_MEASURE_SAT_BAR, OnViewMeasureSatBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MEASURE_SAT_BAR, OnUpdateViewMeasureSatBar)
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_DDE_INITIATE, OnDDEInitiate)
	ON_MESSAGE(WM_DDE_EXECUTE, OnDDEExecute)
	ON_MESSAGE(WM_DDE_REQUEST, OnDDERequest)
	ON_MESSAGE(WM_DDE_POKE, OnDDEPoke)
	ON_MESSAGE(WM_DDE_TERMINATE, OnDDETerminate)
	ON_MESSAGE(WM_BKGND_MEASURE_READY, OnBkgndMeasureReady)
	ON_MESSAGE(WM_TOOLBAR_RBUTTONDOWN, OnRightButton)
	ON_MESSAGE(WM_TOOLBAR_MBUTTONDOWN, OnMiddleButton)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_LUX_VALUE,
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	p_wndPatternDisplay = NULL;

	m_mainToolbarID = IDR_MEDIUMTOOLBAR_MAIN;
	m_viewToolbarID = IDR_MEDIUMTOOLBAR_VIEWS;
	m_measureToolbarID = IDR_MEDIUMTOOLBAR_MEASURES;
	m_measureexToolbarID = IDR_MEDIUMTOOLBAR_MEASURES_EX;
	m_measuresatToolbarID = IDR_MEDIUMTOOLBAR_MEASURES_SAT;
}

CMainFrame::~CMainFrame()
{
}

BOOL CMainFrame::LoadToolbars()
{
	// GGA TODO: very slow, need to be optimized: maybe create a LoadToolBar with no useless internal call to LoadHiColor.
	if(!m_wndToolBar.LoadToolBar(m_mainToolbarID))
		return FALSE;
	if(!m_wndToolBar.LoadHiColor(MAKEINTRESOURCE(IDB_MEDIUMTOOLBAR_MAIN_HICOL),RGB(0,0,0)))
		return FALSE;
	if(!m_wndToolBarViews.LoadToolBar(m_viewToolbarID))
		return FALSE;
	if(!m_wndToolBarViews.LoadHiColor(MAKEINTRESOURCE(IDB_MEDIUMTOOLBAR_VIEW_HICOL),RGB(0,0,0)))
		return FALSE;
	if(!m_wndToolBarMeasures.LoadToolBar(m_measureToolbarID))
		return FALSE;
	if(!m_wndToolBarMeasures.LoadHiColor(MAKEINTRESOURCE(IDB_MEDIUMTOOLBAR_MEASURE_HICOL),RGB(0,0,0)))
		return FALSE;
	if(!m_wndToolBarMeasuresEx.LoadToolBar(m_measureexToolbarID))
		return FALSE;
	if(!m_wndToolBarMeasuresEx.LoadHiColor(MAKEINTRESOURCE(IDB_MEDIUMTOOLBAR_MEAS_EX_HICOL),RGB(0,0,0)))
		return FALSE;
	if(!m_wndToolBarMeasuresSat.LoadToolBar(m_measuresatToolbarID))
		return FALSE;
	if(!m_wndToolBarMeasuresSat.LoadHiColor(MAKEINTRESOURCE(IDB_MEDIUMTOOLBAR_MEAS_SAT_HICOL),RGB(0,0,0)))
		return FALSE;

	return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// Subclass m_hWndMDIClient
	m_LogoWnd.SubclassWindow ( m_hWndMDIClient );

	DragAcceptFiles ();

	if (!m_wndMenuBar.Create(this,WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC|CBRS_TOOLTIPS
		|CBRS_GRIPPER) )
	{
		TRACE0("Failed to create menu bar\n"); 
		return -1; // fail to create
	}

	if (!m_wndToolBar.CreateEx(this, TBSTYLE_AUTOSIZE | TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, CRect(0,0,0,0), IDC_MAINTOOLBAR) )
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	if (!m_wndToolBarMeasures.CreateEx(this, TBSTYLE_AUTOSIZE | TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, CRect(0,0,0,0), IDC_MEASURETOOLBAR) )
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}
 
	if (!m_wndToolBarMeasuresEx.CreateEx(this, TBSTYLE_AUTOSIZE | TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, CRect(0,0,0,0), IDC_MEASUREEXTOOLBAR) )
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}
 
	if (!m_wndToolBarMeasuresSat.CreateEx(this, TBSTYLE_AUTOSIZE | TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, CRect(0,0,0,0), IDC_MEASURESATTOOLBAR) )
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}
 
	if (!m_wndToolBarViews.CreateEx(this, TBSTYLE_AUTOSIZE | TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, CRect(0,0,0,0), IDC_VIEWTOOLBAR) )
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}
  
	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	m_wndMenuBar.SetWindowText(_T("Menu standard"));
	m_wndToolBar.SetWindowText(_T("Toolbar standard"));
	m_wndToolBarViews.SetWindowText(_T("Toolbar views"));
	m_wndToolBarMeasures.SetWindowText(_T("Toolbar measures"));
	m_wndToolBarMeasuresEx.SetWindowText(_T("Toolbar measures ext"));
	m_wndToolBarMeasuresSat.SetWindowText(_T("Toolbar measures sat"));

	if(!LoadToolbars())
	{
		TRACE0("Failed to load toolbars\n");
		return -1;      // fail to create
	}

	m_wndTestColorWnd.Create(CTestColorWnd::IDD, this);
	m_tooltip.Create(this);	
	m_tooltip.AddToolBar(&m_wndToolBar);
	m_tooltip.AddToolBar(&m_wndToolBarMeasures);
	m_tooltip.AddToolBar(&m_wndToolBarMeasuresEx);
	m_tooltip.AddToolBar(&m_wndToolBarMeasuresSat);
	m_tooltip.AddToolBar(&m_wndToolBarViews);

	m_wndMenuBar.EnableDocking(CBRS_ALIGN_ANY); 
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	m_wndToolBarMeasures.EnableDocking(CBRS_ALIGN_ANY);
	m_wndToolBarMeasuresEx.EnableDocking(CBRS_ALIGN_ANY);
	m_wndToolBarMeasuresSat.EnableDocking(CBRS_ALIGN_ANY);
	m_wndToolBarViews.EnableDocking(CBRS_ALIGN_ANY);

	// Enable docking

//    EnableDocking(CBRS_ALIGN_ANY); 
	EnableDocking(CBRS_ALIGN_TOP);	// Use this to make bottom bar expanding only to left bar right side
    EnableDocking(CBRS_ALIGN_LEFT);
    EnableDocking(CBRS_ALIGN_RIGHT);
    EnableDocking(CBRS_ALIGN_BOTTOM); 

// Used by CControlBar to change bar visual aspect
#ifdef _SCB_REPLACE_MINIFRAME
    m_pFloatingFrameClass = RUNTIME_CLASS(CSCBMiniDockFrameWnd);
#endif //_SCB_REPLACE_MINIFRAME

	DockControlBar(&m_wndMenuBar);
	DockControlBar(&m_wndToolBar);
	DockControlBarNextTo(&m_wndToolBarMeasures,&m_wndToolBar);
	DockControlBarNextTo(&m_wndToolBarViews,&m_wndToolBarMeasures);
	DockControlBar(&m_wndToolBarMeasuresEx);
	DockControlBarNextTo(&m_wndToolBarMeasuresSat,&m_wndToolBarMeasuresEx);

	// Restore previous bar state
	CString sProfile = _T("MeasureBarState");

	if (VerifyBarState(sProfile))
	{
		CDockState state;
		state.LoadState(sProfile);

		if ( state.m_arrBarInfo.GetSize() == 0 )
		{
			ShowControlBar ( &m_wndToolBarMeasuresEx, FALSE, TRUE );
			ShowControlBar ( &m_wndToolBarMeasuresSat, FALSE, TRUE );
		}
		else
		{
			CSizingControlBar::GlobalLoadState(this, sProfile);
			LoadBarState(sProfile);
		}
	}

	m_DefaultNewMenu.LoadToolBar(IDR_MENUBARGRAPH);

//	m_SystemNewMenu.SetMenuTitle(_T("Menu principal"),MFT_SUNKEN|MFT_LINE|MFT_CENTER|MFT_SIDE_TITLE);
//	m_SystemNewMenu.SetMenuTitleColor(RGB(255,0,0),CLR_DEFAULT,RGB(0,0,255));

	RecalcLayout(FALSE);

	// Clear initial lux value
	m_wndStatusBar.SetPaneText ( m_wndStatusBar.CommandToIndex ( ID_LUX_VALUE ), "" );

	return 0;
}

BOOL CMainFrame::DestroyWindow() 
{
	if ( p_wndPatternDisplay )
	{
		p_wndPatternDisplay->DestroyWindow ();
		delete p_wndPatternDisplay;
		p_wndPatternDisplay = NULL;
	}

	CString sProfile = _T("MeasureBarState");
	CSizingControlBar::GlobalSaveState(this, sProfile);
	SaveBarState(sProfile);

	return CMDIFrameWnd::DestroyWindow();
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMDIFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

// This function is Copyright (c) 2000, Cristi Posea.
// See www.datamekanix.com for more control bars tips&tricks.
BOOL CMainFrame::VerifyBarState(LPCTSTR lpszProfileName)
{
    CDockState state;
    state.LoadState(lpszProfileName);

    for (int i = 0; i < state.m_arrBarInfo.GetSize(); i++)
    {
        CControlBarInfo* pInfo = (CControlBarInfo*)state.m_arrBarInfo[i];
        ASSERT(pInfo != NULL);
        int nDockedCount = pInfo->m_arrBarID.GetSize();
        if (nDockedCount > 0)
        {
            // dockbar
            for (int j = 0; j < nDockedCount; j++)
            {
                UINT nID = (UINT) pInfo->m_arrBarID[j];
                if (nID == 0) continue; // row separator
                if (nID > 0xFFFF)
                    nID &= 0xFFFF; // placeholder - get the ID
                if (GetControlBar(nID) == NULL)
                    return FALSE;
            }
        }
        
        if (!pInfo->m_bFloating) // floating dockbars can be created later
            if (GetControlBar(pInfo->m_nBarID) == NULL)
                return FALSE; // invalid bar ID
    }

    return TRUE;
}

void CMainFrame::DockControlBarNextTo(CControlBar* pBar,CControlBar* pTargetBar)
{
    ASSERT(pBar != NULL);
    ASSERT(pTargetBar != NULL);
    ASSERT(pBar != pTargetBar);

    // the neighbour must be already docked
    CDockBar* pDockBar = pTargetBar->m_pDockBar;
    ASSERT(pDockBar != NULL);
    UINT nDockBarID = pTargetBar->m_pDockBar->GetDlgCtrlID();
    ASSERT(nDockBarID != AFX_IDW_DOCKBAR_FLOAT);

    bool bHorz = (nDockBarID == AFX_IDW_DOCKBAR_TOP ||
        nDockBarID == AFX_IDW_DOCKBAR_BOTTOM);

    // dock normally (inserts a new row)
    DockControlBar(pBar, nDockBarID);

    // delete the new row (the bar pointer and the row end mark)
    pDockBar->m_arrBars.RemoveAt(pDockBar->m_arrBars.GetSize() - 1);
    pDockBar->m_arrBars.RemoveAt(pDockBar->m_arrBars.GetSize() - 1);

    // find the target bar
    for (int i = 0; i < pDockBar->m_arrBars.GetSize(); i++)
    {
        void* p = pDockBar->m_arrBars[i];
        if (p == pTargetBar) // and insert the new bar after it
            pDockBar->m_arrBars.InsertAt(i + 1, pBar);
    }

    // move the new bar into position
    CRect rBar;
    pTargetBar->GetWindowRect(rBar);
    rBar.OffsetRect(bHorz ? 1 : 0, bHorz ? 0 : 1);
    pBar->MoveWindow(rBar);
}


void CMainFrame::OnViewTestColor() 
{
	BOOL bShow = m_wndTestColorWnd.IsWindowVisible();
	m_wndTestColorWnd.ShowWindow ( bShow ? SW_HIDE : SW_SHOW );
}

void CMainFrame::OnUpdateViewTestColor(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_wndTestColorWnd.IsWindowVisible());
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) 
{
	m_tooltip.RelayEvent(pMsg);	

	if(m_wndMenuBar.TranslateFrameMessage(pMsg)) 
		return TRUE; 

	return CMDIFrameWnd::PreTranslateMessage(pMsg);
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	cs.style |= WS_HSCROLL | WS_VSCROLL;
	return CMDIFrameWnd::PreCreateWindow(cs);
}

void CMainFrame::OnViewViewBar() 
{
	BOOL bShow = m_wndToolBarViews.IsVisible();
	ShowControlBar(&m_wndToolBarViews, !bShow, FALSE);
}

void CMainFrame::OnUpdateViewViewBar(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_wndToolBarViews.IsVisible());
}

void CMainFrame::OnViewMeasureBar() 
{
	BOOL bShow = m_wndToolBarMeasures.IsVisible();
	ShowControlBar(&m_wndToolBarMeasures, !bShow, FALSE);
}

void CMainFrame::OnUpdateViewMeasureBar(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_wndToolBarMeasures.IsVisible());
}

void CMainFrame::OnViewMeasureExBar() 
{
	BOOL bShow = m_wndToolBarMeasuresEx.IsVisible();
	ShowControlBar(&m_wndToolBarMeasuresEx, !bShow, FALSE);
}

void CMainFrame::OnUpdateViewMeasureExBar(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_wndToolBarMeasuresEx.IsVisible());
}

void CMainFrame::OnViewMeasureSatBar() 
{
	BOOL bShow = m_wndToolBarMeasuresSat.IsVisible();
	ShowControlBar(&m_wndToolBarMeasuresSat, !bShow, FALSE);
}

void CMainFrame::OnUpdateViewMeasureSatBar(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_wndToolBarMeasuresSat.IsVisible());
}
void CMainFrame::OnViewToolbar() 
{
	BOOL bShow = m_wndToolBar.IsVisible();
	ShowControlBar(&m_wndToolBar, !bShow, FALSE);
}

void CMainFrame::OnUpdateViewToolbar(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable();
	pCmdUI->SetCheck(m_wndToolBar.IsVisible());
}

void CMainFrame::OnGeneralConfig() 
{
	GetConfig()->ChangeSettings(0);
}

void CMainFrame::OnSysColorChange() 
{
	CNewMDIFrameWnd::OnSysColorChange();
	
	// reload toolbar to re-create grayed icons with correct color
//	m_wndToolBar.LoadHiColor(MAKEINTRESOURCE(IDB_MEDIUMTOOLBAR_MAIN),RGB(0,0,0));
//	m_wndToolBarViews.LoadToolBar(m_viewToolbarID);
	LoadToolbars();

	// Redraw all toolbars including non-client areas
	m_wndMenuBar.RedrawWindow(NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME );
	m_wndToolBar.RedrawWindow(NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME );
	m_wndToolBarViews.RedrawWindow(NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME );
	m_wndToolBarMeasures.RedrawWindow(NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME );
	m_wndStatusBar.RedrawWindow(NULL,NULL,RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME );

	Invalidate(TRUE); // redraw window
}


void CMainFrame::OnDropFiles(HDROP hDropInfo) 
{
	// Default implementation is OK.
	CNewMDIFrameWnd::OnDropFiles(hDropInfo);
}

void DdeParseString ( LPCSTR lpszString, CString & strCmd, CStringList & CmdParams );

extern "C" __declspec (dllexport) const char g_szShortApplicationName [] = "COLORH~1";
extern "C" __declspec (dllexport) const char g_szLongApplicationName [] = "COLORHCFR";

static const char * g_szDDETopicName = "General";

LRESULT CMainFrame::OnDDEInitiate(WPARAM wParam, LPARAM lParam)
{
	LRESULT		lResult = -1;
	BOOL		bOk = FALSE;
	POSITION	pos;
	HWND		hWndClient = (HWND) wParam;
	ATOM		nAtomAppli = (ATOM) LOWORD(lParam);
	ATOM		nAtomTopic = (ATOM) HIWORD(lParam);
	ATOM		nAtomSrvAppli;
	ATOM		nAtomSrvTopic;
	char		szBuf [ 256 ];

	if ( nAtomAppli )
	{
		if ( nAtomAppli != AfxGetApp () -> m_atomApp )
		{
			GlobalGetAtomName ( nAtomAppli, szBuf, sizeof ( szBuf ) );
			if ( _stricmp ( szBuf, g_szShortApplicationName ) == 0 || _stricmp ( szBuf, g_szLongApplicationName ) == 0 )
			{
				// Received	good name, compressed or not. Get good atom name
				nAtomAppli = AfxGetApp () -> m_atomApp;
				lParam = MAKELONG ( nAtomAppli, nAtomTopic );
			}
		}

		if ( nAtomAppli == AfxGetApp () -> m_atomApp )
		{
			if ( nAtomTopic )
			{
				GlobalGetAtomName ( nAtomTopic, szBuf, sizeof ( szBuf ) );
				if ( _stricmp ( szBuf, g_szDDETopicName ) == 0 )
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
		nAtomSrvTopic = GlobalAddAtom ( g_szDDETopicName );
		lParam = MAKELPARAM ( nAtomSrvAppli, nAtomSrvTopic );
		::SendMessage ( hWndClient, WM_DDE_ACK, (WPARAM) m_hWnd, lParam );
		lResult = 0;
	}

	if ( lResult == -1 )
	{
		// Transfer initiate to all opened views
		pos = gOpenedFramesList.GetHeadPosition ();
		while ( pos && lResult == -1 )
		{
			CMultiFrame * pFrame = ( CMultiFrame * ) gOpenedFramesList.GetNext ( pos );
			lResult = pFrame -> OnDDEInitiate(wParam, lParam);
		}
	}

	if ( lResult != -1 )
		return lResult;
	else
	{
		// Transfer initiate command to MFC file open handler
		return CNewMDIFrameWnd::OnDDEInitiate(wParam, lParam);
	}
}

LRESULT CMainFrame::OnDDEExecute(WPARAM wParam, LPARAM lParam)
{
	int			i;
	int			nRow, nCol;
	int			nCount;
	char		c;
	Matrix		SensorMatrix ( 0.0, 3, 3 );
	BOOL		bOk = FALSE;
	BOOL		bSomethingDone = FALSE;
	LRESULT		lRes = -1;
	HWND		hWndClient = (HWND) wParam;
	HGLOBAL		hMem = (HGLOBAL) lParam;
	LPCSTR		lpszCommand;
	LPCSTR		lpStr;
	LPCSTR		lpStr2;
	LPCSTR		lpStart = NULL;
	char		szBuf [ 256 ];
	CString		strCommand;
	CString		strCmd;
	CString		strParam;
	CStringList	CmdList;
	CStringList	CmdParams;
	POSITION	pos, pos2;
	DDEACK		Ack;
	
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
				{
					lpStart = lpStr;
					nCount = 1;
				}
			}
		}
		else
		{
			if ( c == '[' )
				nCount ++;

			if ( c == ']' )
			{
				if ( lpStr [ 1 ] == ']' && nCount == 1 )
					lpStr ++;
				else
				{
					nCount --;

					if ( nCount == 0 )
					{
						i = lpStr - lpStart;
						strncpy_s ( szBuf, lpStart, i );
						szBuf [ i ] = '\0';
						CmdList.AddTail ( szBuf );
						bOk = TRUE;
						lpStart = NULL;
					}
				}
			}
		}
		lpStr ++;
	}
	GlobalUnlock ( hMem );

	if ( bOk )
	{
		pos = CmdList.GetHeadPosition ();
		while ( pos && bOk )
		{
			strCommand = CmdList.GetNext ( pos );

			bOk = FALSE;
			DdeParseString ( (LPCSTR) strCommand, strCmd, CmdParams );
			if ( _stricmp ( (LPCSTR) strCmd, "OpenColorDataSet" ) == 0 || _stricmp ( (LPCSTR) strCmd, "Open" ) == 0 )
			{
				lRes = 0;
				nCount = CmdParams.GetCount ();
				
				if ( nCount == 1 )
				{
					bOk = TRUE;
					bSomethingDone = TRUE;

					// First param is the file name to open
					strParam = CmdParams.GetHead ();

					if ( ! AfxGetApp () -> OpenDocumentFile ( strParam ) )
						bOk = FALSE;
				}
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "NewColorDataSet" ) == 0 )
			{
				nCount = CmdParams.GetCount ();
				
				if ( nCount == 4 )
				{
					bOk = TRUE;
					bSomethingDone = TRUE;

					pos2 = CmdParams.GetHeadPosition ();
					
					// Retrieve Document name
					strParam = CmdParams.GetNext ( pos2 );
					g_NewDocName = strParam;

					// Retrieve Generator ID
					strParam = CmdParams.GetNext ( pos2 );
					g_NewDocGeneratorID = atoi ( (LPCSTR) strParam );

					// Retrieve Sensor ID
					strParam = CmdParams.GetNext ( pos2 );
					g_NewDocSensorID = atoi ( (LPCSTR) strParam );

					// Retrieve Matrix
					strParam = CmdParams.GetNext ( pos2 );

					lpStr2 = strtok ( strParam.GetBuffer(1), "," );
					if ( lpStr2 )
						lpStr2 ++;
					for ( nRow = 0; nRow < 3 && lpStr2 ; nRow ++ )
					{
						for ( nCol = 0; nCol < 3 && lpStr2 ; nCol ++ )
						{
							SensorMatrix [nRow] [nCol] = atof(lpStr2);
							lpStr2 = strtok ( NULL, "," );
						}
					}

					if ( nRow == 3 && nCol == 3 && lpStr2 == NULL )
					{
						g_bNewDocUseDDEInfo = TRUE;
						g_ThcFileName.Empty ();
						g_NewDocSensorMatrix = SensorMatrix;

						GetColorApp() -> OnFileNew ();

						g_bNewDocUseDDEInfo = FALSE;
					}
					else
					{
						// Invalid parameters
						bOk = FALSE;
					}
				}
				
				lRes = 0;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "NewColorDataSetWithThc" ) == 0 )
			{
				nCount = CmdParams.GetCount ();
				
				if ( nCount == 4 )
				{
					bOk = TRUE;
					bSomethingDone = TRUE;

					pos2 = CmdParams.GetHeadPosition ();
					
					// Retrieve Document name
					strParam = CmdParams.GetNext ( pos2 );
					g_NewDocName = strParam;

					// Retrieve Generator ID
					strParam = CmdParams.GetNext ( pos2 );
					g_NewDocGeneratorID = atoi ( (LPCSTR) strParam );

					// Retrieve Generator ID
					strParam = CmdParams.GetNext ( pos2 );
					g_NewDocSensorID = atoi ( (LPCSTR) strParam );

					// Retrieve thc file name
					strParam = CmdParams.GetNext ( pos2 );

					if ( ! strParam.IsEmpty () )
					{
						CString strPath, strSubDir;

						g_bNewDocUseDDEInfo = TRUE;

						strPath = GetConfig () -> m_ApplicationPath;
						strSubDir = CSensorSelectionPropPage::GetThcFileSubDir ( g_NewDocSensorID );
						if ( ! strSubDir.IsEmpty () )
						{
							strPath += strSubDir;
							strPath += '\\';
						}
						
						g_ThcFileName = strPath + strParam + ".thc";

						GetColorApp() -> OnFileNew ();

						g_bNewDocUseDDEInfo = FALSE;
					}
					else
					{
						// Invalid parameters
						bOk = FALSE;
					}
				}
				
				lRes = 0;
			}
			else if ( _stricmp ( (LPCSTR) strCmd, "SetLogFileName" ) == 0 )
			{
				nCount = CmdParams.GetCount ();
				
				if ( nCount == 1 )
				{
					bOk = TRUE;
					bSomethingDone = TRUE;

					// Retrieve log file name
					strParam = CmdParams.GetHead ();

					strcpy_s ( GetConfig () -> m_logFileName, (LPCSTR) strParam );
				}
			}
		}

		if ( bSomethingDone )
		{
			Ack.bAppReturnCode = 0;
			Ack.fAck = bOk;
			Ack.fBusy = FALSE;

			lParam = PackDDElParam ( WM_DDE_ACK, *(UINT*)(&Ack), lParam );
			::PostMessage ( hWndClient, WM_DDE_ACK, (WPARAM) m_hWnd, lParam );
		}
		else
		{
			lRes = -1;
		}
	}

	if ( lRes != -1 )
		return lRes;
	else
		return CNewMDIFrameWnd::OnDDEExecute(wParam, lParam);
}

LRESULT CMainFrame::OnDDERequest(WPARAM wParam, LPARAM lParam)
{
	LRESULT	lRes = -1;
	
	if ( lRes != -1 )
		return lRes;
	else
		return Default ();
}

LRESULT CMainFrame::OnDDEPoke(WPARAM wParam, LPARAM lParam)
{
	LRESULT	lRes = -1;
	
	if ( lRes != -1 )
		return lRes;
	else
		return Default ();
}

LRESULT CMainFrame::OnDDETerminate(WPARAM wParam, LPARAM lParam)
{
	LRESULT	lRes = -1;
	
	if ( lRes != -1 )
		return lRes;
	else
		return CNewMDIFrameWnd::OnDDETerminate(wParam, lParam);
}


LRESULT CMainFrame::OnBkgndMeasureReady(WPARAM wParam, LPARAM lParam)
{
	CDataSetDoc * pDoc = (CDataSetDoc *) wParam;
	
	pDoc -> OnBkgndMeasureReady();
	
	return 0;
}

void CMainFrame::OnDuplicateDoc()
{
	g_bNewDocIsDuplication = TRUE;
	CDataSetDoc * pDoc =(CDataSetDoc*)MDIGetActive()->GetActiveDocument();
	g_DocToDuplicate = pDoc;
	GetColorApp() -> OnFileNew ();
	g_bNewDocIsDuplication = FALSE;
}

LRESULT CMainFrame::OnRightButton(WPARAM wParam, LPARAM lParam)
{
	CDataSetDoc *	pCurrentDocument = NULL;
	CMDIChildWnd *  pMDIFrameWnd = MDIGetActive();
	
	if (pMDIFrameWnd != NULL)
	{
		pCurrentDocument = (CDataSetDoc *) pMDIFrameWnd->GetActiveDocument();

		if ( pMDIFrameWnd -> IsKindOf ( RUNTIME_CLASS(CMultiFrame) ) )
			( (CMultiFrame *) pMDIFrameWnd ) -> OnRightToolbarButton ( wParam );
	}

	return 0;
}

LRESULT CMainFrame::OnMiddleButton(WPARAM wParam, LPARAM lParam)
{
	CDataSetDoc *	pCurrentDocument = NULL;
	CMDIChildWnd *  pMDIFrameWnd = MDIGetActive();
	
	if (pMDIFrameWnd != NULL)
	{
		pCurrentDocument = (CDataSetDoc *) pMDIFrameWnd->GetActiveDocument();

		if ( pMDIFrameWnd -> IsKindOf ( RUNTIME_CLASS(CMultiFrame) ) )
			( (CMultiFrame *) pMDIFrameWnd ) -> OnMiddleToolbarButton ( wParam );
	}

	return 0;
}


BEGIN_MESSAGE_MAP(CMDIClientWithLogo, CWnd)
	//{{AFX_MSG_MAP(CMDIClientWithLogo)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


CMDIClientWithLogo::CMDIClientWithLogo ()
{
	m_LogoBitmap.LoadBitmap ( IDB_LOGO );
}

CMDIClientWithLogo::~CMDIClientWithLogo ()
{
	m_LogoBitmap.DeleteObject ();
}

void CMDIClientWithLogo::OnSize(UINT nType, int cx, int cy) 
{
	InvalidateRect ( NULL, TRUE );
	Default ();
}

BOOL CMDIClientWithLogo::OnEraseBkgnd(CDC* pDC) 
{
	// TODO: Add your message handler code here and/or call default
	RECT		Rect;
	RECT		ClientRect;
	CDC			BmpDC;
	CBitmap *	pOldBmp;
	CBrush		Brush ( RGB ( 56, 76, 104 ) );
	BITMAP		bm;
	
	GetClientRect ( & ClientRect );

	m_LogoBitmap.GetBitmap ( & bm );

	// Draw four rectangles around bitmap, instead of only one covering the whole client area
	// This technique avoids flickering during window resize, because of bitmap temporary masking
	Rect.left = ClientRect.left;
	Rect.right = ClientRect.right;
	Rect.top = ClientRect.top;
	Rect.bottom = (ClientRect.bottom - bm.bmHeight) / 2;
	pDC -> FillRect ( & Rect, & Brush );

	Rect.top = ( (ClientRect.bottom - bm.bmHeight) / 2 ) + bm.bmHeight;
	Rect.bottom = ClientRect.bottom;
	pDC -> FillRect ( & Rect, & Brush );

	Rect.left = ClientRect.left;
	Rect.right = ( ClientRect.right - bm.bmWidth) / 2;
	Rect.top = (ClientRect.bottom - bm.bmHeight) / 2;
	Rect.bottom = ( (ClientRect.bottom - bm.bmHeight) / 2 ) + bm.bmHeight;
	pDC -> FillRect ( & Rect, & Brush );

	Rect.left = ( ( ClientRect.right - bm.bmWidth) / 2 ) + bm.bmWidth;
	Rect.right = ClientRect.right;
	pDC -> FillRect ( & Rect, & Brush );

	// Draw the bitmap itself in the center "hole"
	BmpDC.CreateCompatibleDC ( pDC );
	pOldBmp = BmpDC.SelectObject ( & m_LogoBitmap );
	pDC -> BitBlt ( (ClientRect.right - bm.bmWidth) / 2, (ClientRect.bottom - bm.bmHeight) / 2, bm.bmWidth, bm.bmHeight, & BmpDC, 0, 0, SRCCOPY );

	BmpDC.SelectObject ( pOldBmp );
	BmpDC.DeleteDC ();

	return TRUE;
}

void CMainFrame::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_WELCOME, NULL );
}

void CMainFrame::OnHelpForum() 
{
	GetConfig () -> DisplayHelp ( HID_FORUM, NULL );
}

void CMainFrame::OnHelpInstall() 
{
	GetConfig () -> DisplayHelp ( HID_INSTALL, NULL );
}

void CMainFrame::OnHelpSupport() 
{
	GetConfig () -> DisplayHelp ( HID_SUPPORT, NULL );
}

void CMainFrame::OnLanguage() 
{
	char			szBuf [ 256 ];
	CString			strLang, strPath, strSearch;
	LPSTR			lpStr;
	HANDLE			hFind;
	WIN32_FIND_DATA	wfd;
	CLangSelection	dlg;

	// Define language DLL, which will be loaded on next launch
	strLang = GetConfig () -> GetProfileString ( "Options", "Language", "" );
	
	strPath = GetConfig () -> m_ApplicationPath;
	
	strSearch = strPath + LANG_PREFIX + "*.DLL";
		
	hFind = FindFirstFile ( (LPCSTR) strSearch, & wfd );
	if ( hFind != INVALID_HANDLE_VALUE )
	{
		do
		{
			strcpy_s ( szBuf, wfd.cFileName + strlen ( LANG_PREFIX ) );
			lpStr = strrchr ( szBuf, '.' );
			if ( lpStr )
				lpStr [ 0 ] = '\0';
			
			if ( _stricmp ( szBuf, "PATTERNS" ) != 0 )
				dlg.m_Languages.AddTail ( szBuf );
		} while ( FindNextFile ( hFind, & wfd ) );

		FindClose ( hFind );
	}
	
	dlg.m_Selection = strLang;
	if ( IDOK == dlg.DoModal () )
	{
		strLang = dlg.m_Selection;
		GetConfig () -> WriteProfileString ( "Options", "Language", strLang );
		GetConfig () -> PurgeHelpLanguageSection ();

		strLang.LoadString ( IDS_MUSTRESTART );
		AfxMessageBox ( strLang );
	}
}


void CMainFrame::OnRefreshLux() 
{
	char			szBuf [ 256 ];
	CColorHCFRApp *	pApp = GetColorApp();

	EnterCriticalSection ( & pApp -> m_LuxCritSec );
	
	if ( pApp -> m_bHighLuxValue )
		strcpy_s ( szBuf, "++++" );
	else if ( pApp -> m_bLowLuxValue )
		strcpy_s ( szBuf, "----" );
	else
	{
		if ( GetConfig () ->m_bUseImperialUnits )
			sprintf_s ( szBuf, "%.5g Ft-cd", pApp -> m_CurrentLuxValue * 0.0929 );
		else
			sprintf_s ( szBuf, "%.5g Lux", pApp -> m_CurrentLuxValue );
	}

	LeaveCriticalSection ( & pApp -> m_LuxCritSec );
	
	m_wndStatusBar.SetPaneText ( m_wndStatusBar.CommandToIndex ( ID_LUX_VALUE ), szBuf );
	m_wndLuminanceWnd.Refresh ( szBuf );
}

int __stdcall MyDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	return FALSE;
}

void CMainFrame::OnUpdateSoft()
{
	CString strNewFtpFile;
	CString strDefaultFileName;
	CWebUpdate WebUpdate;
	HWND	hDlg, hCtrl;

	hDlg = ::CreateDialog ( AfxGetResourceHandle (), MAKEINTRESOURCE(IDD_WEB_UPDATE), m_hWnd, MyDialogProc );
	hCtrl = ::GetDlgItem ( hDlg, IDC_STATIC1 );
	::ShowWindow ( hDlg, SW_SHOW );
	::UpdateWindow ( hDlg );

	if (WebUpdate.Connect() == FALSE)
	{
		::DestroyWindow ( hDlg );
		AfxMessageBox(IDS_UPD_IMPOSSIBLE, MB_OK | MB_ICONWARNING);
		return;
	}


	::SetWindowText ( hCtrl, "Looking for new version... please wait..." );
	strNewFtpFile = WebUpdate.CheckNewSoft();
	if (strNewFtpFile.IsEmpty())
	{
		::DestroyWindow ( hDlg );
		AfxMessageBox(IDS_NO_UPD, MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		::ShowWindow ( hDlg, SW_HIDE );
		if (AfxMessageBox(IDS_UPD_ASK_DOWNLOAD, MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			strDefaultFileName = strNewFtpFile.Mid(strNewFtpFile.ReverseFind('/')+1);
			CFileDialog FileDlg(FALSE,
								"exe",
								strDefaultFileName,
								OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
								"Executables (*.exe)|*.exe||");
			if (FileDlg.DoModal() == IDOK)
			{
				::SetWindowText ( hCtrl, "Loading file... please wait..." );
				::ShowWindow ( hDlg, SW_SHOW );
				::UpdateWindow ( hDlg );
				if (WebUpdate.TransferFile(strNewFtpFile, FileDlg.GetPathName()) == TRUE)
				{
					::ShowWindow ( hDlg, SW_HIDE );
					AfxMessageBox(IDS_UPD_DOWNLOAD_SUCCEED, MB_OK | MB_ICONINFORMATION);
				}
				else
				{
					::ShowWindow ( hDlg, SW_HIDE );
					AfxMessageBox(IDS_UPD_DOWNLOAD_FAILED, MB_OK | MB_ICONWARNING);
				}
			}
		}
		::DestroyWindow ( hDlg );
	}
}

void CMainFrame::OnUpdateEtalons()
{
	CWebUpdate WebUpdate;
	HWND	hDlg, hCtrl;

	hDlg = ::CreateDialog ( AfxGetResourceHandle (), MAKEINTRESOURCE(IDD_WEB_UPDATE), m_hWnd, MyDialogProc );
	hCtrl = ::GetDlgItem ( hDlg, IDC_STATIC1 );
	::ShowWindow ( hDlg, SW_SHOW );
	::UpdateWindow ( hDlg );

	if (WebUpdate.Connect() == FALSE)
	{
		::DestroyWindow ( hDlg );
		AfxMessageBox(IDS_UPD_IMPOSSIBLE, MB_OK | MB_ICONWARNING);
		return;
	}

	::SetWindowText ( hCtrl, "Loading files... please wait..." );
	if (WebUpdate.TransferCalibrationFiles(hCtrl) == TRUE)
	{
		AfxMessageBox(IDS_UPD_DOWNLOAD_SUCCEED, MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		AfxMessageBox(IDS_UPD_DOWNLOAD_FAILED, MB_OK | MB_ICONWARNING);
	}
	::DestroyWindow ( hDlg );
}


void CMainFrame::OnUpdateIRProfiles() 
{
	CWebUpdate WebUpdate;
	HWND	hDlg, hCtrl;

	hDlg = ::CreateDialog ( AfxGetResourceHandle (), MAKEINTRESOURCE(IDD_WEB_UPDATE), m_hWnd, MyDialogProc );
	hCtrl = ::GetDlgItem ( hDlg, IDC_STATIC1 );
	::ShowWindow ( hDlg, SW_SHOW );
	::UpdateWindow ( hDlg );

	if (WebUpdate.Connect() == FALSE)
	{
		::DestroyWindow ( hDlg );
		AfxMessageBox(IDS_UPD_IMPOSSIBLE, MB_OK | MB_ICONWARNING);
		return;
	}

	::SetWindowText ( hCtrl, "Loading files... please wait..." );
	if (WebUpdate.TransferIRProfiles(hCtrl) == TRUE)
	{
		AfxMessageBox(IDS_UPD_DOWNLOAD_SUCCEED, MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		AfxMessageBox(IDS_UPD_DOWNLOAD_FAILED, MB_OK | MB_ICONWARNING);
	}
	::DestroyWindow ( hDlg );
}


void CMainFrame::OnPatternDisplay()
{
        if ( p_wndPatternDisplay == NULL )
	    {
		    p_wndPatternDisplay = new CPatternDisplay;
		    p_wndPatternDisplay->Create(IDD_PATTERNS,this);
	    }
	    else
	    {
		    BOOL bShow = p_wndPatternDisplay->IsWindowVisible();
		    if (!bShow)
			    p_wndPatternDisplay->ShowWindow(SW_SHOW);
		    else
			    p_wndPatternDisplay->ShowWindow(SW_HIDE);
	    }
}

void CMainFrame::OnViewLuminance() 
{
	// TODO: Add your command handler code here
	if ( m_wndLuminanceWnd.m_hWnd == NULL )
	{
		// Create luminance window
		m_wndLuminanceWnd.CreateEx(0, AfxRegisterWndClass ( CS_VREDRAW|CS_HREDRAW, LoadCursor ( NULL, IDC_ARROW ), (HBRUSH) GetStockObject ( WHITE_BRUSH ), NULL ), "Luminance", WS_POPUP | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_CHILD | WS_VISIBLE, 0, 0, 200, 100, m_hWnd, NULL, NULL );
	}
	else
	{
		if ( m_wndLuminanceWnd.IsWindowVisible () )
			m_wndLuminanceWnd.ShowWindow ( SW_HIDE );
		else
			m_wndLuminanceWnd.ShowWindow ( SW_SHOW );
	}
	
}

void CMainFrame::OnUpdateViewLuminance(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	BOOL	bEnable = ! GetColorApp() -> m_LuxPort.IsEmpty ();
	
	pCmdUI -> Enable ( bEnable );

	if ( ! bEnable && m_wndLuminanceWnd.m_hWnd != NULL )
		m_wndLuminanceWnd.DestroyWindow ();

	pCmdUI -> SetCheck ( m_wndLuminanceWnd.m_hWnd != NULL && m_wndLuminanceWnd.IsWindowVisible () );

}

