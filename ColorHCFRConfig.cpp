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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// ColorHCFRConfig.cpp: implementation of the CColorHCFRConfig class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ColorHCFRConfig.h"
#include "DataSetDoc.h"
#include "DocEnumerator.h"
#include "LangSelection.h"
#include "HtmlHelp.h"
#include <io.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// This define must be the same than in MainFrm.cpp
#define LANG_PREFIX "CHCFR21_"
#define HELP_PREFIX "ColorHCFR_"

static struct
{ LPCSTR	lpszLangCode;
  LPCSTR	lpszHelpName;
} g_HelpFileNames [] = { 
						{ "en", "English help" },
						{ "fr", "Aide en Français" },
						{ "de", "Hilfe in Deutsch" },
						{ "es", "Ayuda en español" }
					   };


CColorHCFRConfig::CColorHCFRConfig()
{
	char			szBuf [ 256 ];
	CString			str, strDLL, strLang, strPath, strSearch, strHelp;
	LPSTR			lpStr;
	HANDLE			hFind;
	WIN32_FIND_DATA	wfd;
	CLangSelection	dlg;

	// Retrieve module path and name
	GetModuleFileName ( NULL, m_ApplicationPath, sizeof ( m_ApplicationPath ) );
	strcpy ( m_iniFileName, m_ApplicationPath );
	strcpy ( m_chmFileName, m_ApplicationPath );
	strcpy ( m_logFileName, m_ApplicationPath );

	lpStr = strrchr ( m_ApplicationPath, (int) '\\' );
	lpStr [ 1 ] = '\0';

	// Build ini file name (replace .exe extention with .ini)
	lpStr = strrchr ( m_iniFileName, (int) '.' );
	strcpy ( lpStr + 1, "ini" );
	
	// Build chm file name (replace .exe extention with .chm)
	lpStr = strrchr ( m_chmFileName, (int) '.' );
	strcpy ( lpStr + 1, "chm" );
	
	// Build log file name
	lpStr = strrchr ( m_logFileName, (int) '\\' );
	strcpy ( lpStr + 1, "decoder.log" );

	// Initialize language DLL, which is definitively loaded
	strLang = GetProfileString ( "Options", "Language", "" );
	
	strPath = m_ApplicationPath;
	
	strSearch = strPath + LANG_PREFIX + "*.DLL";
		
	hFind = FindFirstFile ( (LPCSTR) strSearch, & wfd );
	if ( hFind != INVALID_HANDLE_VALUE )
	{
		do
		{
			strcpy ( szBuf, wfd.cFileName + strlen ( LANG_PREFIX ) );
			lpStr = strrchr ( szBuf, '.' );
			if ( lpStr )
				lpStr [ 0 ] = '\0';
			
			if ( _stricmp ( szBuf, "PATTERNS" ) != 0 )
				dlg.m_Languages.AddTail ( szBuf );
		} while ( FindNextFile ( hFind, & wfd ) );

		FindClose ( hFind );
	}
	
	switch ( dlg.m_Languages.GetCount () )
	{
		case 0:
			 // Use default
			 strLang = "ENGLISH";
			 break;

		case 1:
			 strLang = dlg.m_Languages.GetHead ();
			 break;

		default:
			 if ( strLang.IsEmpty () || ! dlg.m_Languages.Find ( strLang ) )
			 {
				dlg.DoModal ();
				strLang = dlg.m_Selection;
				WriteProfileString ( "Options", "Language", strLang );
			 }
			 break;
	}
	
	strDLL = LANG_PREFIX+strLang+".DLL";
	HMODULE hResDll = LoadLibrary ( strDLL );
	AfxSetResourceHandle ( hResDll );

	strHelp.LoadString ( IDS_HELPFILE_NAME );
	strcpy ( m_chmFileName, m_ApplicationPath );
	strcat ( m_chmFileName, (LPCSTR) strHelp );

	if ( _access( m_chmFileName, 0 ) == -1 )
	{
		// Help file not found
		str = GetProfileString ( "HelpFile", (LPCSTR) strHelp, "" );

		if ( ! str.IsEmpty () )
		{
			strcpy ( m_chmFileName, m_ApplicationPath );
			strcat ( m_chmFileName, (LPCSTR) str );
		}
		else
		{
			// Manual Help language selection
			int				i;
			CLangSelection	dlgHelp;
			
			strPath = m_ApplicationPath;
			
			strSearch = strPath + HELP_PREFIX + "*.chm";
				
			hFind = FindFirstFile ( (LPCSTR) strSearch, & wfd );
			if ( hFind != INVALID_HANDLE_VALUE )
			{
				do
				{
					strcpy ( szBuf, wfd.cFileName + strlen ( HELP_PREFIX ) );
					lpStr = strrchr ( szBuf, '.' );
					if ( lpStr )
						lpStr [ 0 ] = '\0';
					
					// Translate language suffixes into readable names
					for ( i = 0 ; i < sizeof ( g_HelpFileNames ) / sizeof ( g_HelpFileNames [ 0 ] ) ; i ++ )
					{
						if ( _stricmp ( szBuf, g_HelpFileNames[i].lpszLangCode ) == 0 )
						{
							dlgHelp.m_Languages.AddTail ( g_HelpFileNames[i].lpszHelpName );
							break;
						}
					}

				} while ( FindNextFile ( hFind, & wfd ) );

				FindClose ( hFind );
			}
			
			if ( dlgHelp.m_Languages.GetCount () > 0 )
			{
				if ( IDOK == dlgHelp.DoModal () )
				{
					str = dlgHelp.m_Selection;

					for ( i = 0 ; i < sizeof ( g_HelpFileNames ) / sizeof ( g_HelpFileNames [ 0 ] ) ; i ++ )
					{
						if ( _stricmp ( (LPCSTR) str, g_HelpFileNames[i].lpszHelpName ) == 0 )
						{
							str = HELP_PREFIX;
							str += g_HelpFileNames[i].lpszLangCode;
							str += ".chm";
							
							WriteProfileString ( "HelpFile", (LPCSTR) strHelp, (LPCSTR) str );

							strcpy ( m_chmFileName, m_ApplicationPath );
							strcat ( m_chmFileName, (LPCSTR) str );
							break;
						}
					}
				}
			}
		}
	}

	// Set global object instance handles (they already call AfxGetResourceHandle before EXE hInstance was replaced)
	m_generalPropertiesPage.m_psp.hInstance = hResDll;
	m_appearancePropertiesPage.m_psp.hInstance = hResDll;
	m_referencesPropertiesPage.m_psp.hInstance = hResDll;
	m_advancedPropertiesPage.m_psp.hInstance = hResDll;
	m_toolbarPropertiesPage.m_psp.hInstance = hResDll;

	str.LoadString(IDS_SETTINGS_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str);
	m_propertySheet.AddPage(&m_generalPropertiesPage);
	m_propertySheet.AddPage(&m_referencesPropertiesPage);
	m_propertySheet.AddPage(&m_appearancePropertiesPage);
	m_propertySheet.AddPage(&m_toolbarPropertiesPage);
	m_propertySheet.AddPage(&m_advancedPropertiesPage);

	InitDefaults();
	LoadSettings();
	ApplySettings(TRUE);

	InitializeCriticalSection ( & LogFileCritSec );
}

CColorHCFRConfig::~CColorHCFRConfig()
{
	DeleteCriticalSection ( & LogFileCritSec );
	SaveSettings();
}

void CColorHCFRConfig::InitDefaults()
{
	m_colorStandard=SDTV;
	m_whiteTarget=D65;
	m_bDisplayTestColors=TRUE;
	m_bContinuousMeasures=TRUE;
	m_bDetectPrimaries=FALSE;
	m_latencyTime=0;
	m_bLatencyBeep=FALSE;
	m_bSatUseMeasuredRef=TRUE;
	m_BWColorsToAdd=1;
	m_GammaRef=2.22;
	m_GammaOffsetType=1;
	m_manualGOffset=0.099;

		// Set drawing style of all menus to XP-Mode
	CNewMenu::SetAcceleratorsDraw(TRUE);

	m_menuDrawMode=CNewMenu::STYLE_XP_2003;
	m_drawMenuBorder=TRUE;
	m_useCustomColor=FALSE;
	m_doSelectDisabledItem=FALSE;
	m_doXpBlending=FALSE;
	m_doGlooming=FALSE;
	m_bWhiteBkgndOnScreen = FALSE;
	m_bWhiteBkgndOnFile = TRUE;

	m_fxColorWindow=CLR_DEFAULT;
	m_fxColorMenu=CLR_DEFAULT;
	m_fxColorSelection=CLR_DEFAULT;
	m_fxColorText=CLR_DEFAULT;

	m_doMultipleInstance=FALSE;
	m_doSavePosition=TRUE;

	m_PercentGray.LoadString ( IDS_PERCENTGRAY );
	
	m_TBViewsRightClickMode = 0;
	m_TBViewsMiddleClickMode = 1;
	m_bConfirmMeasures = TRUE;
	m_bUseCalibrationFilesOnAllProbes = FALSE;
	m_bControlledMode = FALSE;
	m_bUseOnlyPrimaries = FALSE;
	m_bUseImperialUnits = FALSE;
	m_nLuminanceCurveMode = 0;
	m_bPreferLuxmeter = FALSE;
	m_bUseOldDeltaEFormula = FALSE;
	m_bUseDeltaELumaOnGrays = FALSE;
}

BOOL CColorHCFRConfig::LoadSettings()
{
	m_doMultipleInstance=GetProfileInt("General","DoMultipleInstance",FALSE);
	m_doSavePosition=GetProfileInt("General","DoSavePosition",TRUE);
	m_BWColorsToAdd=GetProfileInt("General","BWColorsToAdd",1);
	if ( m_BWColorsToAdd < 0 ) 
		m_BWColorsToAdd = 0;
	else if ( m_BWColorsToAdd > 2 ) 
		m_BWColorsToAdd = 2;

	m_colorStandard=(ColorStandard)GetProfileInt("References","ColorStandard",SDTV);
	m_whiteTarget=(WhiteTarget)GetProfileInt("References","WhiteTarget",D65);
	m_bDisplayTestColors=GetProfileInt("References","DisplayTestColors",1);
	m_bContinuousMeasures=GetProfileInt("References","ContinuousMeasures",1);
	m_bDetectPrimaries=GetProfileInt("References","DetectPrimaries",0);
	m_latencyTime=GetProfileInt("References","IrisLatencyTime",0);
	m_bLatencyBeep=GetProfileInt("References","IrisLatencyBeep",0);
	m_bSatUseMeasuredRef=GetProfileInt("References","SatUseMeasuredRef",1);
	m_GammaRef=GetProfileDouble("References","GammaRefValue",2.22);

	m_GammaOffsetType=GetProfileInt("References","GammaOffsetType",1);
	m_manualGOffset=GetProfileDouble("References","ManualGamOffset",0.099);

	if (m_manualGOffset == 0) {
		m_manualGOffset = 0.099;
		if(m_colorStandard == sRGB)
			m_manualGOffset = 0.055;
	}

	m_menuDrawMode=GetProfileInt("Appearance","DrawMode",CNewMenu::STYLE_XP_2003);
	m_drawMenuBorder=GetProfileInt("Appearance","DrawMemuBorder",TRUE);
	m_doSelectDisabledItem=GetProfileInt("Appearance","SelectDisabledItem",FALSE);
	m_doXpBlending=GetProfileInt("Appearance","DoXpBlending",FALSE);
	m_doGlooming=GetProfileInt("Appearance","DoGlooming",FALSE);
	m_bWhiteBkgndOnScreen=GetProfileInt("Appearance","WhiteBkgndOnScreen",FALSE);
	m_bWhiteBkgndOnFile=GetProfileInt("Appearance","WhiteBkgndOnFile",TRUE);
	m_useCustomColor=GetProfileInt("Appearance","UseCustomColor",FALSE);
	m_fxColorWindow=GetProfileColor("Appearance","Window Color");
	m_fxColorMenu=GetProfileColor("Appearance","Menu Color");
	m_fxColorSelection=GetProfileColor("Appearance","Selection Color");
	m_fxColorText=GetProfileColor("Appearance","Text Color");

	m_TBViewsRightClickMode = GetProfileInt("Advanced","TBViewsRightClickMode",0);
	m_TBViewsMiddleClickMode = GetProfileInt("Advanced","TBViewsMiddleClickMode",1);
	m_bConfirmMeasures = GetProfileInt("Advanced","ConfirmMeasures",1);
	m_bUseCalibrationFilesOnAllProbes = GetProfileInt("Advanced","UseCalibrationFilesOnAllProbes",0);
	m_bControlledMode = GetProfileInt("Advanced","ControlledMode",0);
	m_bUseOnlyPrimaries = GetProfileInt("Advanced","UseOnlyPrimaries",0);
	m_bUseImperialUnits = GetProfileInt("Advanced","UseImperialUnits",0);
	m_nLuminanceCurveMode = GetProfileInt("Advanced","LuminanceCurveMode",0);
	m_bPreferLuxmeter = GetProfileInt("Advanced","PreferLuxmeter",0);
	m_bUseOldDeltaEFormula = GetProfileInt("Advanced","UseOldDeltaEFormula",0);
	m_bUseDeltaELumaOnGrays = GetProfileInt("Advanced","UseDeltaELumaOnGrays",0);


	return TRUE;
}

void CColorHCFRConfig::SaveSettings()
{
	WriteProfileInt("General","DoMultipleInstance",m_doMultipleInstance);
	WriteProfileInt("General","DoSavePosition",m_doSavePosition);
	WriteProfileInt("General","BWColorsToAdd",m_BWColorsToAdd);

	WriteProfileInt("References","GammaOffsetType",m_GammaOffsetType);
	WriteProfileDouble("References","ManualGamOffset",m_manualGOffset);
	WriteProfileInt("References","ColorStandard",m_colorStandard);
	WriteProfileInt("References","WhiteTarget",m_whiteTarget);
	WriteProfileInt("References","DisplayTestColors",m_bDisplayTestColors);
	WriteProfileInt("References","ContinuousMeasures",m_bContinuousMeasures);
	WriteProfileInt("References","DetectPrimaries",m_bDetectPrimaries);
	WriteProfileInt("References","IrisLatencyTime",m_latencyTime);
	WriteProfileInt("References","IrisLatencyBeep",m_bLatencyBeep);
	WriteProfileInt("References","SatUseMeasuredRef",m_bSatUseMeasuredRef);
	WriteProfileDouble("References","GammaRefValue",m_GammaRef);

	WriteProfileInt("Appearance","DrawMode",m_menuDrawMode);
	WriteProfileInt("Appearance","DrawMemuBorder",m_drawMenuBorder);
	WriteProfileInt("Appearance","SelectDisabledItem",m_doSelectDisabledItem);
	WriteProfileInt("Appearance","DoXpBlending",m_doXpBlending);
	WriteProfileInt("Appearance","DoGlooming",m_doGlooming);
	WriteProfileInt("Appearance","WhiteBkgndOnScreen",m_bWhiteBkgndOnScreen);
	WriteProfileInt("Appearance","WhiteBkgndOnFile",m_bWhiteBkgndOnFile);

	WriteProfileInt("Appearance","UseCustomColor",m_useCustomColor);
	WriteProfileColor("Appearance","Window Color",m_fxColorWindow);
	WriteProfileColor("Appearance","Menu Color",m_fxColorMenu);
	WriteProfileColor("Appearance","Selection Color",m_fxColorSelection);
	WriteProfileColor("Appearance","Text Color",m_fxColorText);

	WriteProfileInt("Advanced","TBViewsRightClickMode",m_TBViewsRightClickMode);
	WriteProfileInt("Advanced","TBViewsMiddleClickMode",m_TBViewsMiddleClickMode);
	WriteProfileInt("Advanced","ConfirmMeasures",m_bConfirmMeasures);
	WriteProfileInt("Advanced","UseCalibrationFilesOnAllProbes",m_bUseCalibrationFilesOnAllProbes);
	WriteProfileInt("Advanced","ControlledMode",m_bControlledMode);
	WriteProfileInt("Advanced","UseOnlyPrimaries",m_bUseOnlyPrimaries);
	WriteProfileInt("Advanced","UseImperialUnits",m_bUseImperialUnits);
	WriteProfileInt("Advanced","LuminanceCurveMode",m_nLuminanceCurveMode);
	WriteProfileInt("Advanced","PreferLuxmeter",m_bPreferLuxmeter);
	WriteProfileInt("Advanced","UseOldDeltaEFormula",m_bUseOldDeltaEFormula);
	WriteProfileInt("Advanced","UseDeltaELumaOnGrays",m_bUseDeltaELumaOnGrays);
}

void CColorHCFRConfig::ChangeSettings(int aPage)
{
	SetPropertiesSheetValues();
	m_propertySheet.SetActivePage(aPage);
	m_propertySheet.DoModal();
}

void CColorHCFRConfig::SetPropertiesSheetValues()
{
	m_generalPropertiesPage.m_doMultipleInstance=m_doMultipleInstance;
	m_generalPropertiesPage.m_doSavePosition=m_doSavePosition;
	m_generalPropertiesPage.m_BWColorsToAdd=m_BWColorsToAdd;
	
	m_generalPropertiesPage.m_bDisplayTestColors=m_bDisplayTestColors;
	m_generalPropertiesPage.m_bContinuousMeasures=m_bContinuousMeasures;
	m_generalPropertiesPage.m_bDetectPrimaries=m_bDetectPrimaries;
	m_generalPropertiesPage.m_latencyTime=m_latencyTime;
	m_generalPropertiesPage.m_bLatencyBeep=m_bLatencyBeep;
	m_generalPropertiesPage.m_bSatUseMeasuredRef=m_bSatUseMeasuredRef;
	m_generalPropertiesPage.m_isModified=FALSE;

	m_referencesPropertiesPage.m_colorStandard=m_colorStandard;
	m_referencesPropertiesPage.m_whiteTarget=m_whiteTarget;
	m_referencesPropertiesPage.m_GammaRef=m_GammaRef;
	m_referencesPropertiesPage.m_manualGOffset=m_manualGOffset;
	m_referencesPropertiesPage.m_GammaOffsetType=m_GammaOffsetType;
	m_appearancePropertiesPage.m_isModified=FALSE;
	switch(m_menuDrawMode)
	{
		case CNewMenu::STYLE_XP_2003:
			m_appearancePropertiesPage.m_themeComboIndex=0;
			break;
		case CNewMenu::STYLE_XP:
			m_appearancePropertiesPage.m_themeComboIndex=1;
			break;
		case CNewMenu::STYLE_ORIGINAL:
			m_appearancePropertiesPage.m_themeComboIndex=2;
			break;
		case CNewMenu::STYLE_ICY:
			m_appearancePropertiesPage.m_themeComboIndex=3;
			break;
		default:
			m_appearancePropertiesPage.m_themeComboIndex=0;
	}

	m_appearancePropertiesPage.m_drawMenuborder=m_drawMenuBorder;
	m_appearancePropertiesPage.m_doXpBlending=m_doXpBlending;
	m_appearancePropertiesPage.m_doSelectDisabledItem=m_doSelectDisabledItem;
	m_appearancePropertiesPage.m_doGlooming=m_doGlooming;
	m_appearancePropertiesPage.m_useCustomColor=m_useCustomColor;
	m_appearancePropertiesPage.m_colorWindowButton.SetColor(m_fxColorWindow);
	m_appearancePropertiesPage.m_colorMenuButton.SetColor(m_fxColorMenu);
	m_appearancePropertiesPage.m_colorSelectionButton.SetColor(m_fxColorSelection);
	m_appearancePropertiesPage.m_colorTextButton.SetColor(m_fxColorText);
	m_appearancePropertiesPage.m_bWhiteBkgndOnScreen=m_bWhiteBkgndOnScreen;
	m_appearancePropertiesPage.m_bWhiteBkgndOnFile=m_bWhiteBkgndOnFile;

	m_toolbarPropertiesPage.m_TBViewsRightClickMode = m_TBViewsRightClickMode;
	m_toolbarPropertiesPage.m_TBViewsMiddleClickMode = m_TBViewsMiddleClickMode;

	m_advancedPropertiesPage.m_bConfirmMeasures = m_bConfirmMeasures;
	m_advancedPropertiesPage.m_bUseCalibrationFilesOnAllProbes = m_bUseCalibrationFilesOnAllProbes;
	m_advancedPropertiesPage.m_bControlledMode = m_bControlledMode;
	m_advancedPropertiesPage.m_comPort = GetColorApp() -> m_LuxPort;
	m_advancedPropertiesPage.m_bUseOnlyPrimaries = m_bUseOnlyPrimaries;
	m_advancedPropertiesPage.m_bUseImperialUnits = m_bUseImperialUnits;
	m_advancedPropertiesPage.m_nLuminanceCurveMode = m_nLuminanceCurveMode;
	m_advancedPropertiesPage.m_bPreferLuxmeter = m_bPreferLuxmeter;
	m_advancedPropertiesPage.m_bUseOldDeltaEFormula = m_bUseOldDeltaEFormula;
	m_advancedPropertiesPage.m_bUseDeltaELumaOnGrays = m_bUseDeltaELumaOnGrays;

	m_advancedPropertiesPage.m_isModified=FALSE;
}

BOOL CColorHCFRConfig::GetPropertiesSheetValues()
{
	BOOL	needRestart=FALSE;
	CString	strPort;

	if ( _strnicmp ( (LPCSTR) m_advancedPropertiesPage.m_comPort, "COM", 3 ) == 0 )
		strPort = m_advancedPropertiesPage.m_comPort;

	if ( strPort != GetColorApp() -> m_LuxPort )
	{
		GetConfig () -> WriteProfileString ( "Defaults", "Luxmeter", strPort );
		GetColorApp() -> m_LuxPort = strPort;
		GetColorApp() -> StartLuxMeasures ();
	}


	if(	m_doMultipleInstance!=m_generalPropertiesPage.m_doMultipleInstance)
	{
		m_doMultipleInstance=m_generalPropertiesPage.m_doMultipleInstance;
		needRestart=TRUE;
	}
	m_doSavePosition=m_generalPropertiesPage.m_doSavePosition;
	m_BWColorsToAdd=m_generalPropertiesPage.m_BWColorsToAdd;

	m_bDisplayTestColors=m_generalPropertiesPage.m_bDisplayTestColors;
	m_bContinuousMeasures=m_generalPropertiesPage.m_bContinuousMeasures;
	m_bDetectPrimaries=m_generalPropertiesPage.m_bDetectPrimaries;
	m_latencyTime=m_generalPropertiesPage.m_latencyTime;
	m_bLatencyBeep=m_generalPropertiesPage.m_bLatencyBeep;
	m_bSatUseMeasuredRef=m_generalPropertiesPage.m_bSatUseMeasuredRef;

	m_colorStandard=(ColorStandard)(m_referencesPropertiesPage.m_colorStandard);
	m_whiteTarget=(WhiteTarget)m_referencesPropertiesPage.m_whiteTarget;
	m_GammaRef=m_referencesPropertiesPage.m_GammaRef;
	m_manualGOffset=m_referencesPropertiesPage.m_manualGOffset;
	m_GammaOffsetType=m_referencesPropertiesPage.m_GammaOffsetType;
	switch(m_appearancePropertiesPage.m_themeComboIndex)
	{
		case 0:
			m_menuDrawMode=CNewMenu::STYLE_XP_2003;
			break;
		case 1:
			m_menuDrawMode=CNewMenu::STYLE_XP;
			break;
		case 2:
			m_menuDrawMode=CNewMenu::STYLE_ORIGINAL;
			break;
		case 3:
			m_menuDrawMode=CNewMenu::STYLE_ICY;
			break;
		default:
			m_menuDrawMode=CNewMenu::STYLE_XP_2003;
	}

	m_drawMenuBorder=m_appearancePropertiesPage.m_drawMenuborder;
	m_doXpBlending=m_appearancePropertiesPage.m_doXpBlending;
	m_doSelectDisabledItem=m_appearancePropertiesPage.m_doSelectDisabledItem;
	m_doGlooming=m_appearancePropertiesPage.m_doGlooming;
	m_bWhiteBkgndOnScreen=m_appearancePropertiesPage.m_bWhiteBkgndOnScreen;
	m_bWhiteBkgndOnFile=m_appearancePropertiesPage.m_bWhiteBkgndOnFile;
	m_useCustomColor=m_appearancePropertiesPage.m_useCustomColor;
	m_fxColorWindow=m_appearancePropertiesPage.m_colorWindowButton.GetColor();
	m_fxColorMenu=m_appearancePropertiesPage.m_colorMenuButton.GetColor();
	m_fxColorSelection=m_appearancePropertiesPage.m_colorSelectionButton.GetColor();
	m_fxColorText=m_appearancePropertiesPage.m_colorTextButton.GetColor();

	m_TBViewsRightClickMode = m_toolbarPropertiesPage.m_TBViewsRightClickMode;
	m_TBViewsMiddleClickMode = m_toolbarPropertiesPage.m_TBViewsMiddleClickMode;
	m_bConfirmMeasures = m_advancedPropertiesPage.m_bConfirmMeasures;
	m_bUseCalibrationFilesOnAllProbes = m_advancedPropertiesPage.m_bUseCalibrationFilesOnAllProbes;
	m_bControlledMode = m_advancedPropertiesPage.m_bControlledMode;
	m_bUseOnlyPrimaries = m_advancedPropertiesPage.m_bUseOnlyPrimaries;
	m_bUseImperialUnits = m_advancedPropertiesPage.m_bUseImperialUnits;
	m_nLuminanceCurveMode = m_advancedPropertiesPage.m_nLuminanceCurveMode;
	m_bPreferLuxmeter = m_advancedPropertiesPage.m_bPreferLuxmeter;
	m_bUseOldDeltaEFormula = m_advancedPropertiesPage.m_bUseOldDeltaEFormula;
	m_bUseDeltaELumaOnGrays = m_advancedPropertiesPage.m_bUseDeltaELumaOnGrays;

	return needRestart;
}

void CColorHCFRConfig::ApplySettings(BOOL isStartupApply)
{
	ColorStandard OriginalColorStandard = m_colorStandard;
	WhiteTarget OriginalWhiteTarget = m_whiteTarget;

	if(!isStartupApply)
	{
		if( GetPropertiesSheetValues() ) // True if restart needed
		{
			CString	Msg, Title;
			Msg.LoadString ( IDS_MUSTRESTART );
			Title.LoadString ( IDS_SETTINGS_PROPERTIES_TITLE );
			MessageBox(NULL,Msg,Title,MB_ICONINFORMATION | MB_OK);
		}
	}

	// Apply appearance settings
	CNewMenu::SetMenuDrawMode(m_menuDrawMode);
	fxDrawMenuBorder=m_drawMenuBorder;
	fxUseCustomColor=m_useCustomColor;
	CNewMenu::SetSelectDisableMode(m_doSelectDisabledItem);
	CNewMenu::SetXpBlending(m_doXpBlending);
	if(m_doGlooming)
		CNewMenu::SetGloomFactor(50);
	else
		CNewMenu::SetGloomFactor(0);
	SetFxColors(m_fxColorWindow,m_fxColorMenu,m_fxColorSelection,m_fxColorText);

	// Apply reference settings
    *(GetColorApp()->m_pColorReference)=CColorReference(m_colorStandard, m_whiteTarget,-1);
	
	if ( ! isStartupApply )
	{
		if ( OriginalColorStandard != m_colorStandard || OriginalWhiteTarget != m_whiteTarget )
		{
			CWaitCursor	wait;
			GetColorApp() -> CreateCIEBitmaps ();
		}
	}

	if(!isStartupApply)
	{
		if(m_appearancePropertiesPage.m_isModified || m_generalPropertiesPage.m_isModified)	
			AfxGetMainWnd()->SendMessage(WM_SYSCOLORCHANGE); // to update color changes 

		if(m_referencesPropertiesPage.m_isModified || m_advancedPropertiesPage.m_isModified || m_appearancePropertiesPage.m_isWhiteModified)	
		{
			CDocEnumerator docEnumerator;	// Update all views of all docs
			CDocument* pDoc;
			while ((pDoc=docEnumerator.Next())!=NULL) 
			   pDoc->UpdateAllViews(NULL, UPD_GENERALREFERENCES);

			AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	    }
	}
}

BOOL CColorHCFRConfig::IsProfileEntryDefined(LPCTSTR lpszSection,LPCTSTR lpszEntry)
{
	return GetProfileString(lpszSection,lpszEntry,"") != "";

}

CString CColorHCFRConfig::GetProfileString(LPCTSTR lpszSection,LPCTSTR lpszEntry,LPCTSTR lpszDefault)
{
   CHAR   inBuf[2048]; 
   GetPrivateProfileString (lpszSection, 
                            lpszEntry, 
                            lpszDefault, 
                            inBuf, 
                            2048, 
                            m_iniFileName); 
   return CString(inBuf);
}

BOOL CColorHCFRConfig::WriteProfileString(LPCTSTR lpszSection,LPCTSTR lpszEntry,LPCTSTR lpszValue)
{
	return WritePrivateProfileString(lpszSection, 
									 lpszEntry, 
									 lpszValue, 
									 m_iniFileName); 
}


UINT CColorHCFRConfig::GetProfileInt(LPCTSTR lpszSection,LPCTSTR lpszEntry,int nDefault)
{
   CString defaultValueStr;
   CString readStr;

   defaultValueStr.Format("%d",nDefault);
   readStr=GetProfileString(lpszSection, lpszEntry, defaultValueStr);
   return atoi(readStr);
}

BOOL CColorHCFRConfig::WriteProfileInt(LPCTSTR lpszSection,LPCTSTR lpszEntry,int nValue)
{
	CString valueStr;

	valueStr.Format("%d",nValue);
	return WriteProfileString(lpszSection,lpszEntry,valueStr); 
}

double CColorHCFRConfig::GetProfileDouble(LPCTSTR lpszSection,LPCTSTR lpszEntry,double nDefault)
{
   CString defaultValueStr;
   CString readStr;

   defaultValueStr.Format("%g",nDefault);
   readStr=GetProfileString(lpszSection, lpszEntry, defaultValueStr);
   return atof(readStr);
}

BOOL CColorHCFRConfig::WriteProfileDouble(LPCTSTR lpszSection,LPCTSTR lpszEntry,double nValue)
{
	CString valueStr;

	valueStr.Format("%g",nValue);
	return WriteProfileString(lpszSection,lpszEntry,valueStr); 
}

COLORREF CColorHCFRConfig::GetProfileColor(LPCTSTR lpszSection,LPCTSTR lpszEntry)
{
   CString colorStr=GetProfileString(lpszSection,lpszEntry,"Auto");
   if(colorStr=="Auto")
	   return CLR_DEFAULT;
   int r,g,b;
   sscanf(colorStr,"RGB(%d,%d,%d)",&r,&g,&b);
   return RGB(r,g,b);
}

BOOL CColorHCFRConfig::WriteProfileColor(LPCTSTR lpszSection,LPCTSTR lpszEntry,COLORREF nValue)
{
   CString colorStr;
   colorStr.Format("RGB(%d,%d,%d)",GetRValue(nValue),GetGValue(nValue),GetBValue(nValue));
   if(nValue==CLR_DEFAULT)
	   colorStr = "Auto";
   WriteProfileString(lpszSection,lpszEntry,colorStr);
   return WriteProfileString(lpszSection,lpszEntry,colorStr); 
}

void CColorHCFRConfig::PurgeHelpLanguageSection ()
{
	WritePrivateProfileSection ( "HelpFile", "\0\0", m_iniFileName );
}

void CColorHCFRConfig::DisplayHelp ( UINT nId, LPCSTR lpszTopic )
{
	BOOL	bDisplayed = FALSE;
	
	if ( lpszTopic )
		bDisplayed = ( HtmlHelp ( GetDesktopWindow (), m_chmFileName, HH_DISPLAY_TOPIC, (DWORD)(LPSTR) lpszTopic ) != NULL );
	else if ( nId )
		bDisplayed = ( HtmlHelp ( GetDesktopWindow (), m_chmFileName, HH_HELP_CONTEXT, (DWORD) nId ) != NULL );
	
	if ( ! bDisplayed )
		HtmlHelp ( GetDesktopWindow (), m_chmFileName, HH_DISPLAY_TOC, 0 );
}

void CColorHCFRConfig::EnsurePathExists ( CString strPath )
{
	CreateDirectory ( strPath, NULL );
}


