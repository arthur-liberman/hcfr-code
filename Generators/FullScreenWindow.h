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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// FullScreenWindow.h : Defines the full screen color window
//
#if !defined(_MSC_VER) || _MSC_VER < 1600
#include <multimon.h>
#endif // _MSC_VER
#include <ddraw.h>

#include "../libnum/numsup.h"
#include "../libconv/conv.h"
#include "../libccast/ccmdns.h"
#include "../libccast/ccwin.h"
#include "../libccast/ccast.h"

// Display modes
#define DISPLAY_GDI		0
#define DISPLAY_OVERLAY	1
#define DISPLAY_VMR9	6
#define DISPLAY_madVR   2
#define DISPLAY_GDI_nBG		3
#define DISPLAY_ccast   4
#define DISPLAY_GDI_Hide 5

// Overlay surface resolution (can be anything)
#define OVERLAY_SURFACE_WIDTH	720
#define OVERLAY_SURFACE_HEIGHT	576

#define OVERLAY_SMALL_WIDTH		40
#define OVERLAY_SMALL_HEIGHT	32

class CFullScreenWindow : public CWnd
{
public:
	CFullScreenWindow(BOOL bTestOverlay = FALSE);

// Attributes
public:
	COLORREF				m_Color;
	UINT					m_rectSizePercent;
	UINT					m_bgStimPercent;
	UINT 					m_Intensity;
	UINT					m_nPat;
							
	UINT					m_nDisplayMode;
	INT						m_ansiCcast;
	BOOL					m_bDisableCursorHiding;
	BOOL					m_bIRE;
	BOOL					m_b16_235;
	BOOL					m_busePic;
	BOOL					m_bdispTrip;
	BOOL					m_bLinear;

	BOOL					m_bTestOverlay;
	LPDIRECTDRAW			m_lpDD;
	LPDIRECTDRAWSURFACE     m_lpDDPrimarySurface;
	LPDIRECTDRAWSURFACE     m_lpDDOverlay;

	HMONITOR				m_hCurrentMon;
	POINT					m_ptMonitorPos;
	// PatternDisplay
	BOOL					m_bPatternMode;
	COLORREF				m_clrPattern;
	UINT					m_nPadsPattern;
	BOOL					m_dot2Pattern;
	BOOL					m_bHLines;
	BOOL					m_bVLines;
	BOOL					m_bColorLevel;
	INT						m_iClrLevel;
	BOOL					m_bConv;
	BOOL					m_bGeom;
	BOOL					m_bColorPattern;
	BOOL					m_bPatternPict;
	UINT					m_uiPictRess;
	HMODULE					m_hPatternInst;
	BOOL					m_bResizePict;
	HWND					hWnd;

protected:
	void DisplayRGBColorInternal(COLORREF clr, BOOL bDisableWaiting);
	BOOL					m_bAnimated;
	BOOL					m_bWhite;
	UINT					m_IdTimer;
	int						m_XCurrent;
	int						m_AbsDeltaX;
	int						m_deltaX;
	int						m_XMax;
	int						m_YMax;
	int						m_XWidth;
	int						m_XMargin;
	int						m_YMargin;
	int						m_iOffset;

// Operations
public:
	void MoveToMonitor ( HMONITOR hMon );

	bool InitOverlay ();
	void ExitOverlay ();
	void SetOverlayPosition ();

	bool InitVMR9 ();
	void ExitVMR9 ();

	void SetDisplayMode (UINT nMode = DISPLAY_GDI);
	void SetRGBScale (BOOL b16_235);
	void DisplayRGBColor(const ColorRGBDisplay& clr, BOOL bDisableWaiting = FALSE );
	void DisplayAnsiBWRects(BOOL bInvert);
	void DisplayAnimatedBlack();
	void DisplayAnimatedWhite();
	void DisplayGradient();
	void DisplayRG();
	void DisplayRB();
	void DisplayGB();
	void DisplayRGd();
	void DisplayRBd();
	void DisplayGBd();
	void DisplayBN();
	void DisplayGradient2();
	void DisplayLramp();
	void DisplayGranger();
	void Display80();
	void DisplayTV();
	void DisplayTV2();
	void DisplaySpectrum();
	void DisplayTestimg();
	void DisplaySramp();
	void DisplayVSMPTE();
	void DisplayEramp();
	void DisplayAlign();
	void DisplayTC0();
	void DisplayTC1();
	void DisplayTC2();
	void DisplayTC3();
	void DisplayTC4();
	void DisplayTC5();
	void DisplayDR0();
	void DisplayDR1();
	void DisplayDR2();
	void DisplaySharp();
	void DisplayClipL();
	void DisplayClipH();
	void DisplayClipHO();
	void DisplayClipLO();
	void DisplayISO12233();
	void DisplayBBCHD();
	void DisplayCROSSd();
	void DisplayCROSSl();
	void DisplayPM5644();
	void DisplayZONE();

	//Patterns
	void DisplayDotPattern( const ColorRGBDisplay&  clr , BOOL dot2, UINT nPads);
	void DisplayHVLinesPattern( const ColorRGBDisplay&  clr , BOOL dot2, BOOL vLines);
	void DisplayColorLevelPattern(INT clrLevel , BOOL dot2, UINT nPads);
	void DisplayGeomPattern(BOOL dot2, UINT nPads);
	void DisplayConvPattern(BOOL dot2, UINT nPads);
	void DisplayColorPattern(BOOL dot2);
	void DisplayPatternPicture(HMODULE hInst, UINT nIDResource, BOOL bResizePict);

	void Hide ();
	void GetOverlayErrorText ( HRESULT ddrval, LPSTR lpszText );

	void ClearFlags ();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFullScreenWindow)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CFullScreenWindow();

// Generated message map functions
protected:
	//{{AFX_MSG(CFullScreenWindow)
	void OnPaint ();
	void OnTimer (UINT nIDEvent);
	afx_msg BOOL OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
