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

// LuminanceHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "LuminanceHistoView.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

double Y_to_L ( double val )
{
    //option for plotting L* instead of relative Y
    double fy;
    if (val * 24389 >= 216)
    {
        fy = pow(val, 1.0/3.0);
    }
    else
    {
        fy = (24389 * val / 27 + 16) / 116;
    }
    return 116 * fy - 16;
}

double Y_to_abY ( double val, double Y )
{
    //option for plotting absolute Y
    double fy = val * Y;
    return fy;
}

/////////////////////////////////////////////////////////////////////////////
// CLuminanceGrapher

CLuminanceGrapher::CLuminanceGrapher()
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
	Msg.LoadString ( IDS_SMOOTHEDAVERAGE );
	m_avgGraphID = m_graphCtrl.AddGraph(RGB(0,255,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg += " (lux)";
	m_luxmeterAvgGraphID = m_graphCtrl.AddGraph(RGB(128,0,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);

	Msg.LoadString ( IDS_LUMINANCEDATAREF );
	m_luminanceDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg += " (lux)";
	m_luxmeterDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,128,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_REDDATAREF );
	m_redLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_GREENDATAREF );
	m_greenLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_BLUEDATAREF );
	m_blueLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki	
	
	m_graphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_showL=GetConfig()->GetProfileInt("Luminance Histo","Show L",FALSE);
	m_abY=GetConfig()->GetProfileInt("Luminance Histo","Absolute Y",FALSE);
	m_graphCtrl.SetYAxisProps(m_abY?" cd m-2":m_showL?"":"%", 10, 0, 100);
	m_graphCtrl.SetScale(0,100,0,100);
	m_graphCtrl.ReadSettings("Luminance Histo");

	m_showReference=GetConfig()->GetProfileInt("Luminance Histo","Show Reference",TRUE);
	m_showAverage=GetConfig()->GetProfileInt("Luminance Histo","Show Average",TRUE);
	m_showYLum=GetConfig()->GetProfileInt("Luminance Histo","Show Y lum",TRUE);
	m_showRedLum=GetConfig()->GetProfileInt("Luminance Histo","Show Red",FALSE);
	m_showGreenLum=GetConfig()->GetProfileInt("Luminance Histo","Show Green",FALSE);
	m_showBlueLum=GetConfig()->GetProfileInt("Luminance Histo","Show Blue",FALSE);
	m_showDataRef=GetConfig()->GetProfileInt("Luminance Histo","Show Reference Data",TRUE);	//Ki
}

void CLuminanceGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	BOOL	bDataPresent = FALSE;
	BOOL	bIRE = pDoc->GetMeasure()->m_bIREScaleMode;
	double GammaOffset,GammaOpt,RefGammaOffset,RefGammaOpt,LuxGammaOffset,LuxGammaOpt,RefLuxGammaOffset,RefLuxGammaOpt;

	m_graphCtrl.SetXAxisProps(bIRE?"IRE":(LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);

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

	if (pDoc->GetMeasure()->GetGray(0).isValid())
		bDataPresent = TRUE;

	if (pDataRef && !pDataRef->GetMeasure()->GetGray(0).isValid())
		pDataRef = NULL;

	m_graphCtrl.ClearGraph(m_refGraphID);
	m_graphCtrl.ClearGraph(m_avgGraphID);
	m_graphCtrl.ClearGraph(m_luxmeterAvgGraphID);

	
// Compute offset
	pDoc->ComputeGammaAndOffset(&GammaOpt, &GammaOffset, 1,1,size,false);
	pDoc->ComputeGammaAndOffset(&LuxGammaOpt, &LuxGammaOffset, 2,1,size,false);

	if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef != pDoc))
	{
		pDoc->ComputeGammaAndOffset(&RefGammaOpt, &RefGammaOffset, 1,1,size,false);
		pDoc->ComputeGammaAndOffset(&RefLuxGammaOpt, &RefLuxGammaOffset, 2,1,size,false);
	}
			
	int mode = GetConfig()->m_GammaOffsetType;
    
	CColor White = pDoc -> GetMeasure () -> GetOnOffWhite();
	if (m_showReference && m_refGraphID != -1)// && size > 0)// && bDataPresent)
	{	
		for (int i=0; i<size; i++)
		{
			double x, valx, valy;
			x = ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit);
	    	CColor Black = pDoc -> GetMeasure () -> GetOnOffBlack();
			if (GetConfig()->m_colorStandard == sRGB) mode = 99;
			if (  (mode >= 4) )
			{
				valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit);
				if (mode == 5)
					valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap) * 100. / (White.GetY());
				else
	                valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			}
			else
            {
                valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit)+GammaOffset)/(1.0+GammaOffset);
                valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
				if (mode == 1) //black compensation target
					valy = (Black.GetY() + ( valy * ( White.GetY() - Black.GetY() ) )) / White.GetY();
            }

            if (!m_showL && !m_abY)
				m_graphCtrl.AddPoint(m_refGraphID, x, 100.0*valy, NULL, White.GetY());
            else
            {
				if (m_showL)
					valy = Y_to_L (valy);
				else
					valy = Y_to_abY (valy, White.GetY());

                m_graphCtrl.AddPoint(m_refGraphID, x, valy);
            }
		}
	}
	

	if (m_showAverage && m_avgGraphID != -1 && size > 0 && bDataPresent)
	{	
		int		nb = 0;
		double	avg = 0.0;
    	CColor White = pDoc -> GetMeasure () -> GetOnOffWhite();

		// Le calcul de la moyenne des gamma et la représentation en échelle log 
		// ne se fait plus avec l'échelle des x = % de blanc mais avec la formule : 
		// (x + offset) / (1+offset) 
		for (int i=0; i<size; i++)
		{
			double x, valx, valy;
			x = ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit);

			valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit)+GammaOffset)/(1.0+GammaOffset);
            valy=pow(valx, GammaOpt );
            if (mode != 5)
			{
				if (!m_showL && !m_abY)
					m_graphCtrl.AddPoint(m_avgGraphID, x, 100.0*valy, NULL, White.GetY());
	            else
		        {
					if (m_showL)
					    valy = Y_to_L (valy);
					else
		                valy = Y_to_abY (valy, White.GetY());

					m_graphCtrl.AddPoint(m_avgGraphID, x, valy);
			    }
			}
		}
	}

	if ( GetConfig () -> m_nLuminanceCurveMode == 2 )
	{
		if (m_showAverage && m_luxmeterAvgGraphID != -1 && size > 0 && bDataPresent && pDoc->GetMeasure()->GetGray(0).HasLuxValue() )
		{	
			int		nb = 0;
			double	avg = 0.0;

			// Le calcul de la moyenne des gamma et la représentation en échelle log 
			// ne se fait plus avec l'échelle des x = % de blanc mais avec la formule : 
			// (x + offset) / (1+offset) 
			for (int i=0; i<size; i++)
			{
				double x, valx, valy;
				x = ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit );

				valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit)+LuxGammaOffset)/(1.0+LuxGammaOffset);
				valy=pow(valx, LuxGammaOpt );

				m_graphCtrl.AddPoint(m_luxmeterAvgGraphID, x, valy*100.0);
			}
		}
	}

	m_graphCtrl.ClearGraph(m_redLumGraphID);
	m_graphCtrl.ClearGraph(m_redLumDataRefGraphID);	//Ki
	if (m_showRedLum && m_redLumGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,0,size,i,GammaOffset,pDoc,m_redLumGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,0,size,i,RefGammaOffset,pDataRef,m_redLumDataRefGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_greenLumGraphID);
	m_graphCtrl.ClearGraph(m_greenLumDataRefGraphID); //Ki
	if (m_showGreenLum && m_greenLumGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,1,size,i,GammaOffset,pDoc,m_greenLumGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,1,size,i,RefGammaOffset,pDataRef,m_greenLumDataRefGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_blueLumGraphID);
	m_graphCtrl.ClearGraph(m_blueLumDataRefGraphID); //Ki
	if (m_showBlueLum && m_blueLumGraphID != -1 && size > 0) 
	{
		for (int i=0; i<size; i++) 
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,2,size,i,GammaOffset,pDoc,m_blueLumGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,2,size,i,RefGammaOffset,pDataRef,m_blueLumDataRefGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_luminanceGraphID);
	m_graphCtrl.ClearGraph(m_luminanceDataRefGraphID); //Ki
	if (m_showYLum && m_luminanceGraphID != -1 && size > 0)	
	{
		for (int i=0; i<size; i++) 
		{
			if (bDataPresent)
				AddPointtoLumGraph(1,1,size,i,GammaOffset,pDoc,m_luminanceGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(1,1,size,i,RefGammaOffset,pDataRef,m_luminanceDataRefGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_luxmeterGraphID);
	m_graphCtrl.ClearGraph(m_luxmeterDataRefGraphID);
	if ( GetConfig () -> m_nLuminanceCurveMode == 2 )
	{
		if (m_showYLum && m_luxmeterGraphID != -1 && size > 0)	
		{
			for (int i=0; i<size; i++) 
			{
				if (bDataPresent)
				{
					if (pDoc->GetMeasure()->GetGray(0).HasLuxValue())
						AddPointtoLumGraph(2,1,size,i,LuxGammaOffset,pDoc,m_luxmeterGraphID,bIRE);
				}
				
				if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				{
					if (pDataRef->GetMeasure()->GetGray(0).HasLuxValue())
						AddPointtoLumGraph(2,1,size,i,RefLuxGammaOffset,pDataRef,m_luxmeterDataRefGraphID,bIRE);
				}
			}
		}
	}
}

/*
ColorSpace : 0 : RGB, 1 : Luma or Lux, 2 : Lux
ColorIndex : 0 : R, 1 : G, 2 : B
Size : Total number of points
PointIndex : Point index on graph
*pDataSet : pointer on data set to display
GraphID : graph ID for drawing
*/

void CLuminanceGrapher::AddPointtoLumGraph(int ColorSpace,int ColorIndex,int Size,int PointIndex,double GammaOffset,CDataSetDoc *pDataSet,long GraphID, BOOL bIRE)
{
	double colorlevel;
	double blacklvl, whitelvl;
	char	szBuf [ 64 ];
	LPCSTR	lpMsg = NULL;
	if (pDataSet->GetMeasure()->GetGray(PointIndex).isValid())
	{
		pDataSet->GetMeasure()->SetGray(Size-1, pDataSet->GetMeasure()->GetOnOffWhite());

		if (ColorSpace == 0) 
		{
			blacklvl=pDataSet->GetMeasure()->GetGray(0).GetRGBValue((GetColorReference()))[ColorIndex];
//			whitelvl=pDataSet->GetMeasure()->GetGray(Size-1).GetRGBValue((GetColorReference()))[ColorIndex];
			whitelvl=pDataSet->GetMeasure()->GetGray(Size-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
			colorlevel=pDataSet->GetMeasure()->GetGray(PointIndex).GetRGBValue((GetColorReference()))[ColorIndex];
		}
		else if (ColorSpace == 1) 
		{
			blacklvl=pDataSet->GetMeasure()->GetGray(0).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
			whitelvl=pDataSet->GetMeasure()->GetGray(Size-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
			colorlevel=pDataSet->GetMeasure()->GetGray(PointIndex).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		}
		else
		{
			blacklvl=pDataSet->GetMeasure()->GetGray(0).GetLuxValue();
			whitelvl=pDataSet->GetMeasure()->GetGray(Size-1).GetLuxValue();
			colorlevel=pDataSet->GetMeasure()->GetGray(PointIndex).GetLuxValue();
		}

//		if(GetConfig() -> m_GammaOffsetType == 1 )
//		{
//			colorlevel-=blacklvl;
//			whitelvl-=blacklvl;
//			blacklvl = 0;
//		}

		if ( pDataSet->GetMeasure()->GetGray(PointIndex).HasLuxValue () )
		{
			if ( GetConfig () ->m_bUseImperialUnits )
				sprintf ( szBuf, "%.5g Ft-cd", pDataSet->GetMeasure()->GetGray(PointIndex).GetLuxValue() * 0.0929 );
			else
				sprintf ( szBuf, "%.5g Lux", pDataSet->GetMeasure()->GetGray(PointIndex).GetLuxValue() );
		
			lpMsg = szBuf;
		}
	
	
	// Le calcul de la moyenne des gamma et la représentation en échelle log 
	// ne se fait plus avec l'échelle des x = % de blanc mais avec la formule : 
	// (x + offset) / (1+offset) 

		double x = ArrayIndexToGrayLevel ( PointIndex, Size, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit );
		if (!m_showL && !m_abY)
		{
			if (ColorSpace == 1 && ColorIndex == 1)
				m_graphCtrl.AddPoint(GraphID, x, (colorlevel/whitelvl)*100.0, lpMsg, whitelvl);
			else
				m_graphCtrl.AddPoint(GraphID, x, (colorlevel/whitelvl)*100.0, lpMsg);
		}
		else
		{
			if (m_abY)
				m_graphCtrl.AddPoint(GraphID, x, Y_to_abY(colorlevel,1.0), lpMsg);
			else
				m_graphCtrl.AddPoint(GraphID, x, Y_to_L(colorlevel/whitelvl), lpMsg);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CLuminanceHistoView

IMPLEMENT_DYNCREATE(CLuminanceHistoView, CSavingView)

CLuminanceHistoView::CLuminanceHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CLuminanceHistoView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
 
CLuminanceHistoView::~CLuminanceHistoView()
{
}
 
BEGIN_MESSAGE_MAP(CLuminanceHistoView, CSavingView)
	//{{AFX_MSG_MAP(CLuminanceHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_LUM_GRAPH_BLUELUM, OnLumGraphBlueLum)
	ON_COMMAND(IDM_LUM_GRAPH_GREENLUM, OnLumGraphGreenLum)
	ON_COMMAND(IDM_LUM_GRAPH_REDLUM, OnLumGraphRedLum)
	ON_COMMAND(IDM_LUM_GRAPH_SHOWREF, OnLumGraphShowRef)
	ON_COMMAND(IDM_LUM_GRAPH_SHOW_AVG, OnLumGraphShowAvg)
	ON_COMMAND(IDM_LUM_GRAPH_L, OnLumGraphL)
	ON_COMMAND(IDM_LUM_GRAPH_Yab, OnLumGraphYab)
	ON_COMMAND(IDM_LUM_GRAPH_DATAREF, OnLumGraphShowDataRef)	//Ki
	ON_COMMAND(IDM_LUM_GRAPH_YLUM, OnLumGraphYLum)
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
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE1, OnLuminanceGraphYScale1)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLuminanceHistoView diagnostics

#ifdef _DEBUG
void CLuminanceHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CLuminanceHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CLuminanceHistoView message handlers

void CLuminanceHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTO_GRAPH);

	OnUpdate(NULL,UPD_EVERYTHING,NULL);
}

void CLuminanceHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Update this view only when we are concerned
	if ( lHint == UPD_EVERYTHING || lHint == UPD_GRAYSCALEANDCOLORS || lHint == UPD_GRAYSCALE || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_SENSORCONFIG || lHint == UPD_FREEMEASUREAPPENDED || lHint >= UPD_REALTIME)
	{
		m_Grapher.UpdateGraph ( GetDocument () );
		Invalidate(TRUE);
	}
}

DWORD CLuminanceHistoView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showAverage		& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showYLum		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_showRedLum		& 0x0001 )	<< 3 )
		  + ( ( m_Grapher.m_showGreenLum	& 0x0001 )	<< 4 )
		  + ( ( m_Grapher.m_showBlueLum		& 0x0001 )	<< 5 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 6 )
		  + ( ( m_Grapher.m_showL		& 0x0001 )		<< 7 )
		  + ( ( m_Grapher.m_abY		& 0x0001 )			<< 8 );
}

void CLuminanceHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showAverage		= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showYLum		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_showRedLum		= ( dwUserInfo >> 3 ) & 0x0001;
	m_Grapher.m_showGreenLum	= ( dwUserInfo >> 4 ) & 0x0001;
	m_Grapher.m_showBlueLum		= ( dwUserInfo >> 5 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 6 ) & 0x0001;
	m_Grapher.m_showL			= ( dwUserInfo >> 7 ) & 0x0001;
	m_Grapher.m_abY				= ( dwUserInfo >> 8 ) & 0x0001;
}

void CLuminanceHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
}

void CLuminanceHistoView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
	m_Grapher.m_graphCtrl.WriteSettings("Luminance Histo");
}

BOOL CLuminanceHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CLuminanceHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_LUM_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
	pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOW_AVG, m_Grapher.m_showAverage ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_YLUM, m_Grapher.m_showYLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_REDLUM, m_Grapher.m_showRedLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_GREENLUM, m_Grapher.m_showGreenLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_BLUELUM, m_Grapher.m_showBlueLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_L, m_Grapher.m_showL ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_Yab, m_Grapher.m_abY ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, point.x, point.y, this);
}

void CLuminanceHistoView::OnLumGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("Luminance Histo","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CLuminanceHistoView::OnLumGraphShowDataRef()  //Ki
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Luminance Histo","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CLuminanceHistoView::OnLumGraphShowAvg() 
{
	m_Grapher.m_showAverage = !m_Grapher.m_showAverage;
	GetConfig()->WriteProfileInt("Luminance Histo","Show Average",m_Grapher.m_showAverage);
	OnUpdate(NULL,NULL,NULL);
}

void CLuminanceHistoView::OnLumGraphYLum() 
{
	m_Grapher.m_showYLum = !m_Grapher.m_showYLum;
	GetConfig()->WriteProfileInt("Luminance Histo","Show Y lum",m_Grapher.m_showYLum);
	OnUpdate(NULL,NULL,NULL);
}

void CLuminanceHistoView::OnLumGraphL() 
{
	m_Grapher.m_showL = !m_Grapher.m_showL;
	if (m_Grapher.m_abY)
		m_Grapher.m_abY = !m_Grapher.m_showL;
	GetConfig()->WriteProfileInt("Luminance Histo","Absolute Y",m_Grapher.m_abY);
	GetConfig()->WriteProfileInt("Luminance Histo","Show L",m_Grapher.m_showL);
	m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_showL?"":"%", 10, 0, 100);
	OnUpdate(NULL,NULL,NULL);
	OnGraphYScaleFit(); 
}

void CLuminanceHistoView::OnLumGraphYab() 
{
	m_Grapher.m_abY = !m_Grapher.m_abY;
	if (m_Grapher.m_showL)
		m_Grapher.m_showL = !m_Grapher.m_abY;
	GetConfig()->WriteProfileInt("Luminance Histo","Absolute Y",m_Grapher.m_abY);
	GetConfig()->WriteProfileInt("Luminance Histo","Show L",m_Grapher.m_showL);
	m_Grapher.m_graphCtrl.SetYAxisProps(m_Grapher.m_abY?" cd m-2 ":m_Grapher.m_abY?"":"%", 10, 0, 100);
	OnUpdate(NULL,NULL,NULL);
	OnGraphYScaleFit();
}

void CLuminanceHistoView::OnLumGraphRedLum() 
{
	m_Grapher.m_showRedLum = !m_Grapher.m_showRedLum;
	GetConfig()->WriteProfileInt("Luminance Histo","Show Red",m_Grapher.m_showRedLum);
	OnUpdate(NULL,NULL,NULL);
}

void CLuminanceHistoView::OnLumGraphGreenLum() 
{
	m_Grapher.m_showGreenLum = !m_Grapher.m_showGreenLum;
	GetConfig()->WriteProfileInt("Luminance Histo","Show Green",m_Grapher.m_showGreenLum);
	OnUpdate(NULL,NULL,NULL);
}

void CLuminanceHistoView::OnLumGraphBlueLum() 
{
	m_Grapher.m_showBlueLum = !m_Grapher.m_showBlueLum;
	GetConfig()->WriteProfileInt("Luminance Histo","Show Blue",m_Grapher.m_showBlueLum);
	OnUpdate(NULL,NULL,NULL);
}

void CLuminanceHistoView::OnGraphSave() 
{
	m_Grapher.m_graphCtrl.SaveGraphs();
}

void CLuminanceHistoView::OnGraphSettings() 
{
	// Add log graphs to first graph control to allow setting change 
	m_Grapher.m_graphCtrl.ChangeSettings();

	OnUpdate(NULL,NULL,NULL);
}

void CLuminanceHistoView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
	m_Grapher.m_graphCtrl.WriteSettings("Luminance Histo");
}

void CLuminanceHistoView::OnGraphScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	m_Grapher.m_graphCtrl.FitYScale(TRUE);
	m_Grapher.m_graphCtrl.WriteSettings("Luminance Histo");
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnLuminanceGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(0,100);
	m_Grapher.m_graphCtrl.WriteSettings("Luminance Histo");
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE);
	m_Grapher.m_graphCtrl.WriteSettings("Luminance Histo");
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(10);
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-10);
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0,-10);
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0,10);
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphXScale1() 
{
	m_Grapher.m_graphCtrl.SetXScale(0,100);
	m_Grapher.m_graphCtrl.WriteSettings("Luminance Histo");
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphXScale2() 
{
	m_Grapher.m_graphCtrl.SetXScale(20,100);
	m_Grapher.m_graphCtrl.WriteSettings("Luminance Histo");
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphXScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	m_Grapher.m_graphCtrl.WriteSettings("Luminance Histo");
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphXZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowXScale(+10,-10);
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphXZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowXScale(-10,+10);
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphXShiftLeft() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(-10);
	Invalidate(TRUE);
}

void CLuminanceHistoView::OnGraphXShiftRight() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(+10);
	Invalidate(TRUE);
}


void CLuminanceHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_LUMINANCE, NULL );
}

