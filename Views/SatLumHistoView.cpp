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

// SatLumHistoView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "SatLumHistoView.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSatLumGrapher

CSatLumGrapher::CSatLumGrapher ()
{
	CString		Msg;

	Msg.LoadString ( IDS_RED );
	m_redLumGraphID = m_graphCtrl.AddGraph(RGB(255,0,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_GREEN );
	m_greenLumGraphID = m_graphCtrl.AddGraph(RGB(0,255,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_BLUE );
	m_blueLumGraphID = m_graphCtrl.AddGraph(RGB(0,0,255),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_YELLOW );
	m_yellowLumGraphID = m_graphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_CYAN );
	m_cyanLumGraphID = m_graphCtrl.AddGraph(RGB(0,255,255),(LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_MAGENTA );
	m_magentaLumGraphID = m_graphCtrl.AddGraph(RGB(255,0,255),(LPSTR)(LPCSTR)Msg);

	Msg.LoadString ( IDS_REDDATAREF );
	m_redLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_GREENDATAREF );
	m_greenLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_BLUEDATAREF );
	m_blueLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,0,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_YELLOWDATAREF );
	m_yellowLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_CYANDATAREF );
	m_cyanLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_MAGENTADATAREF );
	m_magentaLumDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,255),(LPSTR)(LPCSTR)Msg,1,PS_DOT);

	Msg.LoadString ( IDS_REFRED );
	m_ref_redLumGraphID = m_graphCtrl.AddGraph(RGB(128,0,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_REFGREEN );
	m_ref_greenLumGraphID = m_graphCtrl.AddGraph(RGB(0,128,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_REFBLUE );
	m_ref_blueLumGraphID = m_graphCtrl.AddGraph(RGB(0,0,128),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_REFYELLOW );
	m_ref_yellowLumGraphID = m_graphCtrl.AddGraph(RGB(128,128,0),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_REFCYAN );
	m_ref_cyanLumGraphID = m_graphCtrl.AddGraph(RGB(0,128,128),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_REFMAGENTA );
	m_ref_magentaLumGraphID = m_graphCtrl.AddGraph(RGB(128,0,128),(LPSTR)(LPCSTR)Msg,1,PS_DOT);
	
	m_graphCtrl.SetXAxisProps("% Sat", 10, 0, 100);
	m_graphCtrl.SetYAxisProps("%", 10, 0, 100);
	m_graphCtrl.SetScale(0,100,0,100);
	m_graphCtrl.ReadSettings("Saturation Luminance Histo");

	m_showReferences=GetConfig()->GetProfileInt("Saturation Luminance Histo","Show References",TRUE);
	m_showPrimaries=GetConfig()->GetProfileInt("Saturation Luminance Histo","Show Primaries",TRUE);
	m_showSecondaries=GetConfig()->GetProfileInt("Saturation Luminance Histo","Show Secondaries",FALSE);
	m_showDataRef=GetConfig()->GetProfileInt("Saturation Luminance Histo","Show Reference Data",TRUE);
}

void CSatLumGrapher::UpdateGraph ( CDataSetDoc * pDoc )
{
	CDataSetDoc *pDataRef = GetDataRef();
	int size=pDoc->GetMeasure()->GetSaturationSize();

	if ( pDataRef )
	{
		// Check if data reference is comparable
		if ( pDataRef->GetMeasure()->GetSaturationSize() != size )
		{
			// Cannot use data reference
			pDataRef = NULL;
		}
	}
	
	// Retrieve color luminance coefficients matching actual reference
	double KR=pDoc->GetMeasure()->GetRefPrimary(0).GetLuminance();
	double KG=pDoc->GetMeasure()->GetRefPrimary(1).GetLuminance();
	double KB=pDoc->GetMeasure()->GetRefPrimary(2).GetLuminance();
	double KY=pDoc->GetMeasure()->GetRefSecondary(0).GetLuminance();
	double KC=pDoc->GetMeasure()->GetRefSecondary(1).GetLuminance();
	double KM=pDoc->GetMeasure()->GetRefSecondary(2).GetLuminance();

    CColor satcolor;

	if(IsWindow(m_graphCtrl.m_hWnd))
		m_graphCtrl.ShowWindow(SW_SHOW);

	m_graphCtrl.ClearGraph(m_ref_redLumGraphID);
	m_graphCtrl.ClearGraph(m_ref_greenLumGraphID);
	m_graphCtrl.ClearGraph(m_ref_blueLumGraphID);
	m_graphCtrl.ClearGraph(m_ref_yellowLumGraphID);
	m_graphCtrl.ClearGraph(m_ref_cyanLumGraphID);
	m_graphCtrl.ClearGraph(m_ref_magentaLumGraphID);

	double WhiteY = pDoc->GetMeasure()->GetOnOffWhite().GetLuminance(), ref_coeff=1.0;
	if (pDoc->GetMeasure()->GetPrimeWhite().isValid() &&  !(GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb))
		WhiteY = pDoc->GetMeasure()->GetPrimeWhite().GetLuminance();

	if (GetConfig()->m_GammaOffsetType==5)
	{
		WhiteY = 100.;
		ref_coeff = 100.;
	}

	double luma_coeff = ( 100.0 ) / WhiteY;
	
	
	if (m_showReferences && m_ref_redLumGraphID != -1 && size > 0)
	{	
		for (int i=0; i<size; i++)
		{
            satcolor = pDoc->GetMeasure()->GetRefSat(0,double (i)/ double(size-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
            KR = satcolor.GetLuminance() * ref_coeff;
            satcolor = pDoc->GetMeasure()->GetRefSat(1,double (i)/ double(size-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
            KG = satcolor.GetLuminance() * ref_coeff;
            satcolor = pDoc->GetMeasure()->GetRefSat(2,double (i)/ double(size-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
            KB = satcolor.GetLuminance() * ref_coeff;
            satcolor = pDoc->GetMeasure()->GetRefSat(3,double (i)/ double(size-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
            KY = satcolor.GetLuminance() * ref_coeff;
            satcolor = pDoc->GetMeasure()->GetRefSat(4,double (i)/ double(size-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
            KC = satcolor.GetLuminance() * ref_coeff;
            satcolor = pDoc->GetMeasure()->GetRefSat(5,double (i)/ double(size-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
            KM = satcolor.GetLuminance() * ref_coeff;
			m_graphCtrl.AddPoint(m_ref_redLumGraphID, i*100/(size-1), KR * 100.0);
			m_graphCtrl.AddPoint(m_ref_greenLumGraphID, i*100/(size-1), KG * 100.0);
			m_graphCtrl.AddPoint(m_ref_blueLumGraphID, i*100/(size-1), KB * 100.0);
			m_graphCtrl.AddPoint(m_ref_yellowLumGraphID, i*100/(size-1), KY * 100.0);
			m_graphCtrl.AddPoint(m_ref_cyanLumGraphID, i*100/(size-1), KC * 100.0);
			m_graphCtrl.AddPoint(m_ref_magentaLumGraphID, i*100/(size-1), KM * 100.0);
		}
	}
	
	m_graphCtrl.ClearGraph(m_redLumGraphID);
	
	if (m_showPrimaries && m_redLumGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetRedSat(0).isValid() )
	{
		
		for (int i=0; i<size; i++)
		{
			double red_val = pDoc->GetMeasure()->GetRedSat(i).GetLuminance() * luma_coeff;
			
			m_graphCtrl.AddPoint(m_redLumGraphID, i*100/(size-1), red_val);
		}
	}

	m_graphCtrl.ClearGraph(m_greenLumGraphID);
	if (m_showPrimaries && m_greenLumGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetGreenSat(0).isValid() )
	{
		
		for (int i=0; i<size; i++)
		{
			double green_val = pDoc->GetMeasure()->GetGreenSat(i).GetLuminance() * luma_coeff;
			
			m_graphCtrl.AddPoint(m_greenLumGraphID, i*100/(size-1), green_val);
		}
	}

	m_graphCtrl.ClearGraph(m_blueLumGraphID);
	if (m_showPrimaries && m_blueLumGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetBlueSat(0).isValid() )
	{
		
		for (int i=0; i<size; i++)
		{
			double blue_val = pDoc->GetMeasure()->GetBlueSat(i).GetLuminance() * luma_coeff;
			
			m_graphCtrl.AddPoint(m_blueLumGraphID, i*100/(size-1), blue_val);
		}
	}

	m_graphCtrl.ClearGraph(m_yellowLumGraphID);
	if (m_showSecondaries && m_yellowLumGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetYellowSat(0).isValid() )
	{
		
		for (int i=0; i<size; i++)
		{
			double yellow_val = pDoc->GetMeasure()->GetYellowSat(i).GetLuminance() * luma_coeff;
			
			m_graphCtrl.AddPoint(m_yellowLumGraphID, i*100/(size-1), yellow_val);
		}
	}

	m_graphCtrl.ClearGraph(m_cyanLumGraphID);
	if (m_showSecondaries && m_cyanLumGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetCyanSat(0).isValid() )
	{
		
		for (int i=0; i<size; i++)
		{
			double cyan_val = pDoc->GetMeasure()->GetCyanSat(i).GetLuminance() * luma_coeff;
			
			m_graphCtrl.AddPoint(m_cyanLumGraphID, i*100/(size-1), cyan_val);
		}
	}

	m_graphCtrl.ClearGraph(m_magentaLumGraphID);
	if (m_showSecondaries && m_magentaLumGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetMagentaSat(0).isValid() )
	{
		
		for (int i=0; i<size; i++)
		{
			double magenta_val = pDoc->GetMeasure()->GetMagentaSat(i).GetLuminance() * luma_coeff;
			
			m_graphCtrl.AddPoint(m_magentaLumGraphID, i*100/(size-1), magenta_val);
		}
	}

	m_graphCtrl.ClearGraph(m_redLumDataRefGraphID);
	m_graphCtrl.ClearGraph(m_greenLumDataRefGraphID);
	m_graphCtrl.ClearGraph(m_blueLumDataRefGraphID);
	m_graphCtrl.ClearGraph(m_yellowLumDataRefGraphID);
	m_graphCtrl.ClearGraph(m_cyanLumDataRefGraphID);
	m_graphCtrl.ClearGraph(m_magentaLumDataRefGraphID);

	if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
	{
		if (m_showPrimaries && m_redLumDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetRedSat(0).isValid() )
		{
//			double red_coeff = ( KR * 100.0 ) / pDataRef->GetMeasure()->GetRedSat(size-1).GetLuminance();
			
			for (int i=0; i<size; i++)
			{
				double red_val = pDataRef->GetMeasure()->GetRedSat(i).GetLuminance() * luma_coeff;
				
				m_graphCtrl.AddPoint(m_redLumDataRefGraphID, i*100/(size-1), red_val);
			}
		}

		if (m_showPrimaries && m_greenLumDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetGreenSat(0).isValid() )
		{
//			double green_coeff = ( KG * 100.0 ) / pDataRef->GetMeasure()->GetGreenSat(size-1).GetLuminance();
			
			for (int i=0; i<size; i++)
			{
				double green_val = pDataRef->GetMeasure()->GetGreenSat(i).GetLuminance() * luma_coeff;
				
				m_graphCtrl.AddPoint(m_greenLumDataRefGraphID, i*100/(size-1), green_val);
			}
		}

		if (m_showPrimaries && m_blueLumDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetBlueSat(0).isValid() )
		{
//			double blue_coeff = ( KB * 100.0 ) / pDataRef->GetMeasure()->GetBlueSat(size-1).GetLuminance();
			
			for (int i=0; i<size; i++)
			{
				double blue_val = pDataRef->GetMeasure()->GetBlueSat(i).GetLuminance() * luma_coeff;
				
				m_graphCtrl.AddPoint(m_blueLumDataRefGraphID, i*100/(size-1), blue_val);
			}
		}

		if (m_showSecondaries && m_yellowLumDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetYellowSat(0).isValid() )
		{
//			double yellow_coeff = ( KY * 100.0 ) / pDataRef->GetMeasure()->GetYellowSat(size-1).GetLuminance();
			
			for (int i=0; i<size; i++)
			{
				double yellow_val = pDataRef->GetMeasure()->GetYellowSat(i).GetLuminance() * luma_coeff;
				
				m_graphCtrl.AddPoint(m_yellowLumDataRefGraphID, i*100/(size-1), yellow_val);
			}
		}

		if (m_showSecondaries && m_cyanLumDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetCyanSat(0).isValid() )
		{
//			double cyan_coeff = ( KC * 100.0 ) / pDataRef->GetMeasure()->GetCyanSat(size-1).GetLuminance();
			
			for (int i=0; i<size; i++)
			{
				double cyan_val = pDataRef->GetMeasure()->GetCyanSat(i).GetLuminance() * luma_coeff;
				
				m_graphCtrl.AddPoint(m_cyanLumDataRefGraphID, i*100/(size-1), cyan_val);
			}
		}

		if (m_showSecondaries && m_magentaLumDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetMagentaSat(0).isValid() )
		{
//			double magenta_coeff = ( KM * 100.0 ) / pDataRef->GetMeasure()->GetMagentaSat(size-1).GetLuminance();
			
			for (int i=0; i<size; i++)
			{
				double magenta_val = pDataRef->GetMeasure()->GetMagentaSat(i).GetLuminance() * luma_coeff;
				
				m_graphCtrl.AddPoint(m_magentaLumDataRefGraphID, i*100/(size-1), magenta_val);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CSatLumHistoView

IMPLEMENT_DYNCREATE(CSatLumHistoView, CSavingView)

CSatLumHistoView::CSatLumHistoView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CSatLumHistoView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
 
CSatLumHistoView::~CSatLumHistoView()
{
}
 
BEGIN_MESSAGE_MAP(CSatLumHistoView, CSavingView)
	//{{AFX_MSG_MAP(CSatLumHistoView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_LUM_GRAPH_PRIMARIES, OnLumGraphPrimaries)
	ON_COMMAND(IDM_LUM_GRAPH_SECONDARIES, OnLumGraphSecondaries)
	ON_COMMAND(IDM_LUM_GRAPH_SHOWREF, OnLumGraphShowRef)
	ON_COMMAND(IDM_LUM_GRAPH_DATAREF, OnLumGraphShowDataRef)
	ON_COMMAND(IDM_GRAPH_SETTINGS, OnGraphSettings)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE1, OnLuminanceGraphYScale1)
	ON_COMMAND(IDM_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSatLumHistoView diagnostics

#ifdef _DEBUG
void CSatLumHistoView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CSatLumHistoView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSatLumHistoView message handlers

void CSatLumHistoView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTO_GRAPH);

	OnUpdate(NULL,NULL,NULL);
}

void CSatLumHistoView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// Do nothing when not concerned
	switch ( lHint )
	{
		case UPD_GRAYSCALE:
		case UPD_NEARBLACK:
		case UPD_NEARWHITE:
		case UPD_CONTRAST:
		case UPD_FREEMEASURES:
		case UPD_FREEMEASUREAPPENDED:
		case UPD_GENERATORCONFIG:
		case UPD_SELECTEDCOLOR:
			 return;
	}

	m_Grapher.UpdateGraph ( GetDocument () );

	Invalidate(TRUE);
}

DWORD CSatLumHistoView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReferences	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showPrimaries	& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showSecondaries	& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 3 );
}

void CSatLumHistoView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReferences	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showPrimaries	= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showSecondaries	= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 3 ) & 0x0001;
}

void CSatLumHistoView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy);
}

void CSatLumHistoView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Luminance Histo");	
}

BOOL CSatLumHistoView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CSatLumHistoView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_SATLUM_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReferences ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_PRIMARIES, m_Grapher.m_showPrimaries ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SECONDARIES, m_Grapher.m_showSecondaries ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, point.x, point.y, this);
}

void CSatLumHistoView::OnLumGraphShowRef() 
{
	m_Grapher.m_showReferences = !m_Grapher.m_showReferences;
	GetConfig()->WriteProfileInt("Saturation Luminance Histo","Show References",m_Grapher.m_showReferences);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumHistoView::OnLumGraphShowDataRef()
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Saturation Luminance Histo","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumHistoView::OnLumGraphPrimaries() 
{
	m_Grapher.m_showPrimaries = !m_Grapher.m_showPrimaries;
	GetConfig()->WriteProfileInt("Saturation Luminance Histo","Show Primaries",m_Grapher.m_showPrimaries);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumHistoView::OnLumGraphSecondaries() 
{
	m_Grapher.m_showSecondaries = !m_Grapher.m_showSecondaries;
	GetConfig()->WriteProfileInt("Saturation Luminance Histo","Show Secondaries",m_Grapher.m_showSecondaries);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumHistoView::OnGraphSave() 
{
	m_Grapher.m_graphCtrl.SaveGraphs();
}

void CSatLumHistoView::OnGraphScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE,10);
	Invalidate(TRUE);
}

void CSatLumHistoView::OnLuminanceGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(0,100);
	Invalidate(TRUE);
}

void CSatLumHistoView::OnGraphSettings() 
{
	m_Grapher.m_graphCtrl.ChangeSettings();
	m_Grapher.m_graphCtrl.WriteSettings("Saturation Luminance Histo");
	Invalidate(TRUE);
}

void CSatLumHistoView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_SATLUM, NULL );
}
