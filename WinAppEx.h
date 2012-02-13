
// Copyright(c) Armen Hakobyan 2002.
// http://www.codeproject.com/script/profile/whos_who.asp?vt=arts&id=25653
// mailto:armenh@web.am

#ifndef __WINAPP_H__
#define __WINAPP_H__
#pragma once

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

// Auxiliary AFX functions
/*
#if (_MFC_VER < 0x0700) // 0x0421, 0x0600, 0x0700
	#include <..\src\afximpl.h>
#else
	#include <..\src\mfc\afximpl.h>	
#endif
*/

// CWinAppEx

class CWinAppEx : public CWinApp
{
	DECLARE_DYNCREATE( CWinAppEx )

protected:
	CWinAppEx( LPCTSTR lpszAppName = NULL );
	virtual ~CWinAppEx( void );
	
public:	
	BOOL PostInstanceMessage( WPARAM wParam, LPARAM lParam );		
	BOOL EnableTokenPrivilege( LPCTSTR lpszSystemName, BOOL bEnable = TRUE );
	
public:
	AFX_INLINE void RegisterShellFileTypes( BOOL bCompat = FALSE );
	AFX_INLINE void UnregisterShellFileTypes( void );

public:
	virtual BOOL InitInstance( LPCTSTR lpszUID = NULL );
	virtual BOOL PreTranslateMessage( LPMSG pMsg );
	virtual BOOL OnAnotherInstanceMessage( LPMSG pMsg) ;

protected:
	BOOL FindAnotherInstance( LPCTSTR lpszUID );
	void RegisterShellFileTypesEx( BOOL bCompat = FALSE, BOOL bRegister = TRUE );
		
protected:
	DECLARE_MESSAGE_MAP()

protected:	
	UINT	m_uMsgCheckInst;
	
};

AFX_INLINE CWinAppEx* GetApp( void )
	{ return STATIC_DOWNCAST( CWinAppEx, AfxGetApp() ); }

AFX_INLINE void CWinAppEx::RegisterShellFileTypes( BOOL bCompat /*FALSE*/ )
	{ RegisterShellFileTypesEx( bCompat, TRUE ); }

AFX_INLINE void CWinAppEx::UnregisterShellFileTypes( void) 
	{ RegisterShellFileTypesEx( FALSE, FALSE ); }

#endif // __WINAPP_H__