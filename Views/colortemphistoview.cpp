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
//	Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// ColorTempHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "ColorTempHistoView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
 
/////////////////////////////////////////////////////////////////////////////
// CColorTempGrapher

CColorTempGrapher::CColorTempGrapher()
{
	CString	Msg;

	Msg.LoadString ( IDS_COLORTEMP );
	m_ColorTempGraphID = m_graphCtrl.AddGraph(RGB(0,255,255),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_REFERENCE );
	m_refGraphID = m_graphCtrl.AddGraph(RGB(230,230,230),(LPSTR)(LPCSTR)Msg,1,PS_DOT);

	Msg.LoadString ( IDS_COLORTEMPDATAREF );
	m_ColorTempDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	
	m_graphCtrl.SetScale(0,100,3000,9500);
	m_graphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl.SetYAxisProps("K", 500, 1500, 15000);

	m_showReference=GetConfig()->GetProfileInt("ColorTemp Histo","Show Reference",TRUE);
	m_showDataRef=GetConfig()->GetProfileInt("ColorTemp Hist","Show Reference Data",TRUE);	//Ki

	m_graphCtrl.ReadSettings("ColorTemp Histo");
}

void CColorTempGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	BOOL	bIRE = pDoc->GetMeasure()->m_bIREScaleMode;

	m_graphCtrl.SetXAxisProps(bIRE?"IRE":(LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);

	m_graphCtrl.ClearGraph(m_refGraphID);
	if (m_showReference )
	{	
		m_graphCtrl.AddPoint(m_refGraphID, bIRE ? 7.5 : 0.0, GetColorReference().GetWhite().GetColorTemp());
		m_graphCtrl.AddPoint(m_refGraphID, 100, GetColorReference().GetWhite().GetColorTemp());
	}

	m_graphCtrl.ClearGraph(m_ColorTempGraphID);
	m_graphCtrl.ClearGraph(m_ColorTempDataRefGraphID); //Ki

	CDataSetDoc *pDataRef = GetDataRef();
	int size=pDoc->GetMeasure()->GetGrayScaleSize();

	if ( pDataRef )
	{
		// Check if data reference is comparable
		if ( pDataRef->GetMeasure()->GetGrayScaleSize() != size || pDataRef->GetMeasure()->m_bIREScaleMode != bIRE )
		{
			// Cannot use data reference
			pDataRef = NULL;
		}
	}

	for (int i=0; i<size; i++)
	{
		int colorTemp=pDoc->GetMeasure()->GetGray(i).GetColorTemp();
		if(colorTemp > 1500 && colorTemp < 12000)
			m_graphCtrl.AddPoint(m_ColorTempGraphID, ArrayIndexToGrayLevel ( i, size, bIRE ), colorTemp);

		if ( m_showDataRef && pDataRef !=NULL && pDataRef != pDoc )
		{
			colorTemp = pDataRef->GetMeasure()->GetGray(i).GetColorTemp();
			if(colorTemp > 1500 && colorTemp < 12000)
				m_graphCtrl.AddPoint(m_ColorTempDataRefGraphID, ArrayIndexToGrayLevel ( i, size, bIRE ), colorTemp);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CColorTempHistoView

IMPLEMENT_DYNCREATE(CColorTempHistoView, CSavingView)

CColorTempHistoView::CColorTempHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CColorTempHistoView)
	//}}AFX_DATA_INIT
}

CColorTempHistoView::~CColorTempHistoView()
{
}

BEGIN_MESSAGE_MAP(CColorTempHistoView, CSavingView)
	//{{AFX_MSG_MAP(CColorTempHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_COLORTEMP_GRAPH_REFERENCE, OnColorTempGraphShowRef)
	ON_COMMAND(IDM_COLORTEMP_GRAPH_DATAREF, OnColorTempGraphShowDataRef)	//Ki
	ON_COMMAND(IDM_GRAPH_SETTINGS, OnGraphSettings)
	ON_COMMAND(IDM_GRAPH_X_SCALE_FIT, OnGraphXScaleFit)
	ON_COMMAND(IDM_GRAPH_X_ZOOM_IN, OnGraphXZoomIn)
	ON_COMMAND(IDM_GRAPH_X_ZOOM_OUT, OnGraphXZoomOut)
	ON_COMMAND(IDM_GRAPH_X_SHIFT_LEFT, OnGraphXShiftLeft)
	ON_COMMAND(IDM_GRAPH_X_SHIFT_RIGHT, OnGraphXShiftRight)
	ON_COMMAND(IDM_GRAPH_X_SCALE1, OnGraphXScale1)
	ON_COMMAND(IDM_GRAPH_X_SCALE2, OnGraphXScale2)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_BOTTOM, OnGraphYShiftBottom)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_TOP, OnGraphYShiftTop)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_IN, OnGraphYZoomIn)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_OUT, OnGraphYZoomOut)
	ON_COMMAND(IDM_COLORTEMP_GRAPH_Y_SCALE1, OnColortempGraphYScale1)
	ON_COMMAND(IDM_COLORTEMP_GRAPH_Y_SCALE2, OnColortempGraphYScale2)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CColorTempHistoView diagnostics

#ifdef _DEBUG
void CColorTempHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CColorTempHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CColorTempHistoView message handlers

void CColorTempHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();
	
	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_COLORTEMPHISTO_GRAPH);

	OnUpdate(NULL,UPD_EVERYTHING,NULL);
}

void CColorTempHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Update this view only when we are concerned
	if ( lHint == UPD_EVERYTHING || lHint == UPD_GRAYSCALEANDCOLORS || lHint == UPD_GRAYSCALE || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_SENSORCONFIG )
	{
		m_Grapher.UpdateGraph ( GetDocument () );

		Invalidate(TRUE);
	}
}

DWORD CColorTempHistoView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 1 );
}

void CColorTempHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 1 ) & 0x0001;
}

void CColorTempHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);

	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
}

BOOL CColorTempHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

void CColorTempHistoView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	
}

void CColorTempHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_COLORTEMP_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_COLORTEMP_GRAPH_REFERENCE, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
	pPopup->CheckMenuItem(IDM_COLORTEMP_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CColorTempHistoView::OnColorTempGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("ColorTemp Histo","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CColorTempHistoView::OnColorTempGraphShowDataRef()  //Ki
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("ColorTemp Histo","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CColorTempHistoView::OnGraphSave() 
{
	m_Grapher.m_graphCtrl.SaveGraphs();
}

void CColorTempHistoView::OnGraphSettings() 
{
	m_Grapher.m_graphCtrl.ChangeSettings();
	m_Grapher.m_graphCtrl.WriteSettings("ColorTemp Histo");
}

void CColorTempHistoView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
}

void CColorTempHistoView::OnGraphScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale();
	m_Grapher.m_graphCtrl.FitYScale();
	Invalidate(TRUE);
}

void CColorTempHistoView::OnColortempGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(3000,9500);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnColortempGraphYScale2() 
{
	m_Grapher.m_graphCtrl.SetYScale(4000,8000);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE,100);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(100);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-100);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(+500,-500);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(-500,+500);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphXScale1() 
{
	m_Grapher.m_graphCtrl.SetXScale(0,100);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphXScale2() 
{
	m_Grapher.m_graphCtrl.SetXScale(20,100);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphXScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale();
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphXZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowXScale(+10,-10);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphXZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowXScale(-10,+10);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphXShiftLeft() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(-10);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnGraphXShiftRight() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(+10);
	Invalidate(TRUE);
}

void CColorTempHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_COLORTEMP, NULL );
}
