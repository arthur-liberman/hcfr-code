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

// GDIGenerator.h: interface for the CGDIGenerator class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_GDIGENERATOR_H__B88C4ECA_B358_4964_B549_69B51A691C42__INCLUDED_)
#define AFX_GDIGENERATOR_H__B88C4ECA_B358_4964_B549_69B51A691C42__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Generator.h"
#include "GDIGenePropPage.h"

#define MAX_MONITOR_NB 4

class CGDIGenerator : public CGenerator   
{
public:
	DECLARE_SERIAL(CGDIGenerator) ;

// Parameters
protected:
	BOOL	m_bBlankingCanceled;
	int		m_activeMonitorNum;
	BOOL	m_bConnect;

// Attributes
	CFullScreenWindow m_displayWindow;
	CGDIGenePropPage m_GDIGenePropertiesPage;
public:										// public because of callback
	UINT m_monitorNb;						// number of detected monitors
	HMONITOR m_hMonitor[MAX_MONITOR_NB];	// array of detected monitors handles
	int		m_nDisplayMode;
    int     m_rectSizePercent;
	int     m_bgStimPercent;
	UINT    m_Intensity;
	BOOL	IsOnOtherMonitor ();
	BOOL	m_bisInited;
// Implementation
public:
	CGDIGenerator();
	CGDIGenerator(int nDisplayMode, BOOL b16_235);
	virtual ~CGDIGenerator();

	virtual	void Copy(CGenerator * p);
	virtual void Serialize(CArchive& archive); 
	CGenerator * m_patternDGenerator;

	virtual BOOL Init(UINT nbMeasure = 0);
	virtual BOOL DisplayRGBColor(const ColorRGBDisplay& aRGBColor, MeasureType nPatternType = MT_UNKNOWN, UINT nPatternInfo = 0, BOOL bChangePattern = TRUE,BOOL bSilentMode = FALSE);
	virtual BOOL DisplayRGBColormadVR(const ColorRGBDisplay& aRGBColor, bool first);
	virtual BOOL DisplayRGBCCast(const ColorRGBDisplay& aRGBColor, bool first);
	virtual BOOL CanDisplayAnsiBWRects(); 
	virtual BOOL CanDisplayAnimatedPatterns(); 
	virtual BOOL DisplayAnsiBWRects(BOOL bInvert);
	virtual BOOL DisplayAnimatedBlack();
	virtual BOOL DisplayAnimatedWhite();
	virtual BOOL DisplayGradient();
	virtual BOOL DisplayGradient2();
	virtual BOOL DisplayRG();
	virtual BOOL DisplayRB();
	virtual BOOL DisplayGB();
	virtual BOOL DisplayRGd();
	virtual BOOL DisplayRBd();
	virtual BOOL DisplayGBd();
	virtual BOOL DisplayLramp();
	virtual BOOL DisplayGranger();
	virtual BOOL DisplayTV();
	virtual BOOL DisplaySpectrum();
	virtual BOOL DisplaySramp();
	virtual BOOL DisplayTestimg();
	virtual BOOL DisplayVSMPTE();
	virtual BOOL DisplayEramp();
	virtual BOOL DisplayAlign();
	virtual BOOL DisplayTC0();
	virtual BOOL DisplayTC1();
	virtual BOOL DisplayTC2();
	virtual BOOL DisplayTC3();
	virtual BOOL DisplayTC4();
	virtual BOOL DisplayTC5();
	virtual BOOL DisplayBN();
	virtual BOOL DisplayDR0();
	virtual BOOL DisplayDR1();
	virtual BOOL DisplayDR2();
	virtual BOOL DisplaySharp();
	virtual BOOL DisplayClipL();
	virtual BOOL DisplayClipH();
	virtual BOOL DisplayDotPattern( const ColorRGBDisplay&  clr , BOOL dot2, UINT nPads);
	virtual	BOOL DisplayHVLinesPattern( const ColorRGBDisplay&  clr , BOOL dot2, BOOL vLines);
	virtual BOOL DisplayColorLevelPattern( INT clrLevel , BOOL dot2, UINT nPads);
	virtual BOOL DisplayGeomPattern(BOOL dot2, UINT nPads);
	virtual BOOL DisplayConvPattern(BOOL dot2, UINT nPads);
	virtual BOOL DisplayColorPattern( BOOL dot2);
	virtual BOOL DisplayPatternPicture(HMODULE hInst, UINT nIDResource, BOOL bResizePict);

	virtual BOOL Release(INT nbNext = -1);

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

	virtual CString GetActiveDisplayName();

protected:
	void GetMonitorList();
	std::string GetMonitorName(const MONITORINFOEX *m) const;
};

#endif // !defined(AFX_GDIGENERATOR_H__B88C4ECA_B358_4964_B549_69B51A691C42__INCLUDED_)
