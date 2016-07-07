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

// NearBlackHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "NearBlackHistoView.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Implemented in luminancehistoview.cpp
double Y_to_L( double val);

/////////////////////////////////////////////////////////////////////////////
// CNearBlackGrapher

CNearBlackGrapher::CNearBlackGrapher()
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

	m_showL=GetConfig()->GetProfileInt("Near Black Histo","Show L",FALSE);
	m_graphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 1, 0, 10);
	m_graphCtrl.SetYAxisProps(m_showL?"":"%", m_showL?1:0.1, 0, 20);
    m_graphCtrl.SetScale(0,10,0,m_showL?10:0.5);

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

	m_logGraphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 1, 0, 10);
	m_logGraphCtrl.SetYAxisProps("", 0.1, 0, 1);
	m_logGraphCtrl.SetScale(0,10,1,3);


	//Load user scales if any
	m_logGraphCtrl.ReadSettings("Near Black Histo Log");
	m_graphCtrl.ReadSettings("Near Black Histo");
	
	m_doLogMode=GetConfig()->GetProfileInt("Near Black Histo","Log Mode",FALSE);
	m_showReference=GetConfig()->GetProfileInt("Near Black Histo","Show Reference",TRUE);
	m_showYLum=GetConfig()->GetProfileInt("Near Black Histo","Show Y lum",TRUE);
	m_showRedLum=GetConfig()->GetProfileInt("Near Black Histo","Show Red",FALSE);
	m_showGreenLum=GetConfig()->GetProfileInt("Near Black Histo","Show Green",FALSE);
	m_showBlueLum=GetConfig()->GetProfileInt("Near Black Histo","Show Blue",FALSE);
	m_showDataRef=GetConfig()->GetProfileInt("Near Black Histo","Show Reference Data",TRUE);	//Ki
}

void CNearBlackGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	BOOL	bDataPresent = FALSE;
	CDataSetDoc *pDataRef = GetDataRef();
	int size=pDoc->GetMeasure()->GetNearBlackScaleSize();

    if ( pDataRef )
	{
		// Check if data reference is comparable
		if ( pDataRef->GetMeasure()->GetNearBlackScaleSize() != size )
		{
			// Cannot use data reference
			pDataRef = NULL;
		}
	}
	
	if (pDoc->GetMeasure()->GetNearBlack(0).isValid())
		bDataPresent = TRUE;

	if (pDataRef && !pDataRef->GetMeasure()->GetNearBlack(0).isValid())
		pDataRef = NULL;

	if(IsWindow(m_graphCtrl.m_hWnd))
		m_graphCtrl.ShowWindow(m_doLogMode ? SW_HIDE : SW_SHOW);
	if(IsWindow(m_logGraphCtrl.m_hWnd))
		m_logGraphCtrl.ShowWindow(m_doLogMode ? SW_SHOW : SW_HIDE);

	double valformax = 1.0;
	
	if ( size > 0 )
	{
		valformax=pow((double)(size-1)/100.0, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef) );
	}
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
			double x = ArrayIndexToGrayLevel ( i*(GetConfig()->m_GammaOffsetType==5?2:1), 101, GetConfig () -> m_bUseRoundDown); 
			double valx,val;//=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef) );
			int mode = GetConfig()->m_GammaOffsetType;
			if (GetConfig()->m_colorStandard == sRGB) mode = 99;
			if ( mode >= 4 ) 
			{
			    valx = (GrayLevelToGrayProp( x, GetConfig () -> m_bUseRoundDown));
				if  (mode == 5)
	                val = getL_EOTF( valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode) * 100 / White.GetY();
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

			//Reference plots
			if (!m_showL)
				if (mode == 5)
					m_graphCtrl.AddPoint(m_refGraphID, x, 100.0*val, NULL, White.GetY() * 105.95640);
				else
					m_graphCtrl.AddPoint(m_refGraphID, x, 100.0*val, NULL, White.GetY());
            else
                m_graphCtrl.AddPoint(m_refGraphID, x, Y_to_L(val));

			if((val > 0) && (i != 0))	// log scale is not valid for first value
				m_logGraphCtrl.AddPoint(m_refLogGraphID, valx * 100. , log(val)/log(valx), NULL, NULL);

			valformax = val;
		}
	}
	
	m_graphCtrl.ClearGraph(m_redLumGraphID);
	m_graphCtrl.ClearGraph(m_redLumDataRefGraphID); //Ki
	m_logGraphCtrl.ClearGraph(m_redLumLogGraphID);
	m_logGraphCtrl.ClearGraph(m_redLumDataRefLogGraphID); //Ki

	if (m_showRedLum && m_redLumGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if ( bDataPresent )
				AddPointtoLumGraph(0,0,size,i,pDoc,m_redLumGraphID,m_redLumLogGraphID,valformax);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,0,size,i,pDataRef,m_redLumDataRefGraphID,m_redLumDataRefLogGraphID,valformax);
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
				AddPointtoLumGraph(0,1,size,i,pDoc,m_greenLumGraphID,m_greenLumLogGraphID,valformax);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,1,size,i,pDataRef,m_greenLumDataRefGraphID,m_greenLumDataRefLogGraphID,valformax);
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
				AddPointtoLumGraph(0,2,size,i,pDoc,m_blueLumGraphID,m_blueLumLogGraphID,valformax);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,2,size,i,pDataRef,m_blueLumDataRefGraphID,m_blueLumDataRefLogGraphID,valformax);
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
				AddPointtoLumGraph(1,1,size,i,pDoc,m_luminanceGraphID,m_luminanceLogGraphID,valformax);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(1,1,size,i,pDataRef,m_luminanceDataRefGraphID,m_luminanceDataRefLogGraphID,valformax);
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
				if ( bDataPresent && pDoc->GetMeasure()->GetNearBlack(0).HasLuxValue() )
					AddPointtoLumGraph(2,1,size,i,pDoc,m_luxmeterGraphID,m_luxmeterLogGraphID,valformax);
				if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc) && pDataRef->GetMeasure()->GetNearBlack(0).HasLuxValue() )
					AddPointtoLumGraph(2,1,size,i,pDataRef,m_luxmeterDataRefGraphID,m_luxmeterDataRefLogGraphID,valformax);
			}
		}
	}
	m_graphCtrl.FitXScale(1,1);
	m_graphCtrl.FitYScale(1,m_showL?1.0:0.1);
	m_logGraphCtrl.ReadSettings("Near Black Histo Log");
	m_graphCtrl.ReadSettings("Near Black Histo");
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

void CNearBlackGrapher::AddPointtoLumGraph(int ColorSpace,int ColorIndex,int Size,int PointIndex,CDataSetDoc *pDataSet,long GraphID,long LogGraphID, double Valformax)
{
	double max,whitelvl;
	double colorlevel;
	char	szBuf [ 64 ];
	LPCSTR	lpMsg = NULL;
	
	if (ColorSpace == 0) 
	{
		max=pDataSet->GetMeasure()->GetNearBlack(Size-1).GetRGBValue(GetColorReference())[ColorIndex] / Valformax;
		colorlevel=pDataSet->GetMeasure()->GetNearBlack(PointIndex).GetRGBValue(GetColorReference())[ColorIndex];

		int grayscalesize=pDataSet->GetMeasure()->GetGrayScaleSize();
		int nearwhitescalesize=pDataSet->GetMeasure()->GetNearWhiteScaleSize();

		if (pDataSet->GetMeasure()->GetGray(grayscalesize-1).isValid())
			whitelvl=pDataSet->GetMeasure()->GetGray(grayscalesize-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		else if (pDataSet->GetMeasure()->GetNearWhite(nearwhitescalesize-1).isValid())
			whitelvl=pDataSet->GetMeasure()->GetNearWhite(nearwhitescalesize-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		else
			whitelvl=0;
	}
	else if (ColorSpace == 1) 
	{
		max=pDataSet->GetMeasure()->GetNearBlack(Size-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode) / Valformax;
		colorlevel=pDataSet->GetMeasure()->GetNearBlack(PointIndex).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);

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
		max=pDataSet->GetMeasure()->GetNearBlack(Size-1).GetLuxValue() / Valformax;
		colorlevel=pDataSet->GetMeasure()->GetNearBlack(PointIndex).GetLuxValue();

		int grayscalesize=pDataSet->GetMeasure()->GetGrayScaleSize();
		int nearwhitescalesize=pDataSet->GetMeasure()->GetNearWhiteScaleSize();

		whitelvl=0;
		
		if (pDataSet->GetMeasure()->GetGray(grayscalesize-1).isValid())
			whitelvl=pDataSet->GetMeasure()->GetGray(grayscalesize-1).GetLuxValue();
		
		if (whitelvl == 0 && pDataSet->GetMeasure()->GetNearWhite(nearwhitescalesize-1).isValid())
			whitelvl=pDataSet->GetMeasure()->GetNearWhite(nearwhitescalesize-1).GetLuxValue();
	}
/*
	if(GetConfig() -> m_GammaOffsetType == 1 ) {
		if (ColorSpace == 0) 
		{
			colorlevel-=pDataSet->GetMeasure()->GetNearBlack(0).GetRGBValue(GetColorReference())[ColorIndex];
			max-=pDataSet->GetMeasure()->GetNearBlack(0).GetRGBValue(GetColorReference())[ColorIndex];
		}
		else if (ColorSpace == 1) 
		{
			colorlevel-=pDataSet->GetMeasure()->GetNearBlack(0).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
			max-=pDataSet->GetMeasure()->GetNearBlack(0).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		}
		else 
		{
			colorlevel-=pDataSet->GetMeasure()->GetNearBlack(0).GetLuxValue();
			max-=pDataSet->GetMeasure()->GetNearBlack(0).GetLuxValue();
		}
	}
*/
	if ( pDataSet->GetMeasure()->GetNearBlack(PointIndex).HasLuxValue () )
	{
		if ( GetConfig () ->m_bUseImperialUnits )
			sprintf ( szBuf, "%.5g Ft-cd", pDataSet->GetMeasure()->GetNearBlack(PointIndex).GetLuxValue() * 0.0929 );
		else
			sprintf ( szBuf, "%.5g Lux", pDataSet->GetMeasure()->GetNearBlack(PointIndex).GetLuxValue() );
		
		lpMsg = szBuf;
	}
	
	if((whitelvl > 0)&&(PointIndex != 0))	// log scale is not valid for first value
	{
		m_logGraphCtrl.AddPoint(LogGraphID, GrayLevelToGrayProp( (double)PointIndex*(GetConfig()->m_GammaOffsetType==5?2:1), GetConfig () -> m_bUseRoundDown) * 100., log(colorlevel/whitelvl)/log(GrayLevelToGrayProp( (double)PointIndex, GetConfig () -> m_bUseRoundDown)), lpMsg, NULL);
	}

    if (!m_showL)
	{
		max = whitelvl;
		if (ColorSpace == 1 && ColorIndex == 1)
	        m_graphCtrl.AddPoint(GraphID, GrayLevelToGrayProp( (double)PointIndex*(GetConfig()->m_GammaOffsetType==5?2:1), GetConfig () -> m_bUseRoundDown) * 100., (colorlevel/max)*100.0, lpMsg, whitelvl);
		else
	        m_graphCtrl.AddPoint(GraphID, GrayLevelToGrayProp( (double)PointIndex*(GetConfig()->m_GammaOffsetType==5?2:1), GetConfig () -> m_bUseRoundDown) * 100., (colorlevel/max)*100.0, lpMsg, NULL);
	}
	else 
	{
        m_graphCtrl.AddPoint(GraphID, GrayLevelToGrayProp( (double)PointIndex*(GetConfig()->m_GammaOffsetType==5?2:1), GetConfig () -> m_bUseRoundDown) * 100., Y_to_L(whitelvl?colorlevel/whitelvl:colorlevel/max), lpMsg, NULL);
	}
}

/////////////////////////////////////////////////////////////////////////////
// CNearBlackHistoView

IMPLEMENT_DYNCREATE(CNearBlackHistoView, CSavingView)

CNearBlackHistoView::CNearBlackHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CNearBlackHistoView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
 
CNearBlackHistoView::~CNearBlackHistoView()
{
}
 
BEGIN_MESSAGE_MAP(CNearBlackHistoView, CSavingView)
	//{{AFX_MSG_MAP(CNearBlackHistoView)
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
// CNearBlackHistoView diagnostics

#ifdef _DEBUG
void CNearBlackHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CNearBlackHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CNearBlackHistoView message handlers

void CNearBlackHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTO_GRAPH);
	m_Grapher.m_logGraphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTOLOG_GRAPH);

	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Update this view only when we are concerned
	if ( lHint == UPD_EVERYTHING || lHint == UPD_GRAYSCALEANDCOLORS || lHint == UPD_GRAYSCALE || lHint == UPD_NEARBLACK || lHint == UPD_NEARWHITE || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_SENSORCONFIG || lHint >= UPD_REALTIME)
	{
		m_Grapher.UpdateGraph ( GetDocument () );
		Invalidate(TRUE);
	}
}

DWORD CNearBlackHistoView::GetUserInfo ()
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

void CNearBlackHistoView::SetUserInfo ( DWORD dwUserInfo )
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

void CNearBlackHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
	if(IsWindow(m_Grapher.m_logGraphCtrl.m_hWnd))
		m_Grapher.m_logGraphCtrl.MoveWindow(0,0,cx,cy);
}

void CNearBlackHistoView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
}

BOOL CNearBlackHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CNearBlackHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_NEARBLACK_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_L, m_Grapher.m_showL ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_LOGMODE, m_Grapher.m_doLogMode ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
	pPopup->CheckMenuItem(IDM_LUM_GRAPH_YLUM, m_Grapher.m_showYLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
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

void CNearBlackHistoView::OnLumGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("Near Black Histo","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnLumGraphShowDataRef() //Ki
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Near Black Histo","Show Reference Data",m_Grapher.m_showDataRef);	// Ki
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnLumGraphLogMode() 
{
	m_Grapher.m_doLogMode = !m_Grapher.m_doLogMode;
	GetConfig()->WriteProfileInt("Near Black Histo","Log Mode",m_Grapher.m_doLogMode);
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnLumGraphYLum() 
{
	m_Grapher.m_showYLum = !m_Grapher.m_showYLum;
	GetConfig()->WriteProfileInt("Near Black Histo","Show Y lum",m_Grapher.m_showYLum);
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnLumGraphL() 
{
	m_Grapher.m_showL = !m_Grapher.m_showL;
	m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_showL?"":"%", m_Grapher.m_showL?1:0.1, 0, m_Grapher.m_showL?10:1);
	m_Grapher.m_graphCtrl.FitYScale(1, m_Grapher.m_showL?1:0.1);
	GetConfig()->WriteProfileInt("Near Black Histo","Show L",m_Grapher.m_showL);
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnLumGraphRedLum() 
{
	m_Grapher.m_showRedLum = !m_Grapher.m_showRedLum;
	GetConfig()->WriteProfileInt("Near Black Histo","Show Red",m_Grapher.m_showRedLum);
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnLumGraphGreenLum() 
{
	m_Grapher.m_showGreenLum = !m_Grapher.m_showGreenLum;
	GetConfig()->WriteProfileInt("Near Black Histo","Show Green",m_Grapher.m_showGreenLum);
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnLumGraphBlueLum() 
{
	m_Grapher.m_showBlueLum = !m_Grapher.m_showBlueLum;
	GetConfig()->WriteProfileInt("Near Black Histo","Show Blue",m_Grapher.m_showBlueLum);
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnGraphSave() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.SaveGraphs();
	else
		m_Grapher.m_graphCtrl.SaveGraphs();
}

void CNearBlackHistoView::OnGraphSettings() 
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

void CNearBlackHistoView::OnGraphScaleCustom() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.ChangeScale();
	else
		m_Grapher.m_graphCtrl.ChangeScale();

	m_Grapher.m_graphCtrl.WriteSettings("Near Black Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near Black Histo Log");
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnGraphScaleFit() 
{
	if(m_Grapher.m_doLogMode)
	{
		m_Grapher.m_logGraphCtrl.FitXScale(TRUE,1);
		m_Grapher.m_logGraphCtrl.FitYScale(TRUE,0.1);
	}
	else
	{
		m_Grapher.m_graphCtrl.FitXScale(TRUE, 1);
		m_Grapher.m_graphCtrl.FitYScale(TRUE,m_Grapher.m_showL?1:0.1);
	}

	m_Grapher.m_graphCtrl.WriteSettings("Near Black Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near Black Histo Log");
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnLuminanceGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(0,m_Grapher.m_showL?10:1.0);

	m_Grapher.m_graphCtrl.WriteSettings("Near Black Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near Black Histo Log");
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnGammaGraphYScale1() 
{
	m_Grapher.m_logGraphCtrl.SetYScale(1,3);

	m_Grapher.m_graphCtrl.WriteSettings("Near Black Histo");
	m_Grapher.m_logGraphCtrl.WriteSettings("Near Black Histo Log");
	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnGraphYScaleFit() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.FitYScale(TRUE,0.1);
	else
		m_Grapher.m_graphCtrl.FitYScale(TRUE,m_Grapher.m_showL?1:0.1);

	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnGraphYShiftBottom() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.ShiftYScale(0.1);
	else
		m_Grapher.m_graphCtrl.ShiftYScale(2);

	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnGraphYShiftTop() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.ShiftYScale(-0.1);
	else
		m_Grapher.m_graphCtrl.ShiftYScale(-2);

	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnGraphYZoomIn() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.GrowYScale(0.1,-0.1);
	else
		m_Grapher.m_graphCtrl.GrowYScale(0,-2);

	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnGraphYZoomOut() 
{
	if(m_Grapher.m_doLogMode)
		m_Grapher.m_logGraphCtrl.GrowYScale(-0.1,0.1);
	else
		m_Grapher.m_graphCtrl.GrowYScale(0,2);

	OnUpdate(NULL,NULL,NULL);
}

void CNearBlackHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_NEARBLACK, NULL );
}

