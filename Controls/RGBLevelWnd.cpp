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

// RGBLevelWnd.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DataSetDoc.h"
#include "MainView.h"
#include "RGBLevelWnd.h"
#include "Color.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRGBLevelWnd

CRGBLevelWnd::CRGBLevelWnd()
{
	m_pRefColor = NULL;
	m_pDocument = NULL;
	m_bLumaMode = FALSE;
}

CRGBLevelWnd::~CRGBLevelWnd()
{
}

void CRGBLevelWnd::Refresh()
{
	BOOL bWasLumaMode = m_bLumaMode;
	m_bLumaMode = FALSE;
	if ( m_pRefColor && (*m_pRefColor).isValid() )
	{
		double RefLuma = 1.0;
        CColor aReference;
        
		//look for white first and disable if detect for primaries/secondaries is not selected
        if ( (m_pRefColor -> GetDeltaxy ( GetColorReference().GetWhite(), GetColorReference() ) < 0.05) )
		{
			m_bLumaMode = FALSE;
		}
		else if ( m_pRefColor -> GetDeltaxy ( GetColorReference().GetRed(), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetRedReferenceLuma ();
            aReference = GetColorReference().GetRed();
		}
		else if ( m_pRefColor -> GetDeltaxy ( GetColorReference().GetGreen(), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetGreenReferenceLuma ();
            aReference = GetColorReference().GetGreen();
		}
		else if ( m_pRefColor -> GetDeltaxy ( GetColorReference().GetBlue(), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetBlueReferenceLuma ();
            aReference = GetColorReference().GetBlue();
		}
		else if ( m_pRefColor -> GetDeltaxy ( GetColorReference().GetYellow(), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetYellowReferenceLuma ();
            aReference = GetColorReference().GetYellow();
		}
		else if ( m_pRefColor -> GetDeltaxy ( GetColorReference().GetCyan(), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetCyanReferenceLuma ();
            aReference = GetColorReference().GetCyan();
		}
		else if ( m_pRefColor -> GetDeltaxy ( GetColorReference().GetMagenta(), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetMagentaReferenceLuma ();
            aReference = GetColorReference().GetMagenta();
		}
                        		
		if (m_bLumaMode && GetConfig()->m_bDetectPrimaries)
		{
			CColor white = m_pDocument->GetMeasure()->GetOnOffWhite();
            ColorXYZ selectColor=m_pRefColor->GetXYZValue(), refColor=aReference.GetXYZValue() ;
			
			m_redValue=0.;
			m_greenValue=0.;
			m_blueValue=0.;
            if ( white.isValid() )
            {
                ColorLCH selectColorLCH(selectColor, white.GetY(), GetColorReference());
                ColorLCH refColorLCH(refColor, 1.0, GetColorReference());

                m_redValue=(float)(100+(selectColorLCH[0] - refColorLCH[0])/refColorLCH[0]*100);
                m_greenValue=(float)(100+(selectColorLCH[1] - refColorLCH[1])/refColorLCH[1]*100);
                m_blueValue=(float)(100+(selectColorLCH[2] - refColorLCH[2])/refColorLCH[2]*100);
            }
/*			if ( white.isValid() && white.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter) > 0.0001 )
			{
				double d = m_pRefColor -> GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter) / white.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter);

				m_greenValue = (int) ( ( d / RefLuma ) * 100.0 );
			} */
		}
		else
		{
            ColorxyY aColor = m_pRefColor -> GetxyYValue();
            ColorXYZ normColor;

            if(aColor[1] > 0.0)
            {
                normColor[0]=(aColor[0]/aColor[1]);
                normColor[1]=1.0;
                normColor[2]=((1.0-(aColor[0]+aColor[1]))/aColor[1]);
            }
            else
            {
                normColor[0]=0.0;
                normColor[1]=0.0;
                normColor[2]=0.0;
            }


            ColorRGB normColorRGB(normColor, GetColorReference());
                        
            m_redValue=(float)(normColorRGB[0]*100.0);
            m_greenValue=(float)(normColorRGB[1]*100.0);
            m_blueValue=(float)(normColorRGB[2]*100.0);

        }
    }
	else
	{
		m_redValue=0.;
		m_greenValue=0.;
		m_blueValue=0.;
	}
	Invalidate(FALSE);

	if (m_bLumaMode != bWasLumaMode)
	{
		CString title;
        title.LoadString(m_bLumaMode && GetConfig()->m_bDetectPrimaries ? IDS_LCHLEVELS : IDS_RGBLEVELS);
		((CMainView *)GetParent())->m_RGBLevelsLabel.SetWindowText ((LPCSTR)title);
	}
}


BEGIN_MESSAGE_MAP(CRGBLevelWnd, CWnd)
	//{{AFX_MSG_MAP(CRGBLevelWnd)
	ON_WM_PAINT()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CRGBLevelWnd message handlers

void CRGBLevelWnd::OnPaint() 
{
	int widthMargin=2;
	int heightMargin=5;
	int interBarMargin=4;

	CPaintDC dc(this); // device context for painting
	
    CHMemDC pDC(&dc);
//	CDC *pDC=&dc;

	CRect rect;
	GetClientRect(&rect);

	// fill the background with black
	HBRUSH hBrBkgnd = (HBRUSH) GetParent () -> SendMessage ( WM_CTLCOLORSTATIC, (WPARAM) dc.m_hDC, (LPARAM) m_hWnd );
	
	if ( hBrBkgnd )
		FillRect ( pDC -> m_hDC, & rect, hBrBkgnd );
	else
		pDC->FillSolidRect(0,0,rect.Width(),rect.Height(),RGB(0,0,0));

/*
	if( (*m_pRefColor) == noDataColor)
		return;		// Draw no bar
*/
	CRect drawRect=rect;
	drawRect.DeflateRect(widthMargin,heightMargin);

	int barWidth = (drawRect.Width()-2*interBarMargin)/3.0;
	int maxYValue=max(m_redValue,max(m_greenValue,m_blueValue));
	CSize labelSize=pDC->GetTextExtent("100%");

	float yScale;
	if(maxYValue < 200 )
		yScale=(float)drawRect.Height()/200.0f;		// if value is bellow 200% => scale is 0-200% 
	else		
		yScale=(float)drawRect.Height()/(float)maxYValue;	// else scale is 0-max value
	
	// Adjust scale if label cannot be drawn correctly
	if(maxYValue*yScale + labelSize.cy > drawRect.Height() )
		yScale=(float)(drawRect.Height() - labelSize.cy)/(float)maxYValue;  

	if(maxYValue < 200) 	// draw 100% line only if scale is 0-200%
	{
		CPen aPen(PS_DOT,1,RGB(128,128,128));
		pDC->SelectObject(&aPen);
		pDC->MoveTo(0,rect.Height()-(int)(100.0*yScale)-heightMargin);
		pDC->LineTo(rect.Width(),rect.Height()-(int)(100.0*yScale)-heightMargin);
	}

	int redBarHeight=(int)(m_redValue*yScale);
	int redBarX=widthMargin;
	int redBarY=rect.Height()-redBarHeight-heightMargin;
	
	int greenBarHeight= (int)(m_greenValue*yScale);
	int greenBarX=widthMargin+barWidth+interBarMargin;
	int greenBarY=rect.Height()-greenBarHeight-heightMargin;

	int blueBarHeight= (int)(m_blueValue*yScale);
	int blueBarX=widthMargin+2*barWidth+2*interBarMargin;
	int blueBarY=rect.Height()-blueBarHeight-heightMargin;

	// draw RGB bars
	int ellipseHeight=barWidth/4;
	if (m_bLumaMode && GetConfig()->m_bDetectPrimaries)
	{
    	DrawGradientBar(pDC,RGB(0,125,125),redBarX,redBarY,barWidth,redBarHeight);
	    DrawGradientBar(pDC,RGB(125,0,125),greenBarX,greenBarY,barWidth,greenBarHeight);
	    DrawGradientBar(pDC,RGB(125,125,0),blueBarX,blueBarY,barWidth,blueBarHeight);
    }
    else
    {
    	DrawGradientBar(pDC,RGB(255,0,0),redBarX,redBarY,barWidth,redBarHeight);
	    DrawGradientBar(pDC,RGB(0,255,0),greenBarX,greenBarY,barWidth,greenBarHeight);
	    DrawGradientBar(pDC,RGB(0,0,255),blueBarX,blueBarY,barWidth,blueBarHeight);
    }

/*    if (m_bLumaMode)
	{
		DrawGradientBar(pDC,RGB(255,255,0),greenBarX,greenBarY,barWidth,greenBarHeight);

		// Display elipse on top of bar
		if(ellipseHeight > 3)
		{
			CPen *pOldPen;
			CPen anEllipsePen(PS_SOLID,1,RGB(0,0,0));
			pOldPen=pDC->SelectObject(&anEllipsePen);
			// Red ellipse
			CBrush *pOldBrush;
			CBrush anEllipseBrush;
			anEllipseBrush.CreateSolidBrush(RGB(164,164,0));
			pOldBrush=pDC->SelectObject(&anEllipseBrush);
			pDC->Ellipse(greenBarX,greenBarY-ellipseHeight/2,greenBarX+barWidth,greenBarY+ellipseHeight/2);

			pDC->SelectObject(pOldBrush);
			pDC->SelectObject(pOldPen);
		}
	}
	else
	{
    DrawGradientBar(pDC,RGB(255,0,0),redBarX,redBarY,barWidth,redBarHeight);
	DrawGradientBar(pDC,RGB(0,255,0),greenBarX,greenBarY,barWidth,greenBarHeight);
	DrawGradientBar(pDC,RGB(0,0,255),blueBarX,blueBarY,barWidth,blueBarHeight);
    */

	// Display elipses on top of bars
	if(ellipseHeight > 3)
	{
		CPen *pOldPen;
		CPen anEllipsePen(PS_SOLID,1,RGB(0,0,0));
		pOldPen=pDC->SelectObject(&anEllipsePen);
		// Red ellipse
		CBrush *pOldBrush;
		CBrush anEllipseBrush;
		anEllipseBrush.CreateSolidBrush(RGB(164,0,0));
		pOldBrush=pDC->SelectObject(&anEllipseBrush);
		pDC->Ellipse(redBarX,redBarY-ellipseHeight/2,redBarX+barWidth,redBarY+ellipseHeight/2);
		// Green ellipse
		pDC->SelectObject(pOldBrush);
		anEllipseBrush.DeleteObject();
		anEllipseBrush.CreateSolidBrush(RGB(0,164,0));
		pOldBrush=pDC->SelectObject(&anEllipseBrush);
		pDC->Ellipse(greenBarX,greenBarY-ellipseHeight/2,greenBarX+barWidth,greenBarY+ellipseHeight/2);
		// Blue ellipse
		pDC->SelectObject(pOldBrush);
		anEllipseBrush.DeleteObject();
		anEllipseBrush.CreateSolidBrush(RGB(0,0,164));
		pOldBrush=pDC->SelectObject(&anEllipseBrush);
		pDC->Ellipse(blueBarX,blueBarY-ellipseHeight/2,blueBarX+barWidth,blueBarY+ellipseHeight/2);

		pDC->SelectObject(pOldBrush);
		pDC->SelectObject(pOldPen);
//	}
	}

	// Display values on top of bars
	pDC->SetTextAlign(TA_CENTER | TA_BOTTOM);
	if ( ! hBrBkgnd )
		pDC->SetTextColor(RGB(255,255,255));
	pDC->SetBkMode(TRANSPARENT);

	// Initializes a CFont object with the specified characteristics. 
	CFont font;
	VERIFY(font.CreateFont(
	   15,						  // nHeight
	   5,                         // nWidth
	   0,                         // nEscapement
	   0,                         // nOrientation
	   FW_BOLD,					  // nWeight
	   FALSE,                     // bItalic
	   FALSE,                     // bUnderline
	   0,                         // cStrikeOut
	   ANSI_CHARSET,              // nCharSet
	   OUT_DEFAULT_PRECIS,        // nOutPrecision
	   CLIP_DEFAULT_PRECIS,       // nClipPrecision
	   DEFAULT_QUALITY,           // nQuality
	   FIXED_PITCH,				  // nPitchAndFamily
	   "Arial"));                 // lpszFacename

	// Do something with the font just created...
	CFont* pOldFont = pDC->SelectObject(&font);

	char aBuf[32];
//	if (! m_bLumaMode)
//	{
	sprintf(aBuf,"%3.1f%%",m_redValue);
	pDC->TextOut(redBarX+barWidth/2,redBarY-ellipseHeight/2,aBuf);
		sprintf(aBuf,"%3.1f%%",m_blueValue);
		pDC->TextOut(blueBarX+barWidth/2,blueBarY-ellipseHeight/2,aBuf);
//	}
	sprintf(aBuf,"%3.1f%%",m_greenValue);
	pDC->TextOut(greenBarX+barWidth/2,greenBarY-ellipseHeight/2,aBuf);

	pDC->SelectObject(pOldFont);

	rect.top = rect.bottom - heightMargin;
	pDC -> FillSolidRect ( & rect, RGB(0,0,0) );
}

void CRGBLevelWnd::DrawGradientBar(CDC *pDC,COLORREF aColor, int aX, int aY, int aWidth, int aHeight) 
{
	aWidth=aWidth/2;  // Split the drawing in 2 parts white to color and color to black

	// fill white to color gradient
	int r1=255,g1=255,b1=255; // start with white
	int r2=GetRValue(aColor),g2=GetGValue(aColor),b2=GetBValue(aColor); //Any stop color
	for(int i=0;i<aWidth;i++)
	{ 
		int r,g,b;
		r = r1 + (i * (r2-r1) / aWidth);
		g = g1 + (i * (g2-g1) / aWidth);
		b = b1 + (i * (b2-b1) / aWidth);
		pDC->FillSolidRect(aX+i,aY,1,aHeight,RGB(r,g,b));
	}

	// fill color to black gradient
	r1=GetRValue(aColor),g1=GetGValue(aColor),b1=GetBValue(aColor); //Any stop color
	r2=0,g2=0,b2=0; // end with black
	for(int i=0;i<aWidth;i++)
	{ 
		int r,g,b;
		r = r1 + (i * (r2-r1) / aWidth);
		g = g1 + (i * (g2-g1) / aWidth);
		b = b1 + (i * (b2-b1) / aWidth);
		pDC->FillSolidRect(aX+i+aWidth,aY,1,aHeight,RGB(r,g,b));
	}
}

void CRGBLevelWnd::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CRGBLevelWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CTRL_RGBLEVELS, NULL );
}

