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

// TargetWnd.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "TargetWnd.h"
#include "Color.h"
#include "BitmapTools.h"
#include "MainFrm.h"
#include "DataSetDoc.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTargetWnd

CTargetWnd::CTargetWnd()
{
	m_pointBitmap.LoadBitmap(IDB_POINT_BITMAP);
	m_deltax=0.0;
	m_deltay=0.0;
	m_marginInPercent=15;
	m_targetRectInPercent=10;
	m_pointSizeInPercent=50;
	m_prev_cx = -1; 
	m_prev_cy = -1; 
	m_pRefColor = NULL;
	pTooltipText = NULL;
    m_pDocument = NULL;
	centerXYZ = GetColorReference().GetWhite();
	nR = 0;
	nG = 0;
	nB = 0;
}

CTargetWnd::~CTargetWnd()
{
}

void CTargetWnd::Refresh(BOOL m_b16_235, int minCol, int nSize, int m_DisplayMode, CDataSetDoc * pDoc, int target)
{	
	if (m_pRefColor)
	{
		CString	Msg, str;
		Msg.LoadString ( IDS_GDIGENERATOR_NAME );
		CString m_generatorChoice = GetConfig()->GetProfileString("Defaults","Generator",(LPCSTR)Msg);
		str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
		bool DVD = (m_generatorChoice == str);
		bool isUHDTV3 = GetConfig()->m_colorStandard == UHDTV3;
		bool isUHDTV4 = GetConfig()->m_colorStandard == UHDTV4;
		bool isHDR10 = GetConfig()->m_GammaOffsetType == 5;
		bool isHLG = GetConfig()->m_GammaOffsetType == 7;

		if (minCol > 0)
		{
			int		y1 = 0, y2 = 0, y3 = 0;
			double	x1, x2, x3, p1, p2, p3, z1, z2, z3;
			ColorRGBDisplay	GenColors [MAX_USER_CC_PATCH_SIZE + 10];
		
			if ((m_DisplayMode == 0 || m_DisplayMode == 2 || m_DisplayMode == 3 || m_DisplayMode == 4 ))
			{
				centerXYZ = GetColorReference().GetWhite();
	            if (nSize > 0)
				{
					if (isHDR10 && m_DisplayMode == 3)
						p1 = (double)(minCol - 1) * 2. / (double)(nSize-1);
					else
						p1 = (double)(minCol-1) / (double)(nSize-1);
				}
			    else
    				p1 =(double)(pDoc->GetMeasure()->m_NearWhiteClipCol+nSize+minCol-1) / (double)(100.);

				//fix 255->235 rounding errors that the generator will create
				x1 =  (int)floor(p1 * 255.0 + 0.5);
				y1 =  (int)floor(p1 * 219.0 + 0.5);
				z1= y1 - x1 / 255. * 219.;
				x1 += floor( z1 + 0.5);

				nR=(int)x1;
				nG=(int)x1;
				nB=(int)x1;
 				m_clr = RGB(nR,nG,nB);
			}
			else if (m_DisplayMode > 4 && m_DisplayMode < 12) //saturations
			{
				switch (m_DisplayMode)
				{
					case 5:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(0, double(minCol-1) / double(nSize - 1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, false, false, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 6:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(1, double(minCol-1) / double(nSize - 1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, true, false, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 7:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(2, double(minCol-1) / double(nSize - 1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, false, true,  GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 8:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(3, double(minCol-1) / double(nSize-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, true, false, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 9:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(4, double(minCol-1) / double(nSize-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, false, true, true, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 10:
					centerXYZ =  pDoc->GetMeasure()->GetRefSat(5, double(minCol-1) / double(nSize-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb)).GetXYZValue();
					GenerateSaturationColors (GetColorReference(), GenColors, nSize, true, false, true, GetConfig()->m_GammaOffsetType);				
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				case 11:
					centerXYZ =  pDoc->GetMeasure()->GetRefCC24Sat(minCol-1).GetXYZValue();
					GenerateCC24Colors (GetColorReference(), GenColors, GetConfig()->m_CCMode, GetConfig()->m_GammaOffsetType);
					if (GetConfig()->m_CCMode == MCD)
					{
						if (minCol >= 7)
							minCol -= 6;
						else
							switch (minCol)
							{
								case 1:
									minCol = 24;
									break;
								case 2:
									minCol = 23;
									break;
								case 3:
									minCol = 22;
									break;
								case 4:
									minCol = 21;
									break;
								case 5:
									minCol = 20;
									break;
								case 6:
									minCol = 19;
									break;
							}
					}
					p1=GenColors[minCol-1][0] / 100.;
					p2=GenColors[minCol-1][1] / 100.;
					p3=GenColors[minCol-1][2] / 100.;
					break;
				default:
					p1=0.1;
					p2=0.2;
					p3=0.3;
				}

				x1 =  (int)floor(p1 * 255.0 + 0.5);
				x2 =  (int)floor(p2 * 255.0 + 0.5);
				x3 =  (int)floor(p3 * 255.0 + 0.5);
				y1 =  (int)floor(p1 * 219.0 + 0.5);
				y2 =  (int)floor(p2 * 219.0 + 0.5);
				y3 =  (int)floor(p3 * 219.0 + 0.5);
				z1 = y1 - x1 / 255. * 219.;
				z2 = y2 - x2 / 255. * 219.;
				z3 = y3 - x3 / 255. * 219.;
				x1 += floor( z1 + 0.5);
				x2 += floor( z2 + 0.5);
				x3 += floor( z3 + 0.5);

				nR=(int)x1;
				nG=(int)x2;
				nB=(int)x3;
				m_clr = RGB(nR,nG,nB);
			}
			else if ( m_DisplayMode == 1 && minCol == 1 )
			{
				centerXYZ = GetColorReference().GetRed();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
						nR = 174;
						nG = 51;
						nB = 51;
						m_clr = RGB(nR,nG,nB);
						break;
					case UHDTV3:
							nR = isHDR10?121:isHLG?242:225;
							nG = isHDR10?61:isHLG?82:64;
							nB = 0;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?116:isHLG?233:207;
							nG = isHDR10?68:isHLG?115:77;
							nB = isHDR10?44:isHLG?0:40;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 26;
							nB = 26;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = 0;
							nB = 0;
   						m_clr = RGB(192,0,0);
						break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 2)
			{
				centerXYZ = GetColorReference().GetGreen();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 71;
							nG = 186;
							nB = 71;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?90:isHLG?177:123;
							nG = isHDR10?127:isHLG?252:248;
							nB = isHDR10?45:isHLG?0:41;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?101:isHLG?202:155;
							nG = isHDR10?126:isHLG?251:246;
							nB = isHDR10?72:isHLG?131:85;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 77;
							nG = 204;
							nB = 77;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = isHDR10?128:255;
							nB = 0;
	   				  m_clr = RGB(0,192,0);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 3)
			{
				centerXYZ = GetColorReference().GetBlue();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 49;
							nG = 49;
							nB = 128;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?62:isHLG?86:65;
							nG = isHDR10?41:isHLG?0:35;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?59:isHLG?77:62;
							nG = isHDR10?40:isHLG?0:34;
							nB = isHDR10?126:isHLG?250:242;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 128;
							nG = 128;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = 0;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(0,0,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 4)
			{
				centerXYZ = GetColorReference().GetYellow();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 189;
							nG = 189;
							nB = 85;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?127:isHLG?253:249;
							nG = isHDR10?128:isHLG?254:254;
							nB = isHDR10?44:isHLG?0:40;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?127:isHLG?253:250;
							nG = isHDR10?128:isHLG?254:254;
							nB = isHDR10?76:isHLG?141:92;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 204;
							nB = 26;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = 0;
	   				  m_clr = RGB(192,192,0);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 5)
			{
				centerXYZ = GetColorReference().GetCyan();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 92;
							nG = 186;
							nB = 186;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?94:isHLG?188:135;
							nG = isHDR10?127:isHLG?253:249;
							nB = isHDR10?128:isHLG?255:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?104:isHLG?208:163;
							nG = isHDR10?126:isHLG?252:247;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 26;
							nG = 204;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(0,192,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 6)
			{
				centerXYZ = GetColorReference().GetMagenta();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 164;
							nG = 74;
							nB = 164;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?122:isHLG?245:231;
							nG = isHDR10?65:isHLG?103:71;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?118:isHLG?236:213;
							nG = isHDR10?71:isHLG?125:82;
							nB = isHDR10?126:isHLG?251:245;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 27;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = 0;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(192,0,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && minCol == 7 )
			{
				centerXYZ =  GetColorReference().GetWhite();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 191;
							nG = 191;
							nB = 191;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 255;
							nG = 255;
							nB = 255;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(255,255,255);
					  break;
				}
			}    //update RGB
		} //mincol > 0
		else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetRed(),GetColorReference()) < 0.05 && DVD ) 
			{
				centerXYZ = GetColorReference().GetRed();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
						nR = 174;
						nG = 51;
						nB = 51;
						m_clr = RGB(nR,nG,nB);
						break;
					case UHDTV3:
							nR = isHDR10?121:isHLG?242:225;
							nG = isHDR10?61:isHLG?82:64;
							nB = 0;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?116:isHLG?233:207;
							nG = isHDR10?68:isHLG?115:77;
							nB = isHDR10?44:isHLG?0:40;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 26;
							nB = 26;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = 0;
							nB = 0;
   						m_clr = RGB(192,0,0);
						break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetGreen(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetGreen();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 71;
							nG = 186;
							nB = 71;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?90:isHLG?177:123;
							nG = isHDR10?127:isHLG?252:248;
							nB = isHDR10?45:isHLG?0:41;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?101:isHLG?202:155;
							nG = isHDR10?126:isHLG?251:246;
							nB = isHDR10?72:isHLG?131:85;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 77;
							nG = 204;
							nB = 77;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = isHDR10?128:255;
							nB = 0;
	   				  m_clr = RGB(0,192,0);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetBlue(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetBlue();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 49;
							nG = 49;
							nB = 128;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?62:isHLG?86:65;
							nG = isHDR10?41:isHLG?0:35;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?59:isHLG?77:62;
							nG = isHDR10?40:isHLG?0:34;
							nB = isHDR10?126:isHLG?250:242;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 128;
							nG = 128;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = 0;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(0,0,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetYellow(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetYellow();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 189;
							nG = 189;
							nB = 85;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?127:isHLG?253:249;
							nG = isHDR10?128:isHLG?254:254;
							nB = isHDR10?44:isHLG?0:40;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?127:isHLG?253:250;
							nG = isHDR10?128:isHLG?254:254;
							nB = isHDR10?76:isHLG?141:92;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 204;
							nB = 26;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = 0;
	   				  m_clr = RGB(192,192,0);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetCyan(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetCyan();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 92;
							nG = 186;
							nB = 186;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?94:isHLG?188:135;
							nG = isHDR10?127:isHLG?253:249;
							nB = isHDR10?128:isHLG?255:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?104:isHLG?208:163;
							nG = isHDR10?126:isHLG?252:247;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 26;
							nG = 204;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = 0;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(0,192,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetMagenta(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ = GetColorReference().GetMagenta();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 164;
							nG = 74;
							nB = 164;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?122:isHLG?245:231;
							nG = isHDR10?65:isHLG?103:71;
							nB = isHDR10?128:isHLG?254:253;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?118:isHLG?236:213;
							nG = isHDR10?71:isHLG?125:82;
							nB = isHDR10?126:isHLG?251:245;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 204;
							nG = 26;
							nB = 204;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = 0;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(192,0,192);
					  break;
				}
			}
			else if ( m_DisplayMode == 1 && m_pRefColor->GetDeltaxy(GetColorReference().GetWhite(),GetColorReference()) < 0.05 && GetConfig()->m_bDetectPrimaries && DVD )
			{
				centerXYZ =  GetColorReference().GetWhite();
				switch (GetConfig()->m_colorStandard)
				{
					case HDTVa:
							nR = 255;
							nG = 255;
							nB = 255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV3:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case UHDTV4:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
						m_clr = RGB(nR,nG,nB);
					break;
					case HDTVb:
							nR = 191;
							nG = 191;
							nB = 191;
						m_clr = RGB(nR,nG,nB);
					break;
					default:
							nR = isHDR10?128:255;
							nG = isHDR10?128:255;
							nB = isHDR10?128:255;
	   				  m_clr = RGB(255,255,255);
					  break;
				}
			}

		//Update test window for display when selected
		BOOL		bDisplayColor = GetConfig () -> m_bDisplayTestColors;

		if (bDisplayColor && target <= TARGET_ALL)
		{
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.SetColor ( RGB(nR,nG,nB) );
			( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.RedrawWindow ();
		}
		
		if (target >= TARGET_ALL)
		{
			//update RGB

			ColorxyY aColor = m_pRefColor -> GetxyYValue();
			ColorxyY centerxyY(centerXYZ);

			m_deltax = (aColor[0]-centerxyY[0])/centerxyY[0];
			m_deltay = (aColor[1]-centerxyY[1])/centerxyY[1];
		
			if ( m_tooltip.IsWindowVisible() )
			{
				if ( pTooltipText )
				{
					pTooltipText -> Format("<b>delta x</b>: %.1f%% <br><b>delta y</b>: %.1f%%",m_deltax*100.0,m_deltay*100.0);
					m_tooltip.Invalidate();
				}
			}
			UpdateScaledBitmap();
			Invalidate(TRUE);
		}
	} //have valid m_prefcolor
}


BEGIN_MESSAGE_MAP(CTargetWnd, CWnd)
	//{{AFX_MSG_MAP(CTargetWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	//}}AFX_MSG_MAP
	ON_NOTIFY (UDM_TOOLTIP_DISPLAY, NULL, NotifyDisplayTooltip)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTargetWnd message handlers

void CTargetWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
    CHMemDC pDC(&dc);
	
	CRect rect;
	GetClientRect(&rect);

	// Draw background bitmap
	CDC dcBg;
	dcBg.CreateCompatibleDC(pDC);
	CBitmap *pOldBitmap=dcBg.SelectObject(&m_bgBitmap);
	pDC->BitBlt(0,0,rect.Width(),rect.Height(),&dcBg,0,0,SRCCOPY);
	dcBg.SelectObject(pOldBitmap);

	// Draw target frame
	int widthMargin=rect.Width()*m_marginInPercent/100;
	int heightMargin=rect.Height()*m_marginInPercent/100;

	CRect frameRect(rect.left+widthMargin,rect.top+heightMargin,rect.right-widthMargin,rect.bottom-heightMargin);
	
	CBrush br(m_clr);
	pDC->SelectObject ( & br );
	pDC->SelectObject ( GetStockObject ( WHITE_PEN ) );
	pDC->RoundRect ( & frameRect, CPoint ( 20, 20 ) );
	
	int targetRectWidth=rect.Width()*m_targetRectInPercent/100;
	int targetRectHeight=rect.Height()*m_targetRectInPercent/100;

    CPen crossPen(PS_DASH,1,RGB(255,255,255));
    CPen crossPen2(PS_DOT,1,RGB(0,0,0));

    CPen *pOldPen = pDC->SelectObject(&crossPen); 

	// auto-zoom
	double zoomFactor=GetZoomFactor();

	int cornerWidth=(int)(zoomFactor*targetRectWidth/2.0);
	int cornerHeight=(int)(zoomFactor*targetRectHeight/2.0);
	
	// draw white cross 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x,rect.CenterPoint().y+2*cornerHeight); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x+2*cornerWidth,rect.CenterPoint().y); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x,rect.CenterPoint().y-2*cornerHeight); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x-2*cornerWidth,rect.CenterPoint().y); 

    CPen *pOldPen2 = pDC->SelectObject(&crossPen2); 

	// draw black cross 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x,rect.CenterPoint().y+2*cornerHeight); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x+2*cornerWidth,rect.CenterPoint().y); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x,rect.CenterPoint().y-2*cornerHeight); 
	pDC->MoveTo(rect.CenterPoint());
	pDC->LineTo(rect.CenterPoint().x-2*cornerWidth,rect.CenterPoint().y); 

    // draw circle
	pDC->SelectObject ( GetStockObject ( NULL_BRUSH ) );
	pDC->Ellipse ( rect.CenterPoint().x-cornerWidth, rect.CenterPoint().y-cornerHeight, rect.CenterPoint().x+cornerWidth, rect.CenterPoint().y+cornerHeight );
    pDC->SelectObject(pOldPen);

    int r1=0,g1=255,b1=119;
    int r2=255,g2=192,b2=0;
    int r3=0,g3=80,b3=255;
    int r4=255,g4=0,b4=136;

	// draw corners
	for(int i=1;i<8;i++)
	{
		if(rect.CenterPoint().x-(i+1)*cornerWidth > frameRect.left + 4)	// Clipping
		{
			CPen cornerPen1(PS_SOLID,1,RGB(r1,g1,b1));
			pOldPen = pDC->SelectObject(&cornerPen1); 
			pDC -> Arc ( rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight, 
						 rect.CenterPoint().x-(i+1)*cornerWidth/2, rect.CenterPoint().y-(i+1)*cornerHeight,
						 rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight/2 ); 

			pDC->SelectObject(pOldPen);

			CPen cornerPen2(PS_SOLID,1,RGB(r2,g2,b2));
			pOldPen = pDC->SelectObject(&cornerPen2); 
			pDC -> Arc ( rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight/2,
						 rect.CenterPoint().x+(i+1)*cornerWidth/2, rect.CenterPoint().y-(i+1)*cornerHeight );

			pDC->SelectObject(pOldPen);

			CPen cornerPen3(PS_SOLID,1,RGB(r3,g3,b3));
			pOldPen = pDC->SelectObject(&cornerPen3); 
			pDC -> Arc ( rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight, 
						 rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight/2,
						 rect.CenterPoint().x-(i+1)*cornerWidth/2, rect.CenterPoint().y+(i+1)*cornerHeight ); 

			pDC->SelectObject(pOldPen);

			CPen cornerPen4(PS_SOLID,1,RGB(r4,g4,b4));
			pOldPen = pDC->SelectObject(&cornerPen4); 
			pDC -> Arc ( rect.CenterPoint().x-(i+1)*cornerWidth, rect.CenterPoint().y-(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight, 
						 rect.CenterPoint().x+(i+1)*cornerWidth/2, rect.CenterPoint().y+(i+1)*cornerHeight,
						 rect.CenterPoint().x+(i+1)*cornerWidth, rect.CenterPoint().y+(i+1)*cornerHeight/2 ); 

			pDC->SelectObject(pOldPen);
		}
	}

	if( m_pRefColor == NULL || !m_pRefColor->isValid())
		return;		// Draw nothing more

	CPoint targetPoint=rect.CenterPoint();
	targetPoint+=CPoint((int)(zoomFactor*m_deltax*frameRect.Width()/2.0),-(int)(zoomFactor*m_deltay*frameRect.Height()/2.0));

	// draw Arrow
	int pointWidth=(int)(rect.Width()*m_pointSizeInPercent/100.0);
	int pointHeight=(int)(rect.Height()*m_pointSizeInPercent/100.0);

	DrawTransparentBitmap(pDC->m_hDC,(HBITMAP)m_scaledPointBitmap.m_hObject,(short)(targetPoint.x-pointWidth/4),(short)(targetPoint.y-pointHeight*3/4),RGB(0,0,0));
}

double CTargetWnd::GetZoomFactor()
{
		// auto-zoom
	double zoomFactor;
	int maxDelta=max(abs((int)(m_deltax*100.0)), abs((int)(m_deltay*100.0)) )/10;
	switch(maxDelta)
	{
		case 0:
			zoomFactor = 4.0;
			break;
		case 1:
			zoomFactor = 3.0;
			break;
		case 2:
			zoomFactor = 2.0;
			break;
		case 3:
			zoomFactor = 1.5;
			break;
		default:
			zoomFactor = 1.0;
			break;
	}
	return zoomFactor;
}

void CTargetWnd::UpdateScaledBitmap()
{

    CRect rect;
    GetClientRect(&rect);

	// draw arrow
	int pointWidth=(int)(rect.Width()*m_pointSizeInPercent/100.0);
	int pointHeight=(int)(rect.Height()*m_pointSizeInPercent/100.0);

    CPaintDC dc(this);

	BITMAP bm;
	m_pointBitmap.GetBitmap(&bm);

	if(m_scaledPointBitmap.m_hObject)
		m_scaledPointBitmap.DeleteObject();
	m_scaledPointBitmap.CreateCompatibleBitmap(&dc,pointWidth,pointHeight);


	CDC memDCSrc;
	memDCSrc.CreateCompatibleDC( &dc );
	CBitmap* pOld = memDCSrc.SelectObject(&m_pointBitmap);
	int oldMode=memDCSrc.GetStretchBltMode();

	CDC memDCScaled;
	memDCScaled.CreateCompatibleDC( &dc );
	memDCScaled.SetStretchBltMode(HALFTONE);
   // The docs say that you should call SetBrushOrgEx after SetStretchBltMode,
   // but not what the arguments should be.
    SetBrushOrgEx(memDCScaled, 0,0, NULL);
	CBitmap* pOldScaled = memDCScaled.SelectObject(&m_scaledPointBitmap);
	memDCScaled.StretchBlt(0,0,pointWidth,pointHeight,&memDCSrc,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);
    memDCScaled.SelectObject(pOldScaled); 

	memDCSrc.SetStretchBltMode(oldMode);
    memDCSrc.SelectObject(pOld); 
	ReleaseDC(&dc);
}

void CTargetWnd::MakeBgBitmap()	// Create background bitmap
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

	// windows problem with "negative" client height. Can occur when mainframe is really small
	if ( rect.bottom == 32767 )
		rect.bottom = 0;

    int r1=0,g1=255,b1=119;
    int r1b=65,g1b=255,b1b=0;
    int r2=255,g2=192,b2=0;
    int r2b=255,g2b=0,b2b=0;
    int r3=0,g3=0,b3=255;
    int r4=255,g4=0,b4=136;

    int r,g,b;

    CDC dc2;
    dc2.CreateCompatibleDC(&dc);

   if(m_bgBitmap.m_hObject)
        m_bgBitmap.DeleteObject();
    m_bgBitmap.CreateCompatibleBitmap(&dc,rect.Width(),rect.Height());

    CBitmap *pOldBitmap=dc2.SelectObject(&m_bgBitmap);

	for(int i=0; i<rect.Width() / 2;i++)
	{
        r = r1 + (i * (r1b-r1) / (rect.Width()/2) );
        g = g1 + (i * (g1b-g1) / (rect.Width()/2) );
        b = b1 + (i * (b1b-b1) / (rect.Width()/2) );

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(i,0);

        dc2.SelectObject(pOldPen);
	}

	for(int i=rect.Width() / 2; i<rect.Width();i++)
	{
        r = r1b + ( (i-(rect.Width()/2)) * (r2-r1b) / (rect.Width()/2) );
        g = g1b + ( (i-(rect.Width()/2)) * (g2-g1b) / (rect.Width()/2) );
        b = b1b + ( (i-(rect.Width()/2)) * (b2-b1b) / (rect.Width()/2) );

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(i,0);

        dc2.SelectObject(pOldPen);
	}

	for(int i=0; i<rect.Width();i++)
	{
        r = r3 + (i * (r4-r3) / rect.Width());
        g = g3 + (i * (g4-g3) / rect.Width());
        b = b3 + (i * (b4-b3) / rect.Width());

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(i,rect.Height());

        dc2.SelectObject(pOldPen);
	}

	for(int i=0; i<rect.Height();i++)
	{
        r = r1 + (i * (r3-r1) / rect.Height());
        g = g1 + (i * (g3-g1) / rect.Height());
        b = b1 + (i * (b3-b1) / rect.Height());

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(0,i);

        dc2.SelectObject(pOldPen);
	}

	for(int i=0; i<rect.Height()/2;i++)		
	{
        r = r2 + (i * (r2b-r2) / (rect.Height()/2) );
        g = g2 + (i * (g2b-g2) / (rect.Height()/2) );
        b = b2 + (i * (b2b-b2) / (rect.Height()/2) );

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(rect.Width()-1,i);	// -1 to avoid bad junction

        dc2.SelectObject(pOldPen);
	}

	for(int i=rect.Height()/2; i<rect.Height();i++)		
	{
        r = r2b + ( (i-(rect.Height()/2)) * (r4-r2b) / (rect.Height()/2) );
        g = g2b + ( (i-(rect.Height()/2)) * (g4-g2b) / (rect.Height()/2) );
        b = b2b + ( (i-(rect.Height()/2)) * (b4-b2b) / (rect.Height()/2) );

        CPen aPen(PS_SOLID,1,RGB(r,g,b));
        CPen *pOldPen = dc2.SelectObject(&aPen); 

        dc2.MoveTo(rect.CenterPoint());
        dc2.LineTo(rect.Width()-1,i);	// -1 to avoid bad junction

        dc2.SelectObject(pOldPen);
	}

    dc2.SelectObject(pOldBitmap);
}



void CTargetWnd::OnSize(UINT nType, int cx, int cy) 
{
	if(cx != m_prev_cx || cy != m_prev_cy)
	{	
		MakeBgBitmap();
		UpdateScaledBitmap();
		Invalidate(FALSE);
	}
	m_prev_cx=cx;
	m_prev_cy=cy;
}

BOOL CTargetWnd::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);

	m_tooltip.Create(this);	
	m_tooltip.AddTool(this, "Tooltip for rectangle area");
	m_tooltip.SetBehaviour(PPTOOLTIP_MULTIPLE_SHOW);
	m_tooltip.SetNotify(TRUE);
	m_tooltip.SetColorBk(RGB(255,165,0),RGB(0,128,128));
	m_tooltip.SetEffectBk(CPPDrawManager::EFFECT_HGRADIENT);
	m_tooltip.SetBorder(::CreateSolidBrush(RGB(212,175,55)),1,1);
	m_tooltip.SetTransparency(250);

	return TRUE;
}

BOOL CTargetWnd::PreTranslateMessage(MSG* pMsg) 
{
	m_tooltip.RelayEvent(pMsg);

	return CWnd::PreTranslateMessage(pMsg);
}

void CTargetWnd::NotifyDisplayTooltip(NMHDR * pNMHDR, LRESULT * result)
{
    *result = 0;
    NM_PPTOOLTIP_DISPLAY * pNotify = (NM_PPTOOLTIP_DISPLAY*)pNMHDR;
	CString valueStr;
    
	if( m_pRefColor && (*m_pRefColor).isValid())
		valueStr.Format("<b>delta x</b>: %.1f%% <br><b>delta y</b>: %.1f%%",m_deltax*100.0,m_deltay*100.0);
	else
		valueStr="No data";
    pNotify->ti->sTooltip = valueStr;
	pTooltipText = & pNotify->ti->sTooltip;
}

BOOL CTargetWnd::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;	
}

void CTargetWnd::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CTargetWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CTRL_TARGET, NULL );
}

