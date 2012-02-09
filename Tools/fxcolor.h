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
/////////////////////////////////////////////////////////////////////////////

#ifndef __FxColor_H_
#define __FxColor_H_

// gradient defines (if not already defined)
#ifndef COLOR_GRADIENTACTIVECAPTION
#define COLOR_GRADIENTACTIVECAPTION     27
#define COLOR_GRADIENTINACTIVECAPTION   28
#define SPI_GETGRADIENTCAPTIONS         0x1008
#endif


extern BOOL fxDrawMenuBorder;
extern BOOL fxUseCustomColor;

extern void SetFxColors(COLORREF aColorWindow,COLORREF aColorMenu,COLORREF aColorSelection,COLORREF aColorText);
extern COLORREF FxGetSysColor(int nIndex);
extern HRESULT FxLocalGetThemeColor(HANDLE hTheme,int iPartId,int iStateId,int iColorId,COLORREF *pColor);

extern void FxGetMenuBarColors(COLORREF& colorTop, COLORREF& colorBottom);
extern COLORREF FxGetMenuBgColor();
extern void FxGetMenuBgColors(COLORREF& colorTop, COLORREF& colorBottom);
extern COLORREF FxSaturateColor(int aSatPercent,COLORREF aColor);
#endif
