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

// Generator.h: interface for the CGenerator class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GENERATOR_H__A28653EC_9D7F_406C_8F30_9D7187AFFC97__INCLUDED_)
#define AFX_GENERATOR_H__A28653EC_9D7F_406C_8F30_9D7187AFFC97__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FullScreenWindow.h"
#include "GeneratorPropPage.h"

class CGenerator: public CObject    
{
public:
	DECLARE_SERIAL(CGenerator) ;
	enum MeasureType
	{
		MT_ACTUAL = -1,
		MT_UNKNOWN = 0,
		MT_IRE = 1,
		MT_NEARBLACK,
		MT_NEARWHITE,
		MT_PRIMARY,
		MT_SECONDARY,
		MT_SAT_RED,
		MT_SAT_GREEN,
		MT_SAT_BLUE,
		MT_SAT_YELLOW,
		MT_SAT_CYAN,
		MT_SAT_MAGENTA,
		MT_SAT_CC24,
		MT_CONTRAST,
		MT_CALIBRATE,
		MT_SAT_ALL
	};

	UINT nMeasureNumber;
	BOOL m_doScreenBlanking;
	BOOL m_b16_235;

protected:
	BOOL m_isModified;
	CFullScreenWindow m_blankingWindow;

	CPropertySheetWithHelp m_propertySheet;
	CGeneratorPropPage m_GeneratorPropertiePage;
	CString m_name;

public:
	CGenerator();
	virtual ~CGenerator();

	virtual	void Copy(CGenerator * p);
	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init(UINT nbMeasure = 0);
	virtual BOOL DisplayGray(double aLevel,MeasureType nPatternType, BOOL bChangePattern = TRUE);
	virtual BOOL DisplayRGBColor(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo = 0,BOOL bChangePattern = TRUE,BOOL bSilentMode = FALSE);	// need to be overriden
	virtual BOOL DisplayRGBColormadVR(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType, UINT nPatternInfo = 0,BOOL bChangePattern = TRUE,BOOL bSilentMode = FALSE);	// need to be overriden
	virtual BOOL DisplayAnsiBWRects(BOOL bInvert);		// need to be overriden
	virtual BOOL DisplayAnimatedBlack();				// need to be overriden
	virtual BOOL DisplayAnimatedWhite();				// need to be overriden
	virtual BOOL DisplayDotPattern(const ColorRGBDisplay& clr , BOOL dot2, UINT nPads)  { return TRUE; };
	virtual BOOL DisplayHVLinesPattern(const ColorRGBDisplay& clr , BOOL dot2, BOOL vLines)  { return TRUE; };
	virtual BOOL DisplayColorLevelPattern( INT clrLevel , BOOL dot2, UINT nPads) { return TRUE; };
	virtual BOOL DisplayGeomPattern(BOOL dot2, UINT nPads) { return TRUE; };
	virtual BOOL DisplayConvPattern(BOOL dot2, UINT nPads) { return TRUE; };
	virtual BOOL DisplayColorPattern( BOOL dot2) { return TRUE; };
	virtual BOOL DisplayPatternPicture(HMODULE hInst, UINT nIDResource, BOOL bResizePict) { return TRUE; };

	virtual BOOL CanDisplayAnsiBWRects(); 
	virtual BOOL CanDisplayGrayAndColorsSeries()	{ return TRUE; }
	virtual BOOL CanDisplayAnimatedPatterns()		{ return FALSE; }
	virtual BOOL CanDisplayScale ( MeasureType nScaleType, int nbLevels, BOOL bMute = FALSE )		{ return TRUE; }
	virtual	BOOL HasPatternChanged( MeasureType nScaleType, CColor previousColor,CColor measuredColor)		{ return TRUE; }
	virtual CString GetRetryMessage () { return ""; }
	virtual BOOL Release(INT nbNext = -1);
	virtual BOOL ChangePatternSeries();
	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();
	virtual BOOL Configure();

	virtual BOOL IsModified() { return m_isModified; }
	virtual void SetModifiedFlag( BOOL bModified ) { m_isModified = bModified; }

	CString GetName() { return m_name; }
	void SetName(CString aStr) { m_name=aStr; } 

	virtual CString GetActiveDisplayName() { return ""; }

protected:
	void AddPropertyPage(CPropertyPageWithHelp *apPage);
};

#endif // !defined(AFX_GENERATOR_H__A28653EC_9D7F_406C_8F30_9D7187AFFC97__INCLUDED_)
