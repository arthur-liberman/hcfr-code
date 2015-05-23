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
//	Fran�ois-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// Generator.cpp: implementation of the CGenerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "Generator.h"
#include "Color.h"
#include "madTPG.h"
#include "GDIGenerator.h"
#include "../libnum/numsup.h"
#include "../libconv/conv.h"
#include "../libccast/ccmdns.h"
#include "../libdisp/ccwin.h"
#include "../libccast/ccast.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CGenerator, CObject, 1) ;

CGenerator::CGenerator()
{
	m_isModified=FALSE;
	m_doScreenBlanking=GetConfig()->GetProfileInt("Generator","Blanking",0);
	AddPropertyPage(&m_GeneratorPropertiePage);

	CString str;
	str.LoadString(IDS_GENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str);

	SetName("Not defined");  // Needs to be set for real generators

	m_blankingWindow.m_bDisableCursorHiding = TRUE;
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
}

void CGenerator::Serialize(CArchive& archive)
{
	CObject::Serialize(archive) ;
	if (archive.IsStoring())
	{
		int version=3;
		archive << version;
		archive << m_doScreenBlanking;
		archive << m_b16_235;
		archive << m_busePic;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 3 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_doScreenBlanking;
		if ( version > 1 )
			archive >> m_b16_235;
		if ( version > 2 )
			archive >> m_busePic;
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

BOOL CGenerator::Init(UINT nbMeasure)
{
	nMeasureNumber = nbMeasure; 
	CGDIGenerator Cgen;
	CString str, msg;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	BOOL madVR_Found;
	dispwin *ccwin = NULL;

	if (Cgen.m_nDisplayMode == DISPLAY_ccast && m_name != str)
	{
		ccast_id **ids;
		ids = get_ccids();	
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
				ccwin = new_ccwin(ids[0], 100.0 , 100.0, 0.0, 0.0, 0, 0);
				if (ccwin == NULL) 
				{
					GetColorApp()->InMeasureMessageBox( ids[0]->name, "new_ccwin failed!", MB_ICONERROR);
					free_ccids(ids);
					return -1;
				} 
				else
					GetColorApp()->InMeasureMessageBox( ccwin->description, "ChromeCast Found", MB_ICONINFORMATION);
			}
		}
		ccwin->del(ccwin);
		free_ccids(ids);
	} else if (Cgen.m_nDisplayMode == DISPLAY_madVR && m_name != str)
		{
		if (madVR_IsAvailable())
		{
			int nSettling=GetConfig()->m_isSettling?5:0;
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

BOOL CGenerator::DisplayTV()
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
	}
	if(m_doScreenBlanking)
		m_blankingWindow.Hide();
	return TRUE;
}

BOOL CGenerator::ChangePatternSeries()
{
	return TRUE;
}
