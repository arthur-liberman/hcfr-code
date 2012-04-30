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

// RGBHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "RGBHistoView.h"

#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRGBGrapher

CRGBGrapher::CRGBGrapher ()
{
	CString		Msg;

	m_showReference=GetConfig()->GetProfileInt("RGB Histo","Show Reference",TRUE);
	m_showDeltaE=GetConfig()->GetProfileInt("RGB Histo","Show Delta E",TRUE);
	m_showDataRef=GetConfig()->GetProfileInt("RGB Histo","Show Reference Data",TRUE);	//Ki

	Msg.LoadString ( IDS_RGBREFERENCE );
	m_refGraphID = m_graphCtrl.AddGraph(RGB(230,230,230), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_RGBLEVELRED );
	m_redGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_RGBLEVELGREEN );
	m_greenGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_RGBLEVELBLUE );
	m_blueGraphID = m_graphCtrl.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg);
	
	Msg.LoadString ( IDS_RGBLEVELREDDATAREF );
	m_redDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);	//Ki
	Msg.LoadString ( IDS_RGBLEVELGREENDATAREF );
	m_greenDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);	//Ki
	Msg.LoadString ( IDS_RGBLEVELBLUEDATAREF );
	m_blueDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);	//Ki
	
	m_graphCtrl.SetScale(0,100,50,150);
	m_graphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl.SetYAxisProps("%", 10, 0, 400);

	Msg.LoadString ( IDS_DELTAE );
	m_deltaEGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEDATAREF );
	m_deltaEDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT); // Ki
	Msg.LoadString ( IDS_DELTAEBETWEENDATAANDREF );
	m_deltaEBetweenGraphID = m_graphCtrl2.AddGraph(RGB(255,192,128), (LPSTR)(LPCSTR)Msg);
	
	m_graphCtrl2.SetScale(0,100,0,10);
	m_graphCtrl2.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl2.SetYAxisProps("", 2, 0, 40);

	m_scaleYrgb = 0;
	m_scaleYdeltaE = 0;

	m_graphCtrl.ReadSettings("RGB Histo");
	m_graphCtrl2.ReadSettings("RGB Histo2");
}

void CRGBGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	BOOL	bIRE = pDoc->GetMeasure()->m_bIREScaleMode;
	double	YWhite, YWhiteRefDoc;
	double	Gamma, Offset, OffsetRef;

	m_graphCtrl.SetXAxisProps(bIRE?"IRE":(LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl2.SetXAxisProps(bIRE?"IRE":(LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);

	m_graphCtrl.ClearGraph(m_refGraphID);
	if (m_showReference)
	{	
		m_graphCtrl.AddPoint(m_refGraphID, bIRE ? 7.5 : 0, 100);
		m_graphCtrl.AddPoint(m_refGraphID, 100, 100);
	}

	m_graphCtrl.ClearGraph(m_redGraphID);
	m_graphCtrl.ClearGraph(m_greenGraphID);
	m_graphCtrl.ClearGraph(m_blueGraphID);

	m_graphCtrl.ClearGraph(m_redDataRefGraphID); //Ki
	m_graphCtrl.ClearGraph(m_greenDataRefGraphID); //Ki
	m_graphCtrl.ClearGraph(m_blueDataRefGraphID); //Ki

	m_graphCtrl2.ClearGraph(m_deltaEGraphID);
	m_graphCtrl2.ClearGraph(m_deltaEDataRefGraphID); //Ki
	m_graphCtrl2.ClearGraph(m_deltaEBetweenGraphID);
	
	if(IsWindow(m_graphCtrl2.m_hWnd))
		m_graphCtrl2.ShowWindow(m_showDeltaE ? SW_SHOW : SW_HIDE);

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
	
	if (pDataRef && pDataRef->GetMeasure()->GetGray(0)==noDataColor)
		pDataRef = NULL;

	if (pDoc->GetMeasure()->GetGray(0)!=noDataColor)
	{
		// Retrieve gamma and offset
		CColor			refColor = GetColorReference().GetWhite();

		if ( size )
			pDoc->ComputeGammaAndOffset(&Gamma, &Offset, 3, 1, size);

		YWhite = pDoc->GetMeasure()->GetGray(size-1)[1];
		for (int i=0; i<size; i++)
		{
			CColor aColor=pDoc->GetMeasure()->GetGray(i).GetxyYValue();
			CColor normColor;

			normColor[0]=(aColor[0]/aColor[1]);
			normColor[1]=(1.0);
			normColor[2]=((1.0-(aColor[0]+aColor[1]))/aColor[1]);

			CColor aMeasure(normColor);
			normColor=aMeasure.GetRGBValue();

			double x = ArrayIndexToGrayLevel ( i, size, bIRE );

			m_graphCtrl.AddPoint(m_redGraphID, x, normColor[0]*100.0);
			m_graphCtrl.AddPoint(m_greenGraphID, x, normColor[1]*100.0);
			m_graphCtrl.AddPoint(m_blueGraphID, x, normColor[2]*100.0);

			if(m_showDeltaE) 
			{
				// Determine Reference Y luma for Delta E calculus
				if ( GetConfig () -> m_bUseDeltaELumaOnGrays )
				{
					// Compute reference Luma regarding actual offset and reference gamma
					double valx=(GrayLevelToGrayProp(x,bIRE)+Offset)/(1.0+Offset);
					double valy=pow(valx, GetConfig()->m_GammaRef);
					
					CColor tmpColor = GetColorReference().GetWhite().GetxyYValue();
					tmpColor.SetZ(valy);
					refColor.SetxyYValue(tmpColor);
				}
				else
				{
					// Use actual gray luma as correct reference (Delta E will check color only, not brightness)
					YWhite = aColor [ 2 ];
				}
				m_graphCtrl2.AddPoint(m_deltaEGraphID, x, pDoc->GetMeasure()->GetGray(i).GetDeltaE(YWhite, refColor, 1.0));
			}
		}
	}

	if((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc)) 
	{
		BOOL			bMainDocHasColors = (pDoc->GetMeasure()->GetGray(0)!=noDataColor);
		CColor			refColor = GetColorReference().GetWhite();

		if ( size && pDataRef->GetMeasure()->GetGray(0) != noDataColor )
			pDataRef->ComputeGammaAndOffset(&Gamma, &OffsetRef, 3, 1, size);

		if ( bMainDocHasColors )
			YWhite = pDoc->GetMeasure()->GetGray(size-1)[1];

		YWhiteRefDoc = pDataRef->GetMeasure()->GetGray(size-1)[1];

		for (int i=0; i<size; i++)
		{
			CColor aColor;
			
			if ( bMainDocHasColors )
				aColor=pDoc->GetMeasure()->GetGray(i).GetxyYValue();

			CColor aColorRef=pDataRef->GetMeasure()->GetGray(i).GetxyYValue();
			CColor normColor;

			normColor[0]=(aColorRef[0]/aColorRef[1]);
			normColor[1]=(1.0);
			normColor[2]=((1.0-(aColorRef[0]+aColorRef[1]))/aColorRef[1]);

			CColor aMeasure(normColor);
			normColor=aMeasure.GetRGBValue();

			double x = ArrayIndexToGrayLevel ( i, size, bIRE );

			m_graphCtrl.AddPoint(m_redDataRefGraphID, x, normColor[0]*100.0);
			m_graphCtrl.AddPoint(m_greenDataRefGraphID, x, normColor[1]*100.0);
			m_graphCtrl.AddPoint(m_blueDataRefGraphID, x, normColor[2]*100.0);

			if(m_showDeltaE) 
			{
				// Determine Reference Y luma for Delta E calculus
				if ( GetConfig () -> m_bUseDeltaELumaOnGrays )
				{
					// Compute reference Luma regarding actual offset and reference gamma
					double valxref=(GrayLevelToGrayProp(x,bIRE)+OffsetRef)/(1.0+OffsetRef);
					double valyref=pow(valxref, GetConfig()->m_GammaRef);
					
					CColor tmpColor = GetColorReference().GetWhite().GetxyYValue();
					tmpColor.SetZ(valyref);
					refColor.SetxyYValue(tmpColor);
				}
				else
				{
					// Use actual gray luma as correct reference (Delta E will check color only, not brightness)
					YWhiteRefDoc = aColorRef [ 2 ];

					if ( bMainDocHasColors )
						YWhite = aColor [ 2 ];
				}

				m_graphCtrl2.AddPoint(m_deltaEDataRefGraphID, x, pDataRef->GetMeasure()->GetGray(i).GetDeltaE(YWhiteRefDoc, refColor, 1.0));
				
				if (bMainDocHasColors)
					m_graphCtrl2.AddPoint(m_deltaEBetweenGraphID, x, pDoc->GetMeasure()->GetGray(i).GetDeltaE(YWhite,pDataRef->GetMeasure()->GetGray(i),YWhiteRefDoc)); //Ki
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CRGBHistoView

IMPLEMENT_DYNCREATE(CRGBHistoView, CSavingView)

CRGBHistoView::CRGBHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CRGBHistoView)
	//}}AFX_DATA_INIT
}

CRGBHistoView::~CRGBHistoView()
{
}

BEGIN_MESSAGE_MAP(CRGBHistoView, CSavingView)
	//{{AFX_MSG_MAP(CRGBHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_RGB_GRAPH_REFERENCE, OnRgbGraphReference)
	ON_COMMAND(IDM_RGB_GRAPH_DELTAE, OnRgbGraphDeltaE)
	ON_COMMAND(IDM_RGB_GRAPH_DATAREF, OnRgbGraphDataRef)	//Ki
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
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE1, OnRGBGraphYScale1)
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE2, OnRGBGraphYScale2)
	ON_COMMAND(IDM_RGB_GRAPH_Y_SCALE3, OnRGBGraphYScale3)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE1, OnDeltaEGraphYScale1)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE2, OnDeltaEGraphYScale2)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE3, OnDeltaEGraphYScale3)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE_FIT, OnDeltaEGraphYScaleFit)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SHIFT_BOTTOM, OnDeltaEGraphYShiftBottom)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SHIFT_TOP, OnDeltaEGraphYShiftTop)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_ZOOM_IN, OnDeltaEGraphYZoomIn)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_ZOOM_OUT, OnDeltaEGraphYZoomOut)
	ON_COMMAND(IDM_DELTAE_GRAPH_SCALE_CUSTOM, OnDeltaEGraphScaleCustom)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRGBHistoView diagnostics

#ifdef _DEBUG
void CRGBHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}
 
void CRGBHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CRGBHistoView message handlers

void CRGBHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_RGBHISTO_GRAPH);
	m_Grapher.m_graphCtrl2.Create(_T("Graph2 Window"), rect, this, IDC_RGBHISTO_GRAPH2);

	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to m_showDeltaE
	OnUpdate(NULL,UPD_EVERYTHING,NULL);
}

void CRGBHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Update this view only when we are concerned
	if ( lHint == UPD_EVERYTHING || lHint == UPD_GRAYSCALEANDCOLORS || lHint == UPD_GRAYSCALE || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_SENSORCONFIG )
	{
		m_Grapher.UpdateGraph ( GetDocument () );

		Invalidate(TRUE);
	}
}

DWORD CRGBHistoView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showDeltaE		& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_scaleYrgb		& 0x0003 )	<< 3 )  // 2 bits
		  + ( ( m_Grapher.m_scaleYdeltaE	& 0x0003 )	<< 5 ); // 2 bits
}

void CRGBHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showDeltaE		= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_scaleYrgb		= ( dwUserInfo >> 3 ) & 0x0003;	// 2 bits
	m_Grapher.m_scaleYdeltaE	= ( dwUserInfo >> 5 ) & 0x0003;	// 2 bits

	switch ( m_Grapher.m_scaleYrgb )
	{
		case 0:
			 m_Grapher.m_graphCtrl.SetYScale(50,150);
			 m_Grapher.m_graphCtrl.SetYAxisProps("%", 10, 0, 400);
			 break;

		case 1:
			 m_Grapher.m_graphCtrl.SetYScale(0,200);
		 	 m_Grapher.m_graphCtrl.SetYAxisProps("%", 20, 0, 400);
			 break;

		case 2:
			 m_Grapher.m_graphCtrl.SetYScale(80,120);
			 m_Grapher.m_graphCtrl.SetYAxisProps("%", 5, 0, 400);
			 break;
	}

	switch ( m_Grapher.m_scaleYdeltaE )
	{
		case 0:
			 m_Grapher.m_graphCtrl2.SetYScale(0,10);
			 m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
			 break;

		case 1:
			 m_Grapher.m_graphCtrl2.SetYScale(0,20);
		 	 m_Grapher.m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
			 break;

		case 2:
			 m_Grapher.m_graphCtrl2.SetYScale(0,5);
			 m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
			 break;
	}
}

void CRGBHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(m_Grapher.m_showDeltaE)
	{
		if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
			m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy/2);
		if(IsWindow(m_Grapher.m_graphCtrl2.m_hWnd))
			m_Grapher.m_graphCtrl2.MoveWindow(0,cy/2,cx,cy/2);
	}
	else
		if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
			m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
}

void CRGBHistoView::OnDraw(CDC* pDC) 
{
	
}

BOOL CRGBHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

void CRGBHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_RGB_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_RGB_GRAPH_REFERENCE, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_RGB_GRAPH_DELTAE, m_Grapher.m_showDeltaE ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
	pPopup->CheckMenuItem(IDM_RGB_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CRGBHistoView::OnRgbGraphReference() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("RGB Histo","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CRGBHistoView::OnRgbGraphDeltaE() 
{
	m_Grapher.m_showDeltaE = !m_Grapher.m_showDeltaE;
	GetConfig()->WriteProfileInt("RGB Histo","Show Delta E",m_Grapher.m_showDeltaE);

	CRect rect;
	GetClientRect(&rect);
	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to m_showDeltaE

	OnUpdate(NULL,NULL,NULL);
}

void CRGBHistoView::OnRgbGraphDataRef()  //Ki
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("RGB Histo","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CRGBHistoView::OnGraphSettings() 
{
	// Add delta E graph to first graph control to allow setting change 
	int tmpGraphID=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_deltaEGraphID]);
	int tmpGraphID2=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_deltaEDataRefGraphID]);
	int tmpGraphID3=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_deltaEBetweenGraphID]);

	m_Grapher.m_graphCtrl.ClearGraph(tmpGraphID);
	m_Grapher.m_graphCtrl.ClearGraph(tmpGraphID2);
	m_Grapher.m_graphCtrl.ClearGraph(tmpGraphID3);

	m_Grapher.m_graphCtrl.ChangeSettings();

	// Update graph2 setting according to graph and values of tmpGraphID
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID,m_Grapher.m_deltaEGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID2,m_Grapher.m_deltaEDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID3,m_Grapher.m_deltaEBetweenGraphID);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID3);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID2);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID);

	m_Grapher.m_graphCtrl.WriteSettings("RGB Histo");
	m_Grapher.m_graphCtrl2.WriteSettings("RGB Histo2");

	OnUpdate(NULL,NULL,NULL);
}

void CRGBHistoView::OnGraphSave() 
{
	if(m_Grapher.m_showDeltaE)
		m_Grapher.m_graphCtrl.SaveGraphs(&m_Grapher.m_graphCtrl2);
	else
		m_Grapher.m_graphCtrl.SaveGraphs();
}

void CRGBHistoView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
	m_Grapher.m_scaleYrgb = 0;
}

void CRGBHistoView::OnGraphScaleFit() 
{
	OnGraphXScaleFit();
	OnGraphYScaleFit();
	OnDeltaEGraphYScaleFit();
}

void CRGBHistoView::OnRGBGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(0,200);
	m_Grapher.m_scaleYrgb = 1;
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 20, 0, 400);
	Invalidate(TRUE);
}

void CRGBHistoView::OnRGBGraphYScale2() 
{
	m_Grapher.m_graphCtrl.SetYScale(50,150);
	m_Grapher.m_scaleYrgb = 0;
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 10, 0, 400);
	Invalidate(TRUE);
}

void CRGBHistoView::OnRGBGraphYScale3() 
{
	m_Grapher.m_graphCtrl.SetYScale(80,120);
	m_Grapher.m_scaleYrgb = 2;
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 5, 0, 400);
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphYScale1() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,20);
	m_Grapher.m_scaleYdeltaE = 1;
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphYScale2() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,5);
	m_Grapher.m_scaleYdeltaE = 2;
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphYScale3() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,10);
	m_Grapher.m_scaleYdeltaE = 0;
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl2.FitYScale(TRUE,1);
	m_Grapher.m_scaleYdeltaE = 0;
	m_Grapher.m_graphCtrl2.m_yAxisStep=(m_Grapher.m_graphCtrl2.m_maxY-m_Grapher.m_graphCtrl2.m_minY)/10;
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(1);
	m_Grapher.m_scaleYdeltaE = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(-1);
	m_Grapher.m_scaleYdeltaE = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,-4);
	m_Grapher.m_scaleYdeltaE = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,+4);
	m_Grapher.m_scaleYdeltaE = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnDeltaEGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl2.ChangeScale();
	m_Grapher.m_scaleYdeltaE = 0;
}

void CRGBHistoView::OnGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(10);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-10);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(+10,-10);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(-10,+10);
	m_Grapher.m_scaleYrgb = 0;
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphXScale1() 
{
	m_Grapher.m_graphCtrl.SetXScale(0,100);
	m_Grapher.m_graphCtrl2.SetXScale(0,100);
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphXScale2() 
{
	m_Grapher.m_graphCtrl.SetXScale(20,100);
	m_Grapher.m_graphCtrl2.SetXScale(20,100);
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphXScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale();
	m_Grapher.m_graphCtrl2.FitXScale();
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphXZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowXScale(+10,-10);
	m_Grapher.m_graphCtrl2.GrowXScale(+10,-10);
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphXZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowXScale(-10,+10);
	m_Grapher.m_graphCtrl2.GrowXScale(-10,+10);
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphXShiftLeft() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(-10);
	m_Grapher.m_graphCtrl2.ShiftXScale(-10);
	Invalidate(TRUE);
}

void CRGBHistoView::OnGraphXShiftRight() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(+10);
	m_Grapher.m_graphCtrl2.ShiftXScale(+10);
	Invalidate(TRUE);
}


void CRGBHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_RGBLEVELS, NULL );
}
