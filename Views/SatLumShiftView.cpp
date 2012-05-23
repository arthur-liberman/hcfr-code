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

// SatLumShiftView.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "DocTempl.h"
#include "SatLumShiftView.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// CSatLumShiftGrapher

CSatLumShiftGrapher::CSatLumShiftGrapher ()
{
	CString		Msg;

	Msg.LoadString ( IDS_PERCENTSATRED );
	m_redSatGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATGREEN );
	m_greenSatGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATBLUE );
	m_blueSatGraphID = m_graphCtrl.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATYELLOW );
	m_yellowSatGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATCYAN );
	m_cyanSatGraphID = m_graphCtrl.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_PERCENTSATMAGENTA );
	m_magentaSatGraphID = m_graphCtrl.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);

	Msg.LoadString ( IDS_PERCENTSATREDDATAREF );
	m_redSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATGREENDATAREF );
	m_greenSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATBLUEDATAREF );
	m_blueSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATYELLOWDATAREF );
	m_yellowSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATCYANDATAREF );
	m_cyanSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_PERCENTSATMAGENTADATAREF );
	m_magentaSatDataRefGraphID = m_graphCtrl.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);

	Msg.LoadString ( IDS_REFERENCE );
	m_refGraphID = m_graphCtrl.AddGraph(RGB(230,230,230), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	
	m_graphCtrl.SetXAxisProps("% Sat", 10, 0, 100);
	m_graphCtrl.SetYAxisProps("%", 5, -100, 100);
	m_graphCtrl.SetScale(0,100,-20,20);
	m_graphCtrl.ReadSettings("Saturation Shift Sat");

	Msg.LoadString ( IDS_DELTAERED );
	m_redColorGraphID = m_graphCtrl2.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEGREEN );
	m_greenColorGraphID = m_graphCtrl2.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEBLUE );
	m_blueColorGraphID = m_graphCtrl2.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEYELLOW );
	m_yellowColorGraphID = m_graphCtrl2.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAECYAN );
	m_cyanColorGraphID = m_graphCtrl2.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg);
	Msg.LoadString ( IDS_DELTAEMAGENTA );
	m_magentaColorGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg);

	Msg.LoadString ( IDS_DELTAEREDDATAREF );
	m_redColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,0,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEGREENDATAREF );
	m_greenColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(0,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEBLUEDATAREF );
	m_blueColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(0,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEYELLOWDATAREF );
	m_yellowColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,255,0), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAECYANDATAREF );
	m_cyanColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(0,255,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);
	Msg.LoadString ( IDS_DELTAEMAGENTADATAREF );
	m_magentaColorDataRefGraphID = m_graphCtrl2.AddGraph(RGB(255,0,255), (LPSTR)(LPCSTR)Msg,1,PS_DOT);

	m_graphCtrl2.SetXAxisProps("% Sat", 10, 0, 100);
	m_graphCtrl2.SetYAxisProps("", 2, 0, 20);
	m_graphCtrl2.SetScale(0,100,0,20);
	m_graphCtrl2.ReadSettings("Saturation Shift Color");

	m_showReference=GetConfig()->GetProfileInt("Saturation Shift","Show Reference",TRUE);
	m_showRed=GetConfig()->GetProfileInt("Saturation Shift","Show Red",TRUE);
	m_showGreen=GetConfig()->GetProfileInt("Saturation Shift","Show Green",TRUE);
	m_showBlue=GetConfig()->GetProfileInt("Saturation Shift","Show Blue",TRUE);
	m_showYellow=GetConfig()->GetProfileInt("Saturation Shift","Show Yellow",TRUE);
	m_showCyan=GetConfig()->GetProfileInt("Saturation Shift","Show Cyan",TRUE);
	m_showMagenta=GetConfig()->GetProfileInt("Saturation Shift","Show Magenta",TRUE);
	m_showDataRef=GetConfig()->GetProfileInt("Saturation Shift","Show Reference Data",TRUE);
}

void CSatLumShiftGrapher::UpdateGraph ( CDataSetDoc * pDoc )
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
	
	ColorRGB ClrRGB;
	double	xstart, ystart, xend, yend;
	double	satshift, deltaE;

	// Retrieve gray xy coordinates
    ColorxyY whitexyY(GetColorReference().GetWhite());
	xstart = whitexyY[0];
	ystart = whitexyY[1];

	if(IsWindow(m_graphCtrl.m_hWnd))
		m_graphCtrl.ShowWindow(SW_SHOW);
	if(IsWindow(m_graphCtrl2.m_hWnd))
		m_graphCtrl2.ShowWindow(SW_SHOW);

	m_graphCtrl.ClearGraph(m_refGraphID);
	
	if (m_showReference && m_refGraphID != -1 && size > 0)
	{	
		m_graphCtrl.AddPoint(m_refGraphID, 0, 0.0);
		m_graphCtrl.AddPoint(m_refGraphID, 100, 0.0);
	}
	
	m_graphCtrl.ClearGraph(m_redSatGraphID);
	m_graphCtrl2.ClearGraph(m_redColorGraphID);
	if (m_showRed && m_redSatGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetRedSat(0).isValid() )
	{
		// Compute vector between neutral gray and saturated color in CIExy space
		// Define target color in RGB mode
		ClrRGB[0]=255.0;
		ClrRGB[1]=0.0;
		ClrRGB[2]=0.0;

		// Compute xy coordinates of 100% saturated color
		GetEndPoint ( xend, yend, pDoc->GetMeasure()->GetRedPrimary(), ClrRGB );
		
		for (int i=0; i<size; i++)
	    {
			// Compute shift and delta E for current point
			CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDoc->GetMeasure()->GetRedSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

			// Create graph points
			m_graphCtrl.AddPoint(m_redSatGraphID, i*100/(size-1), satshift);
			m_graphCtrl2.AddPoint(m_redColorGraphID, i*100/(size-1), deltaE);
	    }
	}

	m_graphCtrl.ClearGraph(m_greenSatGraphID);
	m_graphCtrl2.ClearGraph(m_greenColorGraphID);
	if (m_showGreen && m_greenSatGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetGreenSat(0).isValid() )
	{
		// Compute vector between neutral gray and saturated color in CIExy space
		// Define target color in RGB mode
		ClrRGB[0]=0.0;
		ClrRGB[1]=255.0;
		ClrRGB[2]=0.0;

		// Compute xy coordinates of 100% saturated color
		GetEndPoint ( xend, yend, pDoc->GetMeasure()->GetGreenPrimary(), ClrRGB );
		
		for (int i=0; i<size; i++)
	    {
			// Compute shift and delta E for current point
			CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDoc->GetMeasure()->GetGreenSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

			// Create graph points
			m_graphCtrl.AddPoint(m_greenSatGraphID, i*100/(size-1), satshift);
			m_graphCtrl2.AddPoint(m_greenColorGraphID, i*100/(size-1), deltaE);
	    }
	}

	m_graphCtrl.ClearGraph(m_blueSatGraphID);
	m_graphCtrl2.ClearGraph(m_blueColorGraphID);
	if (m_showBlue && m_blueSatGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetBlueSat(0).isValid() )
	{
		// Compute vector between neutral gray and saturated color in CIExy space
		// Define target color in RGB mode
		ClrRGB[0]=0.0;
		ClrRGB[1]=0.0;
		ClrRGB[2]=255.0;

		// Compute xy coordinates of 100% saturated color
		GetEndPoint ( xend, yend, pDoc->GetMeasure()->GetBluePrimary(), ClrRGB );
		
		for (int i=0; i<size; i++)
	    {
			// Compute shift and delta E for current point
			CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDoc->GetMeasure()->GetBlueSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

			// Create graph points
			m_graphCtrl.AddPoint(m_blueSatGraphID, i*100/(size-1), satshift);
			m_graphCtrl2.AddPoint(m_blueColorGraphID, i*100/(size-1), deltaE);
	    }
	}

	m_graphCtrl.ClearGraph(m_yellowSatGraphID);
	m_graphCtrl2.ClearGraph(m_yellowColorGraphID);
	if (m_showYellow && m_yellowSatGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetYellowSat(0).isValid() )
	{
		// Compute vector between neutral gray and saturated color in CIExy space
		// Define target color in RGB mode
		ClrRGB[0]=255.0;
		ClrRGB[1]=255.0;
		ClrRGB[2]=0.0;

		// Compute xy coordinates of 100% saturated color
		GetEndPoint ( xend, yend, pDoc->GetMeasure()->GetYellowSecondary(), ClrRGB );
		
		for (int i=0; i<size; i++)
	    {
			// Compute shift and delta E for current point
			CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDoc->GetMeasure()->GetYellowSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

			// Create graph points
			m_graphCtrl.AddPoint(m_yellowSatGraphID, i*100/(size-1), satshift);
			m_graphCtrl2.AddPoint(m_yellowColorGraphID, i*100/(size-1), deltaE);
	    }
	}

	m_graphCtrl.ClearGraph(m_cyanSatGraphID);
	m_graphCtrl2.ClearGraph(m_cyanColorGraphID);
	if (m_showCyan && m_cyanSatGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetCyanSat(0).isValid() )
	{
		// Compute vector between neutral gray and saturated color in CIExy space
		// Define target color in RGB mode
		ClrRGB[0]=0.0;
		ClrRGB[1]=255.0;
		ClrRGB[2]=255.0;

		// Compute xy coordinates of 100% saturated color
		GetEndPoint ( xend, yend, pDoc->GetMeasure()->GetCyanSecondary(), ClrRGB );
		
		for (int i=0; i<size; i++)
	    {
			// Compute shift and delta E for current point
			CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDoc->GetMeasure()->GetCyanSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

			// Create graph points
			m_graphCtrl.AddPoint(m_cyanSatGraphID, i*100/(size-1), satshift);
			m_graphCtrl2.AddPoint(m_cyanColorGraphID, i*100/(size-1), deltaE);
	    }
	}

	m_graphCtrl.ClearGraph(m_magentaSatGraphID);
	m_graphCtrl2.ClearGraph(m_magentaColorGraphID);
	if (m_showMagenta && m_magentaSatGraphID != -1 && size > 0 && pDoc->GetMeasure()->GetMagentaSat(0).isValid() )
	{
		// Compute vector between neutral gray and saturated color in CIExy space
		// Define target color in RGB mode
		ClrRGB[0]=255.0;
		ClrRGB[1]=0.0;
		ClrRGB[2]=255.0;

		// Compute xy coordinates of 100% saturated color
		GetEndPoint ( xend, yend, pDoc->GetMeasure()->GetMagentaSecondary(), ClrRGB );

		for (int i=0; i<size; i++)
	    {
			// Compute shift and delta E for current point
			CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDoc->GetMeasure()->GetMagentaSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

			// Create graph points
			m_graphCtrl.AddPoint(m_magentaSatGraphID, i*100/(size-1), satshift);
			m_graphCtrl2.AddPoint(m_magentaColorGraphID, i*100/(size-1), deltaE);
	    }
	}

	m_graphCtrl.ClearGraph(m_redSatDataRefGraphID);
	m_graphCtrl2.ClearGraph(m_redColorDataRefGraphID);
	m_graphCtrl.ClearGraph(m_greenSatDataRefGraphID);
	m_graphCtrl2.ClearGraph(m_greenColorDataRefGraphID);
	m_graphCtrl.ClearGraph(m_blueSatDataRefGraphID);
	m_graphCtrl2.ClearGraph(m_blueColorDataRefGraphID);
	m_graphCtrl.ClearGraph(m_yellowSatDataRefGraphID);
	m_graphCtrl2.ClearGraph(m_yellowColorDataRefGraphID);
	m_graphCtrl.ClearGraph(m_cyanSatDataRefGraphID);
	m_graphCtrl2.ClearGraph(m_cyanColorDataRefGraphID);
	m_graphCtrl.ClearGraph(m_magentaSatDataRefGraphID);
	m_graphCtrl2.ClearGraph(m_magentaColorDataRefGraphID);

	if ((m_showDataRef)&&(pDataRef !=NULL)&&(pDataRef !=pDoc))
	{
		if (m_showRed && m_redSatDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetRedSat(0).isValid() )
		{
			// Compute vector between neutral gray and saturated color in CIExy space
			// Define target color in RGB mode
			ClrRGB[0]=255.0;
			ClrRGB[1]=0.0;
			ClrRGB[2]=0.0;

			// Compute xy coordinates of 100% saturated color
			GetEndPoint ( xend, yend, pDataRef->GetMeasure()->GetRedPrimary(), ClrRGB );
			
			for (int i=0; i<size; i++)
			{
				// Compute shift and delta E for current point
				CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDataRef->GetMeasure()->GetRedSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

				// Create graph points
				m_graphCtrl.AddPoint(m_redSatDataRefGraphID, i*100/(size-1), satshift);
				m_graphCtrl2.AddPoint(m_redColorDataRefGraphID, i*100/(size-1), deltaE);
			}
		}

		if (m_showGreen && m_greenSatDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetGreenSat(0).isValid() )
		{
			// Compute vector between neutral gray and saturated color in CIExy space
			// Define target color in RGB mode
			ClrRGB[0]=0.0;
			ClrRGB[1]=255.0;
			ClrRGB[2]=0.0;

			// Compute xy coordinates of 100% saturated color
			GetEndPoint ( xend, yend, pDataRef->GetMeasure()->GetGreenPrimary(), ClrRGB );
			
			for (int i=0; i<size; i++)
			{
				// Compute shift and delta E for current point
				CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDataRef->GetMeasure()->GetGreenSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

				// Create graph points
				m_graphCtrl.AddPoint(m_greenSatDataRefGraphID, i*100/(size-1), satshift);
				m_graphCtrl2.AddPoint(m_greenColorDataRefGraphID, i*100/(size-1), deltaE);
			}
		}

		if (m_showBlue && m_blueSatDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetBlueSat(0).isValid() )
		{
			// Compute vector between neutral gray and saturated color in CIExy space
			// Define target color in RGB mode
			ClrRGB[0]=0.0;
			ClrRGB[1]=0.0;
			ClrRGB[2]=255.0;

			// Compute xy coordinates of 100% saturated color
			GetEndPoint ( xend, yend, pDataRef->GetMeasure()->GetBluePrimary(), ClrRGB );
			
			for (int i=0; i<size; i++)
			{
				// Compute shift and delta E for current point
				CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDataRef->GetMeasure()->GetBlueSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

				// Create graph points
				m_graphCtrl.AddPoint(m_blueSatDataRefGraphID, i*100/(size-1), satshift);
				m_graphCtrl2.AddPoint(m_blueColorDataRefGraphID, i*100/(size-1), deltaE);
			}
		}

		if (m_showYellow && m_yellowSatDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetYellowSat(0).isValid() )
		{
			// Compute vector between neutral gray and saturated color in CIExy space
			// Define target color in RGB mode
			ClrRGB[0]=255.0;
			ClrRGB[1]=255.0;
			ClrRGB[2]=0.0;

			// Compute xy coordinates of 100% saturated color
			GetEndPoint ( xend, yend, pDataRef->GetMeasure()->GetYellowSecondary(), ClrRGB );
			
			for (int i=0; i<size; i++)
			{
				// Compute shift and delta E for current point
				CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDataRef->GetMeasure()->GetYellowSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

				// Create graph points
				m_graphCtrl.AddPoint(m_yellowSatDataRefGraphID, i*100/(size-1), satshift);
				m_graphCtrl2.AddPoint(m_yellowColorDataRefGraphID, i*100/(size-1), deltaE);
			}
		}

		if (m_showCyan && m_cyanSatDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetCyanSat(0).isValid() )
		{
			// Compute vector between neutral gray and saturated color in CIExy space
			// Define target color in RGB mode
			ClrRGB[0]=0.0;
			ClrRGB[1]=255.0;
			ClrRGB[2]=255.0;

			// Compute xy coordinates of 100% saturated color
			GetEndPoint ( xend, yend, pDataRef->GetMeasure()->GetCyanSecondary(), ClrRGB );
			
			for (int i=0; i<size; i++)
			{
				// Compute shift and delta E for current point
				CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDataRef->GetMeasure()->GetCyanSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

				// Create graph points
				m_graphCtrl.AddPoint(m_cyanSatDataRefGraphID, i*100/(size-1), satshift);
				m_graphCtrl2.AddPoint(m_cyanColorDataRefGraphID, i*100/(size-1), deltaE);
			}
		}

		if (m_showMagenta && m_magentaSatDataRefGraphID != -1 && size > 0 && pDataRef->GetMeasure()->GetMagentaSat(0).isValid() )
		{
			// Compute vector between neutral gray and saturated color in CIExy space
			// Define target color in RGB mode
			ClrRGB[0]=255.0;
			ClrRGB[1]=0.0;
			ClrRGB[2]=255.0;

			// Compute xy coordinates of 100% saturated color
			GetEndPoint ( xend, yend, pDataRef->GetMeasure()->GetMagentaSecondary(), ClrRGB );

			for (int i=0; i<size; i++)
			{
				// Compute shift and delta E for current point
				CSatLumShiftGrapher::GetSatShift ( satshift, deltaE, pDataRef->GetMeasure()->GetMagentaSat(i).GetxyYValue(), i, size, xstart, ystart, xend, yend );

				// Create graph points
				m_graphCtrl.AddPoint(m_magentaSatDataRefGraphID, i*100/(size-1), satshift);
				m_graphCtrl2.AddPoint(m_magentaColorDataRefGraphID, i*100/(size-1), deltaE);
			}
		}
	}
}

void CSatLumShiftGrapher::GetEndPoint ( double & xend, double & yend, CColor & SaturatedColor, const ColorRGB & ClrRGB )
{
	if ( GetConfig () -> m_bSatUseMeasuredRef && SaturatedColor.isValid() )
	{
		// Use measured saturated color as target
		xend = SaturatedColor.GetxyYValue()[0];
		yend = SaturatedColor.GetxyYValue()[0];
	}
	else
	{		
		ColorXYZ Clr2(ClrRGB, GetColorReference());
		ColorxyY Clr3(Clr2);
		
		xend=Clr3[0];
		yend=Clr3[1];
	}
}

void CSatLumShiftGrapher::GetSatShift ( double & satshift, double & deltaE, const ColorxyY& SatColor, int num, int count, double xstart, double ystart, double xend, double yend )
{
	double	k;
	double	xtarget, ytarget;
	double	xmeasure, ymeasure;
	double	xpoint, ypoint;
	double	dx, dy;
	double	umeasure, vmeasure;
	double	upoint, vpoint;

	// Retrieve measured point coordinates
	xmeasure = SatColor[0];
	ymeasure = SatColor[1];
	
	// Compute color target coordinates
	xtarget = xstart + ( (xend - xstart) * (double) num / (double) (count - 1) );
	ytarget = ystart + ( (yend - ystart) * (double) num / (double)(count - 1) );

	// Compute projection on line (xstart,ystart) - (xend,yend) from measured point
	dx = xend - xstart; 
	dy = yend - ystart; 

	k = ( ( xmeasure - xstart ) + ( ystart - ymeasure ) * ( dx / dy ) ) / ( ( dx * dx / dy ) + dy );

	xpoint = xmeasure - k * dy;
	ypoint = ymeasure + k * dx;

	// Compute saturation shift
	satshift = ( xpoint - xtarget ) / ( xend - xstart ) * 100.0;

	// Compute color delta E
	umeasure = 4.0*xmeasure / (-2.0*xmeasure + 12.0*ymeasure + 3.0); 
	vmeasure = 9.0*ymeasure / (-2.0*xmeasure + 12.0*ymeasure + 3.0); 

	upoint = 4.0*xpoint / (-2.0*xpoint + 12.0*ypoint + 3.0); 
	vpoint = 9.0*ypoint / (-2.0*xpoint + 12.0*ypoint + 3.0); 
	
	deltaE = 1300.0 * sqrt ( pow((upoint - umeasure),2) + pow((vpoint - vmeasure),2) );
}

/////////////////////////////////////////////////////////////////////////////
// CSatLumShiftView

IMPLEMENT_DYNCREATE(CSatLumShiftView, CSavingView)

CSatLumShiftView::CSatLumShiftView()
	: CSavingView()
{
	//{{AFX_DATA_INIT(CSatLumShiftView)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}
 
CSatLumShiftView::~CSatLumShiftView()
{
}
 
BEGIN_MESSAGE_MAP(CSatLumShiftView, CSavingView)
	//{{AFX_MSG_MAP(CSatLumShiftView)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_LUM_GRAPH_SHOWREF, OnSatGraphShowRef)
	ON_COMMAND(IDM_LUM_GRAPH_REDSHIFT, OnSatGraphRed)
	ON_COMMAND(IDM_LUM_GRAPH_GREENSHIFT, OnSatGraphGreen)
	ON_COMMAND(IDM_LUM_GRAPH_BLUESHIFT, OnSatGraphBlue)
	ON_COMMAND(IDM_LUM_GRAPH_YELLOWSHIFT, OnSatGraphYellow)
	ON_COMMAND(IDM_LUM_GRAPH_CYANSHIFT, OnSatGraphCyan)
	ON_COMMAND(IDM_LUM_GRAPH_MAGENTASHIFT, OnSatGraphMagenta)
	ON_COMMAND(IDM_LUM_GRAPH_DATAREF, OnSatGraphShowDataRef)
	ON_COMMAND(IDM_GRAPH_SETTINGS, OnGraphSettings)
	ON_COMMAND(IDM_GRAPH_SAVE, OnGraphSave)
	ON_COMMAND(IDM_GRAPH_SCALE_FIT, OnGraphScaleFit)
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE1, OnLuminanceGraphYScale1)
	ON_COMMAND(IDM_LUMINANCE_GRAPH_Y_SCALE2, OnLuminanceGraphYScale2)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_BOTTOM, OnGraphYShiftBottom)
	ON_COMMAND(IDM_GRAPH_Y_SHIFT_TOP, OnGraphYShiftTop)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_IN, OnGraphYZoomIn)
	ON_COMMAND(IDM_GRAPH_Y_ZOOM_OUT, OnGraphYZoomOut)
	ON_COMMAND(IDM_GRAPH_Y_SCALE_FIT, OnGraphYScaleFit)
	ON_COMMAND(IDM_GRAPH_SCALE_CUSTOM, OnGraphScaleCustom)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE1, OnDeltaEGraphYScale1)
	ON_COMMAND(IDM_DELTAE_GRAPH_Y_SCALE2, OnDeltaEGraphYScale2)
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
// CSatLumShiftView diagnostics

#ifdef _DEBUG
void CSatLumShiftView::AssertValid() const
{
	CSavingView::AssertValid();
}

void CSatLumShiftView::Dump(CDumpContext& dc) const
{
	CSavingView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CSatLumShiftView message handlers

void CSatLumShiftView::OnInitialUpdate() 
{
	CSavingView::OnInitialUpdate();

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_Grapher.m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTO_GRAPH);
	m_Grapher.m_graphCtrl2.Create(_T("Graph Window"), rect, this, IDC_LUMINANCEHISTOLOG_GRAPH);

	OnSize(SIZE_RESTORED,rect.Width(),rect.Height());	// to size graph according to m_showDeltaE
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
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

DWORD CSatLumShiftView::GetUserInfo ()
{
	return	( ( m_Grapher.m_showReference	& 0x0001 )	<< 0 )
		  + ( ( m_Grapher.m_showRed			& 0x0001 )	<< 1 )
		  + ( ( m_Grapher.m_showGreen		& 0x0001 )	<< 2 )
		  + ( ( m_Grapher.m_showBlue		& 0x0001 )	<< 3 )
		  + ( ( m_Grapher.m_showYellow		& 0x0001 )	<< 4 )
		  + ( ( m_Grapher.m_showCyan		& 0x0001 )	<< 5 )
		  + ( ( m_Grapher.m_showMagenta		& 0x0001 )	<< 6 )
		  + ( ( m_Grapher.m_showDataRef		& 0x0001 )	<< 7 );
}

void CSatLumShiftView::SetUserInfo ( DWORD dwUserInfo )
{
	m_Grapher.m_showReference	= ( dwUserInfo >> 0 ) & 0x0001;
	m_Grapher.m_showRed			= ( dwUserInfo >> 1 ) & 0x0001;
	m_Grapher.m_showGreen		= ( dwUserInfo >> 2 ) & 0x0001;
	m_Grapher.m_showBlue		= ( dwUserInfo >> 3 ) & 0x0001;
	m_Grapher.m_showYellow		= ( dwUserInfo >> 4 ) & 0x0001;
	m_Grapher.m_showCyan		= ( dwUserInfo >> 5 ) & 0x0001;
	m_Grapher.m_showMagenta		= ( dwUserInfo >> 6 ) & 0x0001;
	m_Grapher.m_showDataRef		= ( dwUserInfo >> 7 ) & 0x0001;
}

void CSatLumShiftView::OnSize(UINT nType, int cx, int cy) 
{
	CSavingView::OnSize(nType, cx, cy);
	
	if(IsWindow(m_Grapher.m_graphCtrl.m_hWnd))
		m_Grapher.m_graphCtrl.MoveWindow(0,0,cx,cy/2);
	if(IsWindow(m_Grapher.m_graphCtrl2.m_hWnd))
		m_Grapher.m_graphCtrl2.MoveWindow(0,cy/2,cx,cy/2);
}

void CSatLumShiftView::OnDraw(CDC* pDC) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	
}

BOOL CSatLumShiftView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


void CSatLumShiftView::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_SATLUMSHIFT_GRAPH_MENU);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_SHOWREF, m_Grapher.m_showReference ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
 	pPopup->CheckMenuItem(IDM_LUM_GRAPH_DATAREF, m_Grapher.m_showDataRef ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND); //Ki
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_REDSHIFT, m_Grapher.m_showRed ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_GREENSHIFT, m_Grapher.m_showGreen ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_BLUESHIFT, m_Grapher.m_showBlue ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_YELLOWSHIFT, m_Grapher.m_showYellow ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_CYANSHIFT, m_Grapher.m_showCyan ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);
    pPopup->CheckMenuItem(IDM_LUM_GRAPH_MAGENTASHIFT, m_Grapher.m_showMagenta ? MF_CHECKED : MF_UNCHECKED | MF_BYCOMMAND);

	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL, point.x, point.y, this);
}

void CSatLumShiftView::OnSatGraphShowRef() 
{
	m_Grapher.m_showReference = !m_Grapher.m_showReference;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Reference",m_Grapher.m_showReference);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphRed() 
{
	m_Grapher.m_showRed = !m_Grapher.m_showRed;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Red",m_Grapher.m_showRed);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphGreen() 
{
	m_Grapher.m_showGreen = !m_Grapher.m_showGreen;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Green",m_Grapher.m_showGreen);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphBlue() 
{
	m_Grapher.m_showBlue = !m_Grapher.m_showBlue;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Blue",m_Grapher.m_showBlue);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphYellow() 
{
	m_Grapher.m_showYellow = !m_Grapher.m_showYellow;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Yellow",m_Grapher.m_showYellow);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphCyan() 
{
	m_Grapher.m_showCyan = !m_Grapher.m_showCyan;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Cyan",m_Grapher.m_showCyan);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphMagenta() 
{
	m_Grapher.m_showMagenta = !m_Grapher.m_showMagenta;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Magenta",m_Grapher.m_showMagenta);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnSatGraphShowDataRef()
{
	m_Grapher.m_showDataRef = !m_Grapher.m_showDataRef;
	GetConfig()->WriteProfileInt("Saturation Shift","Show Reference Data",m_Grapher.m_showDataRef);
	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnGraphSave() 
{
	m_Grapher.m_graphCtrl.SaveGraphs(&m_Grapher.m_graphCtrl2);
}

void CSatLumShiftView::OnGraphSettings() 
{
	// Add delta E graph to first graph control to allow setting change 
	int tmpGraphID1=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_redColorGraphID]);
	int tmpGraphID2=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_greenColorGraphID]);
	int tmpGraphID3=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_blueColorGraphID]);
	int tmpGraphID4=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_yellowColorGraphID]);
	int tmpGraphID5=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_cyanColorGraphID]);
	int tmpGraphID6=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_magentaColorGraphID]);
	int tmpGraphID7=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_redColorDataRefGraphID]);
	int tmpGraphID8=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_greenColorDataRefGraphID]);
	int tmpGraphID9=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_blueColorDataRefGraphID]);
	int tmpGraphID10=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_yellowColorDataRefGraphID]);
	int tmpGraphID11=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_cyanColorDataRefGraphID]);
	int tmpGraphID12=m_Grapher.m_graphCtrl.AddGraph(m_Grapher.m_graphCtrl2.m_graphArray[m_Grapher.m_magentaColorDataRefGraphID]);

	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID1 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID2 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID3 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID4 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID5 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID6 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID7 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID8 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID9 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID10 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID11 );
	m_Grapher.m_graphCtrl.ClearGraph ( tmpGraphID12 );

	m_Grapher.m_graphCtrl.ChangeSettings();

	// Update m_Grapher.m_graphCtrl2 setting according to graph and values of tmpGraphIDs
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID1,m_Grapher.m_redColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID2,m_Grapher.m_greenColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID3,m_Grapher.m_blueColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID4,m_Grapher.m_yellowColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID5,m_Grapher.m_cyanColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID6,m_Grapher.m_magentaColorGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID7,m_Grapher.m_redColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID8,m_Grapher.m_greenColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID9,m_Grapher.m_blueColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID10,m_Grapher.m_yellowColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID11,m_Grapher.m_cyanColorDataRefGraphID);
	m_Grapher.m_graphCtrl2.CopySettings(m_Grapher.m_graphCtrl,tmpGraphID12,m_Grapher.m_magentaColorDataRefGraphID);

	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID12);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID11);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID10);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID9);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID8);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID7);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID6);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID5);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID4);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID3);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID2);
	m_Grapher.m_graphCtrl.RemoveGraph(tmpGraphID1);

	m_Grapher.m_graphCtrl.WriteSettings("Saturation Shift Sat");
	m_Grapher.m_graphCtrl2.WriteSettings("Saturation Shift Color");

	OnUpdate(NULL,NULL,NULL);
}

void CSatLumShiftView::OnGraphScaleFit() 
{
	m_Grapher.m_graphCtrl.FitXScale();
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 5, -100, 100);
	m_Grapher.m_graphCtrl.FitYScale(TRUE,10);
	OnDeltaEGraphYScaleFit();
}

void CSatLumShiftView::OnLuminanceGraphYScale1() 
{
	m_Grapher.m_graphCtrl.SetYScale(-100,100);
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 20, -100, 100);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnLuminanceGraphYScale2() 
{
	m_Grapher.m_graphCtrl.SetYScale(-20,20);
	m_Grapher.m_graphCtrl.SetYAxisProps("%", 5, -100, 100);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl.FitYScale(TRUE);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(10);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl.ShiftYScale(-10);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0,-10);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl.GrowYScale(0,10);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl.ChangeScale();
}

void CSatLumShiftView::OnDeltaEGraphYScale1() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,20);
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 2, 0, 40);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnDeltaEGraphYScale2() 
{
	m_Grapher.m_graphCtrl2.SetYScale(0,5);
	m_Grapher.m_graphCtrl2.SetYAxisProps("", 1, 0, 40);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnDeltaEGraphYScaleFit() 
{
	m_Grapher.m_graphCtrl2.FitYScale(TRUE,1);
	m_Grapher.m_graphCtrl2.m_yAxisStep=(m_Grapher.m_graphCtrl2.m_maxY-m_Grapher.m_graphCtrl2.m_minY)/10;
	Invalidate(TRUE);
}

void CSatLumShiftView::OnDeltaEGraphYShiftBottom() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(1);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnDeltaEGraphYShiftTop() 
{
	m_Grapher.m_graphCtrl2.ShiftYScale(-1);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnDeltaEGraphYZoomIn() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,-4);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnDeltaEGraphYZoomOut() 
{
	m_Grapher.m_graphCtrl2.GrowYScale(0,+4);
	Invalidate(TRUE);
}

void CSatLumShiftView::OnDeltaEGraphScaleCustom() 
{
	m_Grapher.m_graphCtrl2.ChangeScale();
}

void CSatLumShiftView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_SATSHIFT, NULL );
}

