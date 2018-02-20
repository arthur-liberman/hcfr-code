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
	m_redValue = 0.;
	m_greenValue = 0.;
	m_blueValue = 0.;
	m_dEValue = 0.;
}

CRGBLevelWnd::~CRGBLevelWnd()
{
}

void CRGBLevelWnd::Refresh(int minCol, int m_displayMode, int nSize)
{
    
	BOOL bWasLumaMode = m_bLumaMode;
    double cx,cy,cz,cxref,cyref,czref;
	CColorReference cRef= (GetColorReference());
	int satsize=m_pDocument->GetMeasure()->GetSaturationSize();
	CString	Msg, dstr;
	Msg.LoadString ( IDS_GDIGENERATOR_NAME );
	CString m_generatorChoice = GetConfig()->GetProfileString("Defaults","Generator",(LPCSTR)Msg);
	dstr.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	BOOL DVD = (m_generatorChoice == dstr);	

	if ( m_pRefColor)
	{
		if (minCol > 0)
		{

			if (m_displayMode == 11)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefCC24Sat(minCol-1);
			}
			else if (m_displayMode == 5)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(0, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 6)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(1, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 7)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(2, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 8)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(3, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 9)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(4, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if (m_displayMode == 10)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSat(5, double(minCol-1) / double(nSize -1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
			}
			else if ( m_displayMode == 1 && (minCol == 1 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(0), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(0);
			}
			else if ( m_displayMode == 1 && (minCol == 2 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(1), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(1);
			}
			else if ( m_displayMode == 1 && (minCol == 3 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(2), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(2);
			}
			else if ( m_displayMode == 1 && (minCol == 4 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(0), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(0);
			}
			else if ( m_displayMode == 1 && (minCol == 5 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(1), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(1);
			}
			else if ( m_displayMode == 1 && (minCol == 6 || m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(2), cRef ) < 0.05))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(2);
			}
			//look for white and disable if detect for primaries/secondaries is not selected
			else if ( m_displayMode == 0 || (m_displayMode == 1 && minCol == 7) || m_displayMode == 3 || m_displayMode == 4 || m_pRefColor->GetDeltaxy ( cRef.GetWhite(), cRef ) < 0.05)
			{
				m_bLumaMode = (m_displayMode==1)?TRUE:FALSE;
				aReference = cRef.GetWhite();
			}
		} //mincol > 0
		else //autodetect if mincol <= 0 (measuring) and this is primaries or grayscale page
		{
			if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(0), cRef ) < 0.05 && GetConfig()->m_bDetectPrimaries))
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(0);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(1), cRef ) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(1);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefPrimary(2), cRef) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefPrimary(2);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(0), cRef ) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(0);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(1), cRef ) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(1);
			}
			else if ( m_displayMode == 1 && (m_pRefColor->GetDeltaxy ( m_pDocument->GetMeasure()->GetRefSecondary(2), cRef ) < 0.05) && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = TRUE;
				aReference = m_pDocument->GetMeasure()->GetRefSecondary(2);
			}
			else if ( (m_displayMode == 1 || m_displayMode == 0) && m_pRefColor->GetDeltaxy ( GetColorReference().GetWhite(), cRef ) < 0.05 && GetConfig()->m_bDetectPrimaries)
			{
				m_bLumaMode = FALSE;
				aReference = cRef.GetWhite();
			}
		} 

		BOOL isHDR = ( GetConfig()->m_GammaOffsetType == 5 && (m_displayMode == 1 || m_displayMode >= 5 && m_displayMode <= 11) );
		CColor white = m_pDocument->GetMeasure()->GetPrimeWhite();

		if (!white.isValid() && isHDR)
			white = m_pDocument->GetMeasure()->GetGray((m_pDocument->GetMeasure()->GetGrayScaleSize()-1) / 2 );

		if (!white.isValid() || m_displayMode == 0 || m_displayMode == 2 || m_displayMode == 3 || m_displayMode == 4)
			white = m_pDocument -> GetMeasure () ->GetOnOffWhite();
		if ( m_displayMode > 4 && (GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb))
			white = m_pDocument -> GetMeasure () ->GetOnOffWhite();

		//special case check if user has done a less than 100% primaries run and use grayscale white instead for colorchecker
		if (m_pDocument->GetMeasure()->GetOnOffWhite().isValid())
			if ((m_pDocument->GetMeasure()->GetPrimeWhite()[1] / m_pDocument->GetMeasure()->GetOnOffWhite()[1] < 0.9) && m_displayMode == 11  && GetConfig()->m_GammaOffsetType != 5)
				white = m_pDocument -> GetMeasure () ->GetOnOffWhite();
		
    	int nCount = m_pDocument -> GetMeasure () -> GetGrayScaleSize ();
        double YWhite = white.GetY();
		double tmWhite = getL_EOTF(0.5022283, noDataColor, noDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.0;

			if ( (GetConfig()->m_GammaOffsetType == 5 && m_displayMode <=11 && m_displayMode >= 5) )
			{
				if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017 && m_displayMode == 11)	
				{
					aReference.SetX((aReference.GetX() * 100.));
					aReference.SetY((aReference.GetY() * 100.));
					aReference.SetZ((aReference.GetZ() * 100.));
				}
				else
				{
					aReference.SetX((aReference.GetX() * 105.95640));
					aReference.SetY((aReference.GetY() * 105.95640));
					aReference.SetZ((aReference.GetZ() * 105.95640));
				}
			}

        if (!m_bLumaMode)
        {
            ColorxyY tmpColor(GetColorReference().GetWhite());
		    // Determine Reference Y luminance for Delta E calculus
			if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
			{
				// Compute reference Luminance regarding actual offset and reference gamma 
                // fixed to use correct gamma predicts
                // and added option to assume perfect gamma
					double x;
					switch (m_displayMode)
					{
						int Count;
						case 3:
						x = ArrayIndexToGrayLevel ( (minCol - 1)*(GetConfig()->m_GammaOffsetType==5?2:1), 101, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit );
						break;
						case 4:
						Count = m_pDocument -> GetMeasure()->GetNearWhiteScaleSize();
						x = ArrayIndexToGrayLevel ( m_pDocument->GetMeasure()->m_NearWhiteClipCol - Count + (minCol - 1), 101, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit );
						break;
						default:
						x = ArrayIndexToGrayLevel ( minCol - 1 , nCount, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit );
					}
					double valy, Gamma, Offset;
                    Gamma = GetConfig()->m_GammaRef;
                    GetConfig()->m_GammaAvg = Gamma;
                    m_pDocument->ComputeGammaAndOffset(&Gamma, &Offset, 1, 1, nCount, false);
                    if (GetConfig()->m_useMeasuredGamma)
						GetConfig()->m_GammaAvg = (Gamma<1?2.2:floor((Gamma+.005)*100.)/100.);
                    GetConfig()->SetPropertiesSheetValues();
            		CColor White = m_pDocument -> GetMeasure () -> GetGray ( nCount - 1 );
//	                CColor Black = m_pDocument -> GetMeasure () -> GetGray ( 0 );
	                CColor Black = m_pDocument -> GetMeasure () -> GetOnOffBlack();
					int mode = GetConfig()->m_GammaOffsetType;
					if (GetConfig()->m_colorStandard == sRGB) mode = 99;
					if ( mode >= 4 )
			        {
						double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit);
						valy = getL_EOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode,GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1);
//						valy = min(valy, GetConfig()->m_TargetMaxL);
			        }
			        else
			        {
				        double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown, GetConfig () -> m_bUse10bit)+Offset)/(1.0+Offset);
				        valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
						if (mode == 1) //black compensation target
							valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			        }

					if ( mode == 5)
						tmpColor[2] = valy * 100. / YWhite;
					else
						tmpColor[2] = valy;
                    if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
			            tmpColor[2] = m_pRefColor->GetY() / YWhite; //perfect gamma
			
					aReference.SetxyYValue(tmpColor);

			}

			//RGB plots now include luminance offset when grayscale dE handling includes it
			double fact;
			ColorxyY aColor = m_pRefColor -> GetxyYValue();
			if ( GetConfig ()->m_dE_gray == 0 )
		    {
						// Use actual gray luminance as correct reference (absolute)
                           YWhite = m_pRefColor->GetY();
						   fact = 1.0;
			}
			else
				fact = aColor[2] / (tmpColor[2] * white.GetY());

            ColorXYZ normColor;

            if(aColor[1] > 0.0 && (minCol != 1 || m_displayMode == 4))
            {
                normColor[0]=(aColor[0]/aColor[1])*fact;
                normColor[1]=1.0*fact;
                normColor[2]=((1.0-(aColor[0]+aColor[1]))/aColor[1])*fact;
            }
            else
            {
                normColor[0]=0.0;
                normColor[1]=0.0;
                normColor[2]=0.0;
            }

            ColorRGB normColorRGB(normColor, cRef);
                        
            m_redValue=(float)(normColorRGB[0]*100.0);
            m_greenValue=(float)(normColorRGB[1]*100.0);
            m_blueValue=(float)(normColorRGB[2]*100.0);
        }
		else if (aReference.isValid())
		{
            ColorXYZ aColor=m_pRefColor->GetXYZValue(), refColor=aReference.GetXYZValue() ;
			
			m_redValue=100.;
			m_greenValue=100.;
			m_blueValue=100.;

            if ( white.isValid() )
            {

				if ( isHDR )
				{
					bool shiftDiffuse=(abs(GetConfig()->m_DiffuseL-94.0)>0.5);
					if (DVD)
					{
						tmWhite = getL_EOTF(0.50, noDataColor, noDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.0;
						if (m_displayMode == 1)
						{
							if (GetColorReference().m_standard == UHDTV || GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == HDTV || minCol == 7)
								white.SetY(!shiftDiffuse?92.254965:tmWhite);
							else
								white.SetY(94.37844);
						}
						else
						{
							if ( ( (GetColorReference().m_standard == UHDTV2 && minCol == satsize) || GetColorReference().m_standard == HDTV || GetColorReference().m_standard == UHDTV) && m_displayMode != 11)// && !shiftDiffuse) //&& nCol == (satsize)
								white.SetY(92.25496);
							else
								white.SetY(94.37844);
						}
					}
					else
					{
						if (m_displayMode == 1)
							if (GetColorReference().m_standard == UHDTV2 || GetColorReference().m_standard == HDTV || GetColorReference().m_standard == UHDTV || minCol == 7)
								white.SetY(tmWhite);
							else
								white.SetY(94.37844);
						else
							if ((GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017) && m_displayMode == 11)
								white.SetY(m_pDocument->GetMeasure()->GetGray((m_pDocument->GetMeasure()->GetGrayScaleSize()-1)).GetY());
							else
								white.SetY(94.37844);
					}
				}
				aColor[0]=aColor[0]/white.GetY();
				aColor[1]=aColor[1]/white.GetY();
				aColor[2]=aColor[2]/white.GetY();

				ColorRGB aColorRGB(aColor, cRef);
				ColorRGB refColorRGB(refColor, cRef);

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

		double RefWhite = 1.0;
		if ( isHDR )
		{
			bool shiftDiffuse = (abs(GetConfig()->m_DiffuseL-94.0)>0.5);
			if (DVD)
			{
				if (m_displayMode == 1)
				{
						tmWhite = getL_EOTF(0.50, noDataColor, noDataColor, GetConfig()->m_GammaRel, GetConfig()->m_Split, 5, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap, FALSE, GetConfig()->m_TargetSysGamma, GetConfig()->m_BT2390_BS, GetConfig()->m_BT2390_WS, GetConfig()->m_BT2390_WS1) * 100.0;								
						if ( (cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV || minCol == 7) ) //fix for P3/Mascior
							RefWhite = YWhite / (!shiftDiffuse?92.254965:tmWhite);
				else
				{
						RefWhite = YWhite / (tmWhite);
						YWhite = YWhite * 94.37844 / (tmWhite);
				}
			}
			else
			{
				if ( ((cRef.m_standard == UHDTV2 && minCol == satsize ) || cRef.m_standard == HDTV || cRef.m_standard == UHDTV)  && m_displayMode != 11)// && !shiftDiffuse) //fixes skin && nCol == satsize
					RefWhite = YWhite / (tmWhite);
				else
				{
					RefWhite = YWhite / (tmWhite) ;
					YWhite = YWhite * 94.37844 / (tmWhite);
				}
			}
		}
		else
		{
			if (m_displayMode == 1)
			{
				if (cRef.m_standard == UHDTV2 || cRef.m_standard == HDTV || cRef.m_standard == UHDTV || minCol == 7)
					RefWhite = YWhite / (tmWhite) ;
				else
				{
					RefWhite = YWhite / (tmWhite) ;					
					YWhite = YWhite * 94.37844 / (tmWhite) ;
				}
			}
			else
			{
				if (GetConfig()->m_CCMode >= MASCIOR50 && GetConfig()->m_CCMode <= LG400017 && m_displayMode == 11)	
					YWhite = m_pDocument->GetMeasure()->GetGray((m_pDocument->GetMeasure()->GetGrayScaleSize()-1)).GetY() ;
				else
				{
					RefWhite = YWhite / (tmWhite) ;
					YWhite = YWhite * 94.37844 / (tmWhite) ;
				}
			}
			}
		}
		m_dEValue = aReference.isValid()?float(m_pRefColor->GetDeltaE(YWhite, aReference, RefWhite, cRef, GetConfig()->m_dE_form, !m_bLumaMode,  GetConfig()->gw_Weight )):0 ;
    } //have valid m_prefcolor
	else
	{
		m_dEValue = 0.;
		//m_pRef
	}

	if (m_dEValue > 40 || (minCol == 1 && !m_bLumaMode && m_displayMode != 4) ) m_dEValue = 0.;
	Invalidate(FALSE);

	CString title;
	title.LoadString(m_bLumaMode ? (GetConfig()->m_useHSV?IDS_LCHLEVELS:IDS_RGBLEVELS) : IDS_RGBLEVELS);
	((CMainView *)GetParent())->m_RGBLevelsLabel.SetWindowText ((LPCSTR)title);
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
	double dEBarHeightmax=dEyScale * 4.8;
	int dEBarHeight= min((int)dEBarHeightmax,(int)(m_dEValue*dEyScale));
	int dEBarX=widthMargin+3*barWidth+3*interBarMargin;
	int dEBarY=rect.Height()-dEBarHeight-heightMargin;

    // draw RGB bars
	int ellipseHeight=barWidth/4;
    if (m_bLumaMode && GetConfig()->m_useHSV)
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
	if (GetConfig()->isHighDPI)
	{
		VERIFY(font.CreateFont(
		   18,						  // nHeight
		   0,                         // nWidth
		   0,                         // nEscapement
		   0,                         // nOrientation
		   FW_BOLD,					  // nWeight
		   FALSE,                     // bItalic
		   FALSE,                     // bUnderline
		   FALSE,                         // cStrikeOut
		   0,              // nCharSet
		   OUT_TT_ONLY_PRECIS,//OUT_DEFAULT_PRECIS,        // nOutPrecision
		   CLIP_DEFAULT_PRECIS,       // nClipPrecision
		   PROOF_QUALITY,//DEFAULT_QUALITY,           // nQuality
		   VARIABLE_PITCH | FF_MODERN,//FIXED_PITCH,				  // nPitchAndFamily
		   _T("Garamond")));                 // lpszFacename
	} else
	{
		VERIFY(font.CreateFont(
		   14,						  // nHeight
		   0,                         // nWidth
		   0,                         // nEscapement
		   0,                         // nOrientation
		   FW_BOLD,					  // nWeight
		   FALSE,                     // bItalic
		   FALSE,                     // bUnderline
		   FALSE,                         // cStrikeOut
		   0,              // nCharSet
		   OUT_TT_ONLY_PRECIS,//OUT_DEFAULT_PRECIS,        // nOutPrecision
		   CLIP_DEFAULT_PRECIS,       // nClipPrecision
		   PROOF_QUALITY,//DEFAULT_QUALITY,           // nQuality
		   VARIABLE_PITCH | FF_MODERN,//FIXED_PITCH,				  // nPitchAndFamily
		   _T("Garamond")));                 // lpszFacename
	}

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

