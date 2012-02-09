/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/

#include "../../stdafx.h"
#include "GraphContainer.h"
#include "Graphframe.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGraphContainer

CGraphContainer::CGraphContainer()
{
    graph_panel = NULL;
}

CGraphContainer::~CGraphContainer()
{
    delete graph_panel;
}


BEGIN_MESSAGE_MAP(CGraphContainer, CWnd)
	//{{AFX_MSG_MAP(CGraphContainer)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CGraphContainer message handlers

int CGraphContainer::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
	    return -1;

    //tool tips
    EnableToolTips(TRUE);

    BOOL b;
    //create rulers
    CRect rect(0, 0, 100, 100);
    b = hruler.Create(NULL, _T(""), WS_CHILD | WS_VISIBLE, rect, this, 0, NULL);
    ASSERT(b);
    b = vruler.Create(NULL, _T(""), WS_CHILD | WS_VISIBLE, rect, this, 0, NULL);
    ASSERT(b);
    //set initial values for rulers
    double x1 = 0, x2 = 10, y1 = 0, y2 = 10;
    hruler.SetMinMax(x1, x2, TRUE);
    vruler.SetMinMax(y1, y2, TRUE);

    //create graph panel
    graph_panel = new CGraphPanel(&vruler, &hruler);
    b = graph_panel->Create(NULL, _T(""), WS_CHILD | WS_VISIBLE, rect, this, 0);
    ASSERT(b);

    return 0;
}

void CGraphContainer::OnSize(UINT nType, int cx, int cy) 
{
    CWnd::OnSize(nType, cx, cy);

    CDC* dc = GetDC();

    DWORD hStyle=hruler.GetStyle();
    DWORD vStyle=vruler.GetStyle();
    int rwx= (hStyle & WS_VISIBLE)==0 ? 0 : vruler.Width(dc);
    int rwy= (vStyle & WS_VISIBLE)==0 ? 0 : hruler.Width(dc);
    vruler.MoveWindow(0,0,rwx,cy);
    hruler.MoveWindow(0,0,cx,rwy);

    graph_panel->MoveWindow(rwx, rwy, cx-rwx, cy-rwy);

    ReleaseDC(dc);
}

BOOL CGraphContainer::OnEraseBkgnd(CDC* pDC) 
{
    return FALSE;
}

void CGraphContainer::UpdateViews(DWORD message_id, void* param)
{
    if (graph_panel != NULL)
    {
	graph_panel->SendMessage(CM_GRAPH_NOTIFICATION, message_id, (DWORD)param);
    };
}

void CGraphContainer::UpdateWindows(unsigned long what_to_update)
{
    if (graph_panel != NULL)
    {
	graph_panel->SendMessage(CM_GRAPH_NOTIFICATION, GRAPH_GRAPHIX_UPDATE, what_to_update);
    };
}

DWORD CGraphContainer::GetGraphFlags()
{
    DWORD res_flags = 0;
    if (graph_panel != NULL)
    {
	res_flags = graph_panel->GetGraphixFlags();
    };
    return res_flags;
}

void CGraphContainer::GetGraphWorldCoords(double* x1, double* x2, double* y1, double* y2)
{
    if (graph_panel != NULL)
    {
	graph_panel->GetWorldCoords(x1, x2, y1, y2);
    };
}


LRESULT CGraphContainer::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
    if (message == WM_PRINTCLIENT)
    {
	HDC dc_to_draw = (HDC)wParam;
	CDC* this_dc = GetWindowDC();
	if (this_dc != NULL)
	{
	    CRect rect;
	    GetClientRect(rect);
	    BitBlt(dc_to_draw, 0, 0, rect.Width(), rect.Height(), this_dc->m_hDC, 0, 0, SRCCOPY);
	};
    };

    return CWnd::WindowProc(message, wParam, lParam);
}

void CGraphContainer::DrawGraphToDC(CDC* dest_dc, CRect& rect_to_draw)
{
    if (graph_panel != NULL)
    {
	CRect hruler_rect(rect_to_draw.left, rect_to_draw.top, rect_to_draw.right, rect_to_draw.top + hruler.Width(dest_dc));
	CRect vruler_rect(rect_to_draw.left, rect_to_draw.top, rect_to_draw.left + vruler.Width(dest_dc), rect_to_draw.bottom);
	CRect graph_rect(rect_to_draw.left + vruler.Width(dest_dc), rect_to_draw.top + hruler.Width(dest_dc), rect_to_draw.right, rect_to_draw.bottom);

	graph_panel->DrawToDC(dest_dc, graph_rect);
	vruler.DrawRuler(dest_dc, vruler_rect);
	hruler.DrawRuler(dest_dc, hruler_rect);
    };
}

