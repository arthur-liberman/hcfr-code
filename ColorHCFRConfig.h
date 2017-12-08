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

// ColorHCFRConfig.h: interface for the CColorHCFRConfig class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COLORHCFRCONFIG_H__669E7D2E_9632_4BCC_8C99_41DA182515F1__INCLUDED_)
#define AFX_COLORHCFRCONFIG_H__669E7D2E_9632_4BCC_8C99_41DA182515F1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "color.h"
#include "fxcolor.h"
#include "ModelessPropertySheet.h"
#include "GeneralPropPage.h"
#include "ReferencesPropPage.h"
#include "AppearancePropPage.h"
#include "AdvancedPropPage.h"
#include "ToolbarPropPage.h"

class CColorHCFRConfig  
{
public:
	// Exe Path, with an ending backslash
	char m_ApplicationPath [MAX_PATH];

	// Global ini file name
	char m_iniFileName [MAX_PATH];

	// Global help file name
	char m_chmFileName [MAX_PATH];

	// decoder.log file name
	char m_logFileName [MAX_PATH];

	// decoder.log access need a critical section during simultaneous measures
	CRITICAL_SECTION	LogFileCritSec;

	// Color settings
	ColorStandard m_colorStandard;
	CCPatterns	m_CCMode;
	WhiteTarget m_whiteTarget;
	BOOL m_bDisplayTestColors;
	BOOL m_bContinuousMeasures;
	BOOL m_bDetectPrimaries;
	BOOL m_isSettling;
	BOOL m_useHSV;
	int m_latencyTime;
	BOOL m_bLatencyBeep;
	BOOL bDisplayRT;
	BOOL m_bABL;
	BOOL m_bDisableHighDPI;
	BOOL m_bUseRoundDown;
	BOOL m_bUse10bit;
	int m_BWColorsToAdd;
	double m_GammaRef;
	double m_GammaAvg;
	double m_MasterMinL, m_MasterMaxL, m_ContentMaxL, m_FrameAvgMaxL, m_TargetMinL, m_TargetMaxL, m_DiffuseL;
	double m_GammaRel;
	double m_Split;
	double m_ManualBlack;
	BOOL m_userBlack, m_bOverRideTargs;
	BOOL m_useToneMap;
	BOOL m_useMeasuredGamma;
	BOOL m_bSave;
	BOOL m_bSave2;
	double m_manualGOffset;
	double m_manualWhitex;
    double m_manualWhitey;
	double m_manualRedx;
    double m_manualRedy;
	double m_manualGreenx;
    double m_manualGreeny;
	double m_manualBluex;
    double m_manualBluey;
	int m_GammaOffsetType;
    int m_nColors;
	bool m_bHDR100;

	// Appearance settings
	BOOL m_drawMenuBorder;
	int m_menuDrawMode;
	BOOL m_useCustomColor;
	BOOL m_doXpBlending;
	BOOL m_doSelectDisabledItem;
	BOOL m_doGlooming;
	BOOL m_bWhiteBkgndOnScreen;
	BOOL m_bWhiteBkgndOnFile;
	BOOL m_bmoveMessage;

	COLORREF m_fxColorWindow;
	COLORREF m_fxColorMenu;
	COLORREF m_fxColorSelection;
	COLORREF m_fxColorText;

	// Other settings
	BOOL m_doMultipleInstance;
	BOOL m_doUpdateCheck;
	BOOL m_doSavePosition;

	// Advanced settings
	int		m_TBViewsRightClickMode;
	int		m_TBViewsMiddleClickMode;
	BOOL	m_bConfirmMeasures;
	BOOL	m_bUseOnlyPrimaries;
	BOOL	m_bUseImperialUnits;
	int		m_nLuminanceCurveMode;
	BOOL	m_bPreferLuxmeter;
	int		m_dE_form;
    int     m_dE_gray;
    int     gw_Weight;
    BOOL    doHighlight;
	BOOL	isHighDPI;


	// Global strings
	CString	m_PercentGray;

public:
	BOOL GetPropertiesSheetValues();
	void SetPropertiesSheetValues();
	void ChangeSettings(int aPage);
	void SaveSettings();
	BOOL LoadSettings();
	void ApplySettings(BOOL isStartupApply=FALSE);
	void InitDefaults();
    int GetCColorsSize();
    ColorRGB GetCColorsT(int index);
	std::string GetCColorsN(int index);
    void GetCColors();
	CString strLang;

	BOOL IsProfileEntryDefined(LPCTSTR lpszSection,LPCTSTR lpszEntry);

	BOOL WriteProfileString(LPCTSTR lpszSection,LPCTSTR lpszEntry,LPCTSTR lpszValue);
	CString GetProfileString(LPCTSTR lpszSection,LPCTSTR lpszEntry,LPCTSTR lpszDefault = NULL);
	UINT GetProfileInt(LPCTSTR lpszSection,LPCTSTR lpszEntry,int nDefault);
	BOOL WriteProfileInt(LPCTSTR lpszSection,LPCTSTR lpszEntry,int nValue);
	double GetProfileDouble(LPCTSTR lpszSection,LPCTSTR lpszEntry,double nDefault);
	BOOL WriteProfileDouble(LPCTSTR lpszSection,LPCTSTR lpszEntry,double nValue);
	COLORREF GetProfileColor(LPCTSTR lpszSection,LPCTSTR lpszEntry);
	BOOL WriteProfileColor(LPCTSTR lpszSection,LPCTSTR lpszEntry,COLORREF nValue);
	void PurgeHelpLanguageSection ();

	void DisplayHelp ( UINT nId, LPCSTR lpszTopic );
	void EnsurePathExists ( CString strPath );
	CReferencesPropPage m_referencesPropertiesPage;
	CAdvancedPropPage m_advancedPropertiesPage;


	CColorHCFRConfig();
	virtual ~CColorHCFRConfig();

private:
	CPropertySheetWithHelp m_propertySheet;
	CGeneralPropPage m_generalPropertiesPage;
	CAppearancePropPage m_appearancePropertiesPage;
	CToolbarPropPage m_toolbarPropertiesPage;
};

#endif // !defined(AFX_COLORHCFRCONFIG_H__669E7D2E_9632_4BCC_8C99_41DA182515F1__INCLUDED_)
