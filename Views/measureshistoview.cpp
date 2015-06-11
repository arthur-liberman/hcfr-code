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

// MeasuresHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "MeasuresHistoView.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
 
/////////////////////////////////////////////////////////////////////////////
// CMeasuresHistoView

IMPLEMENT_DYNCREATE(CMeasuresHistoView, CSavingView)

CMeasuresHistoView::CMeasuresHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CMeasuresHistoView)
	//}}AFX_DATA_INIT
	int		nMaxX=10;
	CString	Msg;
	m_XScale=GetConfig()->GetProfileInt("Combo Histo","XScale",1);

	if ( m_XScale == 1 )
		nMaxX = 20;
	else if ( m_XScale == 2 )
		nMaxX = 50;

	Msg.LoadString ( IDS_LUMINANCE );
	m_luminanceGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg);
	m_LumaMaxY = 1.0;
	m_graphCtrl.SetScale(0,nMaxX,0,m_LumaMaxY);
	m_graphCtrl.SetXAxisProps("", 1, 0, 50);
	m_graphCtrl.SetYAxisProps("", 0.1, 0, 2.0);
	m_graphCtrl.ReadSettings("Combo Luminance Histo");
	m_graphCtrl.SetYScale(0,m_LumaMaxY);
	m_graphCtrl.m_doShowDataLabel = FALSE;
	m_graphCtrl.m_doShowXLabel = FALSE;
	m_graphCtrl.m_doGradientBg = FALSE;

	Msg.LoadString ( IDS_RGBREFERENCE );
	m_refGraphID = m_graphCtrl1.AddGraph(RGB(230,230,230), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_RGBLEVELRED );
	m_redGraphID = m_graphCtrl1.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_RGBLEVELGREEN );
	m_greenGraphID = m_graphCtrl1.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_RGBLEVELBLUE );
	m_blueGraphID = m_graphCtrl1.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg);
	m_graphCtrl1.SetScale(0,nMaxX,0,200);
	m_graphCtrl1.SetXAxisProps("", 1, 0, 50);
	m_graphCtrl1.SetYAxisProps("%", 20, 0, 400);
	m_graphCtrl1.ReadSettings("Combo RGB Histo");
	m_graphCtrl1.m_doShowDataLabel = FALSE;
	m_graphCtrl1.m_doShowXLabel = FALSE;
	m_graphCtrl1.m_doGradientBg = FALSE;

	Msg.LoadString ( IDS_DELTAE );
	m_deltaEGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);
	m_graphCtrl2.SetScale(0,nMaxX,0,20);
	m_graphCtrl2.SetXAxisProps("", 1, 0, 50);
	m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
	m_graphCtrl2.ReadSettings("Combo RGB Histo2");
	m_graphCtrl2.m_doShowDataLabel = FALSE;
	m_graphCtrl2.m_doShowXLabel = FALSE;
	m_graphCtrl2.m_doGradientBg = FALSE;

	Msg.LoadString ( IDS_COLORTEMP );
	m_ColorTempGraphID = m_graphCtrl3.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_COLORTEMPREF );
	m_refGraphID3 = m_graphCtrl3.AddGraph(RGB(230,230,230), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	m_graphCtrl3.SetScale(0,nMaxX,3000,9500);
	m_graphCtrl3.SetXAxisProps("", 1, 0, 50);
	m_graphCtrl3.SetYAxisProps("K", 500, 1500, 15000);
	m_graphCtrl3.ReadSettings("Combo ColorTemp Histo");
	m_graphCtrl3.m_doShowDataLabel = FALSE;
	m_graphCtrl3.m_doShowXLabel = FALSE;
	m_graphCtrl3.m_doGradientBg = FALSE;

	m_showReference=GetConfig()->GetProfileInt("Combo Histo","Show Reference",TRUE);
	m_showLuminance=GetConfig()->GetProfileInt("Combo Histo","Show Luminance",TRUE);
	m_showDeltaE=GetConfig()->GetProfileInt("Combo Histo","Show Delta E",TRUE);
	m_showColorTemp=GetConfig()->GetProfileInt("Combo Histo","Show Color Temperature",TRUE);
}

CMeasuresHistoView::~CMeasuresHistoView()
{
}

BEGIN_MESSAGE_MAP(CMeasuresHistoView, CSavingView)
	//{{AFX_MSG_MAP(CMeasuresHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_COLORTEMP_GRAPH_REFERENCE, OnColorTempGraphShowRef)
	ON_COMMAND(IDM_GRAPH_LUMINANCE, OnGraphShowLuminance)
	ON_COMMAND(IDM_RGB_GRAPH_DELTAE, OnGraphShowDeltaE)
	ON_COMMAND(IDM_GRAPH_COLORTEMP, OnGraphShowColorTemp)
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE1, OnRGBGraphYScale1)
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE2, OnRGBGraphYScale2)
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE3, OnRGBGraphYScale3)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE1, OnDeltaEGraphYScale1)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE2, OnDeltaEGraphYScale2)
	ON_COMMAND(IDM_COLORTEMP_GRAPH_Y_SCALE1, OnColortempGraphYScale1)
	ON_COMMAND(IDM_COLORTEMP_GRAPH_Y_SCALE2, OnColortempGraphYScale2)
	ON_COMMAND(IDM_GRAPH_SCALE_X_10,OnGraphXScale10)
	ON_COMMAND(IDM_GRAPH_SCALE_X_20,OnGraphXScale20)
	ON_COMMAND(IDM_GRAPH_SCALE_X_50,OnGraphXScale50)
	ON_COMMAND(IDM_GRAPH_SETTINGS, OnGraphSettings)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMeasuresHistoView diagnostics

#ifdef _DEBUG
void CMeasuresHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CMeasuresHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMeasuresHistoView message handlers

void CMeasuresHistoView::OnInitialUpdate() 
{
	int				i, NbGraphCtrl;
	int				IdGraphCtrl [ 4 ];
	LPCTSTR			pGraphName [ 4 ];			
	CGraphControl *	pGraphCtrl [ 4 ];
	CRect			rect, rect2;

	CSavingView::OnInitialUpdate();
	
	GetClientRect(&rect);	// fill entire window with three graphs

	NbGraphCtrl = 0;

	IdGraphCtrl [ NbGraphCtrl ] = IDC_LUMINANCEHISTO_GRAPH;
	pGraphName  [ NbGraphCtrl ] = _T("Luminance");
	pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl;

	IdGraphCtrl [ NbGraphCtrl ] = IDC_RGBHISTO_GRAPH;
	pGraphName  [ NbGraphCtrl ] = _T("RGB histo");
	pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl1;

	IdGraphCtrl [ NbGraphCtrl ] = IDC_RGBHISTO_GRAPH2;
	pGraphName  [ NbGraphCtrl ] = _T("Delta E");
	pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl2;

	IdGraphCtrl [ NbGraphCtrl ] = IDC_COLORTEMPHISTO_GRAPH;
	pGraphName  [ NbGraphCtrl ] = _T("Color Temp");
	pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl3;

	rect2.left = rect.left + 14;
	rect2.right = rect.right;
	for ( i = 0; i < NbGraphCtrl ; i ++ )
	{
		rect2.top = rect.top + ( i * (rect.bottom - rect.top) / NbGraphCtrl );
		rect2.bottom = rect.top + ( ( i + 1 ) * (rect.bottom - rect.top) / NbGraphCtrl );

		pGraphCtrl [ i ] -> Create ( pGraphName [ i ], rect2, this, IdGraphCtrl [ i ] );
	}
	
	OnSize(0,rect.right,rect.bottom);

	OnUpdate(NULL,NULL,NULL);
}

void CMeasuresHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	int	i, j;
	int	nStart, nEnd;
	int	nMaxX=10;

	if ( m_XScale == 1 )
		nMaxX = 20;
	else if ( m_XScale == 2 )
		nMaxX = 50;
	
	if(IsWindow(m_graphCtrl.m_hWnd))
		m_graphCtrl.ShowWindow(m_showLuminance ? SW_SHOW : SW_HIDE);

	if(IsWindow(m_graphCtrl2.m_hWnd))
		m_graphCtrl2.ShowWindow(m_showDeltaE ? SW_SHOW : SW_HIDE);

	if(IsWindow(m_graphCtrl3.m_hWnd))
		m_graphCtrl3.ShowWindow(m_showColorTemp ? SW_SHOW : SW_HIDE);

	m_graphCtrl.ClearGraph(m_luminanceGraphID);
	m_graphCtrl1.ClearGraph(m_refGraphID);
	m_graphCtrl1.ClearGraph(m_redGraphID);
	m_graphCtrl1.ClearGraph(m_greenGraphID);
	m_graphCtrl1.ClearGraph(m_blueGraphID);
	m_graphCtrl2.ClearGraph(m_deltaEGraphID);
	m_graphCtrl3.ClearGraph(m_refGraphID3);
	m_graphCtrl3.ClearGraph(m_ColorTempGraphID);

	if (m_showReference )
	{	
		m_graphCtrl1.AddPoint(m_refGraphID, 0, 100);
		m_graphCtrl1.AddPoint(m_refGraphID, nMaxX, 100);

		m_graphCtrl3.AddPoint(m_refGraphID3, 0, GetColorReference().GetWhite().GetColorTemp(GetColorReference()));
		m_graphCtrl3.AddPoint(m_refGraphID3, nMaxX, GetColorReference().GetWhite().GetColorTemp(GetColorReference()));
	}

	int size=GetDocument()->GetMeasure()->GetMeasurementsSize();

	nEnd = size;
	nStart = max(0,nEnd-(nMaxX+1));

	double YMax = 0;

	for (i=nStart, j=0; i<nEnd; i++, j++)
	{
		int colorTemp=GetDocument()->GetMeasure()->GetMeasurement(i).GetXYZValue().GetColorTemp(GetColorReference());
		double Y=GetDocument()->GetMeasure()->GetMeasurement(i).GetXYZValue()[1];
		
		if ( Y > YMax )
			YMax = Y;

		ColorxyY aColor=GetDocument()->GetMeasure()->GetMeasurement(i).GetxyYValue();

		ColorXYZ aMeasure(aColor[0]/aColor[1], 1.0, (1.0-(aColor[0]+aColor[1]))/aColor[1]);
		ColorRGB normColor(aMeasure, GetColorReference());

		m_graphCtrl.AddPoint(m_luminanceGraphID, j, Y);

		m_graphCtrl1.AddPoint(m_redGraphID, j, normColor[0]*100.0);
		m_graphCtrl1.AddPoint(m_greenGraphID, j, normColor[1]*100.0);
		m_graphCtrl1.AddPoint(m_blueGraphID, j, normColor[2]*100.0);
        //don't know reference Y so use old dE formula here
		m_graphCtrl2.AddPoint(m_deltaEGraphID, j, GetDocument()->GetMeasure()->GetMeasurement(i).GetDeltaE(GetColorReference().GetWhite())); //stick with chromiticity only dE

		if(colorTemp > 1500 && colorTemp < 12000)
			m_graphCtrl3.AddPoint(m_ColorTempGraphID, j, colorTemp);
	}

	if ( YMax > m_LumaMaxY || YMax < m_LumaMaxY / 4.0 )
	{
		int		nTensScale = (int) floor(log10(YMax));
		int		nFactor = (int) ( YMax / pow ( 10.0, nTensScale ) ) + 1;	// Between 1 and 10

		m_LumaMaxY = pow ( 10.0, (double) nTensScale ) * (double) nFactor;
		m_graphCtrl.SetYScale(0,m_LumaMaxY);
		
		m_graphCtrl.SetYAxisProps("", pow ( 10.0, (double) ( nFactor < 3 ? nTensScale - 1 : nTensScale ) ), 0, m_LumaMaxY * 2.0);
	}

	Invalidate(TRUE);
}

DWORD CMeasuresHistoView::GetUserInfo ()
{
	return	( ( m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_showLuminance	& 0x0001 )	<< 1 )
		  + ( ( m_showDeltaE	& 0x0001 )	<< 2 )
		  + ( ( m_showColorTemp	& 0x0001 )	<< 3 )
		  + ( ( m_XScale		& 0x0003 )	<< 4 );	// 2 bits
}

void CMeasuresHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_showLuminance	= ( dwUserInfo >> 1 ) & 0x0001;
	m_showDeltaE	= ( dwUserInfo >> 2 ) & 0x0001;
	m_showColorTemp	= ( dwUserInfo >> 3 ) & 0x0001;
	m_XScale		= ( dwUserInfo >> 4 ) & 0x0003;	// 2 bits

	int	nMaxX=10;

	if ( m_XScale == 1 )
		nMaxX = 20;
	else if ( m_XScale == 2 )
		nMaxX = 50;

	m_graphCtrl.SetXScale(0,nMaxX);
	m_graphCtrl1.SetXScale(0,nMaxX);
	m_graphCtrl2.SetXScale(0,nMaxX);
	m_graphCtrl3.SetXScale(0,nMaxX);
}

void CMeasuresHistoView::OnSize(UINT nType, int cx, int cy) 
{
	int				i, NbGraphCtrl;
	CGraphControl *	pGraphCtrl [ 4 ];

	CSavingView::OnSize(nType, cx, cy);

	NbGraphCtrl = 0;

	if ( m_showLuminance )
		pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl;

	pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl1;

	if ( m_showDeltaE )
		pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl2;

	if ( m_showColorTemp )
		pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl3;

	for ( i = 0; i < NbGraphCtrl ; i ++ )
	{
		if(IsWindow(pGraphCtrl[i]->m_hWnd))
			pGraphCtrl [ i ] -> MoveWindow ( 14, i * cy / NbGraphCtrl, cx - 14, cy/NbGraphCtrl + 1 );
	}
}

BOOL CMeasuresHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

void CMeasuresHistoView::OnDraw(CDC* pDC) 
{
	// Draw texts
	int			i, NbGraphCtrl;
	BOOL		bWhiteBkgnd = GetConfig () -> m_bWhiteBkgndOnScreen;
	LPCSTR		lpszTexts [ 4 ];
	COLORREF	clr [ 4 ];
	RECT		rect, rect2;
	CSize		size;
	CFont		font;
	CFont *		pOldFont;

	GetClientRect(&rect);	// fill entire window with three graphs
	
	NbGraphCtrl = 0;

	if ( m_showLuminance )
	{
		clr [ NbGraphCtrl ] = bWhiteBkgnd ? RGB (127,127,0) : RGB(255,255,0);
		lpszTexts [ NbGraphCtrl ++ ] = "Luminance";
	}

	clr [ NbGraphCtrl ] = bWhiteBkgnd ? RGB(0,0,0) : RGB(255,255,255);
	lpszTexts [ NbGraphCtrl ++ ] = "RGB Levels";
	
	if ( m_showDeltaE )
	{
		clr [ NbGraphCtrl ] = RGB(255,0,255);
		lpszTexts [ NbGraphCtrl ++ ] = "Delta E";
	}

	if ( m_showColorTemp )
	{
		clr [ NbGraphCtrl ] = bWhiteBkgnd ? RGB(0,192,192) : RGB(0,255,255);
		lpszTexts [ NbGraphCtrl ++ ] = "Color temp";
	}

	// Declare a "vertical" font
	font.CreateFont( 11, 0, 900, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH | FF_DONTCARE, "Arial" );

	pOldFont = pDC -> SelectObject ( & font );
	pDC -> SetBkColor ( bWhiteBkgnd?RGB(255,255,255):RGB(0,0,0) );

	rect2.left = 0;
	rect2.right = 14;
	for ( i = 0; i < NbGraphCtrl ; i ++ )
	{
		pDC -> SetTextColor ( clr [ i ] );
		rect2.top = i * rect.bottom / NbGraphCtrl;
		rect2.bottom = (i + 1) * rect.bottom / NbGraphCtrl;
		pDC -> FillSolidRect ( & rect2, bWhiteBkgnd?RGB(255,255,255):RGB(0,0,0) );
		size = pDC -> GetTextExtent ( lpszTexts [ i ], strlen ( lpszTexts [ i ] ) );
		pDC -> TextOut ( rect2.left + 1, rect2.top + ( rect2.bottom - rect2.top + size.cx ) / 2, lpszTexts [ i ], strlen ( lpszTexts [ i ] ) );
	}

	pDC -> SetTextColor ( RGB(0,0,0) );
	pDC -> SetBkColor ( RGB(255,255,255) );
	pDC -> SelectObject ( pOldFont );
}

void CMeasuresHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_MEASURES_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_GRAPH_LUMINANCE, m_showLuminance ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_RGB_GRAPH_DELTAE, m_showDeltaE ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_GRAPH_COLORTEMP, m_showColorTemp ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_COLORTEMP_GRAPH_REFERENCE, m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CMeasuresHistoView::OnGraphShowLuminance() 
{
	m_showLuminance = !m_showLuminance;
	GetConfig()->WriteProfileInt("Combo Histo","Show Luminance",m_showLuminance);

	m_graphCtrl.ShowWindow(m_showLuminance ? SW_SHOW : SW_HIDE);

	CRect rect;
	GetClientRect(&rect);
	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to visible graphs

	OnUpdate(NULL,NULL,NULL);
}

void CMeasuresHistoView::OnGraphShowDeltaE() 
{
	m_showDeltaE = !m_showDeltaE;
	GetConfig()->WriteProfileInt("Combo Histo","Show Delta E",m_showDeltaE);

	m_graphCtrl2.ShowWindow(m_showDeltaE ? SW_SHOW : SW_HIDE);

	CRect rect;
	GetClientRect(&rect);
	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to visible graphs

	OnUpdate(NULL,NULL,NULL);
}

void CMeasuresHistoView::OnGraphShowColorTemp() 
{
	m_showColorTemp = !m_showColorTemp;
	GetConfig()->WriteProfileInt("Combo Histo","Show Color Temperature",m_showColorTemp);

	m_graphCtrl3.ShowWindow(m_showColorTemp ? SW_SHOW : SW_HIDE);

	CRect rect;
	GetClientRect(&rect);
	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to visible graphs

	OnUpdate(NULL,NULL,NULL);
}

void CMeasuresHistoView::OnColorTempGraphShowRef() 
{
	m_showReference = !m_showReference;
	GetConfig()->WriteProfileInt("Combo Histo","Show Reference",m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CMeasuresHistoView::OnGraphSave() 
{
	int				NbGraphCtrl;
	CGraphControl *	pGraphCtrl [ 4 ] = { NULL, NULL, NULL, NULL };

	NbGraphCtrl = 0;

	if ( m_showLuminance )
		pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl;

	pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl1;

	if ( m_showDeltaE )
		pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl2;

	if ( m_showColorTemp )
		pGraphCtrl [ NbGraphCtrl ++ ] = & m_graphCtrl3;

	pGraphCtrl [ 0 ] -> SaveGraphs(pGraphCtrl[1],pGraphCtrl[2],pGraphCtrl[3]);
}

void CMeasuresHistoView::OnGraphSettings() 
{
	int tmpGraphID1=m_graphCtrl.AddGraph(m_graphCtrl1.m_graphArray[m_refGraphID]);
	int tmpGraphID2=m_graphCtrl.AddGraph(m_graphCtrl1.m_graphArray[m_redGraphID]);
	int tmpGraphID3=m_graphCtrl.AddGraph(m_graphCtrl1.m_graphArray[m_greenGraphID]);
	int tmpGraphID4=m_graphCtrl.AddGraph(m_graphCtrl1.m_graphArray[m_blueGraphID]);
	int tmpGraphID5=m_graphCtrl.AddGraph(m_graphCtrl2.m_graphArray[m_deltaEGraphID]);
	int tmpGraphID6=m_graphCtrl.AddGraph(m_graphCtrl3.m_graphArray[m_ColorTempGraphID]);
	int tmpGraphID7=m_graphCtrl.AddGraph(m_graphCtrl3.m_graphArray[m_refGraphID3]);

	m_graphCtrl.ClearGraph ( tmpGraphID1 );
	m_graphCtrl.ClearGraph ( tmpGraphID2 );
	m_graphCtrl.ClearGraph ( tmpGraphID3 );
	m_graphCtrl.ClearGraph ( tmpGraphID4 );
	m_graphCtrl.ClearGraph ( tmpGraphID5 );
	m_graphCtrl.ClearGraph ( tmpGraphID6 );
	m_graphCtrl.ClearGraph ( tmpGraphID7 );

	m_graphCtrl.ChangeSettings();

	// Update other graph setting according to graph and values of tmpGraphIDs
	m_graphCtrl1.CopySettings(m_graphCtrl,tmpGraphID1,m_refGraphID);
	m_graphCtrl1.CopySettings(m_graphCtrl,tmpGraphID2,m_redGraphID);
	m_graphCtrl1.CopySettings(m_graphCtrl,tmpGraphID3,m_greenGraphID);
	m_graphCtrl1.CopySettings(m_graphCtrl,tmpGraphID4,m_blueGraphID);
	m_graphCtrl2.CopySettings(m_graphCtrl,tmpGraphID5,m_deltaEGraphID);
	m_graphCtrl3.CopySettings(m_graphCtrl,tmpGraphID6,m_ColorTempGraphID);
	m_graphCtrl3.CopySettings(m_graphCtrl,tmpGraphID7,m_refGraphID3);

	m_graphCtrl.RemoveGraph(tmpGraphID7);
	m_graphCtrl.RemoveGraph(tmpGraphID6);
	m_graphCtrl.RemoveGraph(tmpGraphID5);
	m_graphCtrl.RemoveGraph(tmpGraphID4);
	m_graphCtrl.RemoveGraph(tmpGraphID3);
	m_graphCtrl.RemoveGraph(tmpGraphID2);
	m_graphCtrl.RemoveGraph(tmpGraphID1);

	m_graphCtrl.WriteSettings("Combo Luminance Histo");
	m_graphCtrl1.WriteSettings("Combo RGB Histo");
	m_graphCtrl2.WriteSettings("Combo RGB Histo2");
	m_graphCtrl3.WriteSettings("Combo ColorTemp Histo");

	OnUpdate(NULL,NULL,NULL);
}

void CMeasuresHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_FREE_MEASURES, NULL );
}

void CMeasuresHistoView::OnRGBGraphYScale1() 
{
	m_graphCtrl1.SetYScale(0,200);
	m_graphCtrl1.SetYAxisProps("%", 40, 0, 400);
	Invalidate(TRUE);
}

void CMeasuresHistoView::OnRGBGraphYScale2() 
{
	m_graphCtrl1.SetYScale(50,150);
	m_graphCtrl1.SetYAxisProps("%", 20, 0, 400);
	Invalidate(TRUE);
}

void CMeasuresHistoView::OnRGBGraphYScale3() 
{
	m_graphCtrl1.SetYScale(80,120);
	m_graphCtrl1.SetYAxisProps("%", 10, 0, 400);
	Invalidate(TRUE);
}

void CMeasuresHistoView::OnDeltaEGraphYScale1() 
{
	m_graphCtrl2.SetYScale(0,20);
	m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
	Invalidate(TRUE);
}

void CMeasuresHistoView::OnDeltaEGraphYScale2() 
{
	m_graphCtrl2.SetYScale(0,5);
	m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
	Invalidate(TRUE);
}

void CMeasuresHistoView::OnColortempGraphYScale1() 
{
	m_graphCtrl3.SetYScale(3000,9500);
	Invalidate(TRUE);
}

void CMeasuresHistoView::OnColortempGraphYScale2() 
{
	m_graphCtrl3.SetYScale(4000,8000);
	Invalidate(TRUE);
}

void CMeasuresHistoView::OnGraphXScale10()
{
	m_XScale=0;
	GetConfig()->WriteProfileInt("Combo Histo","XScale",m_XScale);
	m_graphCtrl.SetXScale(0,10);
	m_graphCtrl1.SetXScale(0,10);
	m_graphCtrl2.SetXScale(0,10);
	m_graphCtrl3.SetXScale(0,10);
	OnUpdate(NULL,NULL,NULL);
}

void CMeasuresHistoView::OnGraphXScale20()
{
	m_XScale=1;
	GetConfig()->WriteProfileInt("Combo Histo","XScale",m_XScale);
	m_graphCtrl.SetXScale(0,20);
	m_graphCtrl1.SetXScale(0,20);
	m_graphCtrl2.SetXScale(0,20);
	m_graphCtrl3.SetXScale(0,20);
	OnUpdate(NULL,NULL,NULL);
}

void CMeasuresHistoView::OnGraphXScale50()
{
	m_XScale=2;
	GetConfig()->WriteProfileInt("Combo Histo","XScale",m_XScale);
	m_graphCtrl.SetXScale(0,50);
	m_graphCtrl1.SetXScale(0,50);
	m_graphCtrl2.SetXScale(0,50);
	m_graphCtrl3.SetXScale(0,50);
	OnUpdate(NULL,NULL,NULL);
}

