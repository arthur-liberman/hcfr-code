/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/

#include "../../stdafx.h"
#include "graphcombobox.h"
#include "graph_props.h"
#include "graphwnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGraphComboBox

CGraphComboBox::CGraphComboBox()
{
}

CGraphComboBox::~CGraphComboBox()
{
}


BEGIN_MESSAGE_MAP(CGraphComboBox, CComboBox)
	//{{AFX_MSG_MAP(CGraphComboBox)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraphComboBox message handlers

void CGraphComboBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
    //get pointer to the main graph class
    CGraphWnd* main_graph_wnd = get_main_graph_window();
    ASSERT(main_graph_wnd != NULL);
    if (main_graph_wnd != NULL)
    {
	int index = lpDrawItemStruct->itemID;
	if (index >=0)
	{
	    long current_graph_id = GetItemData(index);
	    CGraphProps* graph_props = main_graph_wnd->GetGraph(current_graph_id);
	    ASSERT(graph_props != NULL);
	    DrawComboItem(graph_props->GetGraphColor(), graph_props->GetTitle(), lpDrawItemStruct);
	};
    };
}

BOOL CGraphComboBox::Create( DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID )
{
    dwStyle |= CBS_OWNERDRAWFIXED | CBS_DROPDOWNLIST | WS_VSCROLL;
    BOOL b = CComboBox::Create(dwStyle, rect, pParentWnd, nID);

    CFont cf;
    cf.Attach((HFONT)GetStockObject(DEFAULT_GUI_FONT));
    SetFont(&cf);

    return b;
}

void CGraphComboBox::DrawComboItem(COLORREF color, const TCHAR* title, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    // text colors
    COLORREF cvBack = ::SetBkColor(lpDrawItemStruct->hDC,
	    ::GetSysColor((lpDrawItemStruct->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHT : COLOR_WINDOW)); 
    COLORREF cvText = ::SetTextColor(lpDrawItemStruct->hDC,
    
    ::GetSysColor((lpDrawItemStruct->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT)); 
    // focus rectangle
    // if item has been selected
    if ((lpDrawItemStruct->itemState & ODS_SELECTED) &&	
	    (lpDrawItemStruct->itemAction & (ODA_SELECT | ODA_DRAWENTIRE)))
	    ::DrawFocusRect(lpDrawItemStruct->hDC, &lpDrawItemStruct->rcItem); 
    // if item has been deselected
    if (!(lpDrawItemStruct->itemState & ODS_SELECTED) &&
	    (lpDrawItemStruct->itemAction & ODA_SELECT))
	    ::DrawFocusRect(lpDrawItemStruct->hDC, &lpDrawItemStruct->rcItem); 
    // Background (fillSolidRect)
    CRect	rb(lpDrawItemStruct->rcItem);
    if (lpDrawItemStruct->itemState & ODS_SELECTED)
	    rb.InflateRect(-1, -1);
    ::ExtTextOut(lpDrawItemStruct->hDC, 0, 0, ETO_OPAQUE, &rb, NULL, 0, NULL);

    //prepare tools for drawing
    CDC* dc = CDC::FromHandle(lpDrawItemStruct->hDC);
    CPen pen(PS_SOLID, 1, color);
    CBrush brush(color);
    CPen* oldpen = (CPen*)dc->SelectObject(&pen);
    CBrush* oldbrush = (CBrush*)dc->SelectObject(&brush);

    //draw rectangle
    CRect r(lpDrawItemStruct->rcItem);
    r.top+=2; r.left+=2; r.bottom-=2;
    r.right = r.left+20;
    dc->Rectangle(r);
    //draw text
    CPen textpen(PS_SOLID, 1, RGB(0, 0, 0));
    dc->SelectObject(&textpen);
    int bound = r.right+3;
    r = lpDrawItemStruct->rcItem;
    r.left = bound; r.top+=3;
    dc->DrawText(title, -1, &r, DT_VCENTER | DT_LEFT);

    dc->SelectObject(oldpen);
    dc->SelectObject(oldbrush);

    // restore DC colors
    ::SetTextColor(lpDrawItemStruct->hDC, cvText); 
    ::SetBkColor(lpDrawItemStruct->hDC, cvBack); 
}

void CGraphComboBox::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
    CRect rect;
    GetClientRect(rect);
    lpMeasureItemStruct->itemWidth = rect.Width();

/*
    CFont* font = GetFont();
    LOGFONT log_font;
    font->GetLogFont(&log_font);

    double ppu = GetDeviceCaps(::GetDC(NULL), LOGPIXELSY);
    lpMeasureItemStruct->itemHeight = log_font.lfHeight/ppu;
*/
    lpMeasureItemStruct->itemHeight = 20;
}
