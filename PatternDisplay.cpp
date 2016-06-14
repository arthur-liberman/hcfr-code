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

// PatternDisplay.cpp : implementation file
//

#include "stdafx.h"
#include "colorhcfr.h"
#include "PatternDisplay.h"
#include "GDIGenerator.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPatternDisplay dialog


CPatternDisplay::CPatternDisplay(CWnd* pParent /*=NULL*/)
	: CDialog(CPatternDisplay::IDD, pParent)
{
	//{{AFX_DATA_INIT(CPatternDisplay)
	//}}AFX_DATA_INIT
	m_patternDGenerator=NULL;
	m_Dot2 = FALSE; 
	m_nPads = 25;
	CreateGenerator();
}


void CPatternDisplay::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPatternDisplay)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPatternDisplay, CDialog)
	//{{AFX_MSG_MAP(CPatternDisplay)
	ON_BN_CLICKED(IDC_PATTERN_ALL_SHAD, OnPatternAllShad)
	ON_BN_CLICKED(IDC_PATTERN_BLACK, OnPatternBlack)
	ON_BN_CLICKED(IDC_PATTERN_BLACK_LEVEL, OnPatternBlackLevel)
	ON_BN_CLICKED(IDC_PATTERN_BLACK_P, OnPatternBlackP)
	ON_BN_CLICKED(IDC_PATTERN_BLUE, OnPatternBlue)
	ON_BN_CLICKED(IDC_PATTERN_BLUE_DOT, OnPatternBlueDot)
	ON_BN_CLICKED(IDC_PATTERN_BLUE_SHAD, OnPatternBlueShad)
	ON_BN_CLICKED(IDC_PATTERN_BW, OnPatternBw)
	ON_BN_CLICKED(IDC_PATTERN_COLOR, OnPatternColor)
	ON_BN_CLICKED(IDC_PATTERN_CONV_GRID, OnPatternConvGrid)
	ON_BN_CLICKED(IDC_PATTERN_CYAN, OnPatternCyan)
	ON_BN_CLICKED(IDC_PATTERN_GEOM, OnPatternGeom)
	ON_BN_CLICKED(IDC_PATTERN_GREEN, OnPatternGreen)
	ON_BN_CLICKED(IDC_PATTERN_GREEN_DOT, OnPatternGreenDot)
	ON_BN_CLICKED(IDC_PATTERN_GREEN_SHAD, OnPatternGreenShad)
	ON_BN_CLICKED(IDC_PATTERN_GREY, OnPatternGrey)
	ON_BN_CLICKED(IDC_PATTERN_H_LINE, OnPatternHLine)
	ON_BN_CLICKED(IDC_PATTERN_MAGENTA, OnPatternMagenta)
	ON_BN_CLICKED(IDC_PATTERN_PICTURE, OnPatternPicture)
	ON_BN_CLICKED(IDC_PATTERN_SMPTE75, OnPatternPictSMPTE)
	ON_BN_CLICKED(IDC_PATTERN_1956, OnPatternPict1956)
	ON_BN_CLICKED(IDC_PATTERN_RED, OnPatternRed)
	ON_BN_CLICKED(IDC_PATTERN_RED_DOT, OnPatternRedDot)
	ON_BN_CLICKED(IDC_PATTERN_RED_SHAD, OnPatternRedShad)
	ON_BN_CLICKED(IDC_PATTERN_V_LINE, OnPatternVLine)
	ON_BN_CLICKED(IDC_PATTERN_WHITE, OnPatternWhite)
	ON_BN_CLICKED(IDC_PATTERN_WHITE_DOT, OnPatternWhiteDot)
	ON_BN_CLICKED(IDC_PATTERN_WHITE_LEVEL, OnPatternWhiteLevel)
	ON_BN_CLICKED(IDC_PATTERN_YELLOW, OnPatternYellow)
	ON_BN_CLICKED(IDC_PATTERN_ANIM_B, OnPatternAnimB)
	ON_BN_CLICKED(IDC_PATTERN_ANIM_W, OnPatternAnimW)
    ON_BN_CLICKED(IDC_PATTERN_PICT_GRID, OnPatternPictGrid)
    ON_BN_CLICKED(IDC_PATTERN_PICT, OnPatternPict)
	ON_BN_CLICKED(IDC_PATTERN_PICT_IRIS, OnPatternPictIris)
    ON_BN_CLICKED(IDC_PATTERN_PICT_CANSI1, OnPatternPictAnsi1)
    ON_BN_CLICKED(IDC_PATTERN_PICT_CANSI2, OnPatternPictAnsi2)
    ON_BN_CLICKED(IDC_PATTERN_PICT_CBOX, OnPatternPictCBox)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

static UINT idPict16Ress[][2] =
{
	{ IDR_PATTERN_16X16_1, FALSE },
	{ IDR_PATTERN_16X16_2, FALSE },
	{ IDR_PATTERN_16X16_3, FALSE },
	{ IDR_PATTERN_16X16_4, FALSE },
	{ IDR_PATTERN_16X16_5, FALSE },
	{ IDR_PATTERN_16X16_6, FALSE },
	{ IDR_PATTERN_16X16_7, FALSE },
	{ IDR_PATTERN_16X16_8, FALSE },
	{ IDR_PATTERN_16X16_9, FALSE },
	{ IDR_PATTERN_16X16_10, FALSE },
	{ IDR_PATTERN_16X16_11, FALSE },
};

static UINT idPictTBRess[][2] =
{

	{ IDR_PATTERN_TESTB_1, TRUE },
	{ IDR_PATTERN_TESTB_2, TRUE },
	{ IDR_PATTERN_TESTB_3, TRUE },
	{ IDR_PATTERN_TESTB_4, TRUE },
	{ IDR_PATTERN_TESTB_5, TRUE },
	{ IDR_PATTERN_TESTB_6, TRUE },
	{ IDR_PATTERN_TESTB_7, TRUE },
	{ IDR_PATTERN_TESTB_8, TRUE },
	{ IDR_PATTERN_TESTB_9, TRUE },
	{ IDR_PATTERN_TESTB_10, TRUE },
	{ IDR_PATTERN_TESTB_11, TRUE },
	{ IDR_PATTERN_TESTB_12, TRUE },
	{ IDR_PATTERN_TESTB_13, TRUE },
	{ IDR_PATTERN_TESTB_14, TRUE },
	{ IDR_PATTERN_TESTB_15, TRUE },
	{ IDR_PATTERN_TESTB_16, TRUE },
	{ IDR_PATTERN_TESTB_17, FALSE },
	{ IDR_PATTERN_TESTB_18, FALSE },
	{ IDR_PATTERN_TESTB_19, FALSE },
	{ IDR_PATTERN_TESTB_20, FALSE },
};

static UINT idPict720Ress[][2] =
{
	{ IDR_PATTERN_720_1, FALSE },
	{ IDR_PATTERN_720_2, TRUE },
	{ IDR_PATTERN_720_3, TRUE },
	{ IDR_PATTERN_720_4, TRUE },
};

static UINT idPictContAnsi1[][2] =
{
	{ IDR_PATTERN_ANSI1_002, TRUE },
	{ IDR_PATTERN_ANSI1_005, TRUE },
	{ IDR_PATTERN_ANSI1_01, TRUE },
	{ IDR_PATTERN_ANSI1_02, TRUE },
	{ IDR_PATTERN_ANSI1_05, TRUE },
	{ IDR_PATTERN_ANSI1_10, TRUE },
	{ IDR_PATTERN_ANSI1_20, TRUE },
	{ IDR_PATTERN_ANSI1_30, TRUE },
	{ IDR_PATTERN_ANSI1_40, TRUE },
	{ IDR_PATTERN_ANSI1_50, TRUE },
};

static UINT idPictContAnsi2[][2] =
{
	{ IDR_PATTERN_ANSI2_002, TRUE },
	{ IDR_PATTERN_ANSI2_005, TRUE },
	{ IDR_PATTERN_ANSI2_01, TRUE },
	{ IDR_PATTERN_ANSI2_02, TRUE },
	{ IDR_PATTERN_ANSI2_05, TRUE },
	{ IDR_PATTERN_ANSI2_10, TRUE },
	{ IDR_PATTERN_ANSI2_20, TRUE },
	{ IDR_PATTERN_ANSI2_30, TRUE },
	{ IDR_PATTERN_ANSI2_40, TRUE },
	{ IDR_PATTERN_ANSI2_50, TRUE },
};

static UINT idPictContBox[][2] =
{
	{ IDR_PATTERN_CONT_01, TRUE },
	{ IDR_PATTERN_CONT_02, TRUE },
	{ IDR_PATTERN_CONT_05, TRUE },
	{ IDR_PATTERN_CONT_10, TRUE },
	{ IDR_PATTERN_CONT_20, TRUE },
	{ IDR_PATTERN_CONT_30, TRUE },
	{ IDR_PATTERN_CONT_40, TRUE },
	{ IDR_PATTERN_CONT_50, TRUE },
};

void CPatternDisplay::CreateGenerator()
{
	if(m_patternDGenerator != NULL)
		delete m_patternDGenerator;
	m_patternDGenerator=new CGDIGenerator(DISPLAY_GDI_Hide, GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",0));
}

void CPatternDisplay::WaitKey()
{
	BOOL	bKeyTyped = FALSE;
	MSG		Msg;
	while ( ! bKeyTyped )
	{
		while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_MOUSELAST, TRUE ) )
		{
			if ( Msg.message == WM_KEYDOWN )
			{
				if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN )
					bKeyTyped = TRUE;
			}
			else if ( Msg.message == WM_TIMER )
			{
				// Dispatch timer message to allow animation run
				DispatchMessage ( &Msg );
			}
		}
		Sleep(10);
	}
}

void CPatternDisplay::DisplayPattern(const ColorRGBDisplay& clr, UINT iMode) 
{
	BOOL	bKeyTyped = FALSE;
	BOOL	doDisplay = FALSE;
	MSG		Msg;

	m_patternDGenerator->Init();

	if (iMode == 0) // Dots Pattern
		m_patternDGenerator->DisplayDotPattern(clr, m_Dot2, m_nPads);
	if (iMode == 1) // Conv Grid Pattern
		m_patternDGenerator->DisplayConvPattern(m_Dot2, m_nPads);
	if (iMode == 2) // Geometry Pattern
		m_patternDGenerator->DisplayGeomPattern(m_Dot2, m_nPads);

	while ( ! bKeyTyped )
	{
		while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_MOUSELAST, TRUE ) )
		{
			if ( Msg.message == WM_KEYDOWN )
			{
				doDisplay = TRUE;
				if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN )
					bKeyTyped = TRUE;
				else if ( ( Msg.wParam == VK_ADD || Msg.wParam == VK_PRIOR ) && m_nPads < 70) m_nPads++;
				else if ( ( Msg.wParam == VK_SUBTRACT || Msg.wParam == VK_NEXT ) && m_nPads > 5) m_nPads--;         
				else if ( ( Msg.wParam == VK_MULTIPLY || Msg.wParam == VK_HOME ) ) m_nPads = 25;
				else if (Msg.wParam == VK_SPACE) m_Dot2 = !m_Dot2;
				else doDisplay = FALSE;
				
				if (doDisplay && !bKeyTyped) {
					if (iMode == 0) // Dots Pattern
						m_patternDGenerator->DisplayDotPattern(clr, m_Dot2, m_nPads);
					if (iMode == 1) // Conv Grid Pattern
						m_patternDGenerator->DisplayConvPattern(m_Dot2, m_nPads);
					if (iMode == 2) // Geometry Pattern
						m_patternDGenerator->DisplayGeomPattern(m_Dot2, m_nPads);
				}

			}
			else if ( Msg.message == WM_TIMER )
			{
				// Dispatch timer message to allow animation run
				DispatchMessage ( &Msg );
			}
		}
		Sleep(10);
	}

	m_patternDGenerator->Release();
}

void CPatternDisplay::DisplayHVLinesPattern(const ColorRGBDisplay& clr, BOOL vLines) 
{
	BOOL	bKeyTyped = FALSE;
	BOOL	doDisplay = FALSE;
	MSG		Msg;
	INT	currentColor = 0;

	m_patternDGenerator->Init();
	m_patternDGenerator->DisplayHVLinesPattern(clr, m_Dot2, vLines);
	while ( ! bKeyTyped )
	{
		while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_MOUSELAST, TRUE ) )
		{
			if ( Msg.message == WM_KEYDOWN )
			{
                ColorRGBDisplay clr2;
				doDisplay = TRUE;
				if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN )
					bKeyTyped = TRUE;
				else if ( Msg.wParam == VK_ADD || Msg.wParam == VK_PRIOR ) currentColor++;
				else if ( Msg.wParam == VK_SUBTRACT || Msg.wParam == VK_NEXT ) currentColor--;         
				else if ( Msg.wParam == VK_MULTIPLY || Msg.wParam == VK_HOME ) clr2 = ColorRGBDisplay(100,100,100);
				else if ( Msg.wParam == VK_SPACE ) m_Dot2 = !m_Dot2;
				else doDisplay = FALSE;
				if (currentColor > 3) currentColor = 0;
				if (currentColor < 0) currentColor = 3;

				switch (currentColor) {
					case 0: clr2 = ColorRGBDisplay(100,100,100); break;
					case 1: clr2 = ColorRGBDisplay(100,0,0); break;
					case 2: clr2 = ColorRGBDisplay(0,100,0); break;
					case 3: clr2 = ColorRGBDisplay(0,0,100); break;
				}

				if (doDisplay && !bKeyTyped)
					m_patternDGenerator->DisplayHVLinesPattern(clr2, m_Dot2, vLines);

			}
			else if ( Msg.message == WM_TIMER )
			{
				// Dispatch timer message to allow animation run
				DispatchMessage ( &Msg );
			}
		}
		Sleep(10);
	}

	m_patternDGenerator->Release();
}

void CPatternDisplay::DisplayColorLevelPattern(INT clrLevel) 
{
	BOOL	bKeyTyped = FALSE;
	BOOL	doDisplay = FALSE;
	MSG		Msg;

	m_patternDGenerator->Init();
	m_patternDGenerator->DisplayColorLevelPattern(clrLevel, m_Dot2, m_nPads);
	while ( ! bKeyTyped )
	{
		while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_MOUSELAST, TRUE ) )
		{
			if ( Msg.message == WM_KEYDOWN )
			{
				doDisplay = TRUE;
				if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN )
					bKeyTyped = TRUE;
				else if ( ( Msg.wParam == VK_ADD || Msg.wParam == VK_PRIOR ) && m_nPads < 255 && clrLevel < 8) m_nPads++;
				else if ( ( Msg.wParam == VK_SUBTRACT || Msg.wParam == VK_NEXT ) && m_nPads > 2 && clrLevel < 8) m_nPads--;         
				else if ( Msg.wParam == VK_MULTIPLY || Msg.wParam == VK_HOME ) m_nPads = 11;
				else if ( Msg.wParam == VK_SPACE ) m_Dot2 = !m_Dot2;
				else doDisplay = FALSE;
				
				if (doDisplay && !bKeyTyped)
					m_patternDGenerator->DisplayColorLevelPattern(clrLevel, m_Dot2, m_nPads);

			}
			else if ( Msg.message == WM_TIMER )
			{
				// Dispatch timer message to allow animation run
				DispatchMessage ( &Msg );
			}
		}
		Sleep(10);
	}

	m_patternDGenerator->Release();
}

/////////////////////////////////////////////////////////////////////////////
// CPatternDisplay message handlers


void CPatternDisplay::OnPatternBlack() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(0,0,0),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternWhite() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(100,100,100),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternRed() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(100,0,0),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternGreen() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(0,100,0),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternBlue() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(0,0,100),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternCyan() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(0,100,100),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternMagenta() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(100,0,100),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternGrey() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(50,50,50),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternYellow() 
{
	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(100,100,0),CGenerator::MT_SPECIAL);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternWhiteDot() 
{
	DisplayPattern(ColorRGBDisplay(100,100,100),0);
}

void CPatternDisplay::OnPatternRedDot() 
{
	DisplayPattern(ColorRGBDisplay(100,0,0),0);
}

void CPatternDisplay::OnPatternGreenDot() 
{
	DisplayPattern(ColorRGBDisplay(0,100,0),0);
}

void CPatternDisplay::OnPatternBlueDot() 
{
	DisplayPattern(ColorRGBDisplay(0,0,100),0);
}

void CPatternDisplay::OnPatternHLine() 
{
	DisplayHVLinesPattern(ColorRGBDisplay(100,100,100), FALSE);
}

void CPatternDisplay::OnPatternVLine() 
{
	DisplayHVLinesPattern(ColorRGBDisplay(100,100,100), TRUE);
}

void CPatternDisplay::OnPatternBw() 
{
	m_nPads = 11;
	DisplayColorLevelPattern(0);
}

void CPatternDisplay::OnPatternRedShad() 
{
	m_nPads = 11;
	DisplayColorLevelPattern(1);
}

void CPatternDisplay::OnPatternGreenShad() 
{
	m_nPads = 11;
	DisplayColorLevelPattern(2);
}

void CPatternDisplay::OnPatternBlueShad() 
{
	m_nPads = 11;
	DisplayColorLevelPattern(3);
}

void CPatternDisplay::OnPatternBlackLevel() 
{
	m_nPads = 15;
	DisplayColorLevelPattern(8);
}

void CPatternDisplay::OnPatternWhiteLevel() 
{
	m_nPads = 15;
	DisplayColorLevelPattern(9);
}

void CPatternDisplay::OnPatternAllShad() 
{
	m_nPads = 11;
	DisplayColorLevelPattern(4);
}

void CPatternDisplay::OnPatternBlackP() 
{
	BOOL	bKeyTyped = FALSE;
	BOOL	doDisplay = FALSE;
	MSG		Msg;
	double iPercent=0;
	double step = 1. / 219. * 100.;

	m_patternDGenerator->Init(0, TRUE);
	m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(iPercent),CGenerator::MT_SPECIAL);
	while ( ! bKeyTyped )
	{
		while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_MOUSELAST, TRUE ) )
		{
			if ( Msg.message == WM_KEYDOWN )
			{
				doDisplay = TRUE;
				if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN )
					bKeyTyped = TRUE;
				else if ( ( Msg.wParam == VK_ADD || Msg.wParam == VK_PRIOR ) && iPercent < 100) iPercent+=step;
				else if ( ( Msg.wParam == VK_SUBTRACT || Msg.wParam == VK_NEXT ) && iPercent > 0) iPercent-=step;         
				else if ( Msg.wParam == VK_MULTIPLY || Msg.wParam == VK_HOME ) iPercent = 50;
				else doDisplay = FALSE;
				
				if (doDisplay && !bKeyTyped)
					m_patternDGenerator->DisplayRGBColor(ColorRGBDisplay(iPercent),CGenerator::MT_PRIMARY);
			}
			else if ( Msg.message == WM_TIMER )
			{
				// Dispatch timer message to allow animation run
				DispatchMessage ( &Msg );
			}
		}
		Sleep(10);
	}

	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternColor() 
{
	m_patternDGenerator->Init();
	m_patternDGenerator->DisplayColorPattern(FALSE);
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternConvGrid() 
{
	DisplayPattern(ColorRGBDisplay(0,0,0),1);
}

void CPatternDisplay::OnPatternGeom() 
{
	DisplayPattern(ColorRGBDisplay(0,0,0),2);
}

void CPatternDisplay::OnPatternPicture() 
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	m_patternDGenerator->Init();
	if (GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",0) == 0)
		m_patternDGenerator->DisplayPatternPicture(hPatterns,IDR_PATTERN_TESTIMG,TRUE);
	else
		m_patternDGenerator->DisplayPatternPicture(hPatterns,IDR_PATTERN_TESTIMGv,TRUE);
	WaitKey();
	m_patternDGenerator->Release();
	FreeLibrary(hPatterns);
}

void CPatternDisplay::OnPatternPictSMPTE() 
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	m_patternDGenerator->Init();
	if (GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",0) == 0)
		m_patternDGenerator->DisplayPatternPicture(hPatterns,IDR_PATTERN_SMPTE75,TRUE);
	else
		m_patternDGenerator->DisplayPatternPicture(hPatterns,IDR_PATTERN_SMPTE75v,TRUE);
	WaitKey();
	m_patternDGenerator->Release();
	FreeLibrary(hPatterns);
}

void CPatternDisplay::OnPatternPict1956() 
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	m_patternDGenerator->Init();
	if (GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",0) == 0)
		m_patternDGenerator->DisplayPatternPicture(hPatterns,IDR_PATTERN_1956,FALSE);
	else
		m_patternDGenerator->DisplayPatternPicture(hPatterns,IDR_PATTERN_1956v,FALSE);
	WaitKey();
	m_patternDGenerator->Release();
	FreeLibrary(hPatterns);
}

void CPatternDisplay::OnPatternTestimg() 
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	m_patternDGenerator->Init();
	if (GetConfig()->GetProfileInt("GDIGenerator","RGB_16_235",0) == 0)
		m_patternDGenerator->DisplayPatternPicture(hPatterns,IDR_PATTERN_TESTIMG,TRUE);
	else
		m_patternDGenerator->DisplayPatternPicture(hPatterns,IDR_PATTERN_TESTIMGv,TRUE);
	WaitKey();
	m_patternDGenerator->Release();
	FreeLibrary(hPatterns);
}


void CPatternDisplay::OnPatternAnimB() 
{
	m_patternDGenerator->Init();
	m_patternDGenerator->DisplayAnimatedBlack();
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternAnimW() 
{
	m_patternDGenerator->Init();
	m_patternDGenerator->DisplayAnimatedWhite();
	WaitKey();
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnPatternPictGrid()
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	DisplayPatternPicture(hPatterns,0);
	FreeLibrary(hPatterns);
}

void CPatternDisplay::OnPatternPict()
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	DisplayPatternPicture(hPatterns,1);
	FreeLibrary(hPatterns);
}

void CPatternDisplay::OnPatternPictIris()
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	DisplayPatternPicture(hPatterns,2);
	FreeLibrary(hPatterns);
}

void CPatternDisplay::OnPatternPictAnsi1()
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	DisplayPatternPicture(hPatterns,3);
	FreeLibrary(hPatterns);
}

void CPatternDisplay::OnPatternPictAnsi2()
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	DisplayPatternPicture(hPatterns,4);
	FreeLibrary(hPatterns);
}

void CPatternDisplay::OnPatternPictCBox()
{
	HMODULE hPatterns;
	hPatterns = LoadLibrary(_T("CHCFR21_PATTERNS.dll"));
	DisplayPatternPicture(hPatterns,5);
	FreeLibrary(hPatterns);
}


void CPatternDisplay::DisplayPatternPicture(HMODULE hPatterns, UINT iMode) 
{
	BOOL	bKeyTyped = FALSE;
	BOOL	doDisplay = FALSE;
	MSG		Msg;
	int		iCurrId=0;
	int		maxPat=0;
	
	m_patternDGenerator->Init();
	if (iMode == 0) { // 16*16 Patterns
		m_patternDGenerator->DisplayPatternPicture(hPatterns,idPict16Ress[iCurrId][0],idPict16Ress[iCurrId][1]);
		maxPat = 10;
	}
	else if (iMode == 1) { // Test Bed Patterns
		m_patternDGenerator->DisplayPatternPicture(hPatterns,idPictTBRess[iCurrId][0],idPictTBRess[iCurrId][1]);
		maxPat = 19;
	}
	else if (iMode == 2) { // 720 Patterns
		m_patternDGenerator->DisplayPatternPicture(hPatterns,idPict720Ress[iCurrId][0],idPict720Ress[iCurrId][1]);
		maxPat = 3;
	}
	else if (iMode == 3) { // Contrast Ansi1 Patterns
		m_patternDGenerator->DisplayPatternPicture(hPatterns,idPictContAnsi1[iCurrId][0],idPictContAnsi1[iCurrId][1]);
		maxPat = 9;
	}
	else if (iMode == 4) { // Contrast Ansi2 Patterns
		m_patternDGenerator->DisplayPatternPicture(hPatterns,idPictContAnsi2[iCurrId][0],idPictContAnsi2[iCurrId][1]);
		maxPat = 9;
	}
	else if (iMode == 5) { // Contrast Box Patterns
		m_patternDGenerator->DisplayPatternPicture(hPatterns,idPictContBox[iCurrId][0],idPictContBox[iCurrId][1]);
		maxPat = 7;
	}
	
	while ( ! bKeyTyped )
	{
		while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_MOUSELAST, TRUE ) )
		{
			if ( Msg.message == WM_KEYDOWN )
			{
				doDisplay = TRUE;
				if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN )
					bKeyTyped = TRUE;
				else if ( ( Msg.wParam == VK_ADD || Msg.wParam == VK_PRIOR ) && iCurrId < maxPat) iCurrId++;
				else if ( ( Msg.wParam == VK_SUBTRACT || Msg.wParam == VK_NEXT ) && iCurrId > 0) iCurrId--;         
				else if ( Msg.wParam == VK_MULTIPLY || Msg.wParam == VK_HOME ) iCurrId = 0;
				else doDisplay = FALSE;
				
				if (doDisplay && !bKeyTyped) {
					if (iMode == 0) //  16*16 Patterns
						m_patternDGenerator->DisplayPatternPicture(hPatterns,idPict16Ress[iCurrId][0],idPict16Ress[iCurrId][1]);
					else if (iMode == 1) // Test Bed Patterns
						m_patternDGenerator->DisplayPatternPicture(hPatterns,idPictTBRess[iCurrId][0],idPictTBRess[iCurrId][1]);
					else if (iMode == 2) // 720 Patterns
						m_patternDGenerator->DisplayPatternPicture(hPatterns,idPict720Ress[iCurrId][0],idPict720Ress[iCurrId][1]);
					else if (iMode == 3) // Contrast Ansi1 Patterns
						m_patternDGenerator->DisplayPatternPicture(hPatterns,idPictContAnsi1[iCurrId][0],idPictContAnsi1[iCurrId][1]);
					else if (iMode == 4) // Contrast Ansi2 Patterns
						m_patternDGenerator->DisplayPatternPicture(hPatterns,idPictContAnsi2[iCurrId][0],idPictContAnsi2[iCurrId][1]);
					else if (iMode == 5) // Contrast Box Patterns
						m_patternDGenerator->DisplayPatternPicture(hPatterns,idPictContBox[iCurrId][0],idPictContBox[iCurrId][1]);
				}
			}
			else if ( Msg.message == WM_TIMER )
			{
				// Dispatch timer message to allow animation run
				DispatchMessage ( &Msg );
			}
		}
		Sleep(10);
	}
	m_patternDGenerator->Release();
}

void CPatternDisplay::OnHelp() 
{
	// TODO: Add your control notification handler code here
	GetConfig () -> DisplayHelp ( HID_TESTPATTERNS, NULL );
}

BOOL CPatternDisplay::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	OnHelp ();
	return TRUE;
}

