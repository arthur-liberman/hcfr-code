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

// LuminanceWnd.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "LuminanceWnd.h"
#include "Color.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLuminanceWnd

CLuminanceWnd::CLuminanceWnd()
{
	m_marginInPercent = 15;
	m_textColor=RGB(255,255,0);		// yellow

	m_logFont.lfHeight =		10;
	m_logFont.lfWidth =			0;
	m_logFont.lfEscapement =	0;
	m_logFont.lfOrientation =	0;
	m_logFont.lfWeight =		FW_BOLD;
	m_logFont.lfItalic =		FALSE;
	m_logFont.lfUnderline =		FALSE;
	m_logFont.lfStrikeOut =		0;
	m_logFont.lfCharSet =		ANSI_CHARSET;
	m_logFont.lfOutPrecision =	OUT_DEFAULT_PRECIS;
	m_logFont.lfClipPrecision =	CLIP_DEFAULT_PRECIS;
	m_logFont.lfQuality =		DEFAULT_QUALITY;
	strcpy(m_logFont.lfFaceName, "Arial");
}

CLuminanceWnd::~CLuminanceWnd()
{
}

void CLuminanceWnd::Refresh ( LPCSTR lpStr )
{
	m_valueStr=lpStr;
	if ( m_hWnd && IsWindowVisible () )
	{
		ComputeFontSize();
		Invalidate(FALSE);
	}
}

BEGIN_MESSAGE_MAP(CLuminanceWnd, CWnd)
	//{{AFX_MSG_MAP(CLuminanceWnd)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CLuminanceWnd message handlers

BOOL CLuminanceWnd::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

void CLuminanceWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
    CMemDC pDC(&dc);
//	CDC *pDC=&dc;

	CRect rect;
	GetClientRect(&rect);

	// Draw background
	CDC dc2;
	dc2.CreateCompatibleDC(pDC);
	CBitmap *pOldBitmap=dc2.SelectObject(&m_bgBitmap);
	pDC->BitBlt(0,0,rect.Width(),rect.Height(),&dc2,0,0,SRCCOPY);
	dc2.SelectObject(pOldBitmap);

	// Draw Text
	pDC->SetTextAlign(TA_CENTER | TA_TOP);
	pDC->SetTextColor(m_textColor);
	pDC->SetBkMode(TRANSPARENT);

	CFont font;
	VERIFY(font.CreateFontIndirect(&m_logFont));

	CFont* def_font = pDC->SelectObject(&font);
	CSize aSize=pDC->GetTextExtent(m_valueStr);
	pDC->TextOut(rect.CenterPoint().x,rect.CenterPoint().y-(aSize.cy/2),m_valueStr);
	pDC->SelectObject(def_font);

	// Done with the font.  Delete the font object.
	font.DeleteObject(); 
}

void CLuminanceWnd::ComputeFontSize()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

	m_logFont.lfHeight=10;	// Init font height to 10 to compute ratio

	// Initializes a CFont object with the specified characteristics. 
	CFont font;
	VERIFY(font.CreateFontIndirect(&m_logFont));

	CFont* def_font = dc.SelectObject(&font);
	CSize aSize=dc.GetTextExtent(m_valueStr);

	// Adjust font size to fill window
	int aDesiredWidth=rect.Width()-2*(rect.Width()*m_marginInPercent/100);
	int aNewFontSize = (int)((float)aDesiredWidth/(float)aSize.cx*(float)m_logFont.lfHeight);

		// Done with the font.  Delete the font object.
	font.DeleteObject(); 

	m_logFont.lfHeight=aNewFontSize;
}

void CLuminanceWnd::MakeBgBitmap()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

    int r1=0,g1=0,b1=0;
    int r2=180,g2=180,b2=180;

    int x1=0,y1=0;
    int x2=0,y2=0;

    CDC dc2;
    dc2.CreateCompatibleDC(&dc);

    if(m_bgBitmap.m_hObject)
        m_bgBitmap.DeleteObject();
    m_bgBitmap.CreateCompatibleBitmap(&dc,rect.Width(),
        rect.Height());

    CBitmap *oldbmap=dc2.SelectObject(&m_bgBitmap);

    while(x1 < rect.Width() && y1 < rect.Height())
    {
        if(y1 < rect.Height()-1)
            y1++;
        else
            x1++;

        if(x2 < rect.Width()-1)
            x2++;
        else
            y2++;

        int r,g,b;
        int i = x1+y1;
        r = r1 + (i * (r2-r1) / (rect.Width()+rect.Height()));
        g = g1 + (i * (g2-g1) / (rect.Width()+rect.Height()));
        b = b1 + (i * (b2-b1) / (rect.Width()+rect.Height()));

        CPen p(PS_SOLID,1,RGB(r,g,b));
        CPen *oldpen = dc2.SelectObject(&p); 

        dc2.MoveTo(x1,y1);
        dc2.LineTo(x2,y2);

        dc2.SelectObject(oldpen);
    } 

    dc2.SelectObject(oldbmap);
}

void CLuminanceWnd::OnSize(UINT nType, int cx, int cy) 
{
	MakeBgBitmap();
	ComputeFontSize();
	CWnd::OnSize(nType, cx, cy);
	Invalidate(FALSE);
}

void CLuminanceWnd::OnClose()
{
	ShowWindow ( SW_HIDE );
}

void CLuminanceWnd::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CLuminanceWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CTRL_LUMINANCE, NULL );
}

BOOL CLuminanceWnd::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	OnHelp ();
	return TRUE;
}


