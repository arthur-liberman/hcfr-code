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

#include "stdafx.h"        // Standard windows header file

#ifndef COLOR_MENUBAR				// copied from NewMenu.cpp
#define COLOR_MENUBAR       30
#endif

#include "fxcolor.h"
#include <math.h> 


BOOL fxDrawMenuBorder = TRUE;
BOOL fxUseCustomColor = FALSE;

COLORREF fxColorWindow;
COLORREF fxColorMenu;
COLORREF fxColorSelection;
COLORREF fxColorText;

//
//	Copied from uxtheme.h
//  If you have this new header, then delete these and
//  #include <uxtheme.h> instead!
//
#define ETDT_DISABLE        0x00000001
#define ETDT_ENABLE         0x00000002
#define ETDT_USETABTEXTURE  0x00000004
#define ETDT_ENABLETAB      (ETDT_ENABLE  | ETDT_USETABTEXTURE)

//---- flags to control theming within an app ----

#define STAP_ALLOW_NONCLIENT    (1 << 0)
#define STAP_ALLOW_CONTROLS     (1 << 1)
#define STAP_ALLOW_WEBCONTENT   (1 << 2)

// 
typedef HRESULT (WINAPI * EnableThemeDialogTextureFunct) (HWND, DWORD);
typedef HRESULT (WINAPI * SetWindowThemeFunct)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
typedef void (WINAPI *SetThemeAppPropertiesFunct)(DWORD dwFlags);

//
//	Try to call EnableThemeDialogTexture, if uxtheme.dll is present
//
BOOL FxEnableThemeDialogTexture(HWND hwnd, BOOL bEnable)
{
	HMODULE hUXTheme;
	EnableThemeDialogTextureFunct fnEnableThemeDialogTexture;

	hUXTheme = LoadLibrary(_T("uxtheme.dll"));

	if(hUXTheme)
	{
		fnEnableThemeDialogTexture = 
			(EnableThemeDialogTextureFunct)GetProcAddress(hUXTheme, "EnableThemeDialogTexture");

		if(fnEnableThemeDialogTexture)
		{
			if(bEnable)
				fnEnableThemeDialogTexture(hwnd, ETDT_ENABLETAB);
			else
				fnEnableThemeDialogTexture(hwnd, ETDT_DISABLE);

			
			FreeLibrary(hUXTheme);
			return TRUE;
		}
		else
		{
			// Failed to locate API!
			FreeLibrary(hUXTheme);
			return FALSE;
		}
	}
	else
	{
		// Not running under XP? Just fail gracefully
		return FALSE;
	}
}

BOOL FxEnableWindowTheme(HWND hwnd, BOOL bEnable)
{
	HMODULE hUXTheme;
	SetWindowThemeFunct fnSetWindowThemeFunct;

	hUXTheme = LoadLibrary(_T("uxtheme.dll"));

	if(hUXTheme)
	{
		fnSetWindowThemeFunct = 
			(SetWindowThemeFunct)GetProcAddress(hUXTheme, "SetWindowTheme");

		if(fnSetWindowThemeFunct)
		{
			if(bEnable)
				fnSetWindowThemeFunct(hwnd, NULL, NULL);
			else
				fnSetWindowThemeFunct(hwnd, L" ", L" ");

			
			FreeLibrary(hUXTheme);
			return TRUE;
		}
		else
		{
			// Failed to locate API!
			FreeLibrary(hUXTheme);
			return FALSE;
		}
	}
	else
	{
		// Not running under XP? Just fail gracefully
		return FALSE;
	}
}

BOOL FxEnableControlsTheme(BOOL bEnable)
{
	HMODULE hUXTheme;
	SetThemeAppPropertiesFunct fnSetThemeAppProperties;

	hUXTheme = LoadLibrary(_T("uxtheme.dll"));

	if(hUXTheme)
	{
		fnSetThemeAppProperties = 
			(SetThemeAppPropertiesFunct)GetProcAddress(hUXTheme, "SetThemeAppProperties");

		if(fnSetThemeAppProperties)
		{
			if(bEnable)
				fnSetThemeAppProperties(STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT);
			else
				fnSetThemeAppProperties(STAP_ALLOW_WEBCONTENT);
			
			FreeLibrary(hUXTheme);
			return TRUE;
		}
		else
		{
			// Failed to locate API!
			FreeLibrary(hUXTheme);
			return FALSE;
		}
	}
	else
	{
		// Not running under XP? Just fail gracefully
		return FALSE;
	}
}

void SetFxColors(COLORREF aColorWindow,COLORREF aColorMenu,COLORREF aColorSelection,COLORREF aColorText)
{
	fxColorWindow=aColorWindow;
	fxColorMenu=aColorMenu;
	fxColorSelection=aColorSelection;
	fxColorText=aColorText;

	if (fxUseCustomColor && fxColorText != CLR_DEFAULT)  // disable XP themed controls if text color is changed
		FxEnableControlsTheme(FALSE);	// Text of controls will be drawned with custom color
	else
		FxEnableControlsTheme(TRUE);
}	
		
COLORREF FxGetWindowColor()
{
	if(fxColorWindow == CLR_DEFAULT)
		return GetSysColor(COLOR_WINDOW);
	else
		return fxColorWindow;
}

COLORREF FxGetMenuColor()
{
	if(fxColorMenu == CLR_DEFAULT)
	{
	  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();
	  switch(nMenuDrawMode)
	  {
		  case CNewMenu::STYLE_XP_2003 :
		  case CNewMenu::STYLE_XP_2003_NOBORDER :
//			return 0x00e55400;
//			return GetSysColor(COLOR_ACTIVECAPTION);
//			return MidColor(GetSysColor(COLOR_WINDOW),GetSysColor(COLOR_ACTIVECAPTION));
			return LightenColor(155,GetSysColor(COLOR_ACTIVECAPTION));

		  case CNewMenu::STYLE_XP :
		  case CNewMenu::STYLE_XP_NOBORDER :
		  case CNewMenu::STYLE_ICY:
		  case CNewMenu::STYLE_ICY_NOBORDER:
		  case CNewMenu::STYLE_ORIGINAL :
		  case CNewMenu::STYLE_ORIGINAL_NOBORDER :
			return GetSysColor(COLOR_3DFACE);

		  default:
			return GetSysColor(COLOR_MENU);
	  }
	}
	else
		return fxColorMenu;
}

COLORREF FxGetSelectionColor()
{
	if(fxColorSelection == CLR_DEFAULT)
	{
	  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();
	  switch(nMenuDrawMode)
	  {
		case CNewMenu::STYLE_XP_2003 :
		case CNewMenu::STYLE_XP_2003_NOBORDER :
		  return RGB(255,238,194);  // Orange of office 2003 by default
		  break;
		default:
		  return GetSysColor(COLOR_HIGHLIGHT);
	  }
	}
	else
		return fxColorSelection;
}

COLORREF FxGetTextColor()
{
	if(fxColorText == CLR_DEFAULT)
		return GetSysColor(COLOR_MENUTEXT);
	else
		return fxColorText;
}

COLORREF FxGetSysColor(int nIndex)
{
	if(fxUseCustomColor)
		switch(nIndex)
	{
	    case COLOR_MENUBAR:
		case COLOR_MENU:
		case COLOR_GRADIENTACTIVECAPTION:
			return FxGetMenuColor();
			break;
		case COLOR_3DFACE:
			return FxGetMenuColor();
			break;
		case COLOR_INACTIVECAPTION:
			return DarkenColor(70,FxSaturateColor(150,FxGetMenuColor()));
			break;
		case COLOR_GRADIENTINACTIVECAPTION:
			return LightenColor(40,FxSaturateColor(150,FxGetMenuColor()));
			break;
		case COLOR_ACTIVECAPTION:
			return FxGetMenuColor();
			break;
		case COLOR_WINDOW:
			return FxGetWindowColor();
			break;
		case COLOR_HIGHLIGHT:
			return FxGetSelectionColor();
			break;
		case COLOR_BTNHIGHLIGHT:
			return DarkenColor(50,FxSaturateColor(200,FxGetSelectionColor()));
			break;
		case COLOR_HIGHLIGHTTEXT:
			return FxSaturateColor(150,FxGetTextColor());
			break;
		case COLOR_BTNSHADOW:
		case COLOR_3DDKSHADOW:
//			return DarkenColor(200,FxGetMenuColor());
			break;
		case COLOR_3DLIGHT:
//			return LightenColor(50,FxGetMenuColor());
			break;
		case COLOR_MENUTEXT:
		case COLOR_BTNTEXT:
			return FxGetTextColor();
			break;
		case COLOR_GRAYTEXT:
			return LightenColor(110,GrayColor(FxGetTextColor()));
			break;
		case COLOR_INACTIVECAPTIONTEXT:
		case COLOR_WINDOWTEXT:
			return GetSysColor(nIndex);
			break;
		default:
			return GetSysColor(nIndex);
	}
    return GetSysColor(nIndex);
}

HRESULT FxLocalGetThemeColor(HANDLE hTheme,
    int iPartId,
    int iStateId,
    int iColorId,
    COLORREF *pColor)
{
  switch(iColorId)
  {
	case 3821:	// color caption
		*pColor=FxGetMenuColor();
		return TRUE;
		break;
	case 3805:	// color window
		*pColor=FxGetWindowColor();
		return TRUE;
		break;
  }
  return FALSE;
}

void FxGetMenuBarColors(COLORREF& colorTop, COLORREF& colorBottom)
{
  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();
  COLORREF colorBack;

  switch(nMenuDrawMode)
  {
	  case CNewMenu::STYLE_XP_2003 :
	  case CNewMenu::STYLE_XP_2003_NOBORDER :
		CNewMenu::GetMenuBarColor2003(colorTop,colorBottom);
		return;

	  case CNewMenu::STYLE_ICY:
	  case CNewMenu::STYLE_ICY_NOBORDER:
		colorBack=CNewMenu::GetMenuColor();
		colorTop=LightenColor(30,colorBack);
		colorBottom=DarkenColor(30,colorBack);
		return;

	  case CNewMenu::STYLE_XP:
	  case CNewMenu::STYLE_XP_NOBORDER:
  		colorTop=colorBottom=CNewMenu::GetMenuBarColorXP();
		return;

	  default:
		colorTop=colorBottom=FxGetSysColor(COLOR_WINDOW);
		return;
  }
}

COLORREF FxGetMenuBgColor()
{
  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();

  switch(nMenuDrawMode)
  {
	  case CNewMenu::STYLE_XP_2003 :
	  case CNewMenu::STYLE_XP_2003_NOBORDER :
	  case CNewMenu::STYLE_ICY:
	  case CNewMenu::STYLE_ICY_NOBORDER:
	  case CNewMenu::STYLE_XP:
	  case CNewMenu::STYLE_XP_NOBORDER:
		return DarkenColor(20,FxGetSysColor(COLOR_WINDOW));

	  case CNewMenu::STYLE_ORIGINAL:
	  case CNewMenu::STYLE_ORIGINAL_NOBORDER:
		  return FxGetSysColor(COLOR_3DFACE);

	  default:
		return FxGetSysColor(COLOR_WINDOW);
  }
}

void FxGetMenuBgColors(COLORREF& colorTop, COLORREF& colorBottom)
{
  UINT nMenuDrawMode = CNewMenu::GetMenuDrawMode();

  COLORREF colorBg=FxGetMenuBgColor();

  switch(nMenuDrawMode)
  {
	  case CNewMenu::STYLE_XP_2003 :
	  case CNewMenu::STYLE_XP_2003_NOBORDER :
		  colorTop=DarkenColor(128,colorBg);
		  colorBottom=LightenColor(128,colorTop);
		  return;

	  case CNewMenu::STYLE_ICY:
	  case CNewMenu::STYLE_ICY_NOBORDER:
		  colorTop=DarkenColor(128,colorBg);
		  colorBottom=LightenColor(128,colorTop);
		  return;

	  case CNewMenu::STYLE_XP:
	  case CNewMenu::STYLE_XP_NOBORDER:
		  colorTop=DarkenColor(128,colorBg);
		  colorBottom=LightenColor(128,colorTop);
		  return;

	  default:
		  colorTop=colorBottom=colorBg;
		  return;
  }
}

void RGBtoHSV(double R, double G, double B, double& H, double& S, double& V)
{
	double  var_R = ( R / 255.0 );                     //RGB values = From 0 to 255
	double var_G = ( G / 255.0 );
	double var_B = ( B / 255.0 );

	double  var_Min = min(min( var_R, var_G), var_B );    //Min. value of RGB
	double  var_Max = max(max( var_R, var_G), var_B );    //Max. value of RGB
	double  del_Max = var_Max - var_Min ;            //Delta RGB value

	V = var_Max;

	if ( del_Max == 0 )                     //This is a gray, no chroma...
	{
	   H = 0;                                //HSL results = From 0 to 1
	   S = 0;
	}
	else                                    //Chromatic data...
	{
	   S = del_Max / var_Max;

	   double  del_R = ( ( ( var_Max - var_R ) / 6.0 ) + ( del_Max / 2.0 ) ) / del_Max;
	   double  del_G = ( ( ( var_Max - var_G ) / 6.0 ) + ( del_Max / 2.0 ) ) / del_Max;
	   double  del_B = ( ( ( var_Max - var_B ) / 6.0 ) + ( del_Max / 2.0 ) ) / del_Max;

	   if      ( var_R == var_Max ) H = del_B - del_G;
	   else if ( var_G == var_Max ) H = ( 1.0 / 3.0 ) + del_R - del_B;
	   else if ( var_B == var_Max ) H = ( 2.0 / 3.0 ) + del_G - del_R;

	   if ( H < 0 ) H += 1.0;
	   if ( H > 1 ) H -= 1.0;
	}
}

void HSVtoRGB(double H, double S, double V, double& R, double& G, double& B)
{
	if ( S == 0 )                       //HSL values = From 0 to 1
	{
	   R = V * 255.0;                      //RGB results = From 0 to 255
	   G = V * 255.0;
	   B = V * 255.0;
	}
	else
	{
	   double var_h = H * 6;
	   double var_i = floor( var_h );             //Or ... var_i = floor( var_h )
	   double var_1 = V * ( 1 - S );
	   double var_2 = V * ( 1 - S * ( var_h - var_i ) );
	   double var_3 = V * ( 1 - S * ( 1 - ( var_h - var_i ) ) );

	   double var_r,var_g,var_b;

	   if      ( var_i == 0 ) { var_r = V     ; var_g = var_3 ; var_b = var_1; }
	   else if ( var_i == 1 ) { var_r = var_2 ; var_g = V     ; var_b = var_1; }
	   else if ( var_i == 2 ) { var_r = var_1 ; var_g = V     ; var_b = var_3; }
	   else if ( var_i == 3 ) { var_r = var_1 ; var_g = var_2 ; var_b = V;     }
	   else if ( var_i == 4 ) { var_r = var_3 ; var_g = var_1 ; var_b = V;     }
	   else                   { var_r = V     ; var_g = var_1 ; var_b = var_2; }

	   R = var_r * 255.0;                  //RGB results = From 0 to 255
	   G = var_g * 255.0;
	   B = var_b * 255.0;
	} 
}

void RGBtoHSL(double R, double G, double B, double& H, double& S, double& L)
{
    double minval = min(R, min(G, B));
    double maxval = max(R, max(G, B));
    double mdiff  = double(maxval) - double(minval);
    double msum   = double(maxval) + double(minval);
   
    L = msum / 510.0f;

    if (maxval == minval) 
    {
      S = 0.0f;
      H = 0.0f; 
    }   
    else 
    { 
      double rnorm = (maxval - R  ) / mdiff;      
      double gnorm = (maxval - G) / mdiff;
      double bnorm = (maxval - B ) / mdiff;   

      S = (L <= 0.5f) ? (mdiff / msum) : (mdiff / (510.0f - msum));

      if (R   == maxval) H = 60.0f * (6.0f + bnorm - gnorm);
      if (G == maxval) H = 60.0f * (2.0f + rnorm - bnorm);
      if (B  == maxval) H = 60.0f * (4.0f + gnorm - rnorm);
      if (H > 360.0f) H = H - 360.0f;
	}
}

double ToRGB1(double rm1, double rm2, double rh)
{
  if      (rh > 360.0f) rh -= 360.0f;
  else if (rh <   0.0f) rh += 360.0f;
 
  if      (rh <  60.0f) rm1 = rm1 + (rm2 - rm1) * rh / 60.0f;   
  else if (rh < 180.0f) rm1 = rm2;
  else if (rh < 240.0f) rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;      
                   
  return (rm1 * 255);
}

void HSLtoRGB(double H, double S, double L, double& R, double& G, double& B)
{
    if (S == 0.0) // Grauton, einfacher Fall
    {
      R = G = B = unsigned char(L * 255.0);
    }
    else
    {
      double rm1, rm2;
         
      if (L <= 0.5f) rm2 = L + L * S;  
      else                     rm2 = L + S - L * S;
      rm1 = 2.0f * L - rm2;   
      R   = ToRGB1(rm1, rm2, H + 120.0f);   
      G = ToRGB1(rm1, rm2, H);
      B  = ToRGB1(rm1, rm2, H - 120.0f);
    }
}

COLORREF FxSaturateColor(int aSatPercent,COLORREF aColor)
{
	double h,s,l;
	double r,g,b;

	RGBtoHSV(GetRValue(aColor),GetGValue(aColor),GetBValue(aColor),h,s,l);
//	RGBtoHSL(GetRValue(aColor),GetGValue(aColor),GetBValue(aColor),h,s,l);
	double s1=s * aSatPercent / 100.0;
	if(s1 < 1.0)
		s=s1;
	HSVtoRGB(h,s,l,r,g,b);
//	HSLtoRGB(h,s,l,r,g,b);
	return RGB(r,g,b);
}

