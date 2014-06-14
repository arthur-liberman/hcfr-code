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

extern CDataSetDoc *	g_pDataDocRunningThread;

/////////////////////////////////////////////////////////////////////////////
// CRGBLevelWnd

void RGBTOHSV(double R, double G, double B, double& H, double& S, double& V)
{
	double var_R = ( R );                     //RGB values = From 0 to 1
	double var_G = ( G );
	double var_B = ( B );

	double  var_Min = min(min( var_R, var_G), var_B );    //Min. value of RGB
	double  var_Max = max(max( var_R, var_G), var_B );    //Max. value of RGB
	double  del_Max = var_Max - var_Min ;            //Delta RGB value

	V = var_Max;

	if ( del_Max == 0 )                     //This is a gray, no chroma...
	{
	   H = 0;                                //HSL results = From 0 to 1
	   S = 0;
	}
	else                                    //Chromatic data...
	{
       double alpha = 0.5 * (2 * var_R - var_G - var_B);
       double beta = pow(3., 0.5) / 2 * (var_G - var_B);
       S = pow(pow(alpha,2)+pow(beta,2),2);
       H = (atan2(beta,alpha) * 180 / PI + 180) / 360.;
	}
}

CRGBLevelWnd::CRGBLevelWnd()
{
	m_pRefColor = NULL;
	m_pDocument = NULL;
	m_bLumaMode = FALSE;
}

CRGBLevelWnd::~CRGBLevelWnd()
{
}

void CRGBLevelWnd::Refresh(int minCol)
{
    
	BOOL bWasLumaMode = m_bLumaMode;
	m_bLumaMode = FALSE;
    double cx,cy,cz,cxref,cyref,czref;

	if ( m_pRefColor && (*m_pRefColor).isValid() )
	{
		double RefLuma = 1.0;
        CColor aReference;
        
		//look for white first and disable if detect for primaries/secondaries is not selected
        if ( (m_pRefColor -> GetDeltaxy ( GetColorReference().GetWhite(), GetColorReference() ) < 0.05) )
		{
			m_bLumaMode = FALSE;
            aReference = GetColorReference().GetWhite();
		}
		else if ( m_pRefColor -> GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(0), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetRedReferenceLuma ();
            aReference = m_pDocument->GetMeasure()->GetRefPrimary(0);//GetColorReference().GetRed();
		}
		else if ( m_pRefColor -> GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(1), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetGreenReferenceLuma ();
            aReference = m_pDocument->GetMeasure()->GetRefPrimary(1);//GetColorReference().GetGreen();
		}
		else if ( m_pRefColor -> GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(2), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetBlueReferenceLuma ();
            aReference = m_pDocument->GetMeasure()->GetRefPrimary(2);//GetColorReference().GetBlue();
		}
		else if ( m_pRefColor -> GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(0), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetYellowReferenceLuma ();
            aReference = m_pDocument->GetMeasure()->GetRefSecondary(0);//GetColorReference().GetYellow();
		}
		else if ( m_pRefColor -> GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(1), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetCyanReferenceLuma ();
            aReference = m_pDocument->GetMeasure()->GetRefSecondary(1);//GetColorReference().GetCyan();
		}
		else if ( m_pRefColor -> GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(2), GetColorReference() ) < 0.05 )
		{
			m_bLumaMode = TRUE;
			RefLuma = GetColorReference().GetMagentaReferenceLuma ();
            aReference = m_pDocument->GetMeasure()->GetRefSecondary(2);//GetColorReference().GetMagenta();
		}
                        		
		CColor white = m_pDocument->GetMeasure()->GetOnOffWhite();
        if (!white.isValid() || !m_bLumaMode)
        {
    		int i = m_pDocument -> GetMeasure () -> GetGrayScaleSize ();
	    	if ( m_pDocument -> GetMeasure () -> GetGray ( i - 1 ).isValid() )
                white.SetY( m_pDocument -> GetMeasure () -> GetGray ( i - 1 ) [ 1 ]);
        }
		if (m_bLumaMode && GetConfig()->m_bDetectPrimaries)
		{
            ColorXYZ aColor=m_pRefColor->GetXYZValue(), refColor=aReference.GetXYZValue() ;
			
			m_redValue=100.;
			m_greenValue=100.;
			m_blueValue=100.;
            if ( white.isValid() )
            {

            aColor[0]=aColor[0]/white.GetY();
            aColor[1]=aColor[1]/white.GetY();
            aColor[2]=aColor[2]/white.GetY();
            ColorRGB aColorRGB(aColor, GetColorReference());
            ColorRGB refColorRGB(refColor, GetColorReference());
            cx = aColorRGB[0];
            cy = aColorRGB[1];
            cz = aColorRGB[2];
            cxref = refColorRGB[0];
            cyref = refColorRGB[1];
            czref = refColorRGB[2];
            // RGB or HSV vector differences for CMS, V = luminance
            if (GetConfig() -> m_useHSV)
            {
                RGBTOHSV(aColorRGB[0],aColorRGB[1],aColorRGB[2],cx,cy,cz);
                RGBTOHSV(refColorRGB[0],refColorRGB[1],refColorRGB[2],cxref,cyref,czref);
                czref = refColor[1]; //set V to luminance
                cz = aColor[1];
            }
            if (cxref > .01)
                m_redValue=(float)(100.-(cxref-cx)/cxref*100.0);
            else
                m_redValue=(abs(cxref-cx)<0.3)?(float)(100.-(cxref-cx)*100.0):(float)(100.-(cxref+1.0-cx)*100.0);
            if (cyref > .01)
                m_greenValue=(float)(100.-(cyref-cy)/cyref*100.0);
            else
                m_greenValue=(float)(100.-(cyref-cy)*100.0);
            if (czref > .01)
                m_blueValue=(float)(100.-(czref-cz)/czref*100.0);
            else
                m_blueValue=(float)(100.-(czref-cz)*100.0);
            
            }
		}
		else //Always use RGB vector balance for grays
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
    	int nCount = m_pDocument -> GetMeasure () -> GetGrayScaleSize ();
        double YWhite = white.GetY();
        if (!m_bLumaMode && minCol != -1 && !g_pDataDocRunningThread)
        {
            ColorxyY tmpColor(GetColorReference().GetWhite());
		    // Determine Reference Y luminance for Delta E calculus
			if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
			{
						// Compute reference Luminance regarding actual offset and reference gamma 
                        // fixed to use correct gamma predicts
                        // and added option to assume perfect gamma
						double x = ArrayIndexToGrayLevel ( minCol - 1 , nCount, GetConfig () -> m_bUseRoundDown );
                        double valy, Gamma, Offset;
                        Gamma = GetConfig()->m_GammaRef;
                        GetConfig()->m_GammaAvg = Gamma;
                        m_pDocument->ComputeGammaAndOffset(&Gamma, &Offset, 1, 1, nCount, false);
            			Gamma = floor(Gamma * 100) / 100;
                        if (GetConfig()->m_useMeasuredGamma)
			                GetConfig()->m_GammaAvg = (Gamma<1?2.22:Gamma);
                        GetConfig()->SetPropertiesSheetValues();
            		    CColor White = m_pDocument -> GetMeasure () -> GetGray ( nCount - 1 );
	                	CColor Black = m_pDocument -> GetMeasure () -> GetGray ( 0 );
                        if (GetConfig()->m_GammaOffsetType == 4 && White.isValid() && Black.isValid() )
			            {
                            double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown);
                            valy = GetBT1886(valx, White, Black, GetConfig()->m_GammaRel);
			            }
			            else
			            {
				            double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+Offset)/(1.0+Offset);
				            valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
			            }

						tmpColor[2] = valy;
                        if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
                            tmpColor[2] = m_pRefColor->GetY() / YWhite; //perfect gamma
						aReference.SetxyYValue(tmpColor);
			}
		    else
		    {
						// Use actual gray luminance as correct reference (absolute)
                           YWhite = m_pRefColor->GetY();
			}
        }
        if ((g_pDataDocRunningThread || minCol <=0) && !m_bLumaMode )
            m_dEValue = float(m_pRefColor->GetDeltaE(GetColorReference().GetWhite()));
        else
            m_dEValue = float(m_pRefColor->GetDeltaE(YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, !m_bLumaMode, GetConfig()->gw_Weight )) ;

    }
	else
	{
		m_redValue=0.;
		m_greenValue=0.;
		m_blueValue=0.;
        m_dEValue = 0.;
	}
	Invalidate(FALSE);

	if (m_bLumaMode != bWasLumaMode)
	{
		CString title;
        title.LoadString(m_bLumaMode && GetConfig()->m_bDetectPrimaries ? (GetConfig()->m_useHSV?IDS_LCHLEVELS:IDS_RGBLEVELS) : IDS_RGBLEVELS);
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

	CRect rect;
	GetClientRect(&rect);

	// fill the background with black
	HBRUSH hBrBkgnd = (HBRUSH) GetParent () -> SendMessage ( WM_CTLCOLORSTATIC, (WPARAM) dc.m_hDC, (LPARAM) m_hWnd );
	
	if ( hBrBkgnd )
		FillRect ( pDC -> m_hDC, & rect, hBrBkgnd );
	else
		pDC->FillSolidRect(0,0,rect.Width(),rect.Height(),RGB(0,0,0));

	CRect drawRect=rect;
	drawRect.DeflateRect(widthMargin,heightMargin);

//	int barWidth = (drawRect.Width()-2*interBarMargin)/3.0;
	int barWidth = int ((drawRect.Width()-2*interBarMargin)/4.0); //includes dE
	int maxYValue = int  max(m_redValue,max(m_greenValue,m_blueValue));
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

	double dEyScale=(float)drawRect.Height()/6.;
	int dEBarHeight= (int)(m_dEValue*dEyScale);
	int dEBarX=widthMargin+3*barWidth+3*interBarMargin;
	int dEBarY=rect.Height()-dEBarHeight-heightMargin;

    // draw RGB bars
	int ellipseHeight=barWidth/4;
    if (m_bLumaMode && GetConfig()->m_bDetectPrimaries && GetConfig()->m_useHSV)
	{
    	DrawGradientBar(pDC,RGB(0,125,125),redBarX,redBarY,barWidth,redBarHeight);
	    DrawGradientBar(pDC,RGB(125,0,125),greenBarX,greenBarY,barWidth,greenBarHeight);
	    DrawGradientBar(pDC,RGB(125,125,0),blueBarX,blueBarY,barWidth,blueBarHeight);
	    DrawGradientBar(pDC,RGB(255,215,0),dEBarX,dEBarY,barWidth,dEBarHeight);
    }
    else
    {
    	DrawGradientBar(pDC,RGB(255,0,0),redBarX,redBarY,barWidth,redBarHeight);
	    DrawGradientBar(pDC,RGB(0,255,0),greenBarX,greenBarY,barWidth,greenBarHeight);
	    DrawGradientBar(pDC,RGB(0,0,255),blueBarX,blueBarY,barWidth,blueBarHeight);
	    DrawGradientBar(pDC,RGB(255,215,0),dEBarX,dEBarY,barWidth,dEBarHeight);
    }


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
		// dE ellipse
		pDC->SelectObject(pOldBrush);
		anEllipseBrush.DeleteObject();
		anEllipseBrush.CreateSolidBrush(RGB(184,134,11));
		pOldBrush=pDC->SelectObject(&anEllipseBrush);
		pDC->Ellipse(dEBarX,dEBarY-ellipseHeight/2,dEBarX+barWidth,dEBarY+ellipseHeight/2);

		pDC->SelectObject(pOldBrush);
		pDC->SelectObject(pOldPen);
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
	sprintf(aBuf,"%3.1f%%",m_redValue);
	pDC->TextOut(redBarX+barWidth/2,redBarY-ellipseHeight/2,aBuf);
	sprintf(aBuf,"%3.1f%%",m_blueValue);
	pDC->TextOut(blueBarX+barWidth/2,blueBarY-ellipseHeight/2,aBuf);
	sprintf(aBuf,"%3.1f%%",m_greenValue);
	pDC->TextOut(greenBarX+barWidth/2,greenBarY-ellipseHeight/2,aBuf);
    sprintf(aBuf,"dE %3.1f",m_dEValue);
	pDC->TextOut(dEBarX+barWidth/2,dEBarY-ellipseHeight/2,aBuf);

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

