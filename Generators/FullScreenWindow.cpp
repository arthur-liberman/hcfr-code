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
//	Patrice AFFLATET
/////////////////////////////////////////////////////////////////////////////

// FullScreenWindow.cpp : Defines the full screen color window
//

#include "stdafx.h"
#include "../ColorHCFR.h"
#if _MSC_VER >= 1600
#define COMPILE_MULTIMON_STUBS
#include <multimon.h>
#endif
#include "FullScreenWindow.h"
#include <math.h>
#include "ximage.h"

#ifndef MAKEFOURCC
	#include <mmsystem.h>
#endif

#define ANSI_CONTRAST_BLOCKS	4

CFullScreenWindow::CFullScreenWindow(BOOL bTestOverlay)
{
	RECT			rect;
	HWND			hWnd, hDesktop;

	m_Color = 0;
	m_bTestOverlay = bTestOverlay;
	m_rectSizePercent = 100;
	
	m_nDisplayMode = DISPLAY_GDI;
	m_bDisableCursorHiding = FALSE;
	m_b16_235 = FALSE;

	// Overlay data
	m_lpDD = NULL;
	m_lpDDPrimarySurface = NULL;    
	m_lpDDOverlay = NULL;    

	m_hCurrentMon = NULL;
	m_ptMonitorPos.x = 0;
	m_ptMonitorPos.y = 0;

	m_bAnimated = FALSE;
	m_bWhite = FALSE;
	m_IdTimer = 0;
	m_XCurrent = -1;

	// PatternDisplay
	m_bPatternMode = FALSE;
	m_clrPattern = ColorRGBDisplay( 0.0 ).GetColorRef(m_b16_235);
	m_nPadsPattern = 25;
	m_dot2Pattern = FALSE;
	m_bHLines = FALSE;
	m_bVLines = FALSE;
	m_bColorLevel = FALSE;
	m_iClrLevel = 0;
	m_bGeom = FALSE;
	m_bConv = FALSE;
	m_bColorPattern = FALSE;
	m_iOffset = 40;
	m_bPatternPict = FALSE;
	m_uiPictRess = 0;
	m_bResizePict = FALSE;

	// Create window
	hDesktop = ::GetDesktopWindow ();
	::GetWindowRect ( hDesktop, & rect );

	if ( m_bTestOverlay )
	{
		rect.left = rect.right - OVERLAY_SMALL_WIDTH;
		rect.bottom = rect.top + OVERLAY_SMALL_HEIGHT;
	}

	hWnd = ::CreateWindowEx ( WS_EX_TOOLWINDOW, AfxRegisterWndClass ( 0, ::LoadCursor(NULL,IDC_ARROW) ), "", WS_POPUP, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, NULL, (HMENU) NULL, GetModuleHandle ( NULL ), NULL );
	SubclassWindow ( hWnd );
}


CFullScreenWindow::~CFullScreenWindow()
{
	Hide ();
	SetDisplayMode ();

	DestroyWindow ();

	ASSERT ( m_lpDD == NULL && m_lpDDPrimarySurface == NULL && m_lpDDOverlay == NULL );
}

BEGIN_MESSAGE_MAP(CFullScreenWindow, CWnd)
	//{{AFX_MSG_MAP(CFullScreenWindow)
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_SETCURSOR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



void CFullScreenWindow::MoveToMonitor ( HMONITOR hMon )
{
	HWND			hDesktop;
	MONITORINFO		mi;

	if ( hMon )
	{
		m_hCurrentMon = hMon;

		mi.cbSize = sizeof ( mi );
		GetMonitorInfo ( hMon, & mi );

		m_ptMonitorPos.x = mi.rcMonitor.left;
		m_ptMonitorPos.y = mi.rcMonitor.top;
	}
	else
	{
		m_hCurrentMon = NULL;

		hDesktop = ::GetDesktopWindow ();
		::GetWindowRect ( hDesktop, & mi.rcMonitor );

		m_ptMonitorPos.x = 0;
		m_ptMonitorPos.y = 0;
	}

	if ( m_bTestOverlay )
	{
		mi.rcMonitor.left = mi.rcMonitor.right - OVERLAY_SMALL_WIDTH;
		mi.rcMonitor.bottom = mi.rcMonitor.top + OVERLAY_SMALL_HEIGHT;
	}
	
	SetWindowPos ( NULL, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER );
	
	if ( m_bTestOverlay || m_nDisplayMode == DISPLAY_OVERLAY )
	{
		// When monitor changes, need to (re)init overlay surfaces and cooperative level
		// To avoid many lines of code, simply reinit all direct draw objects, even if full reinit is not necessary
		SetDisplayMode ( DISPLAY_OVERLAY );
	}
}

void CFullScreenWindow::DisplayRGBColor(const ColorRGBDisplay& clr, BOOL bDisableWaiting )
{
	int				nColorFlag;
	int				nPadWidth, nPadHeight;
	int				R, G, B;
    int				Y, U, V;
	DWORD			dwYUY2_Color;
	DWORD			dwYUY2_Black;
    HRESULT			ddrval;
    DDSURFACEDESC	SurfaceDesc;
	CRect			rect;
	char			szError [ 256 ];
	char			szMsg [ 256 ];
	
	m_Color = clr.GetColorRef(m_b16_235);

	if ( m_nDisplayMode == DISPLAY_OVERLAY )
	{
		// Set color directly in YUY2 format inside overlay surface
		memset(&SurfaceDesc, 0, sizeof(SurfaceDesc));
		SurfaceDesc.dwSize = sizeof(SurfaceDesc);

		ddrval = m_lpDDOverlay->Lock(NULL, &SurfaceDesc, DDLOCK_WAIT, NULL);

		if (SUCCEEDED(ddrval))
		{
			// Compute rectangle to draw
			CRect rect ( 0, 0, SurfaceDesc.dwWidth, SurfaceDesc.dwHeight );
			CRect patternRect=rect;

			if ( ( m_Color & 0xFE000000 ) == 0xFE000000 )
			{
				// Ansi mode
				dwYUY2_Black = 0x80008000;
				dwYUY2_Color = 0x80FF80FF;	// white

				nPadWidth = rect.Width () / ANSI_CONTRAST_BLOCKS;
				nPadHeight = rect.Height () / ANSI_CONTRAST_BLOCKS;

				if ( m_Color == 0xFF000000 )
					nColorFlag = 1;
				else
					nColorFlag = 2;
			}
			else
			{
				// Define rectangle to draw
				int deltaWidth=(int)(rect.Width()*(100-m_rectSizePercent)/100.0);
				int deltaHeight=(int)(rect.Height()*(100-m_rectSizePercent)/100.0);
				
				patternRect.DeflateRect(deltaWidth/2,deltaHeight/2);
				
				// Convert color into YUY2 format
				R = GetRValue ( m_Color );
				G = GetGValue ( m_Color );
				B = GetBValue ( m_Color );

				if ( m_b16_235 )
				{
					ASSERT ( R >= 16 && R <= 235 );
					ASSERT ( G >= 16 && G <= 235 );
					ASSERT ( B >= 16 && B <= 235 );

					R = ( R - 16 ) * 255 / 219;
					G = ( G - 16 ) * 255 / 219;
					B = ( B - 16 ) * 255 / 219;

					if ( R < 0 )
						R = 0;
					else if ( R > 255 )
						R = 255;

					if ( G < 0 )
						G = 0;
					else if ( G > 255 )
						G = 255;

					if ( B < 0 )
						B = 0;
					else if ( B > 255 )
						B = 255;
				}

				Y = ( ( 16829*R + 33039*G +  6416*B + 32768 ) >> 16 ) + 16;
				U = ( ( -9714*R - 19071*G + 28784*B + 32768 ) >> 16 ) + 128;
				V = ( ( 28784*R - 24103*G -  4681*B + 32768 ) >> 16 ) + 128;

				if ( Y < 16 )
					Y = 16;
				else if ( Y > 235 )
					Y = 235;

				if ( U < 16 )
					U = 16;
				else if ( U > 240 )
					U = 240;

				if ( V < 16 )
					V = 16;
				else if ( V > 240 )
					V = 240;

				nColorFlag = 0;
				dwYUY2_Color = (BYTE)Y + (((BYTE)U)<<8) + (((BYTE)Y)<<16) + (((BYTE)V)<<24);
				dwYUY2_Black = 0x80008000;
			}

			for (int i = 0; i < (int)SurfaceDesc.dwHeight; i ++ )
			{
				DWORD*	pPixel = (DWORD*) ((BYTE*)SurfaceDesc.lpSurface + i * SurfaceDesc.lPitch);
				
				if ( nColorFlag )
				{
					// Alternate Black and White rectangles (ANSI contrast)
					for (DWORD j = 0; j < SurfaceDesc.dwWidth / 2; j ++)
					{
						if ( ( ( ( i / nPadHeight ) + ( j * 2 / nPadWidth ) ) & 1 ) == nColorFlag - 1 )
							*pPixel = dwYUY2_Color;
						else
							*pPixel = dwYUY2_Black;
						pPixel++;
					}
				}
				else
				{
					// Colored rectangle mode
					for (int j = 0; j < (int)(SurfaceDesc.dwWidth / 2); j ++)
					{
						if ( i >= patternRect.top && i < patternRect.bottom && j * 2 >= patternRect.left && j * 2 < patternRect.right )
							*pPixel = dwYUY2_Color;
						else
							*pPixel = dwYUY2_Black;
						pPixel++;
					}
				}
			}
			ddrval = m_lpDDOverlay->Unlock(SurfaceDesc.lpSurface);
		}
		else
		{
			GetOverlayErrorText ( ddrval, szError );
			sprintf ( szMsg, "Error \"%s\" while locking overlay surface\nWill use GDI Generator", szError );
			MessageBox(szMsg, "Overlay Generator");
			m_nDisplayMode = DISPLAY_GDI;
			ExitOverlay ();
		}
	}

	if ( ! IsWindowVisible () )
	{
		ShowWindow ( SW_SHOW );
		if ( ! m_bTestOverlay )
		{
			ModifyStyleEx ( 0, WS_EX_TOPMOST );
			BringWindowToTop ();
		}

		if ( m_nDisplayMode == DISPLAY_OVERLAY )
		{
			// Show overlay
			SetOverlayPosition ();
		}
	}

	if ( ! m_bTestOverlay && ! m_bDisableCursorHiding )
		SetCursor ( NULL );
	
	Sleep ( 0 );
	RedrawWindow ();

	if ( ! bDisableWaiting )
	{
		// Sleep 80 ms while dispatching messages to ensure window is really displayed
		MSG		Msg;
		HWND	hEscapeWnd = NULL;
		DWORD	dwWait = GetConfig () -> GetProfileInt ( "Debug", "WaitAfterDisplayPattern", 80 );
		DWORD	dwStart = GetTickCount();
		DWORD	dwNow = dwStart;
		
		// Wait until dwWait time is expired, but ensures all posted messages are treated even if wait time is zero
		while((dwNow - dwStart) < dwWait)
		{
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				if ( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP ) && Msg.wParam == VK_ESCAPE )
				{
					// Do not treat this message, store it for later use
					hEscapeWnd = Msg.hwnd;
				}
				else
				{
					TranslateMessage ( & Msg );
					DispatchMessage ( & Msg );
				}
				Sleep(0);
			}
			dwNow = GetTickCount();
		}

		if ( hEscapeWnd )
		{
			// Escape key detected and stored during above loop: put it again in message loop to allow detection
			::PostMessage ( hEscapeWnd, WM_KEYDOWN, VK_ESCAPE, NULL );
			::PostMessage ( hEscapeWnd, WM_KEYUP, VK_ESCAPE, NULL );
		}
	}
}

void CFullScreenWindow::DisplayAnsiBWRects(BOOL bInvert)
{
	// Use simulated colors
	DisplayRGBColor ( bInvert ? 0xFF000000 : 0xFE000000 );
}

void CFullScreenWindow::DisplayAnimatedBlack()
{
	// Use Animation flags
	m_bAnimated = TRUE;
	m_bWhite = FALSE;
	m_XCurrent = -1;
	DisplayRGBColor ( ColorRGBDisplay ( 0.0 ), TRUE );	// request a black background
	m_IdTimer = SetTimer
	 ( 1, 40, NULL );
	OnTimer ( m_IdTimer );
}

void CFullScreenWindow::DisplayAnimatedWhite()
{
	// Use Animation flags
	m_bAnimated = TRUE;
	m_bWhite = TRUE;
	m_XCurrent = -1;
	DisplayRGBColor ( ColorRGBDisplay ( 100.0 ), TRUE );	// request a white background
	m_IdTimer = SetTimer ( 123, 40, NULL );
	OnTimer ( m_IdTimer );
}


void CFullScreenWindow::DisplayDotPattern( const ColorRGBDisplay& clr , BOOL dot2, UINT nPads)
{
	m_bPatternMode = TRUE;
	m_clrPattern = clr.GetColorRef(m_b16_235);
	m_nPadsPattern = nPads;
	m_dot2Pattern = dot2;
	DisplayRGBColor ( ColorRGBDisplay(0.0), TRUE );
}

void CFullScreenWindow::DisplayHVLinesPattern( const ColorRGBDisplay& clr , BOOL dot2, BOOL vLines)
{
	m_bVLines = vLines ? vLines : FALSE;
	m_bHLines = vLines ? FALSE : TRUE;
	m_clrPattern = clr.GetColorRef(m_b16_235);
	m_dot2Pattern = dot2;
	DisplayRGBColor ( ColorRGBDisplay(0.0), TRUE );
}

void CFullScreenWindow::DisplayColorLevelPattern(INT clrLevel , BOOL dot2, UINT nPads)
{
	m_bColorLevel = TRUE;
	m_iClrLevel = clrLevel;
	m_nPadsPattern = (clrLevel==8 || clrLevel==9) ? 15 : nPads;
	m_dot2Pattern = dot2;
	DisplayRGBColor ( ColorRGBDisplay(0.0), TRUE );
}

void CFullScreenWindow::DisplayConvPattern(BOOL dot2, UINT nPads)
{
	m_bConv = TRUE;
	m_nPadsPattern = nPads;
	m_dot2Pattern = dot2;
	DisplayRGBColor ( ColorRGBDisplay(0.0), TRUE );
}

void CFullScreenWindow::DisplayGeomPattern(BOOL dot2, UINT nPads)
{
	m_bGeom = TRUE;
	m_nPadsPattern = nPads;
	m_dot2Pattern = dot2;
	DisplayRGBColor ( ColorRGBDisplay(0.0), TRUE );
}

void CFullScreenWindow::DisplayColorPattern(BOOL dot2)
{
	m_bColorPattern = TRUE;
	m_dot2Pattern = dot2;
	DisplayRGBColor ( ColorRGBDisplay(0.0), TRUE );
}

void CFullScreenWindow::DisplayPatternPicture(HMODULE hInst, UINT nIDResource, BOOL bResizePict)
{
	m_bPatternPict = TRUE;
	m_hPatternInst = hInst;
	m_bResizePict = bResizePict;
	m_uiPictRess = nIDResource;
	DisplayRGBColor ( ColorRGBDisplay(0.0), TRUE );
}


void CFullScreenWindow::Hide ()
{
    DDOVERLAYFX	DDOverlayFX;

    KillTimer ( m_IdTimer );

	if ( m_lpDDOverlay )
	{
		memset(&DDOverlayFX, 0, sizeof(DDOverlayFX));
		DDOverlayFX.dwSize = sizeof(DDOverlayFX);

		m_lpDDOverlay->UpdateOverlay(NULL, m_lpDDPrimarySurface, NULL, DDOVER_HIDE, &DDOverlayFX);
	}
	
	if ( ! m_bTestOverlay )
		ModifyStyleEx ( WS_EX_TOPMOST, 0 );
	
	ShowWindow ( SW_HIDE );

	ClearFlags ();
}

/////////////////////////////////////////////////////////////////////////////
// CFullScreenWindow message handlers

void CFullScreenWindow::OnPaint() 
{
	int			row, col;
	int			DisplayColor = m_Color;
	BOOL		bDraw = FALSE;
	CRect		rect;
	CBrush		brush;
	CPaintDC	dc(this); // device context for painting

	GetClientRect ( &rect );

	if ( m_nDisplayMode != 0 )
	{
		// Black is the color key
		DisplayColor = 0x00000000;
	}

	if ( ( DisplayColor & 0xFE000000 ) == 0xFE000000 )
	{
		// Ansi mode
		int	nPadWidth = rect.Width () / ANSI_CONTRAST_BLOCKS;
		int nPadHeight = rect.Height () / ANSI_CONTRAST_BLOCKS;
		CRect WhitePad;

		brush.CreateSolidBrush ( RGB(0,0,0) );
		dc.FillRect ( &rect, &brush );
		brush.DeleteObject ();

		if ( m_Color == 0xFF000000 )
			bDraw = TRUE;

		brush.CreateSolidBrush ( RGB(255,255,255) );
		for ( row = 0; row < ANSI_CONTRAST_BLOCKS ; row ++ )
		{
			bDraw = ! bDraw;
			for ( col = 0; col < ANSI_CONTRAST_BLOCKS ; col ++ )
			{
				bDraw = ! bDraw;

				if ( bDraw )
				{
					WhitePad.left = nPadWidth * col;
					WhitePad.right = WhitePad.left + nPadWidth;
					WhitePad.top = nPadHeight * row;
					WhitePad.bottom = WhitePad.top + nPadHeight;
					dc.FillRect ( &WhitePad, &brush );
				}
			}
		}
		brush.DeleteObject ();
	}
	else
	{
		if(m_rectSizePercent < 100)  // Need to draw background
		{
			brush.CreateSolidBrush ( RGB(0,0,0) );
			dc.FillRect ( &rect, &brush );
			brush.DeleteObject ();
		}

		int deltaWidth=(int)(rect.Width()*(100-m_rectSizePercent)/100.0);
		int deltaHeight=(int)(rect.Height()*(100-m_rectSizePercent)/100.0);
		CRect patternRect=rect;
		patternRect.DeflateRect(deltaWidth/2,deltaHeight/2);

		brush.CreateSolidBrush ( DisplayColor );
		dc.FillRect ( &patternRect, &brush );
		brush.DeleteObject ();
	}

	// Pattern Display Common Vars
	CBrush		br0;
	CRect		aRect;
	int			i, j;
	int			minX = m_iOffset;
	int			minY = m_iOffset;
	int			maxX = rect.Width() - m_iOffset;
	int			maxY = rect.Height() - m_iOffset;
	int			stepX = (maxX-m_iOffset) / m_nPadsPattern;
	int			stepY = (maxY-m_iOffset) / m_nPadsPattern;
	int			pSize = m_dot2Pattern ? 2 : 1;
	int			limitY = m_dot2Pattern ? maxY : rect.Height()/2;
	int			offsetY = (limitY-minY)/3;

	
	// Dot Pattern Display
	if (m_bPatternMode)
	{
		br0.CreateSolidBrush ( m_clrPattern );
		for ( i = minX; i < maxX; i+=stepX ) {
			for ( j = minY; j < maxY; j+=stepY ) {
				SetRect ( &aRect, i, j, i + pSize, j + pSize );
				dc.FillRect ( &aRect, &br0 );
			}
		}
		br0.DeleteObject ();
	}

	// H or V Lines Pattern Display
	if (m_bHLines || m_bVLines)
	{
		br0.CreateSolidBrush ( m_clrPattern );
		if (m_bVLines)
			for ( i = minX; i < maxX; i+=(pSize*2) ) {
				SetRect ( &aRect, i, minY, i + pSize, maxY + pSize );
				dc.FillRect ( &aRect, &br0 );
			}
		else // HLines
			for ( j = minY; j < maxY; j+=(pSize*2) ) {
				SetRect ( &aRect, minX, j, maxX + pSize, j + pSize );
				dc.FillRect ( &aRect, &br0 );
			}
		br0.DeleteObject ();
	}

	// Display a Pattern Picture Ressource
	if (m_bPatternPict)
	{
		CRect		aRect;
		int			iW, iH;
		int			destW = rect.Width();
		int			destH = rect.Height();

		CxImage* newImage = new CxImage();
		HRSRC hRsrc = ::FindResource(m_hPatternInst,MAKEINTRESOURCE(m_uiPictRess),"PATTERN");
		if (m_uiPictRess == IDR_PATTERN_TESTIMG)
			newImage->LoadResource(hRsrc,CXIMAGE_FORMAT_JPG,m_hPatternInst);   
		else
			newImage->LoadResource(hRsrc,CXIMAGE_FORMAT_PNG,m_hPatternInst);   
		
		if (m_bResizePict) {
			SetRect ( &aRect, 0, 0, rect.Width(), rect.Height());
		} else {
			int startX, startY, sizeW, sizeH;
			int dispX, dispY, dispW, dispH;

			iW = newImage->GetWidth();
			iH = newImage->GetHeight();

			if (iW <= destW && iH <= destH) 
			{ // image is smaller than dest rect we center it
				SetRect ( &aRect, (destW-iW)/2, (destH-iH)/2, (destW-iW)/2+iW, (destH-iH)/2+iH);
			} else { // image is larger, will be cropped
				if (iW > destW) {
					startX = (iW-destW)/2; sizeW = (iW-destW)/2+destW;
					dispX = 0; dispW = destW;
				} else {
						startX = 0; sizeW = iW;
						dispX = (destW-iW)/2; dispW = dispX+iW;
				}
				if (iH > destH) {
					startY = (iH-destH)/2; sizeH = (iH-destH)/2+destH;
					dispY = 0; dispH = destH;
				} else {
						startY = 0; sizeH = iH;
						dispY = (destH-iH)/2; dispH = dispY+iH;
				}
				SetRect ( &aRect, startX, startY, sizeW, sizeH);
				newImage->Crop(aRect);
				SetRect ( &aRect, dispX, dispY, dispW, dispH);
			}
		}
		newImage->Draw(dc,aRect);

		delete newImage;
	}

	// All or One Color Shading
	if (m_bColorLevel)
	{
		int			cClr = 0;
		int			clrMaxLevel = 255;
		int			clrStartLevel = 0;

		if (m_iClrLevel == 8) {
			clrMaxLevel = 30;
			clrStartLevel = 0;
		} else if (m_iClrLevel == 9) {
			clrMaxLevel = 30;
			clrStartLevel = 255 - clrMaxLevel;
		}

		for (DWORD i = 0; i < m_nPadsPattern; i++)
		{
			cClr = clrStartLevel + (i * clrMaxLevel)/(m_nPadsPattern-1);
			if (m_iClrLevel == 0 || m_iClrLevel >7)
				br0.CreateSolidBrush ( RGB(cClr,cClr,cClr) );
			if (m_iClrLevel == 1)
				br0.CreateSolidBrush ( RGB(cClr,0,0) );
			if (m_iClrLevel == 2)
				br0.CreateSolidBrush ( RGB(0,cClr,0) );
			if (m_iClrLevel == 3)
				br0.CreateSolidBrush ( RGB(0,0,cClr) );
			if (m_iClrLevel == 4) // All Color Shading
			{ 
				br0.CreateSolidBrush ( RGB(cClr,0,0) );
				SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, minY+offsetY);
				dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
				br0.CreateSolidBrush ( RGB(0,cClr,0) );
				SetRect ( &aRect, i*stepX+minX, minY+offsetY, (i+1)*stepX+minX, minY+2*offsetY);
				dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
				br0.CreateSolidBrush ( RGB(0,0,cClr) );
				SetRect ( &aRect, i*stepX+minX, minY+2*offsetY, (i+1)*stepX+minX, limitY);
				dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
			} 
			else // One Color Shading
			{ 
				SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, limitY );
				dc.FillRect ( &aRect, &br0 );
				br0.DeleteObject ();
			}

			if (!m_dot2Pattern) {
			cClr = clrStartLevel + ((m_nPadsPattern-i-1)*clrMaxLevel)/(m_nPadsPattern-1);
			if (m_iClrLevel == 0 || m_iClrLevel >7)
				br0.CreateSolidBrush ( RGB(cClr,cClr,cClr) );
			if (m_iClrLevel == 1)
				br0.CreateSolidBrush ( RGB(cClr,0,0) );
			if (m_iClrLevel == 2)
				br0.CreateSolidBrush ( RGB(0,cClr,0) );
			if (m_iClrLevel == 3)
				br0.CreateSolidBrush ( RGB(0,0,cClr) );
			if (m_iClrLevel == 4) // All Color Shading
			{ 
				br0.CreateSolidBrush ( RGB(cClr,0,0) );
					SetRect ( &aRect, i*stepX+minX, limitY, (i+1)*stepX+minX, limitY+offsetY);
          			dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
				br0.CreateSolidBrush ( RGB(0,cClr,0) );
					SetRect ( &aRect, i*stepX+minX, limitY+offsetY, (i+1)*stepX+minX, limitY+2*offsetY);
					dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
				br0.CreateSolidBrush ( RGB(0,0,cClr) );
					SetRect ( &aRect, i*stepX+minX, limitY+2*offsetY, (i+1)*stepX+minX, maxY);
					dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
			} 
			else // One Color Shading
			{ 
					SetRect ( &aRect, i*stepX+minX, limitY, (i+1)*stepX+minX, maxY );
				dc.FillRect ( &aRect, &br0 );
				br0.DeleteObject ();
			}
		}
	}
	}

	if (m_bColorPattern)
	{
		stepX = (maxX-minX) / 8;

		i=0; br0.CreateSolidBrush ( RGB(0,0,0) ); SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, maxY ); dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
		i=1; br0.CreateSolidBrush ( RGB(255,0,0) ); SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, maxY ); dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
		i=2; br0.CreateSolidBrush ( RGB(0,255,0) ); SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, maxY ); dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
		i=3; br0.CreateSolidBrush ( RGB(255,255,0) ); SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, maxY ); dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
		i=4; br0.CreateSolidBrush ( RGB(0,0,255) ); SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, maxY ); dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
		i=5; br0.CreateSolidBrush ( RGB(255,0,255) ); SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, maxY ); dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
		i=6; br0.CreateSolidBrush ( RGB(0,255,255) ); SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, maxY ); dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
		i=7; br0.CreateSolidBrush ( RGB(255,255,255) ); SetRect ( &aRect, i*stepX+minX, minY, (i+1)*stepX+minX, maxY ); dc.FillRect ( &aRect, &br0 ); br0.DeleteObject ();
	}

	if (m_bConv)
	{
		CBrush		br1, br2;
		int			l;
		int			OrbY = 0;

		maxX = rect.Width();
		maxY = rect.Height();
		stepX = maxX / m_nPadsPattern;
		stepY = maxY / m_nPadsPattern;

		for ( l = 0; l < 3; l++)
		{
            if (l == 0) {
				br0.CreateSolidBrush ( RGB(255,0,0) ); br1.CreateSolidBrush ( RGB(0,255,0) ); br2.CreateSolidBrush ( RGB(0,0,255) );
			} else if (l == 1) {
 				br0.CreateSolidBrush ( RGB(0,255,0) ); br1.CreateSolidBrush ( RGB(0,0,255) ); br2.CreateSolidBrush ( RGB(255,0,0) );
			} else if (l == 2) {
				br0.CreateSolidBrush ( RGB(0,0,255) ); br1.CreateSolidBrush ( RGB(255,0,0) ); br2.CreateSolidBrush ( RGB(0,255,0) );
			}

			for ( i = 0; i < maxX; i+=(stepX*6) )
			{
				for ( j = 0; j < maxY; j+=(stepY*6) )
				{
					SetRect ( &aRect, 0 * stepX * stepX + i - stepX, OrbY +j, 0 * stepX + i + stepX+1, OrbY +j+1 );
					dc.FillRect ( &aRect, &br0 );
					SetRect ( &aRect, 0 * stepX + i, OrbY +j - stepY, 0 * stepX + i+1, OrbY +j + stepY+1 );
					dc.FillRect ( &aRect, &br0 );
				}
				for ( j = 0; j < maxY; j+=(stepY*6) )
				{
					SetRect ( &aRect, 2 * stepX + i - stepX, OrbY +j, 2 * stepX + i + stepX+1, OrbY +j+1 );
					dc.FillRect ( &aRect, &br1 );
					SetRect ( &aRect, 2 * stepX + i, OrbY +j - stepY, 2 * stepX + i+1, OrbY +j + stepY+1 );
					dc.FillRect ( &aRect, &br1 );
				}
				for ( j = 0; j < maxY; j+=(stepY*6) )
				{
					SetRect ( &aRect, 4 * stepX + i - stepX, OrbY +j, 4 * stepX + i + stepX+1, OrbY +j+1 );
					dc.FillRect ( &aRect, &br2 );
					SetRect ( &aRect, 4 * stepX + i, OrbY +j - stepY, 4 * stepX + i+1, OrbY +j + stepY+1 );
					dc.FillRect ( &aRect, &br2 );	
				}
			}
			br0.DeleteObject ();
			br1.DeleteObject ();
			br2.DeleteObject ();
			OrbY+=2*stepY;
		}
	}

	if (m_bGeom)
	{
		CBrush		br1;
		CPen		ePen;
		double		ratio = rect.Width()/rect.Height();
		int			offset;
		int			halfCircle;

		maxX = rect.Width();
		maxY = rect.Height();
		stepY = maxY / m_nPadsPattern;
		stepX = stepY;
		offset = (maxX % stepX) / 2;
		halfCircle = (int)((sqrt(double(maxX * maxX + maxY * maxY)) / 2.0 - (maxY / 2.0) - 1.0) / 2.37 - 1);

		if (halfCircle > (maxY / 4)) 
			halfCircle = (maxY / 4) - 1;

		ePen.CreatePen(PS_SOLID, 1, ColorRGBDisplay(100.0).GetColorRef(m_b16_235));
		br1.CreateSolidBrush (ColorRGBDisplay(0.0).GetColorRef(m_b16_235));
		dc.SelectObject(&ePen);
		dc.SelectObject(&br1);
		dc.Ellipse(halfCircle, 0,maxX-halfCircle,maxY);
		dc.Ellipse(0, 0,halfCircle*2,halfCircle*2);
		dc.Ellipse(0, maxY-halfCircle*2,halfCircle*2,maxY);
		dc.Ellipse(maxX-halfCircle*2, 0,maxX,halfCircle*2);
		dc.Ellipse(maxX-halfCircle*2, maxY-halfCircle*2,maxX,maxY);
		br1.DeleteObject ();
		ePen.DeleteObject ();

		br0.CreateSolidBrush (ColorRGBDisplay(100.0).GetColorRef(m_b16_235));
		for ( i = offset; i < maxX-1; i+=(int)(stepX*ratio) )
		{
			SetRect ( &aRect, i, 0, i+1, maxY );
			dc.FillRect ( &aRect, &br0 );
		}
		for ( j = 0; j < maxY-1; j+=stepY )
		{
			SetRect ( &aRect, 0, j, maxX, j+1 );
			dc.FillRect ( &aRect, &br0 );
		}
		br0.DeleteObject ();
	}
}

void CFullScreenWindow::OnTimer(UINT nIDEvent) 
{					
	BOOL			bOk = TRUE;
	RECT			rect;
	WORD			wYUY2_clr0, wYUY2_clr1, wYUY2_clr2;
	CBrush			br0, br1, br2;
	CDC *			pDC;
	int				i, j;
	int				x0, x1, x2, x0b;
    HRESULT			ddrval;
    DDSURFACEDESC	SurfaceDesc;
	
	if ( m_IdTimer == nIDEvent )
	{
		// Initializations
		if ( m_nDisplayMode == DISPLAY_GDI )
		{
			if ( m_bWhite )
			{
				br0.CreateSolidBrush ( ColorRGBDisplay ( 100.0 ).GetColorRef(m_b16_235) );
				br1.CreateSolidBrush ( ColorRGBDisplay ( 99.0 ).GetColorRef(m_b16_235) );
				br2.CreateSolidBrush ( ColorRGBDisplay ( 98.0 ).GetColorRef(m_b16_235) );
			}
			else
			{
				br0.CreateSolidBrush ( ColorRGBDisplay ( 0.0 ).GetColorRef(m_b16_235) );
				br1.CreateSolidBrush ( ColorRGBDisplay ( 1.0 ).GetColorRef(m_b16_235) );
				br2.CreateSolidBrush ( ColorRGBDisplay ( 2.0 ).GetColorRef(m_b16_235) );
			}
			GetClientRect ( &rect );
			
			pDC = GetDC ();
			pDC -> SelectStockObject ( NULL_PEN );
		}
		else if ( m_nDisplayMode == DISPLAY_OVERLAY )
		{
			// Compute word YU or YV for each pixel (U and V are both neutral because we display grays)
			if ( m_bWhite )
			{
				wYUY2_clr0 = 0x80EB;	// white
				wYUY2_clr1 = 0x80E9;	// IRE 99
				wYUY2_clr2 = 0x80E7;	// IRE 98
			}
			else
			{
				wYUY2_clr0 = 0x8010;	// black
				wYUY2_clr1 = 0x8012;	// IRE 1
				wYUY2_clr2 = 0x8014;	// IRE 2
			}
			
			// Set color directly in YUY2 format inside overlay surface
			memset(&SurfaceDesc, 0, sizeof(SurfaceDesc));
			SurfaceDesc.dwSize = sizeof(SurfaceDesc);

			ddrval = m_lpDDOverlay->Lock(NULL, &SurfaceDesc, DDLOCK_WAIT, NULL);

			if (SUCCEEDED(ddrval))
			{
				// Compute rectangle to draw
				SetRect ( & rect, 0, 0, SurfaceDesc.dwWidth, SurfaceDesc.dwHeight );
			}
			else
			{
				// Error... do nothing
				bOk = FALSE;
				SetRect ( & rect, 0, 0, 0, 0 );
			}
		}

		if ( bOk )
		{
			if ( m_XCurrent < 0 )
			{
				if ( m_rectSizePercent < 100 && m_bWhite )
				{
					m_XMargin = ( ( rect.right / 2 ) * ( 100 - m_rectSizePercent ) ) / 100;
					m_YMargin = ( ( rect.bottom / 2 ) * ( 100 - m_rectSizePercent ) ) / 100;
				}
				else
				{
					m_XMargin = rect.right / 20;
					m_YMargin = rect.bottom / 20;
				}
				m_XWidth = ( ( rect.right - ( m_XMargin * 2 ) ) / 20 ) << 1; // must be a multiple of 2
				m_XCurrent = m_XMargin;
				m_AbsDeltaX = ( ( rect.right / 800 ) + 1 ) << 1;	// must be a multiple of 2
				m_deltaX = m_AbsDeltaX;
				m_XMax = rect.right - m_XMargin - ( m_XWidth * 3 );
				m_YMax = rect.bottom - m_YMargin;

				if ( m_nDisplayMode == DISPLAY_GDI )
				{
					// Initial drawing
					SetRect ( &rect, m_XCurrent, m_YMargin, m_XCurrent + m_XWidth + 1, m_YMax + 1 );
					pDC -> SelectObject ( & br1 );
					pDC -> Rectangle ( & rect );
					
					SetRect ( &rect, m_XCurrent + ( m_XWidth * 2 ), m_YMargin, m_XCurrent + ( m_XWidth * 3 ) + 1, m_YMax + 1 );
					pDC -> SelectObject ( & br2 );
					pDC -> Rectangle ( & rect );
				}
				else if ( m_nDisplayMode == DISPLAY_OVERLAY )
				{
					// Initial drawing
					for ( i = m_YMargin; i < m_YMax; i ++ )
					{
						WORD*	pFirstPixel = (WORD*) ((BYTE*)SurfaceDesc.lpSurface + i * SurfaceDesc.lPitch);
						
						// Draw first rectangle
						for (j = m_XCurrent; j < m_XCurrent + m_XWidth; j ++)
						{
							WORD * pPixel = pFirstPixel + j;
							*pPixel = wYUY2_clr1;
						}

						// Draw second rectangle
						for (j = m_XCurrent + ( m_XWidth * 2 ) ; j < m_XCurrent + ( m_XWidth * 3 ); j ++)
						{
							WORD * pPixel = pFirstPixel + j;
							*pPixel = wYUY2_clr2;
						}
					}
				}
			}

			// Next position
			if ( m_deltaX > 0 )
			{	
				x0 = m_XCurrent;
				x0b = m_XCurrent + ( m_XWidth * 2 );
				x1 = m_XCurrent + m_XWidth;
				x2 = m_XCurrent + ( m_XWidth * 3 );
			}
			else
			{
				x0 = m_XCurrent + ( m_XWidth * 3 ) + m_deltaX;
				x0b = m_XCurrent + m_XWidth + m_deltaX;
				x1 = m_XCurrent + m_deltaX;
				x2 = m_XCurrent + ( m_XWidth * 2 ) + m_deltaX;
			}

			m_XCurrent += m_deltaX;
			if ( m_XCurrent >= m_XMax || m_XCurrent <= m_XMargin )
			{
				// Revert direction
				m_deltaX = -m_deltaX;
			}

			// Draw evolution
			if ( m_nDisplayMode == DISPLAY_GDI )
			{
				SetRect ( &rect, x0, m_YMargin, x0 + m_AbsDeltaX + 1, m_YMax + 1 );
				pDC -> SelectObject ( & br0 );
				pDC -> Rectangle ( & rect );
				
				SetRect ( &rect, x0b, m_YMargin, x0b + m_AbsDeltaX + 1, m_YMax + 1 );
				pDC -> SelectObject ( & br0 );
				pDC -> Rectangle ( & rect );
				
				SetRect ( &rect, x1, m_YMargin, x1 + m_AbsDeltaX + 1, m_YMax + 1 );
				pDC -> SelectObject ( & br1 );
				pDC -> Rectangle ( & rect );
				
				SetRect ( &rect, x2, m_YMargin, x2 + m_AbsDeltaX + 1, m_YMax + 1 );
				pDC -> SelectObject ( & br2 );
				pDC -> Rectangle ( & rect );

				pDC -> SelectStockObject ( WHITE_BRUSH );
				pDC -> SelectStockObject ( BLACK_PEN );
				ReleaseDC ( pDC );
			}
			else if ( m_nDisplayMode == DISPLAY_OVERLAY )
			{
				for ( i = m_YMargin; i < m_YMax; i ++ )
				{
					WORD*	pFirstPixel = (WORD*) ((BYTE*)SurfaceDesc.lpSurface + i * SurfaceDesc.lPitch);
					
					for (j = x0 ; j < x0 + m_AbsDeltaX; j ++ )
					{
						WORD * pPixel = pFirstPixel + j;
						*pPixel = wYUY2_clr0;
					}

					for (j = x0b ; j < x0b + m_AbsDeltaX; j ++ )
					{
						WORD * pPixel = pFirstPixel + j;
						*pPixel = wYUY2_clr0;
					}

					for (j = x1 ; j < x1 + m_AbsDeltaX; j ++ )
					{
						WORD * pPixel = pFirstPixel + j;
						*pPixel = wYUY2_clr1;
					}

					for (j = x2 ; j < x2 + m_AbsDeltaX; j ++ )
					{
						WORD * pPixel = pFirstPixel + j;
						*pPixel = wYUY2_clr2;
					}
				}

				ddrval = m_lpDDOverlay->Unlock(SurfaceDesc.lpSurface);
			}
		}
	}
}

BOOL CFullScreenWindow::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
{
	SetCursor ( NULL );
	return TRUE;
}

void CFullScreenWindow::ClearFlags() 
{
	// Clear working mode flags
	m_bAnimated = FALSE;
	m_bWhite = FALSE;
	m_bPatternMode = FALSE;
	m_clrPattern = ColorRGBDisplay( 0.0 ).GetColorRef(m_b16_235);
	m_nPadsPattern = 25;
	m_dot2Pattern = FALSE;
	m_bHLines = FALSE;
	m_bVLines = FALSE;
	m_bColorLevel = FALSE;
	m_iClrLevel = 0;
	m_bGeom = FALSE;
	m_bConv = FALSE;
	m_bColorPattern = FALSE;
	m_bPatternPict = FALSE;
}

void CFullScreenWindow::SetDisplayMode(UINT nMode) 
{
	if ( m_nDisplayMode == DISPLAY_OVERLAY )
		ExitOverlay ();
	else if ( m_nDisplayMode == DISPLAY_VMR9 )
		ExitVMR9 ();

	m_nDisplayMode = nMode;

    if ( m_nDisplayMode == DISPLAY_OVERLAY )
    {
        if(!InitOverlay())
        {
            m_nDisplayMode = DISPLAY_VMR9;
        }
    }
    if ( m_nDisplayMode == DISPLAY_VMR9 )
    {
        if(!InitVMR9())
        {
            m_nDisplayMode = DISPLAY_GDI;
        }
    }
}


void CFullScreenWindow::SetRGBScale (BOOL b16_235)
{
	m_b16_235 = b16_235;
}

struct MonitorEnumCtx
{
	HMONITOR		m_hSearchedMon;
	LPDIRECTDRAW	m_lpDD;
    HRESULT			m_ddrval;
    HRESULT (WINAPI * m_pDirectDrawCreate) (GUID*, LPDIRECTDRAW*, IUnknown*);
};

static BOOL WINAPI MonitorCallback ( GUID *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext, HMONITOR hMon )
{
	MonitorEnumCtx *	lpCtx = (MonitorEnumCtx *) lpContext;

	if ( lpCtx -> m_hSearchedMon == hMon )
	{
		lpCtx -> m_ddrval = lpCtx->m_pDirectDrawCreate(lpGUID, &lpCtx -> m_lpDD, NULL);
		return FALSE;
	}

	return TRUE;
}

bool CFullScreenWindow::InitOverlay () 
{
	int				i, j;
    HRESULT			ddrval;
    DDSURFACEDESC	SurfaceDesc;
    DDPIXELFORMAT	PixelFormat;
    DDOVERLAYFX		DDOverlayFX;
	MonitorEnumCtx	MonCtx;
	char			szError [ 256 ];
	char			szMsg [ 256 ];
    static HINSTANCE		hLib  = 0;
	HRESULT ( WINAPI * pDirectDrawEnumerateEx ) ( LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags) = NULL;
    HRESULT ( WINAPI * pDirectDrawCreate ) ( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter );

	ASSERT ( m_lpDD == NULL && m_lpDDPrimarySurface == NULL && m_lpDDOverlay == NULL );

    if(!hLib)
        hLib = LoadLibrary("ddraw.dll");

    if ( hLib )
		pDirectDrawCreate = (HRESULT (WINAPI *) (GUID*, LPDIRECTDRAW*, IUnknown*) ) GetProcAddress ( hLib, "DirectDrawCreate" );
    else
    {
        m_nDisplayMode = DISPLAY_GDI;
        return false;
    }

	if ( m_hCurrentMon )
	{
		pDirectDrawEnumerateEx = ( HRESULT (WINAPI*) (LPDDENUMCALLBACKEXA, LPVOID, DWORD) ) GetProcAddress ( hLib, "DirectDrawEnumerateExA" );

		if ( pDirectDrawEnumerateEx )
		{
			MonCtx.m_hSearchedMon = m_hCurrentMon;
			MonCtx.m_lpDD = NULL;
			MonCtx.m_ddrval = 0;
            MonCtx.m_pDirectDrawCreate = pDirectDrawCreate;
			pDirectDrawEnumerateEx ( MonitorCallback, (void *) & MonCtx, DDENUM_ATTACHEDSECONDARYDEVICES );
			m_lpDD = MonCtx.m_lpDD;

			if (FAILED(MonCtx.m_ddrval))
			{
				m_lpDD = NULL;
				GetOverlayErrorText ( MonCtx.m_ddrval, szError );
				sprintf ( szMsg, "Error \"%s\" while calling DirectDrawCreate with display device GUID.\nWill try NULL GUID.", szError );
				MessageBox(szMsg, "Overlay Generator");
			}
		}
		else
		{
			m_lpDD = NULL;
			MessageBox("Error: your Direct Draw version does not allow multi-monitor.\nWill try NULL GUID.", "Overlay Generator");
		}
	}

	if ( m_lpDD == NULL )
	{
		// Retrieve DirectDraw GUID for current monitor

		// Init Direct Draw in normal mode
		ddrval = pDirectDrawCreate(NULL, &m_lpDD, NULL);
		if (FAILED(ddrval))
		{
			GetOverlayErrorText ( ddrval, szError );
			sprintf ( szMsg, "Error \"%s\" while calling DirectDrawCreate with NULL GUID.\nWill use GDI Generator.", szError );
			MessageBox(szMsg, "Overlay Generator");
			m_nDisplayMode = DISPLAY_GDI;
		}
	}

	if ( m_nDisplayMode == DISPLAY_OVERLAY )
	{
		ddrval = m_lpDD->SetCooperativeLevel(AfxGetMainWnd () -> GetSafeHwnd ()/*m_hWnd*/, DDSCL_NORMAL);

		if (FAILED(ddrval))
		{
			GetOverlayErrorText ( ddrval, szError );
			sprintf ( szMsg, "SetCooperativeLevel failed with error \"%s\"\nWill use GDI Generator", szError );
			MessageBox(szMsg, "Overlay Generator");
			m_nDisplayMode = DISPLAY_GDI;
		}
	}

	if ( m_nDisplayMode == DISPLAY_OVERLAY )
	{
		// Create primary surface
		memset(&SurfaceDesc, 0, sizeof(SurfaceDesc));
		SurfaceDesc.dwSize = sizeof(SurfaceDesc);
		SurfaceDesc.dwFlags = DDSD_CAPS;
		SurfaceDesc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		ddrval = m_lpDD->CreateSurface(&SurfaceDesc, &m_lpDDPrimarySurface, NULL);
		
		i = 0;
		while(ddrval == DDERR_NOOVERLAYHW && i < 20)
		{
			i ++;
			Sleep(100);
			ddrval = m_lpDD->CreateSurface(&SurfaceDesc, &m_lpDDPrimarySurface, NULL);
		}

		if (FAILED(ddrval))
		{
			GetOverlayErrorText ( ddrval, szError );
			sprintf ( szMsg, "Error \"%s\" while creating primary surface\nWill use GDI Generator", szError );
			MessageBox(szMsg, "Overlay Generator");
			m_nDisplayMode = DISPLAY_GDI;
		}
	}

	if ( m_nDisplayMode == DISPLAY_OVERLAY )
	{
		// Create overlay surface
		memset(&PixelFormat, 0, sizeof(PixelFormat));
		PixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		PixelFormat.dwFlags = DDPF_FOURCC;
		PixelFormat.dwFourCC = MAKEFOURCC('Y', 'U', 'Y', '2');
		PixelFormat.dwYUVBitCount = 16;

		memset(&SurfaceDesc, 0, sizeof(SurfaceDesc));
		SurfaceDesc.dwSize = sizeof(SurfaceDesc);
		SurfaceDesc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
		SurfaceDesc.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
		SurfaceDesc.dwWidth = ( m_bTestOverlay ? OVERLAY_SMALL_WIDTH : OVERLAY_SURFACE_WIDTH );
		SurfaceDesc.dwHeight = ( m_bTestOverlay ? OVERLAY_SMALL_HEIGHT : OVERLAY_SURFACE_HEIGHT );
		SurfaceDesc.ddpfPixelFormat = PixelFormat;

		ddrval = m_lpDD->CreateSurface(&SurfaceDesc, &m_lpDDOverlay, NULL);
		i = 0;
		while(ddrval == DDERR_NOOVERLAYHW && i < 20 )
		{
			i ++;
			Sleep(100);
			ddrval = m_lpDD->CreateSurface(&SurfaceDesc, &m_lpDDOverlay, NULL);
		}

		if (FAILED(ddrval))
		{
			if ( m_lpDDOverlay )
			{
				m_lpDDOverlay -> Release ();
				m_lpDDOverlay = NULL;
			}
			GetOverlayErrorText ( ddrval, szError );
			sprintf ( szMsg, "Error \"%s\" while creating overlay surface\nWill use GDI Generator", szError );
			MessageBox(szMsg, "Overlay Generator");
			m_nDisplayMode = DISPLAY_GDI;
		}
	}

	if ( m_nDisplayMode == DISPLAY_OVERLAY )
	{
		// Overlay ok. Clean it.
		memset(&SurfaceDesc, 0, sizeof(SurfaceDesc));
		SurfaceDesc.dwSize = sizeof(SurfaceDesc);

		ddrval = m_lpDDOverlay->Lock(NULL, &SurfaceDesc, DDLOCK_WAIT, NULL);

		if (SUCCEEDED(ddrval))
		{
			for ( i = 0; i < (int)SurfaceDesc.dwHeight; i ++ )
			{
				unsigned short* pPixel = (unsigned short*) ((BYTE*)SurfaceDesc.lpSurface + i * SurfaceDesc.lPitch);
				
				for (j = 0; j < (int)SurfaceDesc.dwWidth; j ++)
				{
					*pPixel = 0x8000;	// Y = 0x00 -> black, U (even pixel) or V (odd pixel) = 0x80 -> black
					pPixel++;
				}
			}
			ddrval = m_lpDDOverlay->Unlock(SurfaceDesc.lpSurface);
		}
		else
		{
			GetOverlayErrorText ( ddrval, szError );
			sprintf ( szMsg, "Error \"%s\" while locking overlay surface\nWill use GDI Generator", szError );
			MessageBox(szMsg, "Overlay Generator");
			m_nDisplayMode = DISPLAY_GDI;
		}
	}

	if ( m_nDisplayMode == DISPLAY_OVERLAY )
	{
		// Set overlay position: first hide it
		memset(&DDOverlayFX, 0, sizeof(DDOverlayFX));
		DDOverlayFX.dwSize = sizeof(DDOverlayFX);

		m_lpDDOverlay->UpdateOverlay(NULL, m_lpDDPrimarySurface, NULL, DDOVER_HIDE, &DDOverlayFX);

		if ( IsWindowVisible () )
		{
			// Set overlay position
			SetOverlayPosition ();
		}
        return true;
	}
	else
	{
		// Error: Release direct draw objects
		ExitOverlay ();
        return false;
	}
}

void CFullScreenWindow::ExitOverlay () 
{
	if (m_lpDDOverlay != NULL)
	{
		m_lpDDOverlay->Release();
		m_lpDDOverlay = NULL;
	}

	if (m_lpDDPrimarySurface != NULL) 
	{
		m_lpDDPrimarySurface->Release();
		m_lpDDPrimarySurface = NULL;
	}

    if (m_lpDD != NULL)
    {
        m_lpDD->Release();
        m_lpDD = NULL;
    }
}


void CFullScreenWindow::SetOverlayPosition ()
{
	int				i;
	CRect			rect, overlayrect;
    HRESULT			ddrval;
    DDOVERLAYFX		DDOverlayFX;
	char			szMsg [ 256 ];
	char			szError [ 256 ];

	GetWindowRect ( &rect );

	// Translate window position from desktop coordinates to monitor (primary surface) coordinates
	rect.left -= m_ptMonitorPos.x;
	rect.right -= m_ptMonitorPos.x;
	rect.top -= m_ptMonitorPos.y;
	rect.bottom -= m_ptMonitorPos.y;

    if ( m_lpDDOverlay )
	{
	    overlayrect.left = 0;
	    overlayrect.top = 0;
	    overlayrect.right = ( m_bTestOverlay ? OVERLAY_SMALL_WIDTH : OVERLAY_SURFACE_WIDTH );
		overlayrect.bottom = ( m_bTestOverlay ? OVERLAY_SMALL_HEIGHT : OVERLAY_SURFACE_HEIGHT );

		memset(&DDOverlayFX, 0, sizeof(DDOverlayFX));
		DDOverlayFX.dwSize = sizeof(DDOverlayFX);

		ddrval = m_lpDDOverlay->UpdateOverlay(&overlayrect, m_lpDDPrimarySurface, &rect, DDOVER_SHOW, &DDOverlayFX);
		i = 0;
		while(ddrval == DDERR_NOOVERLAYHW && i < 20)
		{
			Sleep(100);
			ddrval = m_lpDDOverlay->UpdateOverlay(&overlayrect, m_lpDDPrimarySurface, &rect, DDOVER_SHOW, &DDOverlayFX);
		}
		
		if (FAILED(ddrval))
		{
			GetOverlayErrorText ( ddrval, szError );
			sprintf ( szMsg, "Error \"%s\" while updating overlay screen position\nWill use GDI Generator", szError );
			MessageBox(szMsg, "Overlay Generator");
			m_nDisplayMode = DISPLAY_GDI;
			ExitOverlay ();
		}
	}
}

void CFullScreenWindow::GetOverlayErrorText ( HRESULT ddrval, LPSTR lpszText )
{
	switch ( ddrval )
	{
		case DDERR_NOOVERLAYHW:
			 strcpy ( lpszText, "no overlay hardware" ); 
			 break;

		case DDERR_DIRECTDRAWALREADYCREATED:
			 strcpy ( lpszText, "direct draw already created" ); 
			 break;

		case DDERR_GENERIC:
			 strcpy ( lpszText, "generic/undefined error" ); 
			 break;

		case DDERR_INVALIDDIRECTDRAWGUID:
			 strcpy ( lpszText, "invalid direct draw GUID" ); 
			 break;

		case DDERR_INVALIDPARAMS:
			 strcpy ( lpszText, "invalid parameters" ); 
			 break;

		case DDERR_NODIRECTDRAWHW:
			 strcpy ( lpszText, "no direct draw hardware" ); 
			 break;

		case DDERR_OUTOFMEMORY:
			 strcpy ( lpszText, "out of memory" ); 
			 break;

		case DDERR_UNSUPPORTED:
			 strcpy ( lpszText, "unsupported operation" ); 
			 break;

		case DDERR_INCOMPATIBLEPRIMARY:
			 strcpy ( lpszText, "incompatible primary surface" ); 
			 break;

		case DDERR_INVALIDCAPS:
			 strcpy ( lpszText, "invalid caps" ); 
			 break;

		case DDERR_INVALIDOBJECT:
			 strcpy ( lpszText, "invalid object" ); 
			 break;

		case DDERR_INVALIDPIXELFORMAT:
			 strcpy ( lpszText, "invalid pixel format" ); 
			 break;

		case DDERR_NOALPHAHW:
			 strcpy ( lpszText, "no alpha acceleration hardware" ); 
			 break;

		case DDERR_NOCOOPERATIVELEVELSET:
			 strcpy ( lpszText, "no cooperative level set" ); 
			 break;

		case DDERR_NOEMULATION:
			 strcpy ( lpszText, "software emulation not available" ); 
			 break;

		case DDERR_NOEXCLUSIVEMODE:
			 strcpy ( lpszText, "exclusive mode not set" ); 
			 break;

		case DDERR_NOFLIPHW:
			 strcpy ( lpszText, "flipping surfaces not supported" ); 
			 break;

		case DDERR_NOMIPMAPHW:
			 strcpy ( lpszText, "no mipmap texture mapping hardware" ); 
			 break;

		case DDERR_NOZBUFFERHW:
			 strcpy ( lpszText, "no Z-buffer support hardware" ); 
			 break;

		case DDERR_OUTOFVIDEOMEMORY:
			 strcpy ( lpszText, "out of video memory" ); 
			 break;

		case DDERR_PRIMARYSURFACEALREADYEXISTS:
			 strcpy ( lpszText, "primary surface already exists" ); 
			 break;

		case DDERR_UNSUPPORTEDMODE:
			 strcpy ( lpszText, "unsupported mode" ); 
			 break;

		default:
			 sprintf ( lpszText, "#%08X", ddrval );
			 break;
	}
}


bool CFullScreenWindow::InitVMR9 ()
{
	// TODO
    return false;
}

void CFullScreenWindow::ExitVMR9 ()
{
	// TODO
}
