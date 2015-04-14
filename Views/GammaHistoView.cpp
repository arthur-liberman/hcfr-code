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

// GammaHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "GammaHistoView.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

double voltage_to_intensity_srgb( double val );
double voltage_to_intensity_rec709( double val );
double Y_to_L( double val );
/////////////////////////////////////////////////////////////////////////////
// CGammaGrapher

CGammaGrapher::CGammaGrapher ()
{
	CString	Msg;

	Msg.LoadString ( IDS_GAMMA );
	m_luminanceLogGraphID = m_graphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg);
	Msg += " (lux)";
	m_luxmeterLogGraphID = m_graphCtrl.AddGraph(RGB(255,128,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMARED );
	m_redLumLogGraphID = m_graphCtrl.AddGraph(RGB(255,0,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMAGREEN );
	m_greenLumLogGraphID = m_graphCtrl.AddGraph(RGB(0,255,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMABLUE );
	m_blueLumLogGraphID = m_graphCtrl.AddGraph(RGB(0,0,255),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GAMMAREFERENCE );
	m_refLogGraphID = m_graphCtrl.AddGraph(RGB(230,230,230),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_GAMMAAVERAGE );
	m_avgLogGraphID = m_graphCtrl.AddGraph(RGB(0,255,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg += " (lux)";
	m_luxmeterAvgLogGraphID = m_graphCtrl.AddGraph(RGB(128,0,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	
	Msg.LoadString ( IDS_GAMMADATAREF );
	m_luminanceDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg += " (lux)";
	m_luxmeterDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(255,128,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_GAMMAREDDATAREF );
	m_redLumDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_GAMMAGREENDATAREF );
	m_greenLumDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	Msg.LoadString ( IDS_GAMMABLUEDATAREF );
	m_blueLumDataRefLogGraphID = m_graphCtrl.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT); //Ki
	
	m_graphCtrl.SetXAxisProps((LPSTR)(LPCSTR)GetConfig()->m_PercentGray, 10, 0, 100);
	m_graphCtrl.SetYAxisProps("", 0.1, 1, 4);
	m_graphCtrl.SetScale(0,100,1,3);
	m_graphCtrl.ReadSettings("Gamma Histo");

	m_showReference=GetConfig()->GetProfileInt("Gamma Histo","Show Reference",TRUE);
	m_showAverage=GetConfig()->GetProfileInt("Gamma Histo","Show Average",TRUE);
	m_showYLum=GetConfig()->GetProfileInt("Gamma Histo","Show Y lum",TRUE);
	m_showRedLum=GetConfig()->GetProfileInt("Gamma Histo","Show Red",FALSE);
	m_showGreenLum=GetConfig()->GetProfileInt("Gamma Histo","Show Green",FALSE);
	m_showBlueLum=GetConfig()->GetProfileInt("Gamma Histo","Show Blue",FALSE);
	m_showDataRef=GetConfig()->GetProfileInt("Gamma Histo","Show Reference Data",TRUE);	//Ki
}


void CGammaGrapher::UpdateGraph ( CDataSetDoc * pDoc )
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

	m_graphCtrl.ClearGraph(m_refLogGraphID);
	m_graphCtrl.ClearGraph(m_avgLogGraphID);
	m_graphCtrl.ClearGraph(m_luxmeterAvgLogGraphID);

	
	// Compute offset
	pDoc->ComputeGammaAndOffset(&GammaOpt, &GammaOffset, 1,1,size, false);
	pDoc->ComputeGammaAndOffset(&LuxGammaOpt, &LuxGammaOffset, 2,1,size, false);
	if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
	{
		pDoc->ComputeGammaAndOffset(&RefGammaOpt, &RefGammaOffset, 1,1,size,false);
		pDoc->ComputeGammaAndOffset(&RefLuxGammaOpt, &RefLuxGammaOffset, 2,1,size,false);
	}

	if (m_showReference && m_refLogGraphID != -1 && size > 0)
	{	
		// log scale is not valid for first and last value

//		CColor White = pDoc -> GetMeasure () -> GetGray ( size - 1 );
//		CColor Black = pDoc -> GetMeasure () -> GetGray ( 0 );
		CColor White = pDoc -> GetMeasure () -> GetOnOffWhite();
		CColor Black = pDoc -> GetMeasure () -> GetOnOffBlack();
		for (int i=1; i<size-1; i++)
		{
			double x, valx, valy;
			x = ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown );
            if (GetConfig()->m_GammaOffsetType == 4 && White.isValid() && Black.isValid())
			{
				valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown);
                valy = GetBT1886(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split);
			}
			else
			{
				valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+GammaOffset)/(1.0+GammaOffset);
				valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
			}
/*
			if ( GetConfig()->m_bUseReferenceGamma )
			{
				if( GetConfig()->m_colorStandard == sRGB )
					valy=voltage_to_intensity_srgb(GrayLevelToGrayProp(x,bIRE));
				else
					valy=voltage_to_intensity_rec709(GrayLevelToGrayProp(x,bIRE));
			}
*/

			if( valy > 0 && valx > 0)
				m_graphCtrl.AddPoint(m_refLogGraphID, x, log(valy)/ log(valx));
		}
	}
	

	if (m_showAverage && m_avgLogGraphID != -1 && size > 0 && bDataPresent)
	{	
		// Le calcul de la moyenne des gamma et la représentation en échelle log 
		// ne se fait plus avec l'échelle des x = % de blanc mais avec la formule : 
		// (x + offset) / (1+offset) 
		for (int i=1; i<size-1; i++)
			m_graphCtrl.AddPoint(m_avgLogGraphID, ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown ), GammaOpt);
	}

	if ( GetConfig () -> m_nLuminanceCurveMode == 2 )
	{
		if (m_showAverage && m_luxmeterAvgLogGraphID != -1 && size > 0 && bDataPresent && pDoc->GetMeasure()->GetGray(0).HasLuxValue() )
		{	
			// Le calcul de la moyenne des gamma et la représentation en échelle log 
			// ne se fait plus avec l'échelle des x = % de blanc mais avec la formule : 
			// (x + offset) / (1+offset) 
			for (int i=1; i<size-1; i++)
				m_graphCtrl.AddPoint(m_luxmeterAvgLogGraphID, ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown ), LuxGammaOpt);
		}
	}

	m_graphCtrl.ClearGraph(m_redLumLogGraphID);
	m_graphCtrl.ClearGraph(m_redLumDataRefLogGraphID); //Ki
	if (m_showRedLum && m_redLumLogGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,0,size,i,GammaOffset,pDoc,m_redLumLogGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,0,size,i,RefGammaOffset,pDataRef,m_redLumDataRefLogGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_greenLumLogGraphID);
	m_graphCtrl.ClearGraph(m_greenLumDataRefLogGraphID); //Ki
	if (m_showGreenLum && m_greenLumLogGraphID != -1 && size > 0)
	{
		for (int i=0; i<size; i++)
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,1,size,i,GammaOffset,pDoc,m_greenLumLogGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,1,size,i,RefGammaOffset,pDataRef,m_greenLumDataRefLogGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_blueLumLogGraphID);
	m_graphCtrl.ClearGraph(m_blueLumDataRefLogGraphID); //Ki
	if (m_showBlueLum && m_blueLumLogGraphID != -1 && size > 0) 
	{
		for (int i=0; i<size; i++) 
		{
			if (bDataPresent)
				AddPointtoLumGraph(0,2,size,i,GammaOffset,pDoc,m_blueLumLogGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(0,2,size,i,RefGammaOffset,pDataRef,m_blueLumDataRefLogGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_luminanceLogGraphID);
	m_graphCtrl.ClearGraph(m_luminanceDataRefLogGraphID); //Ki
	if (m_showYLum && m_luminanceLogGraphID != -1 && size > 0)	
	{
		for (int i=0; i<size; i++) 
		{
			if (bDataPresent)
				AddPointtoLumGraph(1,1,size,i,GammaOffset,pDoc,m_luminanceLogGraphID,bIRE);
			if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				AddPointtoLumGraph(1,1,size,i,RefGammaOffset,pDataRef,m_luminanceDataRefLogGraphID,bIRE);
		}
	}

	m_graphCtrl.ClearGraph(m_luxmeterLogGraphID);
	m_graphCtrl.ClearGraph(m_luxmeterDataRefLogGraphID);
	if ( GetConfig () -> m_nLuminanceCurveMode == 2 )
	{
		if (m_showYLum && m_luxmeterLogGraphID != -1 && size > 0)	
		{
			for (int i=0; i<size; i++) 
			{
				if (bDataPresent)
				{
					if (pDoc->GetMeasure()->GetGray(0).HasLuxValue())
						AddPointtoLumGraph(2,1,size,i,LuxGammaOffset,pDoc,m_luxmeterLogGraphID,bIRE);
				}
				
				if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
				{
					if (pDataRef->GetMeasure()->GetGray(0).HasLuxValue())
						AddPointtoLumGraph(2,1,size,i,RefLuxGammaOffset,pDataRef,m_luxmeterDataRefLogGraphID,bIRE);
				}
			}
		}
	}
}

/*
ColorSpace : 0 : RGB, 1 : Luminance or Lux, 2 : Lux
ColorIndex : 0 : R, 1 : G, 2 : B
Size : Total number of points
PointIndex : Point index on graph
*pDataSet : pointer on data set to display
GraphID : graph ID for drawing
LogGraphID : graph ID for log drawing, -1 if no Log drawing
*/

void CGammaGrapher::AddPointtoLumGraph(int ColorSpace,int ColorIndex,int Size,int PointIndex,double GammaOffset,CDataSetDoc *pDataSet,long GraphID, BOOL bIRE)
{
	double blacklvl, whitelvl;
	double colorlevel;
	char	szBuf [ 64 ];
	LPCSTR	lpMsg = NULL;

	if (pDataSet->GetMeasure()->GetGray(PointIndex).isValid())
	{
		pDataSet->GetMeasure()->SetGray(Size-1, pDataSet->GetMeasure()->GetOnOffWhite());
	if (ColorSpace == 0) 
	{
		blacklvl=pDataSet->GetMeasure()->GetGray(0).GetRGBValue((GetColorReference()))[ColorIndex];
		whitelvl=pDataSet->GetMeasure()->GetGray(Size-1).GetRGBValue((GetColorReference()))[ColorIndex];
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

	if(GetConfig() -> m_GammaOffsetType == 1 )
	{
		colorlevel-=blacklvl;
		whitelvl-=blacklvl;
		blacklvl = 0;
	}

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

	if((GraphID != -1)&&(PointIndex != 0 && PointIndex != (Size-1)) && colorlevel > 0.0001)	// log scale is not valid for first and last value nor for negative values
	{
		double x = ArrayIndexToGrayLevel ( PointIndex, Size, GetConfig () -> m_bUseRoundDown );

		double valxprime=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+GammaOffset)/(1.0+GammaOffset);

		m_graphCtrl.AddPoint(GraphID, x, log((colorlevel)/whitelvl)/log(valxprime), lpMsg);
	}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CGammaHistoView

IMPLEMENT_DYNCREATE(CGammaHistoView, CSavingView)

CGammaHistoView::CGammaHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CGammaHistoView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
 
CGammaHistoView::~CGammaHistoView()
{
}
 
BEGIN_MESSAGE_MAP(CGammaHistoView, CSavingView)
	//{{AFX_MSG_MAP(CGammaHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_LUM_GRAPH_BLUELUM, OnLumGraphBlueLum)
	ON_COMMAND(IDM_LUM_GRAPH_GREENLUM, OnLumGraphGreenLum)
	ON_COMMAND(IDM_LUM_GRAPH_REDLUM, OnLumGraphRedLum)
	ON_COMMAND(IDM_LUM_GRAPH_SHOWREF, OnLumGraphShowRef)
	ON_COMMAND(IDM_LUM_GRAPH_SHOW_AVG, OnLumGraphShowAvg)
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
	ON_COMMAND(IDM_GAMMA_GRAPH_Y_SCALE1, OnGammaGraphYScale1)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGammaHistoView diagnostics

#ifdef _DEBUG
void CGammaHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CGammaHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CGammaHistoView message handlers

void CGammaHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTOLOG_GRAPH);

	OnUpdate(NULL,UPD_EVERYTHING,NULL);
}

void CGammaHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Update this view only when we are concerned
	if ( lHint == UPD_EVERYTHING || lHint == UPD_GRAYSCALEANDCOLORS || lHint == UPD_GRAYSCALE || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_SENSORCONFIG || lHint == UPD_FREEMEASUREAPPENDED)
	{
		m_Grapher.UpdateGraph ( GetDocument () );

		Invalidate(TRUE);
	}
}

DWORD CGammaHistoView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showAverage		& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showYLum		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_showRedLum		& 0x0001 )	<< 3 )
		  + ( ( m_Grapher.m_showGreenLum	& 0x0001 )	<< 4 )
		  + ( ( m_Grapher.m_showBlueLum		& 0x0001 )	<< 5 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 6 );
}

void CGammaHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showAverage		= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showYLum		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_showRedLum		= ( dwUserInfo >> 3 ) & 0x0001;
	m_Grapher.m_showGreenLum	= ( dwUserInfo >> 4 ) & 0x0001;
	m_Grapher.m_showBlueLum		= ( dwUserInfo >> 5 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 6 ) & 0x0001;
}

void CGammaHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
}

void CGammaHistoView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	
}

BOOL CGammaHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CGammaHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_GAMMA_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
	pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOW_AVG, m_Grapher.m_showAverage ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_YLUM, m_Grapher.m_showYLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_REDLUM, m_Grapher.m_showRedLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_GREENLUM, m_Grapher.m_showGreenLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_BLUELUM, m_Grapher.m_showBlueLum ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, point.x, point.y, this);
}

void CGammaHistoView::OnLumGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphShowDataRef()  //Ki
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphShowAvg() 
{
	m_Grapher.m_showAverage = !m_Grapher.m_showAverage;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Average",m_Grapher.m_showAverage);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphYLum() 
{
	m_Grapher.m_showYLum = !m_Grapher.m_showYLum;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Y lum",m_Grapher.m_showYLum);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphRedLum() 
{
	m_Grapher.m_showRedLum = !m_Grapher.m_showRedLum;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Red",m_Grapher.m_showRedLum);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphGreenLum() 
{
	m_Grapher.m_showGreenLum = !m_Grapher.m_showGreenLum;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Green",m_Grapher.m_showGreenLum);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnLumGraphBlueLum() 
{
	m_Grapher.m_showBlueLum = !m_Grapher.m_showBlueLum;
	GetConfig()->WriteProfileInt("Gamma Histo","Show Blue",m_Grapher.m_showBlueLum);
	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnGraphSave() 
{
	m_Grapher.m_graphCtrl.SaveGraphs();
}

void CGammaHistoView::OnGraphSettings() 
{
	// Add log graphs to first graph control to allow setting change 
	m_Grapher.m_graphCtrl.ChangeSettings();
	m_Grapher.m_graphCtrl.WriteSettings("Gamma Histo");

	OnUpdate(NULL,NULL,NULL);
}

void CGammaHistoView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
}

void CGammaHistoView::OnGraphScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	m_Grapher.m_graphCtrl.FitYScale(TRUE,0.1);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGammaGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(1,3);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE,0.1);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(0.1);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-0.1);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0.1,-0.1);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(-0.1,0.1);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphXScale1() 
{
	m_Grapher.m_graphCtrl.SetXScale(0,100);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphXScale2() 
{
	m_Grapher.m_graphCtrl.SetXScale(20,100);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphXScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale(TRUE);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphXZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowXScale(+10,-10);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphXZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowXScale(-10,+10);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphXShiftLeft() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(-10);
	Invalidate(TRUE);
}

void CGammaHistoView::OnGraphXShiftRight() 
{
	m_Grapher.m_graphCtrl.ShiftXScale(+10);
	Invalidate(TRUE);
}


void CGammaHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_GAMMA, NULL );
}

