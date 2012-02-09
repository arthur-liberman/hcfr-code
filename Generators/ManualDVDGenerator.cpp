/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
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

BOOL CManualDVDGenerator::DisplayGray(double aLevel, BOOL bIRE, MeasureType nPatternType,BOOL bChangePattern)
{
	BOOL	bRet;
	CString str, Msg, Title;

	Title.LoadString ( IDS_INFORMATION );

	Msg.LoadString ( IDS_DVDMANPOS1 );
	str.Format(Msg,aLevel);
	bRet = ( MessageBox(NULL,str,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK );

	if(m_doScreenBlanking)
	{
		m_blankingWindow.BringWindowToTop ();	// refresh black screen
		m_blankingWindow.RedrawWindow ();
	}

	return bRet;
}

BOOL CManualDVDGenerator::DisplayRGBColor( COLORREF clr ,MeasureType nPatternType, UINT nPatternInfo,  BOOL bChangePattern,BOOL bSilentMode)
{
	BOOL	bRet;
	CString Msg, str1,str2,str3, Title;

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
			case MT_PRIMARY:
			case MT_SECONDARY:
				if((GetRValue(clr) > GetGValue(clr)) && (GetRValue(clr) > GetBValue(clr)))
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
				else if(GetRValue(clr) == 255 && GetGValue(clr) == 255 && GetBValue(clr) == 255)
					str2.LoadString ( IDS_WHITE );
				else if(GetRValue(clr) == 0 && GetGValue(clr) == 0 && GetBValue(clr) == 0)
					str2.LoadString ( IDS_BLACK );
				break;
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
		bRet = ( MessageBox(NULL,Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK );

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

	bRet = ( MessageBox(NULL,str,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK );

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

