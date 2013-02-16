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

// TargetWnd.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "TargetWnd.h"
#include "Color.h"
#include "BitmapTools.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTargetWnd

CTargetWnd::CTargetWnd()
{
	m_pointBitmap.LoadBitmap(IDB_POINT_BITMAP);
	m_deltax=0.0;
	m_deltay=0.0;
	m_marginInPercent=15;
	m_targetRectInPercent=10;
	m_pointSizeInPercent=50;
	m_prev_cx = -1; 
	m_prev_cy = -1; 
	m_pRefColor = NULL;
	pTooltipText = NULL;
}

CTargetWnd::~CTargetWnd()
{
}

void CTargetWnd::Refresh()
{
	if ( m_pRefColor )
	{
        ColorXYZ centerXYZ = GetColorReference().GetWhite();
		m_clr = RGB(64,64,64);

		if ( m_pRefColor -> GetDeltaE ( GetColorReference().GetRed() ) < (GetColorReference().m_standard==4 ? 30:75) )
		{
			centerXYZ = GetColorReference().GetRed();
			if (GetColorReference().m_standard != 4)
   			  m_clr = RGB(192,0,0);
			else
			  m_clr = RGB(222,188,175);
		}
		else if ( m_pRefColor -> GetDeltaE ( GetColorReference().GetGreen() ) < (GetColorReference().m_standard==4 ? 30:75) )
		{
			centerXYZ = GetColorReference().GetGreen();
			if (GetColorReference().m_standard != 4)
   			  m_clr = RGB(0,192,0);
			else
			  m_clr = RGB(145,170,193);
		}
		else if ( m_pRefColor -> GetDeltaE ( GetColorReference().GetBlue() ) < (GetColorReference().m_standard==4 ? 30:75) )
		{
			centerXYZ = GetColorReference().GetBlue();
			if (GetColorReference().m_standard != 4)
   			  m_clr = RGB(0,0,192);
			else
			  m_clr = RGB(144,158,122);
		}
		else if ( m_pRefColor -> GetDeltaE ( GetColorReference().GetYellow() ) < (GetColorReference().m_standard==4 ? 30:75) )
		{
			centerXYZ = GetColorReference().GetYellow();
			if (GetColorReference().m_standard != 4)
   			  m_clr = RGB(192,192,0);
			else
			  m_clr = RGB(125,145,203);
		}
		else if ( m_pRefColor -> GetDeltaE ( GetColorReference().GetCyan() ) < (GetColorReference().m_standard==4 ? 30:75) )
		{
			centerXYZ = GetColorReference().GetCyan();
			if (GetColorReference().m_standard != 4)
   			  m_clr = RGB(0,192,192);
			else
			  m_clr = RGB(196,216,120);
		}
		else if ( m_pRefColor -> GetDeltaE ( GetColorReference().GetMagenta() ) < (GetColorReference().m_standard==4 ? 30:75) )
		{
			centerXYZ = GetColorReference().GetMagenta();
			if (GetColorReference().m_standard != 4)
   			  m_clr = RGB(192,0,192);
			else
			  m_clr = RGB(240,197,96);
		}

        ColorxyY aColor = m_pRefColor -> GetxyYValue();
        ColorxyY centerxyY(centerXYZ);

		m_deltax = (aColor[0]-centerxyY[0])/centerxyY[0];
		m_deltay = (aColor[1]-centerxyY[1])/centerxyY[1];
		
		if ( m_tooltip.IsWindowVisible() )
		{
			if ( pTooltipText )
			{
				pTooltipText -> Format("<b>delta x</b>: %.1f%% <br><b>delta y</b>: %.1f%%",m_deltax*100.0,m_deltay*100.0);
				m_tooltip.Invalidate();
			}
		}
	}
	UpdateScaledBitmap();
	Invalidate(TRUE);
}


BEGIN_MESSAGE_MAP(CTargetWnd, CWnd)
	//{{AFX_MSG_MAP(CTargetWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	//}}AFX_MSG_MAP
	ON_NOTIFY (UDM_TOOLTIP_DISPLAY, NULL, NotifyDisplayTooltip)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTargetWnd message handlers

void CTargetWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
    CHMemDC pDC(&dc);
	
	CRect rect;
	GetClientRect(&rect);

	// Draw background bitmap
	CDC dcBg;
	dcBg.CreateCompatibleDC(pDC);
	CBitmap *pOldBitmap=dcBg.SelectObject(&m_bgBitmap);
	pDC->BitBlt(0,0,rect.Width(),rect.Height(),&dcBg,0,0,SRCCOPY);
	dcBg.SelectObject(pOldBitmap);

	// Draw target frame
	int widthMargin=rect.Width()*m_marginInPercent/100;
	int heightMargin=rect.Height()*m_marginInPercent/100;

	CRect frameRect(rect.left+widthMargin,rect.top+heightMargin,rect.right-widthMargin,rect.bottom-heightMargin);
	
	CBrush br(m_clr);
	pDC->SelectObject ( & br );
	pDC->SelectObject ( GetStockObject ( WHITE_PEN ) );
	pDC->RoundRect ( & frameRect, CPoint ( 20, 20 ) );
	
	int targetRectWidth=rect.Width()*m_targetRectInPercent/100;
	int targetRectHeight=rect.Height()*m_targetRectInPercent/100;

    CPen crossPen(PS_SOLID,1,RGB(255,255,255));
    CPen *pOldPen = pDC->SelectObject(&crossPen); 

	// auto-zoom
	double zoomFactor=GetZoomFactor();

	int cornerWidth=(int)(zoomFactor*targetRectWidth/2.0);
	int cornerHeight=(int)(zoomFactor*targetRectHeight/2.0);
	
	// draw cross 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x,rect.CenterPoint().y+2*cornerHeight); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x+2*cornerWidth,rect.CenterPoint().y); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x,rect.CenterPoint().y-2*cornerHeight); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x-2*cornerWidth,rect.CenterPoint().y); 

	// draw circle
	pDC->SelectObject ( GetStockObject ( NULL_BRUSH ) );
	pDC->Ellipse ( rect.CenterPoint().x-cornerWidth, rect.CenterPoint().y-cornerHeight, rect.CenterPoint().x+cornerWidth, rect.CenterPoint().y+cornerHeight );
    pDC->SelectObject(pOldPen);

    int r1=0,g1=255,b1=119;
    int r2=255,g2=192,b2=0;
    int r3=0,g3=80,b3=255;
    int r4=255,g4=0,b4=136;

	// draw corners
	for(int i=1;i<8;i++)
	{
		if(rect.CenterPoint().x-(i+1)*cornerWidth > frameRect.left + 4)	// Clipping
		{
			CPen cornerPen1(PS_SOLID,1,RGB(r1,g1,b1));
			pOldPen = pDC->SelectObject(&cornerPen1); 
			pDC -> Arc ( rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight, 
						 rect.CenterPoint().x-(i+1)*cornerWidth/2, rect.CenterPoint().y-(i+1)*cornerHeight,
						 rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight/2 ); 

			pDC->SelectObject(pOldPen);

			CPen cornerPen2(PS_SOLID,1,RGB(r2,g2,b2));
			pOldPen = pDC->SelectObject(&cornerPen2); 
			pDC -> Arc ( rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight/2,
						 rect.CenterPoint().x+(i+1)*cornerWidth/2, rect.CenterPoint().y-(i+1)*cornerHeight );

			pDC->SelectObject(pOldPen);

			CPen cornerPen3(PS_SOLID,1,RGB(r3,g3,b3));
			pOldPen = pDC->SelectObject(&cornerPen3); 
			pDC -> Arc ( rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight, 
						 rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight/2,
						 rect.CenterPoint().x-(i+1)*cornerWidth/2, rect.CenterPoint().y+(i+1)*cornerHeight ); 

			pDC->SelectObject(pOldPen);

			CPen cornerPen4(PS_SOLID,1,RGB(r4,g4,b4));
			pOldPen = pDC->SelectObject(&cornerPen4); 
			pDC -> Arc ( rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth/2, rect.CenterPoint().y+(i+1)*cornerHeight,
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight/2 ); 

			pDC->SelectObject(pOldPen);
		}
	}

	if( m_pRefColor == NULL || !m_pRefColor->isValid())
		return;		// Draw nothing more

	CPoint targetPoint=rect.CenterPoint();
	targetPoint+=CPoint((int)(zoomFactor*m_deltax*frameRect.Width()/2.0),-(int)(zoomFactor*m_deltay*frameRect.Height()/2.0));

	// draw Arrow
	int pointWidth=(int)(rect.Width()*m_pointSizeInPercent/100.0);
	int pointHeight=(int)(rect.Height()*m_pointSizeInPercent/100.0);

	DrawTransparentBitmap(pDC->m_hDC,(HBITMAP)m_scaledPointBitmap.m_hObject,(short)(targetPoint.x-pointWidth/4),(short)(targetPoint.y-pointHeight*3/4),RGB(0,0,0));
}

double CTargetWnd::GetZoomFactor()
{
		// auto-zoom
	double zoomFactor;
	int maxDelta=max(abs((int)(m_deltax*100.0)), abs((int)(m_deltay*100.0)) )/10;
	switch(maxDelta)
	{
		case 0:
			zoomFactor = 4.0;
			break;
		case 1:
			zoomFactor = 3.0;
			break;
		case 2:
			zoomFactor = 2.0;
			break;
		case 3:
			zoomFactor = 1.5;
			break;
		default:
			zoomFactor = 1.0;
			break;
	}
	return zoomFactor;
}

void CTargetWnd::UpdateScaledBitmap()
{

    CRect rect;
    GetClientRect(&rect);

	// draw arrow
	int pointWidth=(int)(rect.Width()*m_pointSizeInPercent/100.0);
	int pointHeight=(int)(rect.Height()*m_pointSizeInPercent/100.0);

    CPaintDC dc(this);

	BITMAP bm;
	m_pointBitmap.GetBitmap(&bm);

	if(m_scaledPointBitmap.m_hObject)
		m_scaledPointBitmap.DeleteObject();
	m_scaledPointBitmap.CreateCompatibleBitmap(&dc,pointWidth,pointHeight);


	CDC memDCSrc;
	memDCSrc.CreateCompatibleDC( &dc );
	CBitmap* pOld = memDCSrc.SelectObject(&m_pointBitmap);
	int oldMode=memDCSrc.GetStretchBltMode();

	CDC memDCScaled;
	memDCScaled.CreateCompatibleDC( &dc );
	memDCScaled.SetStretchBltMode(HALFTONE);
   // The docs say that you should call SetBrushOrgEx after SetStretchBltMode,
   // but not what the arguments should be.
    SetBrushOrgEx(memDCScaled, 0,0, NULL);
	CBitmap* pOldScaled = memDCScaled.SelectObject(&m_scaledPointBitmap);
	memDCScaled.StretchBlt(0,0,pointWidth,pointHeight,&memDCSrc,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);
    memDCScaled.SelectObject(pOldScaled); 

	memDCSrc.SetStretchBltMode(oldMode);
    memDCSrc.SelectObject(pOld); 
}

void CTargetWnd::MakeBgBitmap()	// Create background bitmap
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

	// windows problem with "negative" client height. Can occur when mainframe is really small
	if ( rect.bottom == 32767 )
		rect.bottom = 0;

    int r1=0,g1=255,b1=119;
    int r1b=65,g1b=255,b1b=0;
    int r2=255,g2=192,b2=0;
    int r2b=255,g2b=0,b2b=0;
    int r3=0,g3=0,b3=255;
    int r4=255,g4=0,b4=136;

    int r,g,b;

    CDC dc2;
    dc2.CreateCompatibleDC(&dc);

   if(m_bgBitmap.m_hObject)
        m_bgBitmap.DeleteObject();
    m_bgBitmap.CreateCompatibleBitmap(&dc,rect.Width(),rect.Height());

    CBitmap *pOldBitmap=dc2.SelectObject(&m_bgBitmap);

	for(int i=0; i<rect.Width() / 2;i++)
	{
        r = r1 + (i * (r1b-r1) / (rect.Width()/2) );
        g = g1 + (i * (g1b-g1) / (rect.Width()/2) );
        b = b1 + (i * (b1b-b1) / (rect.Width()/2) );

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(i,0);

        dc2.SelectObject(pOldPen);
	}

	for(int i=rect.Width() / 2; i<rect.Width();i++)
	{
        r = r1b + ( (i-(rect.Width()/2)) * (r2-r1b) / (rect.Width()/2) );
        g = g1b + ( (i-(rect.Width()/2)) * (g2-g1b) / (rect.Width()/2) );
        b = b1b + ( (i-(rect.Width()/2)) * (b2-b1b) / (rect.Width()/2) );

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(i,0);

        dc2.SelectObject(pOldPen);
	}

	for(int i=0; i<rect.Width();i++)
	{
        r = r3 + (i * (r4-r3) / rect.Width());
        g = g3 + (i * (g4-g3) / rect.Width());
        b = b3 + (i * (b4-b3) / rect.Width());

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(i,rect.Height());

        dc2.SelectObject(pOldPen);
	}

	for(int i=0; i<rect.Height();i++)
	{
        r = r1 + (i * (r3-r1) / rect.Height());
        g = g1 + (i * (g3-g1) / rect.Height());
        b = b1 + (i * (b3-b1) / rect.Height());

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(0,i);

        dc2.SelectObject(pOldPen);
	}

	for(int i=0; i<rect.Height()/2;i++)		
	{
        r = r2 + (i * (r2b-r2) / (rect.Height()/2) );
        g = g2 + (i * (g2b-g2) / (rect.Height()/2) );
        b = b2 + (i * (b2b-b2) / (rect.Height()/2) );

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(rect.Width()-1,i);	// -1 to avoid bad junction

        dc2.SelectObject(pOldPen);
	}

	for(int i=rect.Height()/2; i<rect.Height();i++)		
	{
        r = r2b + ( (i-(rect.Height()/2)) * (r4-r2b) / (rect.Height()/2) );
        g = g2b + ( (i-(rect.Height()/2)) * (g4-g2b) / (rect.Height()/2) );
        b = b2b + ( (i-(rect.Height()/2)) * (b4-b2b) / (rect.Height()/2) );

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(rect.Width()-1,i);	// -1 to avoid bad junction

        dc2.SelectObject(pOldPen);
	}

    dc2.SelectObject(pOldBitmap);
}



void CTargetWnd::OnSize(UINT nType, int cx, int cy) 
{
	if(cx != m_prev_cx || cy != m_prev_cy)
	{	
		MakeBgBitmap();
		UpdateScaledBitmap();
		Invalidate(FALSE);
	}
	m_prev_cx=cx;
	m_prev_cy=cy;
}

BOOL CTargetWnd::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);

	m_tooltip.Create(this);	
	m_tooltip.AddTool(this, "Tooltip for rectangle area");
	m_tooltip.SetBehaviour(PPTOOLTIP_MULTIPLE_SHOW);
	m_tooltip.SetNotify(TRUE);

	m_tooltip.SetTransparency(250);
//	m_tooltip.SetEffectBk(CPPDrawManager::EFFECT_METAL);

	return TRUE;
}

BOOL CTargetWnd::PreTranslateMessage(MSG* pMsg) 
{
	m_tooltip.RelayEvent(pMsg);

	return CWnd::PreTranslateMessage(pMsg);
}

void CTargetWnd::NotifyDisplayTooltip(NMHDR * pNMHDR, LRESULT * result)
{
    *result = 0;
    NM_PPTOOLTIP_DISPLAY * pNotify = (NM_PPTOOLTIP_DISPLAY*)pNMHDR;
	CString valueStr;
    
	if( m_pRefColor && (*m_pRefColor).isValid())
		valueStr.Format("<b>delta x</b>: %.1f%% <br><b>delta y</b>: %.1f%%",m_deltax*100.0,m_deltay*100.0);
	else
		valueStr="No data";
    pNotify->ti->sTooltip = valueStr;
	pTooltipText = & pNotify->ti->sTooltip;
}

BOOL CTargetWnd::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;	
}

void CTargetWnd::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CTargetWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CTRL_TARGET, NULL );
}

