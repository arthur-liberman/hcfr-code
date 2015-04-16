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
//	Patrice AFFLATET
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_PATTERNDISPLAY_H__B6B5F90A_0497_419B_AF4C_A790AFE0ECD0__INCLUDED_)
#define AFX_PATTERNDISPLAY_H__B6B5F90A_0497_419B_AF4C_A790AFE0ECD0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PatternDisplay.h : header file
//

#include "Generator.h"

/////////////////////////////////////////////////////////////////////////////
// CPatternDisplay dialog

class CPatternDisplay : public CDialog
{
// Construction
public:
	CPatternDisplay(CWnd* pParent = NULL);   // standard constructor
	CGenerator * m_patternDGenerator;
	BOOL m_Dot2; 
	UINT m_nPads;


// Dialog Data
	//{{AFX_DATA(CPatternDisplay)
	enum { IDD = IDD_PATTERNS };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPatternDisplay)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	void CreateGenerator();
	void WaitKey();
	void Reset();
	void DisplayPattern(const ColorRGBDisplay& clr, UINT iMode); 
	void DisplayHVLinesPattern(const ColorRGBDisplay& clr, BOOL vLines); 
	void DisplayColorLevelPattern(INT clrLevel);
	void DisplayPatternPicture(HMODULE hPatterns, UINT iMode); 

	// Generated message map functions
	//{{AFX_MSG(CPatternDisplay)
	afx_msg void OnPatternAllShad();
	afx_msg void OnPatternBlack();
	afx_msg void OnPatternBlackLevel();
	afx_msg void OnPatternBlackP();
	afx_msg void OnPatternBlue();
	afx_msg void OnPatternBlueDot();
	afx_msg void OnPatternBlueShad();
	afx_msg void OnPatternBw();
	afx_msg void OnPatternColor();
	afx_msg void OnPatternConvGrid();
	afx_msg void OnPatternCyan();
	afx_msg void OnPatternGeom();
	afx_msg void OnPatternGreen();
	afx_msg void OnPatternGreenDot();
	afx_msg void OnPatternGreenShad();
	afx_msg void OnPatternGrey();
	afx_msg void OnPatternHLine();
	afx_msg void OnPatternMagenta();
	afx_msg void OnPatternPicture();
	afx_msg void OnPatternPictSMPTE();
	afx_msg void OnPatternPict1956();
	afx_msg void OnPatternRed();
	afx_msg void OnPatternRedDot();
	afx_msg void OnPatternRedShad();
	afx_msg void OnPatternVLine();
	afx_msg void OnPatternWhite();
	afx_msg void OnPatternWhiteDot();
	afx_msg void OnPatternWhiteLevel();
	afx_msg void OnPatternYellow();
	afx_msg void OnPatternAnimB();
	afx_msg void OnPatternAnimW();
    afx_msg void OnPatternPictGrid();
    afx_msg void OnPatternPict();
	afx_msg void OnPatternPictIris();
    afx_msg void OnPatternPictAnsi1();
    afx_msg void OnPatternPictAnsi2();
    afx_msg void OnPatternPictCBox();
    afx_msg void OnPatternGradient();
    afx_msg void OnPatternLramp();
    afx_msg void OnPatternGranger();
    afx_msg void OnPatternSramp();
    afx_msg void OnPatternTestimg();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PATTERNDISPLAY_H__D25460FD_8DAC_49F8_ABE0_59F2C12C0668__INCLUDED_)
