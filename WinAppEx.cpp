
// WinAppEx.cpp : implementation file
// Copyright(c) Armen Hakobyan 2002.
// http://www.codeproject.com/script/profile/whos_who.asp?vt=arts&id=25653
// mailto:armenh@web.am

#include "stdafx.h"
#include "WinAppEx.h"
#include <atlbase.h>	// Required for the CRegKey

// CHWinAppEx
 
IMPLEMENT_DYNCREATE( CHWinAppEx, CWinApp)
CHWinAppEx::CHWinAppEx( LPCTSTR lpszAppName /*NULL*/ )
		 : CWinApp( lpszAppName )
		 , m_uMsgCheckInst( NULL )
{	
}

CHWinAppEx::~CHWinAppEx( void )
{	
	m_uMsgCheckInst = NULL;	
}

BEGIN_MESSAGE_MAP(CHWinAppEx, CWinApp)	
END_MESSAGE_MAP()

// CHWinAppEx message handlers

BOOL CHWinAppEx::PreTranslateMessage( LPMSG pMsg )
{
	if( pMsg->message == m_uMsgCheckInst )
		return OnAnotherInstanceMessage( pMsg );	
	
	return CWinApp::PreTranslateMessage( pMsg );	
}

// CHWinAppEx functions

BOOL CHWinAppEx::InitInstance( LPCTSTR lpszUID /*NULL*/ )
{		
	if( lpszUID != NULL )
	{
		// If two different applications register the same message string, 
		// the applications return the same message value. The message 
		// remains registered until the session ends. If the message is 
		// successfully registered, the return value is a message identifier 
		// in the range 0xC000 through 0xFFFF.

		ASSERT( m_uMsgCheckInst == NULL ); // Only once
		m_uMsgCheckInst = ::RegisterWindowMessage( lpszUID );
		ASSERT( m_uMsgCheckInst >= 0xC000 && m_uMsgCheckInst <= 0xFFFF );

		// If another instance is found, pass document file name to it
		// only if command line contains FileNew or FileOpen parameters
		// and exit instance. In other cases such as FilePrint, etc., 
		// do not exit instance because framework will process the commands 
		// itself in invisible mode and will exit.

		if( this->FindAnotherInstance( lpszUID ) )
		{
			
			CCommandLineInfo cmd;
			ParseCommandLine( cmd );
			
			if( cmd.m_nShellCommand == CCommandLineInfo::FileNew ||
			    cmd.m_nShellCommand == CCommandLineInfo::FileOpen )
			{
				WPARAM wpCmdLine = NULL;
				if( cmd.m_nShellCommand == CCommandLineInfo::FileOpen )
					wpCmdLine = (ATOM)::GlobalAddAtom( cmd.m_strFileName );

				this->PostInstanceMessage( wpCmdLine, NULL );
				return FALSE;
			}
		}
	}
		
	return CWinApp::InitInstance();
}

BOOL CHWinAppEx::FindAnotherInstance( LPCTSTR lpszUID )
{
	ASSERT( lpszUID != NULL );
		
	// Create a new mutex. If fails means that process already exists:

	HANDLE hMutex  = ::CreateMutex( NULL, FALSE, lpszUID );
	DWORD  dwError = ::GetLastError();	
			
	if( hMutex != NULL )
	{
		// Close mutex handle
		::ReleaseMutex( hMutex );
		hMutex = NULL;

		// Another instance of application is running:

		if( dwError == ERROR_ALREADY_EXISTS || dwError == ERROR_ACCESS_DENIED )			
			return TRUE;
	}

	return FALSE;
}

BOOL CHWinAppEx::PostInstanceMessage( WPARAM wParam, LPARAM lParam )
{
	ASSERT( m_uMsgCheckInst != NULL );

	// One process can terminate the other process by broadcasting the private message 
	// using the BroadcastSystemMessage function as follows:
		
	DWORD dwReceipents = BSM_APPLICATIONS;

	if( this->EnableTokenPrivilege( SE_TCB_NAME ) )
		dwReceipents |= BSM_ALLDESKTOPS;
		
	// Send the message to all other instances.
	// If the function succeeds, the return value is a positive value.
	// If the function is unable to broadcast the message, the return value is –1.

	LONG lRet = ::BroadcastSystemMessage( BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | 
					BSF_POSTMESSAGE, &dwReceipents, m_uMsgCheckInst, wParam, lParam );

	return (BOOL)( lRet != -1 );
}

BOOL CHWinAppEx::OnAnotherInstanceMessage( LPMSG pMsg )
{
	// Get command line arguments (if any) from new instance.
	
	BOOL bShellOpen = FALSE;
	
	if( pMsg->wParam != NULL ) 
	{
		::GlobalGetAtomName( (ATOM)pMsg->wParam, m_lpCmdLine, _MAX_FNAME );			
		::GlobalDeleteAtom(  (ATOM)pMsg->wParam );		
		bShellOpen = TRUE;		
	}
	
	// If got valid command line then try to open the document -
	// CDocManager will popup main window itself. Otherwise, we
	// have to popup the window 'manually' :

	if( m_pDocManager != NULL && bShellOpen ) 
	{
		CWaitCursor wait;		
		m_pDocManager->OpenDocumentFile( m_lpCmdLine );
	}
	else if( ::IsWindow( m_pMainWnd->GetSafeHwnd() ) )
	{
		// Does the main window have any popups ? If has, 
		// bring the main window or its popup to the top
		// before showing:

		CWnd* pPopupWnd = m_pMainWnd->GetLastActivePopup();
		pPopupWnd->BringWindowToTop();

		// If window is not visible then show it, else if
		// it is iconic, restore it:

		if( !m_pMainWnd->IsWindowVisible() )
			m_pMainWnd->ShowWindow( SW_SHOWNORMAL ); 
		else if( m_pMainWnd->IsIconic() )
			m_pMainWnd->ShowWindow( SW_RESTORE );
		
		// And finally, bring to top after showing again:

		pPopupWnd->BringWindowToTop();
		pPopupWnd->SetForegroundWindow(); 
	}
	
	return TRUE;
}

BOOL CHWinAppEx::EnableTokenPrivilege( LPCTSTR lpszSystemName, 
									  BOOL    bEnable /*TRUE*/ )
{
	ASSERT( lpszSystemName != NULL );
	BOOL bRetVal = FALSE;
	
	if( ::GetVersion() < 0x80000000 ) // NT40/2K/XP // afxData ???
	{
		HANDLE hToken = NULL;		
		if( ::OpenProcessToken( ::GetCurrentProcess(), 
			TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken ) )
		{
			TOKEN_PRIVILEGES tp = { 0 };
			
			if( ::LookupPrivilegeValue( NULL, lpszSystemName, &tp.Privileges[0].Luid ) )
			{
				tp.PrivilegeCount = 1;
				tp.Privileges[0].Attributes = ( bEnable ? SE_PRIVILEGE_ENABLED : 0 );
								
				// To determine whether the function adjusted all of the 
				// specified privileges, call GetLastError:
				
				if( ::AdjustTokenPrivileges( hToken, FALSE, &tp, 
					 sizeof( TOKEN_PRIVILEGES ), (PTOKEN_PRIVILEGES)NULL, NULL ) )
				{				
					bRetVal = ( ::GetLastError() == ERROR_SUCCESS );
				}
			}
			::CloseHandle( hToken );
		}
	}

	return bRetVal;
}

void CHWinAppEx::RegisterShellFileTypesEx( BOOL bCompat   /*FALSE*/, 
										  BOOL bRegister /*TRUE*/ )
{
	// Register all application document types:

	if( bRegister == TRUE )
		CWinApp::RegisterShellFileTypes( bCompat );
		
	// Now register SDI document dde open.
	// Loop through the document templates:

	POSITION pos = GetFirstDocTemplatePosition();
	while( pos != NULL )
	{		
		CString strFileTypeId( _T("") );
		CDocTemplate* pTemplate = GetNextDocTemplate( pos );

		if( pTemplate->GetDocString( strFileTypeId,
		    CDocTemplate::regFileTypeId ) && !strFileTypeId.IsEmpty() )
		{
			// CDocTemplate::windowTitle is present only in the document template 
			// for SDI applications. So, we detected SDI application and should 
			// overregister shell file types :
			
			CString strTemp( _T("") );
			if ( pTemplate->GetDocString( strTemp, CDocTemplate::windowTitle ) &&
				 !strTemp.IsEmpty() )
			{
				// path\shell\open\ddeexec = [open("%1")]
				strTemp.Format( _T( "%s\\shell\\open\\%s" ), (LPCTSTR)strFileTypeId, _T( "ddeexec" ) );
				
				#if ( _MFC_VER >= 0x0700 )
					CRegKey reg( HKEY_CLASSES_ROOT );
				#else
					CRegKey reg;
					reg.Attach( HKEY_CLASSES_ROOT );
				#endif

				if( bRegister )
				{				
					if( reg.SetKeyValue( strTemp, _T( "[open(\"%1\")]" ) ) == ERROR_SUCCESS )
					{
						strTemp += _T( "\\Application" );

						OSVERSIONINFO osvi = { 0 };
						osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
						::GetVersionEx( & osvi );

						if( !( osvi.dwPlatformId == VER_PLATFORM_WIN32_NT &&
							   osvi.dwMajorVersion == 5 && 
							   osvi.dwMinorVersion >= 1 ) )
						{
							reg.SetKeyValue( strTemp, AfxGetAppName() );
						}
						else
							reg.DeleteSubKey( strTemp );
					}
				}
				else // Unregister 'ddeexec' registry entry:
					reg.RecurseDeleteKey( strTemp );
				
				#if ( _MFC_VER >= 0x0700 )
					reg.Flush();
				#else
					::RegFlushKey( reg );
				#endif
				reg.Close(); 
			}
		}
	}

	// Unregister all application document types:

	if( bRegister == FALSE )
		CWinApp::UnregisterShellFileTypes();
}

//