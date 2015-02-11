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
/////////////////////////////////////////////////////////////////////////////

// ManualDVDGenerator.cpp: implementation of the CManualDVDGenerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ManualDVDGenerator.h"
#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CManualDVDGenerator,CGenerator,1) ;

CManualDVDGenerator::CManualDVDGenerator()
{
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str);

	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	SetName(str);
}

CManualDVDGenerator::~CManualDVDGenerator()
{

}

void CManualDVDGenerator::Copy(CGenerator * p)
{
	CGenerator::Copy(p);
}

BOOL CManualDVDGenerator::DisplayGray(double aLevel, MeasureType nPatternType,BOOL bChangePattern)
{
	BOOL	bRet;
	CString str, Msg, Title;

	Title.LoadString ( IDS_INFORMATION );

	Msg.LoadString ( IDS_DVDMANPOS1 );
	str.Format(Msg,floor(aLevel+0.5));
	bRet = ( GetColorApp()->InMeasureMessageBox(str,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK );

	if(m_doScreenBlanking)
	{
		m_blankingWindow.BringWindowToTop ();	// refresh black screen
		m_blankingWindow.RedrawWindow ();
	}

	return bRet;
}

BOOL CManualDVDGenerator::DisplayRGBColor( const ColorRGBDisplay& clrIn ,MeasureType nPatternType, UINT nPatternInfo,  BOOL bChangePattern,BOOL bSilentMode)
{
	BOOL	bRet;
	CString Msg, str1,str2,str3, Title;
    COLORREF clr(clrIn.GetColorRef(false));
char*  PatName[96]={
                    "White",
                    "6J",
                    "5F",
                    "6I",
                    "6K",
                    "5G",
                    "6H",
                    "5H",
                    "7K",
                    "6G",
                    "5I",
                    "6F",
                    "8K",
                    "5J",
                    "Black",
                    "2B",
                    "2C",
                    "2D",
                    "2E",
                    "2F",
                    "2G",
                    "2H",
                    "2I",
                    "2J",
                    "2K",
                    "2L",
                    "2M",
                    "3B",
                    "3C",
                    "3D",
                    "3E",
                    "3F",
                    "3G",
                    "3H",
                    "3I",
                    "3J",
                    "3K",
                    "3L",
                    "3M",
                    "4B",
                    "4C",
                    "4D",
                    "4E",
                    "4F",
                    "4G",
                    "4H",
                    "4I",
                    "4J",
                    "4K",
                    "4L",
                    "4M",
                    "5B",
                    "5C",
                    "5D",
                    "5K",
                    "5L",
                    "5M",
                    "6B",
                    "6C",
                    "6D",
                    "6L",
                    "6M",
                    "7B",
                    "7C",
                    "7D",
                    "7E",
                    "7F",
                    "7G",
                    "7H",
                    "7I",
                    "7J",
                    "7L",
                    "7M",
                    "8B",
                    "8C",
                    "8D",
                    "8E",
                    "8F",
                    "8G",
                    "8H",
                    "8I",
                    "8J",
                    "8L",
                    "8M",
                    "9B",
                    "9C",
                    "9D",
                    "9E",
                    "9F",
                    "9G",
                    "9H",
                    "9I",
                    "9J",
                    "9K",
                    "9L",
                    "9M" };
                    char*  PatNameCMS[19]={
						"White",
						"Black",
						"2E",
						"2F",
						"2K",
						"5D",
						"7E",
						"7F",
						"7G",
						"7H",
						"7I",
						"7J",
						"8D",
						"8E",
						"8F",
						"8G",
						"8H",
						"8I",
						"8J" };
                    char*  PatNameCPS[19]={
						"White",
						"D7",
						"D8",
						"E7",
						"E8",
						"F7",
						"F8",
						"G7",
						"G8",
						"H7",
						"H8",
						"I7",
						"I8",
						"J7",
						"J8",
						"CP-Light",
						"CP-Dark",
						"Dark Skin",
						"Light Skin" };

	Title.LoadString ( IDS_INFORMATION );

	str1.LoadString( IDS_DVDMANCHAPSEL );

	switch ( nPatternType )
		{
			case MT_SAT_RED:
				str3.LoadString ( IDS_REDSATPERCENT );
				str2.Format(str3,nPatternInfo);
				break;
			case MT_SAT_GREEN:
				str3.LoadString ( IDS_GREENSATPERCENT );
				str2.Format(str3,nPatternInfo);
				break;
			case MT_SAT_BLUE:
				str3.LoadString ( IDS_BLUESATPERCENT );
				str2.Format(str3,nPatternInfo);
				break;
			case MT_SAT_YELLOW:
				str3.LoadString ( IDS_YELLOWSATPERCENT );
				str2.Format(str3,nPatternInfo);
				break;
			case MT_SAT_CYAN:
				str3.LoadString ( IDS_CYANSATPERCENT );
				str2.Format(str3,nPatternInfo);
				break;
			case MT_SAT_MAGENTA:
				str3.LoadString ( IDS_MAGENTASATPERCENT );
				str2.Format(str3,nPatternInfo);
				break;
			case MT_SAT_CC24_GCD:
				str3.LoadString ( IDS_CC24SATPERCENT );
				switch ( nPatternInfo )
				{
				case 0:
				str2.Format(str3,"Black");
				break;
				case 1:
				str2.Format(str3,"Gray 35");
				break;
				case 2:
				str2.Format(str3,"Gray 50");
				break;
				case 3:
				str2.Format(str3,"Gray 65");
				break;
				case 4:
				str2.Format(str3,"Gray 80");
				break;
				case 5:
				str2.Format(str3,"White");
				break;
				case 6:
				str2.Format(str3,"Dark skin");
				break;
				case 7:
				str2.Format(str3,"Light skin");
				break;
				case 8:
				str2.Format(str3,"Blue sky");
				break;
				case 9:
				str2.Format(str3,"Foliage");
				break;
				case 10:
				str2.Format(str3,"Blue flower");
				break;
				case 11:
				str2.Format(str3,"Bluish green");
				break;
				case 12:
				str2.Format(str3,"Orange");
				break;
				case 13:
				str2.Format(str3,"Purplish blue");
				break;
				case 14:
				str2.Format(str3,"Moderate red");
				break;
				case 15:
				str2.Format(str3,"Purple");
				break;
				case 16:
				str2.Format(str3,"Yellow green");
				break;
				case 17:
				str2.Format(str3,"Orange yellow");
				break;
				case 18:
				str2.Format(str3,"Blue");
				break;
				case 19:
				str2.Format(str3,"Green");
				break;
				case 20:
				str2.Format(str3,"Red");
				break;
				case 21:
				str2.Format(str3,"Yellow");
				break;
				case 22:
				str2.Format(str3,"Magenta");
				break;
				case 23:
				str2.Format(str3,"Cyan");
				break;
				}
				break;
			case MT_SAT_CC24_CMC:
				str3.LoadString ( IDS_CC24SATPERCENT );
				switch ( nPatternInfo )
				{
				case 0:
				str2.Format(str3,"White");
				break;
				case 1:
				str2.Format(str3,"80% Gray");
				break;
				case 2:
				str2.Format(str3,"65% Gray");
				break;
				case 3:
				str2.Format(str3,"50% Gray");
				break;
				case 4:
				str2.Format(str3,"35% Gray");
				break;
				case 5:
				str2.Format(str3,"Black");
				break;
				case 6:
				str2.Format(str3,"Dark skin");
				break;
				case 7:
				str2.Format(str3,"Light skin");
				break;
				case 8:
				str2.Format(str3,"Blue sky");
				break;
				case 9:
				str2.Format(str3,"Foliage");
				break;
				case 10:
				str2.Format(str3,"Blue flower");
				break;
				case 11:
				str2.Format(str3,"Bluish green");
				break;
				case 12:
				str2.Format(str3,"Orange");
				break;
				case 13:
				str2.Format(str3,"Purplish blue");
				break;
				case 14:
				str2.Format(str3,"Moderate red");
				break;
				case 15:
				str2.Format(str3,"Purple");
				break;
				case 16:
				str2.Format(str3,"Yellow green");
				break;
				case 17:
				str2.Format(str3,"Orange yellow");
				break;
				case 18:
				str2.Format(str3,"Blue");
				break;
				case 19:
				str2.Format(str3,"Green");
				break;
				case 20:
				str2.Format(str3,"Red");
				break;
				case 21:
				str2.Format(str3,"Yellow");
				break;
				case 22:
				str2.Format(str3,"Magenta");
				break;
				case 23:
				str2.Format(str3,"Cyan");
				break;
				}
				break;
			case MT_SAT_CC24_CCSG:
				str3.LoadString ( IDS_CC24SATPERCENT );
                str2.Format(str3, PatName[nPatternInfo]);
                break;
			case MT_SAT_CC24_CMS:
				str3.LoadString ( IDS_CC24SATPERCENT );
                str2.Format(str3, PatNameCMS[nPatternInfo]);
                break;
			case MT_SAT_CC24_CPS:
				str3.LoadString ( IDS_CC24SATPERCENT );
                str2.Format(str3, PatNameCPS[nPatternInfo]);
                break;
			case MT_SAT_CC24_USER:
				str3.LoadString ( IDS_CC24SATPERCENT );
                str2.Format(str3, PatName[nPatternInfo]);
                break;
			case MT_SAT_CC24_MCD:
				str3.LoadString ( IDS_CC24SATPERCENT );
				switch ( nPatternInfo )
				{
				case 23:
				str2.Format(str3,"Black");
				break;
				case 22:
				str2.Format(str3,"Gray 35");
				break;
				case 21:
				str2.Format(str3,"Gray 50");
				break;
				case 20:
				str2.Format(str3,"Gray 65");
				break;
				case 19:
				str2.Format(str3,"Gray 80");
				break;
				case 18:
				str2.Format(str3,"White");
				break;
				case 0:
				str2.Format(str3,"Dark skin");
				break;
				case 1:
				str2.Format(str3,"Light skin");
				break;
				case 2:
				str2.Format(str3,"Blue sky");
				break;
				case 3:
				str2.Format(str3,"Foliage");
				break;
				case 4:
				str2.Format(str3,"Blue flower");
				break;
				case 5:
				str2.Format(str3,"Bluish green");
				break;
				case 6:
				str2.Format(str3,"Orange");
				break;
				case 7:
				str2.Format(str3,"Purplish blue");
				break;
				case 8:
				str2.Format(str3,"Moderate red");
				break;
				case 9:
				str2.Format(str3,"Purple");
				break;
				case 10:
				str2.Format(str3,"Yellow green");
				break;
				case 11:
				str2.Format(str3,"Orange yellow");
				break;
				case 12:
				str2.Format(str3,"Blue");
				break;
				case 13:
				str2.Format(str3,"Green");
				break;
				case 14:
				str2.Format(str3,"Red");
				break;
				case 15:
				str2.Format(str3,"Yellow");
				break;
				case 16:
				str2.Format(str3,"Magenta");
				break;
				case 17:
				str2.Format(str3,"Cyan");
				break;
				}
				break;
			case MT_PRIMARY:
			case MT_SECONDARY:
			  if ( (GetColorReference().m_standard!=CC6))
			  {
				if(GetRValue(clr) == 0 && GetGValue(clr) == 0 && GetBValue(clr) == 0)
					str2.LoadString ( IDS_BLACK );
    			else if(GetRValue(clr) == GetGValue(clr) && GetBValue(clr) == GetGValue(clr))
					str2.LoadString ( IDS_WHITE );
				else if((GetRValue(clr) > GetGValue(clr)) && (GetRValue(clr) > GetBValue(clr)))
					str2.LoadString ( IDS_REDPRIMARY );
				else if((GetGValue(clr) > GetRValue(clr)) && (GetGValue(clr) > GetBValue(clr)))
					str2.LoadString ( IDS_GREENPRIMARY );
				else if((GetBValue(clr) > GetRValue(clr)) && (GetBValue(clr) > GetGValue(clr)))
					str2.LoadString ( IDS_BLUEPRIMARY );
				else if((GetRValue(clr) == GetGValue(clr)) && (GetRValue(clr) > GetBValue(clr)))
					str2.LoadString ( IDS_YELLOWSECONDARY );
				else if((GetGValue(clr) == GetBValue(clr)) && (GetGValue(clr) > GetRValue(clr)))
					str2.LoadString ( IDS_CYANSECONDARY );
				else if((GetRValue(clr) == GetBValue(clr)) && (GetRValue(clr) > GetGValue(clr)))
					str2.LoadString ( IDS_MAGENTASECONDARY );
				break;
			  }
			  else
			  {
				if(GetRValue(clr) == 0 && GetGValue(clr) == 0 && GetBValue(clr) == 0)
					str2.LoadString ( IDS_BLACK );
				else if(GetRValue(clr) == GetGValue(clr) && GetGValue(clr) == GetBValue(clr))
					str2.LoadString ( IDS_WHITE );
				else if(GetRValue(clr) > 180 && GetBValue(clr) > 120)
					str2.LoadString ( IDS_CC6REDPRIMARY );
				else if(GetRValue(clr) < 100 && GetBValue(clr) > 150)
					str2.LoadString ( IDS_CC6GREENPRIMARY );
				else if(GetRValue(clr) < 95 && GetBValue(clr) < 70)
					str2.LoadString ( IDS_CC6BLUEPRIMARY );
				else if(GetRValue(clr) > 120 && GetBValue(clr) > 170)
					str2.LoadString ( IDS_CC6YELLOWSECONDARY );
				else if(GetGValue(clr) > 180)
					str2.LoadString ( IDS_CC6CYANSECONDARY );
				else if(GetRValue(clr) > 220 && GetBValue(clr) < 50)
					str2.LoadString ( IDS_CC6MAGENTASECONDARY );
				break;
			  }
			default:
				if(GetRValue(clr) == 255 && GetGValue(clr) == 0 && GetBValue(clr) == 0)
					str2.LoadString ( IDS_REDPRIMARY );
				else if(GetRValue(clr) == 0 && GetGValue(clr) == 255 && GetBValue(clr) == 0)
					str2.LoadString ( IDS_GREENPRIMARY );
				else if(GetRValue(clr) == 0 && GetGValue(clr) == 0 && GetBValue(clr) == 255)
					str2.LoadString ( IDS_BLUEPRIMARY );
				else if(GetRValue(clr) == 255 && GetGValue(clr) == 255 && GetBValue(clr) == 0)
					str2.LoadString ( IDS_YELLOWSECONDARY );
				else if(GetRValue(clr) == 0 && GetGValue(clr) == 255 && GetBValue(clr) == 255)
					str2.LoadString ( IDS_CYANSECONDARY );
				else if(GetRValue(clr) == 255 && GetGValue(clr) == 0 && GetBValue(clr) == 255)
					str2.LoadString ( IDS_MAGENTASECONDARY );
				else 
				{
					str1.LoadString ( IDS_DVDMANPOS8 );
					str2 = "";
				}
		}			
		
	Msg.Format(str1,str2);

	if ( nPatternType == MT_ACTUAL )
		bRet = TRUE;
	else
		bRet = ( GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK );
//		bRet = ( MessageBox(NULL,Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK );

	if(m_doScreenBlanking)
	{
		m_blankingWindow.BringWindowToTop ();	// refresh black screen
		m_blankingWindow.RedrawWindow ();
	}

	return bRet;
}

BOOL CManualDVDGenerator::DisplayAnsiBWRects(BOOL bInvert)
{
	BOOL	bRet;
	CString str, Title;

	Title.LoadString ( IDS_INFORMATION );

	if(!bInvert)
		str.LoadString ( IDS_DVDMANPOS9 );
	else
		str.LoadString ( IDS_DVDMANPOS10 );

	bRet = ( GetColorApp()->InMeasureMessageBox(str,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK );

	if(m_doScreenBlanking)
	{
		m_blankingWindow.BringWindowToTop ();	// refresh black screen
		m_blankingWindow.RedrawWindow ();
	}

	return bRet;
}

BOOL CManualDVDGenerator::CanDisplayAnsiBWRects()
{
	return TRUE;	  
}

