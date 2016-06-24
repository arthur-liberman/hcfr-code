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

// Generator.cpp: implementation of the CGenerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "GDIGenerator.h"
#include "Color.h"
#include "madTPG.h"
#include "Generator.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CGenerator, CObject, 1) ;
dispwin *dw;

CGenerator::CGenerator()
{
	m_isModified=FALSE;
	m_doScreenBlanking=GetConfig()->GetProfileInt("Generator","Blanking",0);
	m_rectSizePercent=GetConfig()->GetProfileInt("GDIGenerator","SizePercent",10);
	AddPropertyPage(&m_GeneratorPropertiePage);

	CString str;
	str.LoadString(IDS_GENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str);

	SetName("Not defined");  // Needs to be set for real generators
	m_blankingWindow.m_bDisableCursorHiding = TRUE;
	ccwin = dw;
}

CGenerator::~CGenerator()
{

}
void CGenerator::Copy(CGenerator * p)
{
	m_doScreenBlanking = p->m_doScreenBlanking;
	m_name = p->m_name;
	m_b16_235 = p->m_b16_235;
	m_busePic = p->m_busePic;
	m_bdispTrip = p->m_bdispTrip;
	m_bLinear = p->m_bLinear;
}

void CGenerator::Serialize(CArchive& archive)
{
	CObject::Serialize(archive) ;
	if (archive.IsStoring())
	{
		int version=5;
		archive << version;
		archive << m_doScreenBlanking;
		archive << m_b16_235;
		archive << m_busePic;
		archive << m_bdispTrip;
		archive << m_bLinear;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 5 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_doScreenBlanking;
		if ( version > 1 )
			archive >> m_b16_235;
		if ( version > 2 )
			archive >> m_busePic;
		if ( version > 3 )
			archive >> m_bdispTrip;
		if ( version > 4 )
			archive >> m_bLinear;
	}
}

void CGenerator::SetPropertiesSheetValues()
{
	m_GeneratorPropertiePage.m_doScreenBlanking=m_doScreenBlanking;
}

void CGenerator::GetPropertiesSheetValues()
{
	if( m_doScreenBlanking != m_GeneratorPropertiePage.m_doScreenBlanking )
	{
		m_doScreenBlanking=m_GeneratorPropertiePage.m_doScreenBlanking;
		GetConfig()->WriteProfileInt("Generator","Blanking",m_doScreenBlanking);
		SetModifiedFlag(TRUE);
	}
}

void CGenerator::AddPropertyPage(CPropertyPageWithHelp *apPage)
{
	m_propertySheet.AddPage(apPage);
}


BOOL CGenerator::Configure()
{
	SetPropertiesSheetValues();
	m_propertySheet.SetActivePage(1);
	int result=m_propertySheet.DoModal();
	if(result == IDOK)
		GetPropertiesSheetValues();

	return result==IDOK;
}

BOOL CGenerator::Init(UINT nbMeasure, bool isSpecial)
{
	nMeasureNumber = nbMeasure; 
	CGDIGenerator Cgen;
	CString str, msg;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	BOOL madVR_Found;
	
	if (m_name != str)
	{
		if (Cgen.m_nDisplayMode == DISPLAY_ccast)
		{
			ccast_id **ids;
			if ((ids = get_ccids()) == NULL) 
			{
				GetColorApp()->InMeasureMessageBox( "    ** Error discovering ChromeCasts **", "Error", MB_ICONERROR);
				return false;
			} else 
			{
				if (ids[0] == NULL)
				{
					GetColorApp()->InMeasureMessageBox( "    ** No ChromeCasts found **", "Error", MB_ICONERROR);
					return false;
				}
				else 
				{
					double rx = sqrt( double( (double)Cgen.m_rectSizePercent / 100.));
					dw = new_ccwin(ids[0], 1000.0 * rx  , 565.0 * rx, 0.0, 0.0, 0, 0.1234);
					if (dw == NULL) 
					{
						GetColorApp()->InMeasureMessageBox( ids[0]->name, "new_ccwin failed!", MB_ICONERROR);
						free_ccids(ids);
						return -1;
					} 
					ccwin = dw;				
				}
			}
			free_ccids(ids);
		} else if (Cgen.m_nDisplayMode == DISPLAY_madVR)
		{
			if (madVR_IsAvailable())
			{
				int nSettling=GetConfig()->m_isSettling?26:0;
				madVR_Found = madVR_Connect(CM_ConnectToLocalInstance, CM_ConnectToLanInstance, CM_StartLocalInstance  );
				if (m_madVR_vLUT)
					madVR_SetDeviceGammaRamp(NULL);
				if (m_madVR_3d)
					madVR_Disable3dlut();
				if (m_madVR_OSD)
					madVR_ShowProgressBar(nMeasureNumber + nSettling);
			}
			else
			{
				GetColorApp()->InMeasureMessageBox( "madVR dll not found, is madVR installed?", "Error", MB_ICONERROR);
				return false;
			}
		} else
		{
			CGenerator *	m_pGenerator;
			CGenerator *	 t1;			
			m_pGenerator=new CGDIGenerator();
			t1 = m_pGenerator;
		}
	}

	if(m_doScreenBlanking)
	{
		m_blankingWindow.DisplayRGBColor(ColorRGBDisplay(0.0), TRUE);	// show black screen
		Sleep(50);
		m_blankingWindow.DisplayRGBColor(ColorRGBDisplay(0.0), TRUE);	// show black screen again to be sure task bar is hidden
	}
	
	GetColorApp() -> BeginMeasureCursor ();

	return TRUE;
}

BOOL CGenerator::DisplayRGBColormadVR(const ColorRGBDisplay& aRGBColor )
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRGBCCast(const ColorRGBDisplay& aRGBColor )
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRGBColor(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo,  BOOL bChangePattern,BOOL bSilentMode)
{
	return TRUE;	  // need to be overriden
}


BOOL CGenerator::DisplayAnsiBWRects(BOOL bInvert)
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAnimatedBlack()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAnimatedWhite()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGradient()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRG()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayRB()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayGB()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayRGd()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayRBd()
{
	return TRUE;	  // need to be overriden
}
BOOL CGenerator::DisplayGBd()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGradient2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayLramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayGranger()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::Display80()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTV()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTV2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySpectrum()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayVSMPTE()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayEramp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC0()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC1()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC3()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC4()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTC5()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayBN()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR0()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR1()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayDR2()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayAlign()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplaySharp()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipH()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayClipL()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayTestimg()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayISO12233()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayBBCHD()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayCROSSl()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayCROSSd()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayPM5644()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::DisplayZONE()
{
	return TRUE;	  // need to be overriden
}

BOOL CGenerator::CanDisplayAnsiBWRects()
{
	return FALSE;	  // need to be overriden if display AnsiBWRects is implemented
}

BOOL CGenerator::DisplayGray(double aLevel, MeasureType nPatternType , BOOL bChangePattern)
{
	return DisplayRGBColor(ColorRGBDisplay(aLevel), nPatternType ,bChangePattern); 
}


BOOL CGenerator::Release(INT nbNext)
{
	CGDIGenerator Cgen;
	if (Cgen.m_nDisplayMode == DISPLAY_madVR)
	{
	  if (madVR_IsAvailable())
	    madVR_Disconnect();
	} else if (Cgen.m_nDisplayMode == DISPLAY_ccast)
		dw->del(dw);

	if(m_doScreenBlanking)
		m_blankingWindow.Hide();

	return TRUE;
}

BOOL CGenerator::ChangePatternSeries()
{
	return TRUE;
}
