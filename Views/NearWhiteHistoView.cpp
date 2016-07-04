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

// NearWhiteHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "NearWhiteHistoView.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Implemented in luminancehistoview.cpp
double Y_to_L( double val);

/////////////////////////////////////////////////////////////////////////////
// CNearWhiteGrapher

CNearWhiteGrapher::CNearWhiteGrapher()
{
	CString	Msg;

	Msg.LoadString ( IDS_LUMINANCE );
	m_luminanceGraphID = m_graphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg);
	Msg += " (lux)";
	m_luxmeterGraphID = m_graphCtrl.AddGraph(RGB(255,128,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_RED );
	m_redLumGraphID = m_graphCtrl.AddGraph(RGB(255,0,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GREEN );
	m_greenLumGraphID = m_graphCtrl.AddGraph(RGB(0,255,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_BLUE );
	m_blueLumGraphID = m_graphCtrl.AddGraph(RGB(0,0,255),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_REFERENCE );
	m_refGraphID = m_graphCtrl.AddGraph(RGB(230,230,230),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	
	Msg.LoadString ( IDS_LUMINANCEDATAREF );
	m_luminanceDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg += " (lux)";
	m_luxmeterDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,128,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_REDDATAREF );
	m_redLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_GREENDATAREF );
	m_greenLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_BLUEDATAREF );
	m_blueLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	
	m_showL=GetConfig()->GetProfileInt("Near White Histo","Show L",FALSE);
	m_graphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 1, 80, 100);
	m_graphCtrl.SetYAxisProps(m_showL?"":"%", 1, 80, 100);
    m_graphCtrl.SetScale(90,100,80,100);

	Msg.LoadString ( IDS_GAMMA );
	m_luminanceLogGraphID = m_logGraphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg);
	Msg += " (lux)";
	m_luxmeterLogGraphID = m_logGraphCtrl.AddGraph(RGB(255,128,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMARED );
	m_redLumLogGraphID = m_logGraphCtrl.AddGraph(RGB(255,0,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMAGREEN );
	m_greenLumLogGraphID = m_logGraphCtrl.AddGraph(RGB(0,255,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMABLUE );
	m_blueLumLogGraphID = m_logGraphCtrl.AddGraph(RGB(0,0,255),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMAREFERENCE );
	m_refLogGraphID = m_logGraphCtrl.AddGraph(RGB(230,230,230),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	
	Msg.LoadString ( IDS_GAMMADATAREF );
	m_luminanceDataRefLogGraphID = m_logGraphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg += " (lux)";
	m_luxmeterDataRefLogGraphID = m_logGraphCtrl.AddGraph(RGB(255,128,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_GAMMAREDDATAREF );
	m_redLumDataRefLogGraphID = m_logGraphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_GAMMAGREENDATAREF );
	m_greenLumDataRefLogGraphID = m_logGraphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_GAMMABLUEDATAREF );
	m_blueLumDataRefLogGraphID = m_logGraphCtrl.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki

	m_logGraphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 1, 90, 100);
	m_logGraphCtrl.SetYAxisProps("", 0.1, 1, 4);
	m_logGraphCtrl.SetScale(90,100,1,3);
	m_logGraphCtrl.ReadSettings("Near White Histo Log");

	m_doLogMode=GetConfig()->GetProfileInt("Near White Histo","Log Mode",FALSE);
	m_showReference=GetConfig()->GetProfileInt("Near White Histo","Show Reference",TRUE);
	m_showYLum=GetConfig()->GetProfileInt("Near White Histo","Show Y lum",TRUE);
	m_showRedLum=GetConfig()->GetProfileInt("Near White Histo","Show Red",FALSE);
	m_showGreenLum=GetConfig()->GetProfileInt("Near White Histo","Show Green",FALSE);
	m_showBlueLum=GetConfig()->GetProfileInt("Near White Histo","Show Blue",FALSE);
	m_showDataRef=GetConfig()->GetProfileInt("Near White Histo","Show Reference Data",TRUE);	//Ki
	m_graphCtrl.ReadSettings("Near White Histo");
}

void CNearWhiteGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	BOOL	bDataPresent = FALSE;
	CDataSetDoc *pDataRef = GetDataRef();
	int size=pDoc->GetMeasure()->GetNearWhiteScaleSize();
	
	if ( pDataRef )
	{
		// Check if data reference is comparable
		if ( pDataRef->GetMeasure()->GetNearWhiteScaleSize() != size )
		{
			// Cannot use data reference
			pDataRef = NULL;
		}
	}
	
	if (pDoc->GetMeasure()->GetNearWhite(0).isValid())
		bDataPresent = TRUE;

	if (pDataRef && !pDataRef->GetMeasure()->GetNearWhite(0).isValid())
		pDataRef = NULL;

	if(IsWindow(m_graphCtrl.m_hWnd))
		m_graphCtrl.ShowWindow(m_doLogMode ? SW_HIDE : SW_SHOW);
	if(IsWindow(m_logGraphCtrl.m_hWnd))
		m_logGraphCtrl.ShowWindow(m_doLogMode ? SW_SHOW : SW_HIDE);

	m_graphCtrl.ClearGraph(m_refGraphID);
	m_logGraphCtrl.ClearGraph(m_refLogGraphID);
	if (m_showReference && m_refGraphID != -1 && size > 0)
	{	
		for (int i=0; i<size; i++)
		{
        	int g_size=pDoc->GetMeasure()->GetGrayScaleSize();
			double GammaOffset,GammaOpt;
			pDoc->ComputeGammaAndOffset(&GammaOpt, &GammaOffset, 1,1,g_size,false);
    		CColor White = pDoc -> GetMeasure () -> GetGray ( g_size - 1 );
	    	CColor Black = pDoc -> GetMeasure () -> GetGray ( 0 );
			double x = ArrayIndexToGrayLevel ( 101 - size + i, 101, GetConfig () -> m_bUseRoundDown);

			double valx,val;//=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef) );

			int mode = GetConfig()->m_GammaOffsetType;
			if (GetConfig()->m_colorStandard == sRGB) mode = 8;
			if ( mode >= 4 )
			{
			    valx = (GrayLevelToGrayProp( x, GetConfig () -> m_bUseRoundDown));
				if  (mode == 5)
				{
	                val = getL_EOTF( valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode) * 100 / White.GetY();
					val=min(val,1.2);
				}
				else
					val = getL_EOTF( valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);				
			}
			else
			{
                valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+GammaOffset)/(1.0+GammaOffset);
                val=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
				if (mode == 1) //black compensation target
					val = (Black.GetY() + ( val * ( White.GetY() - Black.GetY() ) )) / White.GetY();
			}

			if(val > 0 && i != (size-1))	// log scale is not valid for last value
				 m_logGraphCtrl.AddPoint(m_refLogGraphID, valx * 100, log(val)/log(valx));

            if (!m_showL)
				if (mode == 5)
	    			m_graphCtrl.AddPoint(m_refGraphID, x, 100.0*val, NULL, White.GetY() * 101.23271);
				else
	    			m_graphCtrl.AddPoint(m_refGraphID, x, 100.0*val, NULL, White.GetY());
            else
                m_graphCtrl.AddPoint(m_refGraphID, x, Y_to_L(val));

		}
	}
	
	m_graphCtrl.ClearGraph(m_redLumGraphID);
	m_graphCtrl.ClearGraph(m_redLumDataRefGraphID);	//Ki
	m_logGraphCtrl.ClearGraph(m_redLumLogGraphID);
	m_logGraphCtrl.ClearGraph(m_redLumDataRefLogGraphID); //Ki
	if (m_showRedLum && m_redLumGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if ( bDataPresent )
				AddPointtoLumGraph(0,0,size,i,pDoc,m_redLumGraphID,m_redLumLogGraphID);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,0,size,i,pDataRef,m_redLumDataRefGraphID,m_redLumDataRefLogGraphID);
		}
	}

	m_graphCtrl.ClearGraph(m_greenLumGraphID);
	m_graphCtrl.ClearGraph(m_greenLumDataRefGraphID); //Ki
	m_logGraphCtrl.ClearGraph(m_greenLumLogGraphID);
	m_logGraphCtrl.ClearGraph(m_greenLumDataRefLogGraphID); //Ki
	if (m_showGreenLum && m_greenLumGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if ( bDataPresent )
				AddPointtoLumGraph(0,1,size,i,pDoc,m_greenLumGraphID,m_greenLumLogGraphID);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,1,size,i,pDataRef,m_greenLumDataRefGraphID,m_greenLumDataRefLogGraphID);
		}
	}

	m_graphCtrl.ClearGraph(m_blueLumGraphID);
	m_graphCtrl.ClearGraph(m_blueLumDataRefGraphID); //Ki
	m_logGraphCtrl.ClearGraph(m_blueLumLogGraphID);
	m_logGraphCtrl.ClearGraph(m_blueLumDataRefLogGraphID); //Ki
	if (m_showBlueLum && m_blueLumGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if ( bDataPresent )
				AddPointtoLumGraph(0,2,size,i,pDoc,m_blueLumGraphID,m_blueLumLogGraphID);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,2,size,i,pDataRef,m_blueLumDataRefGraphID,m_blueLumDataRefLogGraphID);
		}
	}

	m_graphCtrl.ClearGraph(m_luminanceGraphID);
	m_graphCtrl.ClearGraph(m_luminanceDataRefGraphID); //Ki
	m_logGraphCtrl.ClearGraph(m_luminanceLogGraphID);
	m_logGraphCtrl.ClearGraph(m_luminanceDataRefLogGraphID); //Ki
	if (m_showYLum && m_luminanceGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if ( bDataPresent )
				AddPointtoLumGraph(1,1,size,i,pDoc,m_luminanceGraphID,m_luminanceLogGraphID);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(1,1,size,i,pDataRef,m_luminanceDataRefGraphID,m_luminanceDataRefLogGraphID);
		}
	}

	m_graphCtrl.ClearGraph(m_luxmeterGraphID);
	m_graphCtrl.ClearGraph(m_luxmeterDataRefGraphID);
	m_logGraphCtrl.ClearGraph(m_luxmeterLogGraphID);
	m_logGraphCtrl.ClearGraph(m_luxmeterDataRefLogGraphID);
	if ( GetConfig () -> m_nLuminanceCurveMode == 2 )
	{
		if (m_showYLum && m_luxmeterGraphID != -1 && size > 0)
		{
			for (int i=0; i<size; i++)
			{
				if ( bDataPresent && pDoc->GetMeasure()->GetNearWhite(0).HasLuxValue() )
					AddPointtoLumGraph(2,1,size,i,pDoc,m_luxmeterGraphID,m_luxmeterLogGraphID);
				if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc) && pDataRef->GetMeasure()->GetNearWhite(0).HasLuxValue() )
					AddPointtoLumGraph(2,1,size,i,pDataRef,m_luxmeterDataRefGraphID,m_luxmeterDataRefLogGraphID);
			}
		}
	}
	m_graphCtrl.FitXScale(1,1);
	m_graphCtrl.FitYScale(1,1);
	m_graphCtrl.ReadSettings("Near White Histo");
	m_logGraphCtrl.ReadSettings("Near White Histo Log"); //in case user has scaled graphs
}

/*
ColorSpace : 0 : RGB, 1 : Luma or Lux, 2 : Lux
ColorIndex : 0 : R, 1 : G, 2 : B
Size : Total number of points
PointIndex : Point index on graph
*pDataSet : pointer on data set to display
GraphID : graph ID for drawing
LogGraphID : graph ID for log drawing, -1 if no Log drawing
*/

void CNearWhiteGrapher::AddPointtoLumGraph(int ColorSpace,int ColorIndex,int Size,int PointIndex,CDataSetDoc *pDataSet,long GraphID,long LogGraphID = -1)
{
	double max;
	double colorlevel, whitelvl;
	char	szBuf [ 64 ];
	LPCSTR	lpMsg = NULL;
	
	if (ColorSpace == 0) 
	{
//		max=pDataSet->GetMeasure()->GetNearWhite(Size-1).GetRGBValue(GetColorReference())[ColorIndex];
		max=pDataSet->GetMeasure()->GetNearWhite(Size-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		colorlevel=pDataSet->GetMeasure()->GetNearWhite(PointIndex).GetRGBValue(GetColorReference())[ColorIndex];
	}
	else if (ColorSpace == 1)
	{
		max=pDataSet->GetMeasure()->GetNearWhite(Size-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		colorlevel=pDataSet->GetMeasure()->GetNearWhite(PointIndex).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		int grayscalesize=pDataSet->GetMeasure()->GetGrayScaleSize();
		int nearwhitescalesize=pDataSet->GetMeasure()->GetNearWhiteScaleSize();

		if (pDataSet->GetMeasure()->GetGray(grayscalesize-1).isValid())
			whitelvl=pDataSet->GetMeasure()->GetGray(grayscalesize-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		else if (pDataSet->GetMeasure()->GetNearWhite(nearwhitescalesize-1).isValid())
			whitelvl=pDataSet->GetMeasure()->GetNearWhite(nearwhitescalesize-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		else
			whitelvl=0;
	}
	else 
	{
		max=pDataSet->GetMeasure()->GetNearWhite(Size-1).GetLuxValue();
		colorlevel=pDataSet->GetMeasure()->GetNearWhite(PointIndex).GetLuxValue();
	}

	if ( pDataSet->GetMeasure()->GetNearWhite(PointIndex).HasLuxValue () )
	{
		if ( GetConfig () ->m_bUseImperialUnits )
			sprintf ( szBuf, "%.5g Ft-cd", pDataSet->GetMeasure()->GetNearWhite(PointIndex).GetLuxValue() * 0.0929 );
		else
			sprintf ( szBuf, "%.5g Lux", pDataSet->GetMeasure()->GetNearWhite(PointIndex).GetLuxValue() );
		
		lpMsg = szBuf;
	}
	
    if(PointIndex != (Size-1))	// log scale is not valid for last value
	{
		m_logGraphCtrl.AddPoint(LogGraphID, GrayLevelToGrayProp( (double)(PointIndex+101-Size), GetConfig () -> m_bUseRoundDown) * 100., log(colorlevel/max)/log(GrayLevelToGrayProp( (double)(PointIndex+101-Size), GetConfig () -> m_bUseRoundDown)), lpMsg);
	}

    if (!m_showL)
	{
		if (ColorSpace == 1 && ColorIndex == 1)
	    	m_graphCtrl.AddPoint(GraphID, GrayLevelToGrayProp( (double)(PointIndex+101-Size), GetConfig () -> m_bUseRoundDown) * 100., (colorlevel/max)*100.0, lpMsg, whitelvl);
		else
	    	m_graphCtrl.AddPoint(GraphID, GrayLevelToGrayProp( (double)(PointIndex+101-Size), GetConfig () -> m_bUseRoundDown) * 100., (colorlevel/max)*100.0, lpMsg);
	}
    else
    	m_graphCtrl.AddPoint(GraphID, GrayLevelToGrayProp( (double)(PointIndex+101-Size), GetConfig () -> m_bUseRoundDown) * 100., Y_to_L(colorlevel/max), lpMsg);

}

/////////////////////////////////////////////////////////////////////////////
// CNearWhiteHistoView

IMPLEMENT_DYNCREATE(CNearWhiteHistoView, CSavingView)

CNearWhiteHistoView::CNearWhiteHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CNearWhiteHistoView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
 
CNearWhiteHistoView::~CNearWhiteHistoView()
{
}
 
BEGIN_MESSAGE_MAP(CNearWhiteHistoView, CSavingView)
	//{{AFX_MSG_MAP(CNearWhiteHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_LUM_GRAPH_LOGMODE, OnLumGraphLogMode)
	ON_COMMAND(IDM_LUM_GRAPH_BLUELUM, OnLumGraphBlueLum)
	ON_COMMAND(IDM_LUM_GRAPH_GREENLUM, OnLumGraphGreenLum)
	ON_COMMAND(IDM_LUM_GRAPH_REDLUM, OnLumGraphRedLum)
	ON_COMMAND(IDM_LUM_GRAPH_SHOWREF, OnLumGraphShowRef)
	ON_COMMAND(IDM_LUM_GRAPH_DATAREF, OnLumGraphShowDataRef)	//Ki
	ON_COMMAND(IDM_LUM_GRAPH_YLUM, OnLumGraphYLum)
	ON_COMMAND(IDM_LUM_GRAPH_L, OnLumGraphL)
	ON_COMMAND(IDM_GRAPH_SETTINGS, OnGraphSettings)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_BOTTOM, OnGraphYShiftBottom)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_TOP, OnGraphYShiftTop)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_IN, OnGraphYZoomIn)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_OUT, OnGraphYZoomOut)
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE1, OnLuminanceGraphYScale1)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GAMMA_GRAPH_Y_SCALE1, OnGammaGraphYScale1)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNearWhiteHistoView diagnostics

#ifdef _DEBUG
void CNearWhiteHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CNearWhiteHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CNearWhiteHistoView message handlers

void CNearWhiteHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTO_GRAPH);
	m_Grapher.m_logGraphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTOLOG_GRAPH);

	m_Grapher.m_graphCtrl.SetXScale ( 101 - GetDocument()->GetMeasure()->GetNearWhiteScaleSize(), 100 );
	m_Grapher.m_logGraphCtrl.SetXScale ( 101 - GetDocument()->GetMeasure()->GetNearWhiteScaleSize(), 100 );

	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Update this view only when we are concerned
	if ( lHint == UPD_EVERYTHING || lHint == UPD_NEARWHITE || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_SENSORCONFIG || lHint >= UPD_REALTIME)
	{
		m_Grapher.UpdateGraph ( GetDocument () );

		Invalidate(TRUE);
	}
}

DWORD CNearWhiteHistoView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_doLogMode		& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showYLum		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_showRedLum		& 0x0001 )	<< 3 )
		  + ( ( m_Grapher.m_showGreenLum	& 0x0001 )	<< 4 )
		  + ( ( m_Grapher.m_showBlueLum		& 0x0001 )	<< 5 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 6 )
		  + ( ( m_Grapher.m_showL		& 0x0001 )	<< 7 );
}

void CNearWhiteHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_doLogMode		= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showYLum		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_showRedLum		= ( dwUserInfo >> 3 ) & 0x0001;
	m_Grapher.m_showGreenLum	= ( dwUserInfo >> 4 ) & 0x0001;
	m_Grapher.m_showBlueLum		= ( dwUserInfo >> 5 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 6 ) & 0x0001;
	m_Grapher.m_showL		= ( dwUserInfo >> 7 ) & 0x0001;
}

void CNearWhiteHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
	if(IsWindow(m_Grapher.m_logGraphCtrl.m_hWnd))
		m_Grapher.m_logGraphCtrl.MoveWindow(0,0,cx,cy);
}

void CNearWhiteHistoView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
}

BOOL CNearWhiteHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CNearWhiteHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_NEARWHITE_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
	pPopup->CheckMenuItem(IDM_LUM_GRAPH_LOGMODE, m_Grapher.m_doLogMode ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_YLUM, m_Grapher.m_showYLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_L, m_Grapher.m_showL ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_REDLUM, m_Grapher.m_showRedLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_GREENLUM, m_Grapher.m_showGreenLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_BLUELUM, m_Grapher.m_showBlueLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	CMenu *pScaleMenu=pPopup->GetSubMenu(10);
	if(m_Grapher.m_doLogMode)
	{
		if(pScaleMenu)
			pScaleMenu->RemoveMenu(1,MF_BYPOSITION);	// Remove luminance menu
	}
	else
	{
		if(pScaleMenu)
			pScaleMenu->RemoveMenu(2,MF_BYPOSITION);	// Remove gamma menu
	}

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, point.x, point.y, this);
}

void CNearWhiteHistoView::OnLumGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("Near White Histo","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnLumGraphShowDataRef() //Ki
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Near White Histo","Show Reference Data",m_Grapher.m_showDataRef);	// Ki
	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnLumGraphLogMode() 
{
	m_Grapher.m_doLogMode = !m_Grapher.m_doLogMode;
	GetConfig()->WriteProfileInt("Near White Histo","Log Mode",m_Grapher.m_doLogMode);
	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnLumGraphYLum() 
{
	m_Grapher.m_showYLum = !m_Grapher.m_showYLum;
	GetConfig()->WriteProfileInt("Near White Histo","Show Y lum",m_Grapher.m_showYLum);
	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnLumGraphL() 
{
	m_Grapher.m_showL = !m_Grapher.m_showL;
	m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_showL?"":"%", m_Grapher.m_showL?1:0.1, 0, 20);
	GetConfig()->WriteProfileInt("Near White Histo","Show L",m_Grapher.m_showL);
	OnUpdate(NULL,NULL,NULL);
    OnGraphScaleFit();
}

void CNearWhiteHistoView::OnLumGraphRedLum() 
{
	m_Grapher.m_showRedLum = !m_Grapher.m_showRedLum;
	GetConfig()->WriteProfileInt("Near White Histo","Show Red",m_Grapher.m_showRedLum);
	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnLumGraphGreenLum() 
{
	m_Grapher.m_showGreenLum = !m_Grapher.m_showGreenLum;
	GetConfig()->WriteProfileInt("Near White Histo","Show Green",m_Grapher.m_showGreenLum);
	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnLumGraphBlueLum() 
{
	m_Grapher.m_showBlueLum = !m_Grapher.m_showBlueLum;
	GetConfig()->WriteProfileInt("Near White Histo","Show Blue",m_Grapher.m_showBlueLum);
	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnGraphSave() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.SaveGraphs();
	else
		m_Grapher.m_graphCtrl.SaveGraphs();
}

void CNearWhiteHistoView::OnGraphSettings() 
{
	// Add log graphs to first graph control to allow setting change 
	int tmpGraphID1=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_luminanceLogGraphID]);
	int tmpGraphID2=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_redLumLogGraphID]);
	int tmpGraphID3=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_greenLumLogGraphID]);
	int tmpGraphID4=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_blueLumLogGraphID]);
	int tmpGraphID5=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_refLogGraphID]);
	int tmpGraphID7=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_luminanceDataRefLogGraphID]);
	int tmpGraphID8=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_redLumDataRefLogGraphID]);
	int tmpGraphID9=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_greenLumDataRefLogGraphID]);
	int tmpGraphID10=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_blueLumDataRefLogGraphID]);
	int tmpGraphID11=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_luxmeterLogGraphID]);
	int tmpGraphID12=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_logGraphCtrl.m_graphArray[m_Grapher.m_luxmeterDataRefLogGraphID]);

	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID1 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID2 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID3 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID4 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID5 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID7 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID8 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID9 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID10 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID11 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID12 );

	m_Grapher.m_graphCtrl.ChangeSettings();

	// Update m_Grapher.m_logGraphCtrl setting according to graph and values of tmpGraphIDs
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID1,m_Grapher.m_luminanceLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID2,m_Grapher.m_redLumLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID3,m_Grapher.m_greenLumLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID4,m_Grapher.m_blueLumLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID5,m_Grapher.m_refLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID7,m_Grapher.m_luminanceDataRefLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID8,m_Grapher.m_redLumDataRefLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID9,m_Grapher.m_greenLumDataRefLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID10,m_Grapher.m_blueLumDataRefLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID11,m_Grapher.m_luxmeterLogGraphID);
	m_Grapher.m_logGraphCtrl.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID12,m_Grapher.m_luxmeterDataRefLogGraphID);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID12);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID11);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID10);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID9);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID8);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID7);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID5);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID4);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID3);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID2);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID1);

	OnUpdate(NULL,NULL,NULL);
}

void CNearWhiteHistoView::OnGraphScaleCustom() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.ChangeScale();
	else
		m_Grapher.m_graphCtrl.ChangeScale();
	m_Grapher.m_graphCtrl.WriteSettings("Near White Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near White Histo Log");
}

void CNearWhiteHistoView::OnGraphScaleFit() 
{
	if(m_Grapher.m_doLogMode)
	{
		m_Grapher.m_logGraphCtrl.FitXScale(TRUE,1);
		m_Grapher.m_logGraphCtrl.FitYScale(TRUE,.1);
	}
	else
	{
		m_Grapher.m_graphCtrl.FitXScale(TRUE,1);
		m_Grapher.m_graphCtrl.FitYScale(TRUE,1);
	}
	m_Grapher.m_graphCtrl.WriteSettings("Near White Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near White Histo Log");
	Invalidate(TRUE);
}

void CNearWhiteHistoView::OnLuminanceGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(80,100);
	m_Grapher.m_graphCtrl.WriteSettings("Near White Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near White Histo Log");
	Invalidate(TRUE);
}

void CNearWhiteHistoView::OnGammaGraphYScale1() 
{
	m_Grapher.m_logGraphCtrl.SetYScale(1,3);
	m_Grapher.m_graphCtrl.WriteSettings("Near White Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near White Histo Log");
	Invalidate(TRUE);
}

void CNearWhiteHistoView::OnGraphYScaleFit() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.FitYScale(TRUE,0.1);
	else
		m_Grapher.m_graphCtrl.FitYScale(TRUE,1);
	m_Grapher.m_graphCtrl.WriteSettings("Near White Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near White Histo Log");
	Invalidate(TRUE);
}

void CNearWhiteHistoView::OnGraphYShiftBottom() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.ShiftYScale(0.1);
	else
		m_Grapher.m_graphCtrl.ShiftYScale(2);
	Invalidate(TRUE);
}

void CNearWhiteHistoView::OnGraphYShiftTop() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.ShiftYScale(-0.1);
	else
		m_Grapher.m_graphCtrl.ShiftYScale(-2);
	Invalidate(TRUE);
}

void CNearWhiteHistoView::OnGraphYZoomIn() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.GrowYScale(0.1,-0.1);
	else
		m_Grapher.m_graphCtrl.GrowYScale(0,-2);
	Invalidate(TRUE);
}

void CNearWhiteHistoView::OnGraphYZoomOut() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.GrowYScale(-0.1,0.1);
	else
		m_Grapher.m_graphCtrl.GrowYScale(0,2);
	Invalidate(TRUE);
}

void CNearWhiteHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_NEARWHITE, NULL );
}

