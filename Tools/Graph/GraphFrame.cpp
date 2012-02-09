/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/

#include "../../stdafx.h"
#include "GraphFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define PVIEWBAR_ID	11003

/////////////////////////////////////////////////////////////////////////////
// CGraphFrame

IMPLEMENT_DYNCREATE(CGraphFrame, CFrameWnd)

CGraphFrame::CGraphFrame()
{
    m_bOwnTimer = FALSE;
}

CGraphFrame::~CGraphFrame()
{
}


BEGIN_MESSAGE_MAP(CGraphFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CGraphFrame)
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
	ON_COMMAND_RANGE(PVIEWBAR_ID, PVIEWBAR_ID, OnPointViewBarCommand)
	ON_UPDATE_COMMAND_UI_RANGE(PVIEWBAR_ID, PVIEWBAR_ID, OnPointViewUpdate)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraphFrame message handlers

BOOL CGraphFrame::Create(LPCTSTR lpszWindowName, const RECT& rect, CWnd* pParentWnd, 
    BOOL bOwnTimer/* = FALSE*/, DWORD dwStyle/* = WS_CHILD | WS_VISIBLE*/)
{
    m_bOwnTimer = bOwnTimer;
    return CFrameWnd::Create(NULL, lpszWindowName, dwStyle, rect, pParentWnd);
};


#define GRAPH_TIMER_ID	0x12345

int CGraphFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
	    return -1;
    
    CRect rect;
    GetClientRect(rect);

    BOOL b = graph_container.Create(NULL, _T(""), WS_VISIBLE | WS_CHILD, rect, this, 11001);
    ASSERT(b);

    CSize size(150, 200);

    if (!pview_bar.Create(_T("Points"), this, size, TRUE, PVIEWBAR_ID))
    {
	    TRACE0("Failed to create toolbar\n");
	    return -1;      // fail to create
    }

    pview_bar.SetBarStyle(pview_bar.GetBarStyle() | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC |
	CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM | CBRS_BORDER_LEFT |
	CBRS_BORDER_RIGHT);

    pview_bar.EnableDocking(CBRS_ALIGN_ANY);
    EnableDocking(CBRS_ALIGN_ANY);
    DockControlBar(&pview_bar, AFX_IDW_DOCKBAR_RIGHT);

    //set timer for explicit idle
    if (m_bOwnTimer)
    {
	SetTimer(GRAPH_TIMER_ID, 100, NULL);
    };

    return 0;
}

void CGraphFrame::OnPointViewBarCommand(UINT nID) 
{
    BOOL bShow = pview_bar.IsVisible();
    ShowControlBar(&pview_bar, !bShow, FALSE);
}

void CGraphFrame::OnPointViewUpdate(CCmdUI* pCmdUI) 
{
    pCmdUI->Enable();
    pCmdUI->SetCheck(pview_bar.IsVisible());
}

void CGraphFrame::RecalcLayout(BOOL bNotify) 
{
    CFrameWnd::RecalcLayout(bNotify);

    CRect rect;
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0, reposQuery, rect);

    if (!rect.IsRectEmpty() && ::IsWindow(graph_container.m_hWnd))
    {
	graph_container.MoveWindow(rect);
    };
}

BOOL CGraphFrame::OnEraseBkgnd(CDC* pDC) 
{
    return FALSE;
}

void CGraphFrame::UpdateViews(DWORD message_id,void* param)
{
    graph_container.UpdateViews(message_id, param);
    pview_bar.SendMessage(CM_GRAPH_NOTIFICATION, message_id, (DWORD)param);
}

void CGraphFrame::UpdateWindows(unsigned long what_to_update)
{
    graph_container.UpdateWindows(what_to_update);
    pview_bar.SendMessage(CM_GRAPH_NOTIFICATION, GRAPH_GRAPHIX_UPDATE, what_to_update);
}

void CGraphFrame::AppendMenuItems(CMenu* menu)
{
    if (menu->GetMenuItemCount() != 0)
    {
	//separator
	menu->AppendMenu(MF_SEPARATOR, 0, _T(""));
    };
    //View
    CMenu sub_menu;
    sub_menu.CreateMenu();
    menu->AppendMenu(MF_POPUP | MF_ENABLED | MF_STRING, (UINT)sub_menu.m_hMenu, _T("View"));
    //Zoom tool
    UINT menu_flags = MF_STRING | MF_ENABLED;
    if (pview_bar.IsWindowVisible())
    {
	menu_flags |= MF_CHECKED;
    };
    sub_menu.AppendMenu(menu_flags, GRAPH_RBMC_VIEW_POINT_WINDOW, _T("Points"));
}

void CGraphFrame::OnRBMenuCommand(UINT command_id)
{
    switch (command_id)
    {
	case GRAPH_RBMC_VIEW_POINT_WINDOW:
	{
	    if (pview_bar.IsWindowVisible())
	    {
		OperateWithPointView(GRAPH_PO_HIDE);
	    } else
	    {
		OperateWithPointView(GRAPH_PO_SHOW);
	    };
	}; break;
    };
}

void CGraphFrame::OperateWithPointView(unsigned long pview_operations)
{
    if ((pview_operations & GRAPH_PO_SHOW) != 0)
    {
	ShowControlBar(&pview_bar, TRUE, FALSE);
    };
    if ((pview_operations & GRAPH_PO_HIDE) != 0)
    {
	ShowControlBar(&pview_bar, FALSE, FALSE);
    };

    if ((pview_operations & GRAPH_PO_DISABLE) != 0)
    {
	pview_bar.SendMessage(CM_GRAPH_NOTIFICATION, GRAPH_PVIEW_DISABLED, 0);
    };
    if ((pview_operations & GRAPH_PO_ENABLE) != 0)
    {
	pview_bar.SendMessage(CM_GRAPH_NOTIFICATION, GRAPH_PVIEW_ENABLED, 0);
    };
}


void CGraphFrame::OnTimer(UINT nIDEvent) 
{
    CFrameWnd::OnTimer(nIDEvent);

    if (nIDEvent == GRAPH_TIMER_ID)
    {
	SendMessage(867, 1, 0);
    };
}

void CGraphFrame::DrawGraphToDC(CDC* dest_dc, CRect& rect_to_draw)
{
    graph_container.DrawGraphToDC(dest_dc, rect_to_draw);
}

