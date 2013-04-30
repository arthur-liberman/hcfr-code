// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__53A7E878_541F_4044_A0E6_3A5BE7C30A41__INCLUDED_)
#define AFX_STDAFX_H__53A7E878_541F_4044_A0E6_3A5BE7C30A41__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// undefine to allow use of orginal devlib dlls
// these should not be linked in distributions
//#define USE_NON_FREE_CODE

#if _MSC_VER >= 1600
// visual studio 2010 ships with mfc that doesn't like 
// low values of WINVER
#define WINVER 0x0501
#else
#define WINVER 0x0410
#endif
#define _WIN32_IE 0x0601
#define _CRT_SECURE_NO_WARNINGS
// disable warning C4786: symbol greater than 255 character, okay to ignore
#pragma warning(disable : 4786)


#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#include "memdc.h"

#define _SCB_REPLACE_MINIFRAME
#define _SCB_MINIFRAME_CAPTION
#include "Tools/sizecbar.h"
#include "Tools/scbarg.h"
#include "Tools/scbarcf.h"
#define baseCMeasureBar CSizingControlBarCF

// enable new toolbar support
#define USE_NEW_DOCK_BAR
// enable new menubar support
#define USE_NEW_MENU_BAR

// we are building this project as a DLL
//#define NEW_MENU_DLL

#include "NewMenu.h"
#include "NewMenuBar.h"
#include "NewToolBar.h"
#include "NewProperty.h"
#include "NewStatusbar.h"
#include "NewDialogBar.h"

#endif // !defined(AFX_STDAFX_H__53A7E878_541F_4044_A0E6_3A5BE7C30A41__INCLUDED_)
