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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// ColorHCFR.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ColorHCFRConfig.h"

#include "MainFrm.h"
#include "MultiFrm.h"
#include "DataSetDoc.h"
#include "DocTempl.h"

#include "Views/CIEChartView.h"
#include "Views/LuminanceHistoView.h"
#include "Views/RGBHistoView.h"
#include "Views/ColorTempHistoView.h"
#include "Views/MainView.h"

#include "CreditsCtrl.h"	// Added by ClassView
#include "Hyperlink.h"
#include "Tools/MainWndPlacement.h"
#include "DocEnumerator.h"	//Ki

#include "EyeOneSensor.h"
#include "SerialCom.h"

#include "ximage.h"

// Include for device interface (this device interface is outside GNU GPL license)
#include "devlib\CHCFRDI3.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CColorHCFRApp

BEGIN_MESSAGE_MAP(CColorHCFRApp, CWinApp)
	//{{AFX_MSG_MAP(CColorHCFRApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_DATASET, OnUpdateViewDataSet)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_CIECHART, OnUpdateViewCiechart)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_COLORTEMPHISTO, OnUpdateViewColortemphisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_LUMINANCEHISTO, OnUpdateViewLuminancehisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_GAMMAHISTO, OnUpdateViewGammahisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_NEARBLACKHISTO, OnUpdateViewNearBlackhisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_NEARWHITEHISTO, OnUpdateViewNearWhitehisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_RGBHISTO, OnUpdateViewRgbhisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_SATLUMHISTO, OnUpdateViewSatLumhisto)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_SATLUMSHIFT, OnUpdateViewSatLumshift)
	ON_UPDATE_COMMAND_UI(IDM_VIEW_MEASURESCOMBO, OnUpdateViewMeasuresCombo)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CColorHCFRApp construction

CColorHCFRApp::CColorHCFRApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
	m_hCIEThread = NULL;
	m_hCIEEvent = CreateEvent ( NULL, TRUE, FALSE, NULL );	// Manual event starting non-signaled
	m_hCursorMeasure = NULL;
	m_hSavedCursor = NULL;
	m_pPatternWnd = NULL;
	m_hLuxThread = NULL;
	m_bStopLuxThread = FALSE;
	InitializeCriticalSection ( & m_LuxCritSec );
	memcpy ( m_RefLuxParams, "\0\0", 2 );
	m_CurrentLuxValue = 0.0;
	m_bLowLuxValue = FALSE;
	m_bHighLuxValue = FALSE;
	m_bLowestLuxScale = FALSE;
	m_bHighestLuxScale = FALSE;
	m_dwPreviousLuxValueTime = 0;
	m_dwLuxValueTime = 0;
	m_bInsideMeasure = FALSE;
	m_dwMeasureStartTime = 0;
	m_NbMeasures = 0;
	m_bLastMeasureOutOfRange = FALSE;
	m_bFirstMeasureWithNewScaleOk = FALSE;
	m_bLuxMeasureValid = FALSE;
	m_nFirstValidMeasure = 0;
	m_MeasuredLuxValue = 0.0;
	m_MeasuredLuxValue_Initial = 0.0;
}

CColorHCFRApp::~CColorHCFRApp()
{
	CEyeOneSensor::CloseEyeOnePipe ();

	if ( m_hCIEThread )
	{
		WaitForSingleObject ( m_hCIEEvent, 10000 );
		CloseHandle ( m_hCIEThread );
		m_hCIEThread = NULL;
	}
	CloseHandle ( m_hCIEEvent );
	m_hCIEEvent = NULL;

	if ( m_hLuxThread )
	{
		m_bStopLuxThread = TRUE;
		
		if ( WaitForSingleObject ( m_hLuxThread, 10000 ) == WAIT_TIMEOUT )
			TerminateThread ( m_hLuxThread, 0 );
		
		CloseHandle ( m_hLuxThread );
		m_hLuxThread = NULL;
	}

	DeleteCriticalSection ( & m_LuxCritSec );
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CColorHCFRApp object

CColorHCFRApp theApp;

CColorHCFRConfig * GetConfig() 
{ 
	return theApp.m_pConfig; 
}

CDataSetDoc * GetDataRef()	//Ki
{ 
	return theApp.m_pReferenceData; 
}

void SetDataRef(CDataSetDoc *m_pRefData)	//Ki
{ 
	theApp.m_pReferenceData = m_pRefData; 
}

void	UpdateDataRef(BOOL ActiveDataRef, CDataSetDoc * pDoc)
{
	CDocEnumerator docEnumerator;	// Update all views of all docs
	CDocument* pOtherDoc;

	if (ActiveDataRef) 
	{
		// Pointer update on reference data
		SetDataRef(pDoc);		
	}
	else 
	{
		SetDataRef(NULL);		
	}

	while ((pOtherDoc=docEnumerator.Next())!=NULL) 
	   pOtherDoc->UpdateAllViews(NULL, UPD_DATAREFDOC);

	AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );
}

/////////////////////////////////////////////////////////////////////////////
// CColorHCFRApp initialization

BOOL CColorHCFRApp::InitInstance()
{
	// Create splash screen
	int		cx, cy;
	RECT	Rect;
	CxImage SplashImage;
	CWnd	SplashWnd;
	CDC *	pDC;

	HRSRC hRsrc = ::FindResource(AfxGetInstanceHandle(),MAKEINTRESOURCE(IDR_SPLASH_SCREEN),"SPLASHSCREEN");
	SplashImage.LoadResource(hRsrc,CXIMAGE_FORMAT_PNG,AfxGetInstanceHandle());   

	cx = SplashImage.GetWidth();
	cy = SplashImage.GetHeight();

	Rect.left = ( GetSystemMetrics(SM_CXSCREEN) - cx ) / 2;
	Rect.top = ( GetSystemMetrics(SM_CYSCREEN) - cy ) / 2;
	Rect.right = Rect.left + cx;
	Rect.bottom = Rect.top + cy;

	SplashWnd.CreateEx(WS_EX_TOPMOST,AfxRegisterWndClass(0,NULL,NULL,NULL),NULL,WS_POPUP|WS_VISIBLE, Rect, NULL, 0 );
	
	pDC = SplashWnd.GetDC ();

	HBITMAP	hMaskBmp, hBmp, hBmp2;
	HBITMAP hOldMaskBmp, hOldBmp, hOldBmp2;
	CDC		MaskDC, MemDC, MemDC2;

	// Draw bitmap in memory
	MemDC.CreateCompatibleDC ( pDC );
	hBmp = CreateCompatibleBitmap ( pDC -> m_hDC, cx, cy );
	hOldBmp = (HBITMAP) MemDC.SelectObject ( hBmp );

	MemDC2.CreateCompatibleDC ( pDC );
	hBmp2 = CreateCompatibleBitmap ( pDC -> m_hDC, cx, cy );
	hOldBmp2 = (HBITMAP) MemDC2.SelectObject ( hBmp2 );

	SetRect ( & Rect, 0, 0, cx, cy );
	SplashImage.Draw ( MemDC.m_hDC, Rect );

	// Create mask bitmap
	MaskDC.CreateCompatibleDC ( pDC );
	hMaskBmp = CreateBitmap ( cx, cy, 1, 1, NULL );
	hOldMaskBmp = (HBITMAP) MaskDC.SelectObject ( hMaskBmp );

	BitBlt ( MaskDC.m_hDC, 0, 0, cx, cy, MemDC.m_hDC, 0, 0, SRCCOPY );

	MaskDC.SelectObject ( hOldMaskBmp );
	MaskDC.DeleteDC ();

	BitBlt ( MemDC2.m_hDC, 0, 0, cx, cy, pDC -> m_hDC, 0, 0, SRCCOPY );
	MaskBlt ( MemDC2.m_hDC, 0, 0, cx, cy, MemDC.m_hDC, 0, 0, hMaskBmp, 0, 0, MAKEROP4(MERGEPAINT,SRCCOPY) );
	BitBlt ( pDC -> m_hDC, 0, 0, cx, cy, MemDC2.m_hDC, 0, 0, SRCCOPY );

	DeleteObject ( hMaskBmp );

	MemDC.SelectObject ( hOldBmp );
	MemDC.DeleteDC ();
	DeleteObject ( hBmp );

	MemDC2.SelectObject ( hOldBmp2 );
	MemDC2.DeleteDC ();
	DeleteObject ( hBmp2 );
	
	SplashWnd.ReleaseDC ( pDC );

	// Beginning of initializations
	m_pColorReference = new CColorReference (SDTV,D65,2.22);
	m_pConfig = new CColorHCFRConfig();

	m_pReferenceData = NULL; //Ki
//	SetRegistryKey(_T("ColorHCFR"));	// used for MRU
	m_pszRegistryKey=NULL;	// used for storing parameters in .ini
	m_pszProfileName=_tcsdup(m_pConfig->m_iniFileName);

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	if( !CWinAppEx::InitInstance( _T("{B7C56C2E-F858-4f2b-9054-2F5626377F03}") ) )
		if(!m_pConfig->m_doMultipleInstance)
			return FALSE;

	AfxEnableControlContainer();

	CreateCIEBitmaps ( TRUE );
	
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CMyMultiDocTemplate* pDocTemplate;
	pDocTemplate = new CMyMultiDocTemplate(
		IDR_MESURETYPE,
		RUNTIME_CLASS(CDataSetDoc),
		//RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CMultiFrame), // custom MDI child frame
		RUNTIME_CLASS(CMainView));
	AddDocTemplate(pDocTemplate);

	// create main MDI Frame window
	CMainFrame* pMainFrame = new CMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
		return FALSE;
	m_pMainWnd = pMainFrame;

	// Loading toolbar icons into the menu 
	pDocTemplate->m_NewMenuShared.LoadToolBar(IDR_MENUBARGRAPH);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	if(cmdInfo.m_nShellCommand != CCommandLineInfo::FileNew)
		// Dispatch commands specified on the command line
		if (!ProcessShellCommand(cmdInfo))			// disabled to avoid default doc creation
			return FALSE;

	EnableShellOpen ();
	RegisterShellFileTypes ();

	// Destroy splash screen before displaying main window
	SplashWnd.DestroyWindow ();

	// The main window has been initialized, so show and update it.
	if(GetConfig()->m_doSavePosition)
		CMainWndPlacement::InitialShow(m_nCmdShow);	// For window restoration 
	else
		pMainFrame->ShowWindow(m_nCmdShow);

	pMainFrame->UpdateWindow();

	// Load measure cursor
	m_hCursorMeasure = LoadCursor(IDC_CURSOR_MEASURE);
	m_hSavedCursor = NULL;

	m_LuxPort = GetConfig () -> GetProfileString ( "Defaults", "Luxmeter", "" );
	if ( ! m_LuxPort.IsEmpty () )
		StartLuxMeasures ();

	return TRUE;
}


//////////////////////////////////////////////////////////////////////
// Help handling in property sheets

IMPLEMENT_DYNAMIC(CPropertyPageWithHelp, CPropertyPage)

BEGIN_MESSAGE_MAP(CPropertyPageWithHelp, CPropertyPage)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDHELP,OnHelp)
END_MESSAGE_MAP()

void CPropertyPageWithHelp::OnContextMenu(CWnd* pWnd, CPoint pos)
{
	Default ();
}

IMPLEMENT_DYNAMIC(CPropertySheetWithHelp, CPropertySheet)

BEGIN_MESSAGE_MAP(CPropertySheetWithHelp, CPropertySheet)
	ON_WM_HELPINFO()
	ON_COMMAND(IDHELP,OnHelp)
END_MESSAGE_MAP()

void CPropertySheetWithHelp::OnHelp()
{
	UINT	nId = ( (CPropertyPageWithHelp *) GetActivePage () ) -> GetHelpId ( NULL );

	if ( nId )
		GetConfig () -> DisplayHelp ( nId, NULL );
}

BOOL CPropertySheetWithHelp::OnHelpInfo ( HELPINFO * pHelpInfo )
{
	UINT	nId = ( (CPropertyPageWithHelp *) GetActivePage () ) -> GetHelpId ( NULL );

	if ( nId )
		GetConfig () -> DisplayHelp ( nId, NULL );

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	CStatic	m_Version;
	CHyperLink	m_hcfrHyperLink;
	CHyperLink	m_donationHyperLink;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CCreditsCtrl m_wndCredits;
	//{{AFX_MSG(CAboutDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	DDX_Control(pDX, IDC_STATIC_VERSION, m_Version);
	DDX_Control(pDX, IDC_HOMECINEMA_HYPERLINK, m_hcfrHyperLink);
	DDX_Control(pDX, IDC_DONATION_HYPERLINK, m_donationHyperLink);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CColorHCFRApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CColorHCFRApp message handlers


int CColorHCFRApp::ExitInstance() 
{
	DisconnectDevice3();

	delete m_pConfig;
	delete m_pColorReference;
	
	return CWinApp::ExitInstance();
}

void CColorHCFRApp::OnUpdateViewDataSet(CCmdUI* pCmdUI) 
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewCiechart(CCmdUI* pCmdUI) 
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewColortemphisto(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewLuminancehisto(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewGammahisto(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewNearBlackhisto(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewNearWhitehisto(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewRgbhisto(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewSatLumhisto(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewSatLumshift(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

void CColorHCFRApp::OnUpdateViewMeasuresCombo(CCmdUI* pCmdUI)
{
	pCmdUI -> SetCheck ( FALSE );
	pCmdUI -> Enable ( FALSE );
}

extern void DrawCIEChart(CDC* pDC,int aWidth, int aHeight, BOOL doFullChart, BOOL doShowBlack, BOOL bCIEuv);
extern void DrawCIEChartWhiteSurrounding(CDC* pDC, int cxMax, int cyMax, BOOL bCIEuv );

#define CX_CIE_BITMAP	600
#define CY_CIE_BITMAP	600

DWORD WINAPI CreateCIEBitmapsThreadFunc ( LPVOID )
{	
	CDC ScreenDC;
	CDC BmpDC;
	CDC WhiteBmpDC;
	CColorHCFRApp *	pApp = GetColorApp();
    
	ScreenDC.CreateDC ( "DISPLAY", NULL, NULL, NULL );
	BmpDC.CreateCompatibleDC ( & ScreenDC );
	WhiteBmpDC.CreateCompatibleDC ( & ScreenDC );

	pApp -> m_chartBitmap.CreateCompatibleBitmap ( & ScreenDC, CX_CIE_BITMAP, CY_CIE_BITMAP );
	pApp -> m_lightenChartBitmap.CreateCompatibleBitmap ( & ScreenDC, CX_CIE_BITMAP, CY_CIE_BITMAP );

	pApp -> m_chartBitmap_white.CreateCompatibleBitmap ( & ScreenDC, CX_CIE_BITMAP, CY_CIE_BITMAP );

	pApp -> m_chartBitmap_uv.CreateCompatibleBitmap ( & ScreenDC, CX_CIE_BITMAP, CY_CIE_BITMAP );
	pApp -> m_lightenChartBitmap_uv.CreateCompatibleBitmap ( & ScreenDC, CX_CIE_BITMAP, CY_CIE_BITMAP );

	pApp -> m_chartBitmap_uv_white.CreateCompatibleBitmap ( & ScreenDC, CX_CIE_BITMAP, CY_CIE_BITMAP );

	ScreenDC.DeleteDC ();

    CBitmap * pOldBitmap = BmpDC.SelectObject ( & pApp -> m_chartBitmap );
	BmpDC.FillSolidRect ( 0, 0, CX_CIE_BITMAP, CY_CIE_BITMAP, RGB(0,0,0) );
	DrawCIEChart ( & BmpDC, CX_CIE_BITMAP, CY_CIE_BITMAP, FALSE, TRUE, FALSE );

    CBitmap * pOldBitmap2 = WhiteBmpDC.SelectObject ( & pApp -> m_chartBitmap_white );
	WhiteBmpDC.BitBlt ( 0, 0, CX_CIE_BITMAP, CY_CIE_BITMAP, & BmpDC, 0, 0, SRCCOPY );
	DrawCIEChartWhiteSurrounding( & WhiteBmpDC, CX_CIE_BITMAP, CY_CIE_BITMAP, FALSE );

    BmpDC.SelectObject ( & pApp -> m_lightenChartBitmap );
	BmpDC.FillSolidRect ( 0, 0, CX_CIE_BITMAP, CY_CIE_BITMAP, RGB(0,0,0) );
	DrawCIEChart ( & BmpDC, CX_CIE_BITMAP, CY_CIE_BITMAP, TRUE, TRUE, FALSE );

    BmpDC.SelectObject ( & pApp -> m_chartBitmap_uv );
	BmpDC.FillSolidRect ( 0, 0, CX_CIE_BITMAP, CY_CIE_BITMAP, RGB(0,0,0) );
	DrawCIEChart ( & BmpDC, CX_CIE_BITMAP, CY_CIE_BITMAP, FALSE, TRUE, TRUE );

    WhiteBmpDC.SelectObject ( & pApp -> m_chartBitmap_uv_white );
	WhiteBmpDC.BitBlt ( 0, 0, CX_CIE_BITMAP, CY_CIE_BITMAP, & BmpDC, 0, 0, SRCCOPY );
	DrawCIEChartWhiteSurrounding( & WhiteBmpDC, CX_CIE_BITMAP, CY_CIE_BITMAP, TRUE );

    BmpDC.SelectObject ( & pApp -> m_lightenChartBitmap_uv );
	BmpDC.FillSolidRect ( 0, 0, CX_CIE_BITMAP, CY_CIE_BITMAP, RGB(0,0,0) );
	DrawCIEChart ( & BmpDC, CX_CIE_BITMAP, CY_CIE_BITMAP, TRUE, TRUE, TRUE );

	BmpDC.SelectObject ( pOldBitmap );
	WhiteBmpDC.SelectObject ( pOldBitmap2 );
	
	BmpDC.DeleteDC ();
	WhiteBmpDC.DeleteDC ();

	Sleep ( 0 );
	SetEvent ( pApp -> m_hCIEEvent );
	
	if ( pApp -> m_hCIEThread )
	{
		CloseHandle ( pApp -> m_hCIEThread );
		pApp -> m_hCIEThread = NULL;
	}
		
	return 0;
}

void CColorHCFRApp::CreateCIEBitmaps ( BOOL bBackGround )
{
	if ( bBackGround )
	{
		// Background call is only for initial bitmap creation
		m_hCIEThread = ::CreateThread ( NULL, 65536, CreateCIEBitmapsThreadFunc, NULL, 0, NULL );
	}
	else
	{
		// Test if the thread is still running
		if ( WAIT_TIMEOUT == WaitForSingleObject ( m_hCIEEvent, 0 ) )
		{
			if ( m_hCIEThread )
			{
				// Increase background thread priority
				::SetThreadPriority ( m_hCIEThread, THREAD_PRIORITY_NORMAL );
			}
			
			WaitForSingleObject ( m_hCIEEvent, INFINITE );
		}

		if ( m_chartBitmap.m_hObject )
			m_chartBitmap.DeleteObject ();

		if ( m_lightenChartBitmap.m_hObject )
			m_lightenChartBitmap.DeleteObject ();

		if ( m_chartBitmap_white.m_hObject )
			m_chartBitmap_white.DeleteObject ();

		if ( m_chartBitmap_uv.m_hObject )
			m_chartBitmap_uv.DeleteObject ();

		if ( m_lightenChartBitmap_uv.m_hObject )
			m_lightenChartBitmap_uv.DeleteObject ();

		if ( m_chartBitmap_uv_white.m_hObject )
			m_chartBitmap_uv_white.DeleteObject ();
	}

	if ( m_hCIEThread == NULL )
	{
		// Cannot create thread: simply call function
		CWaitCursor wait;
		CreateCIEBitmapsThreadFunc ( NULL );
	}
	else
	{
		::SetThreadPriority ( m_hCIEThread, THREAD_PRIORITY_BELOW_NORMAL );
	}
}
	
DWORD WINAPI LuxMeterThreadFunc ( LPVOID lParam )
{	
	int				i;
	BOOL			bRecord = FALSE;
	BOOL			bEndPacket = FALSE;
	BYTE			data = 0;
	DWORD			dwBytesTransferred;
	DWORD			dwStartTick;
	CColorHCFRApp *	pApp = (CColorHCFRApp *) lParam;
	char			szBuf [ 256 ];
	HANDLE			hCom;
	COMMCONFIG		cc;
	COMMTIMEOUTS	ct;

	hCom = CreateFile ( pApp -> m_LuxPort, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );

	if ( hCom == INVALID_HANDLE_VALUE )
	{
		// Error, exit with error code 1
		return 1;
	} 

	memset ( & cc, 0, sizeof(cc) );
	cc.dwSize = sizeof(cc);
	cc.dcb.DCBlength = sizeof(cc.dcb);
	
	dwBytesTransferred = sizeof(cc);
	GetDefaultCommConfig ( pApp -> m_LuxPort, & cc, & dwBytesTransferred );

	cc.dcb.BaudRate = CBR_9600;
	cc.dcb.fBinary = TRUE;
	cc.dcb.ByteSize = 8;
	cc.dcb.Parity = NOPARITY;
	cc.dcb.StopBits = ONESTOPBIT;

	if ( ! SetCommConfig ( hCom, & cc, sizeof(cc) ) )
	{
		CloseHandle ( hCom );
		
		// Exit with error code 2
		return 2;
	} 

	if ( ! SetCommMask ( hCom, 0 ) )
	{
		CloseHandle ( hCom );

		// Exit with error code 3
		return 3;
	} 

	if ( ! PurgeComm ( hCom, PURGE_RXCLEAR ) )
	{
		CloseHandle ( hCom );

		// Exit with error code 4
		return 4;
	}

	ct.ReadIntervalTimeout = 0;
	ct.ReadTotalTimeoutConstant = 1000;
	ct.ReadTotalTimeoutMultiplier = 0;
	ct.WriteTotalTimeoutConstant = 0;
	ct.WriteTotalTimeoutMultiplier = 0;

	if ( ! SetCommTimeouts ( hCom, & ct ) )
	{
		CloseHandle ( hCom );

		// Exit with error code 3
		return 3;
	} 

	do
	{
		if ( ReadFile ( hCom, & data, 1, & dwBytesTransferred, NULL ) )
		{
			if ( dwBytesTransferred > 0 )
			{
				if ( data == 0x02 )
				{
					dwStartTick = GetTickCount ();
					bRecord = TRUE;
					i = 0;
				}

				if ( bRecord )
				{
					szBuf [ i ++ ] = (char) data;

					if ( data == 0x0D )
					{
						bRecord = FALSE;
						szBuf [ i ] = '\0';

						// Interpret data
						pApp -> SetLuxmeterValue ( szBuf, dwStartTick );

						bEndPacket = TRUE;
					}
				}
			}
		}
		if ( bEndPacket )
		{
			bEndPacket = FALSE;
			Sleep ( 10 );
		}
	} while ( ! pApp -> m_bStopLuxThread );

	CloseHandle ( hCom );
	return 0;
}

void CColorHCFRApp::SetPatternWindow ( CWnd * pWnd )
{
	m_pPatternWnd = pWnd;
}

void CColorHCFRApp::BringPatternWindowToTop ()
{
	if ( m_pPatternWnd )
	{
		m_pPatternWnd -> ModifyStyleEx ( 0, WS_EX_TOPMOST );
		m_pPatternWnd -> BringWindowToTop ();
		m_pPatternWnd -> UpdateWindow ();
	}
}

void CColorHCFRApp::BeginMeasureCursor ()
{
	EndMeasureCursor ();
	m_hSavedCursor = ::SetCursor ( m_hCursorMeasure );
}

void CColorHCFRApp::RestoreMeasureCursor ()
{
	if ( m_hSavedCursor )
		::SetCursor ( m_hCursorMeasure );
}

void CColorHCFRApp::EndMeasureCursor ()
{
	if ( m_hSavedCursor )
	{
		::SetCursor ( m_hSavedCursor );
		m_hSavedCursor = NULL;
	}
}

int CColorHCFRApp::InMeasureMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	::SetCursor ( ::LoadCursor ( NULL, IDC_ARROW ) );

	int nRet = m_pMainWnd -> MessageBox ( lpText, lpCaption, uType );
	
	BringPatternWindowToTop ();
	RestoreMeasureCursor ();
	
	return nRet;
}

void CColorHCFRApp::StartLuxMeasures ()
{
	// Test is thread is already running
	if ( m_hLuxThread )
	{
		// Stop it
		m_bStopLuxThread = TRUE;
		
		if ( WaitForSingleObject ( m_hLuxThread, 10000 ) == WAIT_TIMEOUT )
			TerminateThread ( m_hLuxThread, 0 );
		
		CloseHandle ( m_hLuxThread );
		m_hLuxThread = NULL;
	}

	// Start a new thread
	if ( ! m_LuxPort.IsEmpty () )
	{
		m_bStopLuxThread = FALSE;
		m_hLuxThread = ::CreateThread ( NULL, 65536, LuxMeterThreadFunc, this, 0, NULL );
		:: SetThreadPriority ( m_hLuxThread, THREAD_PRIORITY_ABOVE_NORMAL );
	}
}

BOOL CColorHCFRApp::IsLuxmeterRunning ()
{
	return ( m_hLuxThread != NULL && ! m_bStopLuxThread && m_dwLuxValueTime != 0 && m_dwLuxValueTime + 10000 > GetTickCount () );
}


void CColorHCFRApp::SetLuxmeterValue ( LPCSTR lpszString, DWORD dwStartTick )
{
	int		n = strlen ( lpszString );
	double	d = 0.0;
	double	dmul = 1.0;
	BOOL	bLow = FALSE;
	BOOL	bHigh = FALSE;
	BOOL	bFtCd = FALSE;
	BOOL	bLowestScale = FALSE;
	BOOL	bHighestScale = FALSE;

	if ( strncmp ( lpszString, "\002411", 4 ) == 0 
		&& ( lpszString [ 4 ] == '5' || lpszString [ 4 ] == '6' ) 
		&& ( lpszString [ 5 ] == '0' || lpszString [ 5 ] == '1' ) 
		&& ( strchr ( "012378", lpszString [ 6 ] ) != NULL ) 
		&& ( strncmp ( lpszString + 7, "0000", 4 ) == 0 )
		&& lpszString [ n - 1 ] == 0x0D )
	{
		if ( lpszString [ 4 ] == '6' )
			bFtCd = TRUE;

		switch ( lpszString [ 6 ] )
		{
			case '0':	
				 dmul = 1.0;
				 break;

			case '1':
				 dmul = 0.1;
				 break;

			case '2':
				 dmul = 0.01;
				 if ( ! bFtCd )
					bLowestScale = TRUE;
				 break;

			case '3':
				 dmul = 0.001;
				 bLowestScale = TRUE;
				 break;

			case '7':
				 dmul = 10.0;
				 break;

			case '8':
				 dmul = 100.0;
				 bHighestScale = TRUE;
				 break;
		}

		if ( lpszString [ 5 ] == '1' )
			dmul = -dmul;

		if ( isdigit ( lpszString [ 11 ] ) )
		{
			d = ( (double) atoi ( lpszString + 11 ) ) * dmul;

			if ( bFtCd )
			{
				// Convert Ft-cd in Lux
				d = d / 0.0929;
			}
		}
		else if ( lpszString [ 11 ] == 0x18 )
		{
			bHigh = TRUE;
		}
		else if ( lpszString [ 11 ] == 0x19 )
		{
			bLow = TRUE;
		}

		if ( m_RefLuxParams [ 0 ] != lpszString [ 4 ] || m_RefLuxParams [ 1 ] != lpszString [ 6 ] )
		{
			// Save new luxmeter configuration, but ignore measure (it is incoherent)
			m_RefLuxParams [ 0 ] = lpszString [ 4 ];
			m_RefLuxParams [ 1 ] = lpszString [ 6 ];
		}
		else
		{
			// Save data
			EnterCriticalSection ( & m_LuxCritSec );
			m_CurrentLuxValue = d;
			m_bLowLuxValue = bLow;
			m_bHighLuxValue = bHigh;
			m_bLowestLuxScale = bLowestScale;
			m_bHighestLuxScale = bHighestScale;
			m_dwPreviousLuxValueTime = m_dwLuxValueTime;
			m_dwLuxValueTime = dwStartTick;
			
			if ( m_bInsideMeasure )
			{
				// Ignore obsolete measures
				if ( m_dwPreviousLuxValueTime >= m_dwMeasureStartTime )
				{
					if ( ! m_bLowLuxValue && ! m_bHighLuxValue )
					{
						if ( m_bLastMeasureOutOfRange )
						{
							// We were is "out of scale range" mode, advisor dialog is certainly displayed
							// Indicate measures are now valid, but do no reset m_bLastMeasureOutOfRange flag:
							// it will be reset by the main thread
							m_bFirstMeasureWithNewScaleOk = TRUE;
							m_NbMeasures = 0;
						}
						else
						{
							// Store actual measure in measure list
							if ( m_NbMeasures >= sizeof ( m_LastLuxValues ) / sizeof ( m_LastLuxValues [ 0 ] ) )
							{
								// Shift values to keep only the last ones
								memmove ( m_LastLuxValues, m_LastLuxValues + 1, sizeof ( m_LastLuxValues ) - sizeof ( m_LastLuxValues [ 0 ] ) );
								m_NbMeasures --;

								if ( m_nFirstValidMeasure > 0 )
									m_nFirstValidMeasure --;
							}
							m_LastLuxValues [ m_NbMeasures ++ ] = m_CurrentLuxValue;

							if ( m_bLuxMeasureValid )
							{
								int		i, NbValues;
								int		nNbGrow, nNbDec;
								double	d;
								double	dValues [ sizeof ( m_LastLuxValues ) / sizeof ( m_LastLuxValues [ 0 ] ) ]; 
								
								// Retrieve last values since first valid value. 
								// Only values which are in a 0.5% interval around last two measures average are taken into account.
								dValues [ 0 ] = m_LastLuxValues [ m_NbMeasures - 1 ];
								dValues [ 1 ] = m_LastLuxValues [ m_NbMeasures - 2 ];
								NbValues = 2;
								d = ( dValues [ 0 ] + dValues [ 1 ] ) / 2.0;
								for ( i = m_NbMeasures - 3; i >= m_nFirstValidMeasure; i -- )
								{
									if ( NbValues == 0 || ( ( fabs ( m_LastLuxValues [ i ] - d ) / d ) < 0.005 ) )
										dValues [ NbValues ++ ] = m_LastLuxValues [ i ];
								}

								// Analyze those values
								d = dValues [ 0 ];
								nNbGrow = 0;
								nNbDec = 0;
								for ( i = 1; i < NbValues ; i ++ )
								{
									d += dValues [ i ];
									if ( dValues [ i ] > dValues [ i - 1 ] )
										nNbGrow ++;

									if ( dValues [ i ] < dValues [ i - 1 ] )
										nNbDec ++;
								}

								if ( nNbGrow == 0 || nNbDec == 0 )
								{
									// Asymptotic case: use last value
									m_MeasuredLuxValue = dValues [ 0 ];
								}
								else
								{
									// Alternance case: use averaging
									m_MeasuredLuxValue = ( d / (double) NbValues );
								}
								
#ifdef _DEBUG
{
	char	szBuf [ 256 ];
	if ( ( fabs ( m_MeasuredLuxValue_Initial - m_MeasuredLuxValue ) / m_MeasuredLuxValue ) > 0.0001 )
	{
		sprintf ( szBuf, "Luxmeter approximation: %6f (actual) instead of %6f (initial) : %.1f %%\n", m_MeasuredLuxValue, m_MeasuredLuxValue_Initial, ( fabs ( m_MeasuredLuxValue_Initial - m_MeasuredLuxValue ) / m_MeasuredLuxValue ) * 100.0 );
		OutputDebugString ( szBuf );
	}
}
#endif
							}
							else
							{
								// Analyze list of values to check if current measure is valid
								if ( m_NbMeasures >= 3 )
								{
									// We have at least 3 values, we can check validity
									double d0 = m_LastLuxValues [ m_NbMeasures - 3 ];
									double d1 = m_LastLuxValues [ m_NbMeasures - 2 ];
									double d2 = m_LastLuxValues [ m_NbMeasures - 1 ];
									
									// Test if values n and n-2 are within a 2% range
									if ( ( fabs ( d2 - d0 ) / d0 ) < 0.02 )
									{
										// Check if value n-1 is outside [n-2,n] interval, and value n-1 and n within a 0.5% range
										// This is the case of "alternance", eg grow first, then decrease a little to stabilize
										if ( d1 >= max ( d0, d2 ) || d1 <= min ( d0, d2 ) && ( fabs ( d1 - d2 ) / d2 ) < 0.005 )
										{
											// Ok: use average between the two last measures
											m_MeasuredLuxValue = ( d2 + d1 ) / 2.0;
											m_MeasuredLuxValue_Initial = m_MeasuredLuxValue;
											m_bLuxMeasureValid = TRUE;
											m_nFirstValidMeasure = m_NbMeasures - 2;
										}
										else 
										{
											// Check if value n-1 is between values n-2 and n, with value n-1 very near value n (0.2%)
											// This is the case of asymptotic stabilization.
											if ( d1 >= min ( d0, d2 ) || d1 <= max ( d0, d2 ) && ( fabs ( d1 - d2 ) / d2 ) < 0.002 )
											{
												// This value looks good: perform special checking to increase black measure sensitivity
												// Test if values are increasing, or if values are not too near black, or if value is decreasing to black with a very small decrease
												if ( d2 >= d1 || d2 >= 10.0 || ( ( d0 - d2 ) / d2 ) < 0.0001 )
												{
													// Ok: use the last measured value
													m_MeasuredLuxValue = d2;
													m_MeasuredLuxValue_Initial = m_MeasuredLuxValue;
													m_bLuxMeasureValid = TRUE;
													m_nFirstValidMeasure = m_NbMeasures - 1;
												}
											}
										}
									}
								}
							}
						}

#ifdef _DEBUG
char	szBuf [ 256 ];
if ( m_bLuxMeasureValid )
	sprintf ( szBuf, "Luxmeter value: %6f -> %6f validated\n", m_CurrentLuxValue, m_MeasuredLuxValue );
else
	sprintf ( szBuf, "Luxmeter value: %6f\n", m_CurrentLuxValue );
OutputDebugString ( szBuf );
#endif
					}
					else
					{
						m_bLastMeasureOutOfRange = TRUE;
						m_bFirstMeasureWithNewScaleOk = FALSE;
						m_NbMeasures = 0;
						m_bLuxMeasureValid = FALSE;
					}
				}
			}

			LeaveCriticalSection ( & m_LuxCritSec );
			if ( m_pMainWnd )
				m_pMainWnd -> PostMessage ( WM_COMMAND, IDM_REFRESH_LUX );
		}
	}

}

void CColorHCFRApp::BeginLuxMeasure ()
{
	if ( IsLuxmeterRunning () )
	{
		EnterCriticalSection ( & m_LuxCritSec );

		m_bInsideMeasure = TRUE;
		m_dwMeasureStartTime = GetTickCount ();

		m_NbMeasures = 0;
		m_bLastMeasureOutOfRange = FALSE;
		m_bFirstMeasureWithNewScaleOk = FALSE;
		m_bLuxMeasureValid = FALSE;
		m_MeasuredLuxValue = 0.0;
		m_MeasuredLuxValue_Initial = 0.0;

		LeaveCriticalSection ( & m_LuxCritSec );
	}
	else
	{
		m_dwMeasureStartTime = 0;
	}
}

UINT CColorHCFRApp::GetLuxMeasure ( double * pLuxValue )
{
	UINT	nReturnCode;
	BOOL	bContinue;

	if ( m_bInsideMeasure )
	{
		do
		{
			EnterCriticalSection ( & m_LuxCritSec );

			if ( m_bLuxMeasureValid )
			{
				// Ok, we have a valid, confirmed measure
				* pLuxValue = m_MeasuredLuxValue;
				nReturnCode = LUXMETER_OK;
				bContinue = FALSE;
				m_bInsideMeasure = FALSE;
			}
			else if ( m_bFirstMeasureWithNewScaleOk )
			{
				// Scale changed, we can hide advisor and restart measures
				* pLuxValue = 0.0;
				nReturnCode = LUXMETER_NEW_SCALE_OK;
				bContinue = FALSE;
				m_bInsideMeasure = FALSE;
				m_bLastMeasureOutOfRange = FALSE;
				m_bFirstMeasureWithNewScaleOk = FALSE;
			}
			else
			{
				// No measure available
				if ( m_bLastMeasureOutOfRange )
				{
					// Last measure(s) out of range
					bContinue = FALSE;

					if ( m_bLowLuxValue )
					{
						// Range is too high
						nReturnCode = ( m_bLowestLuxScale ? LUXMETER_SCALE_TOO_HIGH_MIN : LUXMETER_SCALE_TOO_HIGH );
					}
					else if ( m_bHighLuxValue )
					{
						nReturnCode = ( m_bHighestLuxScale ? LUXMETER_SCALE_TOO_LOW_MAX : LUXMETER_SCALE_TOO_LOW );
					}
					else
					{
						// Should never occur ! (answer has no meaning)
						ASSERT ( 0 );
						nReturnCode = LUXMETER_SCALE_TOO_HIGH_MIN;
					}

					// Stop measuring when totally out of range
					if ( nReturnCode ==	LUXMETER_SCALE_TOO_LOW_MAX || nReturnCode == LUXMETER_SCALE_TOO_HIGH_MIN )
						m_bInsideMeasure = FALSE;
				}
				else
				{
					// No measure at all, or not enough measures
					if ( IsLuxmeterRunning () )
					{
						// No measure yet, let's wait for the first measure
						bContinue = TRUE;
					}
					else
					{
						// Luxmeter has been stopped after beginning of measures
						nReturnCode = LUXMETER_NOT_RUNNING;
						bContinue = FALSE;
						m_bInsideMeasure = FALSE;
					}
				}
			}

			LeaveCriticalSection ( & m_LuxCritSec );

			if ( bContinue )
			{
				// Sleep 20 ms while dispatching messages
				MSG	Msg;
				DWORD	dwEnd = GetTickCount () + 20;
																		
				while ( GetTickCount () < dwEnd )
				{
					if ( PeekMessage ( & Msg, NULL, NULL, NULL, TRUE ) )
					{
						TranslateMessage ( & Msg );
						DispatchMessage ( & Msg );
					}
					else
					{
						Sleep(0);
					}
				}
			}
		
		} while ( bContinue );
	}
	else
	{
		* pLuxValue = 0.0;
		nReturnCode = LUXMETER_NOT_RUNNING;
	}
	
	return nReturnCode;
}

BOOL CAboutDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	char	szSensor [ 256 ];
	char	szDevApp [ 256 ];
	char	szSensorAndApp [ 256 ];
	char	szExpert [ 256 ];
	char	szGraphics [ 256 ];
	char	szDVD [ 256 ];
	char	szValidation [ 256 ];
	char	szGermanVersion [ 256 ];
	char *auteurStyle[2]={"<p><font style='b' size='12' color='56,76,104'>","<br><font style='i' size='12' color='64,64,64'>"};
	char *auteurStrings[]={	"Patrice AFFLATET",szSensor,
							"Benoit SEGUIN",szSensorAndApp,
							"François-Xavier CHABOUD",szDevApp,
							"Georges GALLERAND",szDevApp,
							"Laurent GARNIER",szDevApp,
							"Michel de LILLERS",szExpert,
							"Emmanuel PIAT",szExpert,
							"Christophe CHEREL",szGraphics,
							"Jean-Luc DENIAUD",szGraphics,
							"David LAIR",szDVD,
							"Henri NICOLAS",szValidation,
							"Franck CAREDDU",szValidation,
							"Maik OPITZ", szGermanVersion };
	int auteursNb=(sizeof(auteurStrings)/sizeof(char *));

	char *classesStyle="<br><font style='u' size='11' color='0,0,255'>";
	char *classesStrings[]={ 
							"CButtonST: <font style='b'>Davide Calabro</font>","http://www.codeproject.com/buttonctrl/cbuttonst.asp",
							"CColourPickerXP: <font style='b'>Zorglab</font>","http://www.codeproject.com/miscctrl/colourpickerxp.asp",
							"CCreditsCtrl: <font style='b'>Marc Richarme</font>","http://www.codeproject.com/dialog/ccreditsctrl.asp",
							"CGridCtrl: <font style='b'>Chris Maunder</font>","http://www.codeproject.com/miscctrl/gridctrl.asp",
							"CHyperlink: <font style='b'>Chris Maunder</font>","http://www.codeproject.com/miscctrl/hyperlink.asp",
							"CMatrix: <font style='b'>Roger Allen</font>","http://www.codeproject.com/cpp/matrixclass.asp",
							"CMainWindowPlacement: <font style='b'>Martin Holzherr </font>","http://www.codeproject.com/dialog/mainwndplacement.asp",
							"CNewMenu: <font style='b'>Bruno Podetti</font>","http://www.codeproject.com/menu/NewMenuXPStyle.asp",
							"CPPToolTip: <font style='b'>Eugene Pustovoyt</font>","http://www.codeproject.com/miscctrl/pptooltip.asp",
							"CSerialCom: <font style='b'>Shibu K.V</font>","http://www.codeproject.com/system/cserialcom.asp",
							"CSizingControlBar: <font style='b'>Cristi Posea</font>","http://www.codeproject.com/docking/sizecbar.asp",
							"CSpreadSheet: <font style='b'>Yap Chun Wei</font>","http://www.codeproject.com/database/cspreadsheet.asp",
							"CWinAppEx: <font style='b'>Armen Hakobyan</font>","http://www.codeproject.com/threads/singleinstancemfc.asp",
							"CxImage: <font style='b'>Davide Pizzolato</font>","http://www.codeproject.com/bitmap/cximage.asp",
							"CCustomTabCtrl: <font style='b'>Andrzej Markowski</font>","http://www.codeproject.com/tabctrl/AMCustomTabCtrlDemo.asp",
							
							"CXPGroupBox: <font style='b'>JackCa</font>","http://www.codeproject.com/miscctrl/xpgroupbox.asp" };
	int classesNb=(sizeof(classesStrings)/sizeof(char *));
	CString	str;

	::LoadString ( ::GetModuleHandle ( NULL ), IDS_STRING_VERSION, szSensor, sizeof ( szSensor ) );
	m_Version.SetWindowText ( szSensor );

	str.LoadString ( IDS_DONATION_HYPERLINK );
	m_donationHyperLink.SetURL ( str );

	str.LoadString ( IDS_ABOUT_SENSOR );
	strcpy ( szSensor, (LPCSTR) str );
	str.LoadString ( IDS_ABOUT_DEVAPP );
	strcpy ( szDevApp, (LPCSTR) str );
	str.LoadString ( IDS_ABOUT_EXPERT );
	strcpy ( szExpert, (LPCSTR) str );
	str.LoadString ( IDS_ABOUT_GRAPHICS );
	strcpy ( szGraphics, (LPCSTR) str );
	str.LoadString ( IDS_ABOUT_DVD );
	strcpy ( szDVD, (LPCSTR) str );
	str.LoadString ( IDS_ABOUT_VALIDATION );
	strcpy ( szValidation, (LPCSTR) str );
	str.LoadString ( IDS_GERMANVERSION );
	strcpy ( szGermanVersion, (LPCSTR) str );

	strcpy ( szSensorAndApp, szSensor );
	strcat ( szSensorAndApp, "\n" );
	strcat ( szSensorAndApp, szDevApp );

	// Content	
	CString s;
	s = "<font size='16' color='0,0,0' style='bu' align='center'>";
	str.LoadString ( IDS_AUTHORS );
	s += str;
	s += ":</font>";
	for(int i=0;i<auteursNb;i+=2)
	{
		s+=CString(auteurStyle[0])+CString(auteurStrings[i])+"</font>";
		s+=CString(auteurStyle[1])+CString(auteurStrings[i+1])+"</font>";
	}
	s+="<font align='center'><p>-----------------------------</font><p>";
	
	str.LoadString ( IDS_ABOUT_THANK_HEADER );
	s += str;

	for(int i=0;i<classesNb;i+=2)
	{
		s+=CString(classesStyle)+ "<a href='" + CString(classesStrings[i+1])+"'>" + CString(classesStrings[i])+ "</a></font>";
	} 

	s += "<vspace size='80'>"; 
	s+="<font align='center'><p>-----------------------------</font><p>";

	// Content Data
	m_wndCredits.SetDataString(s);
	
	m_wndCredits.m_nTimerSpeed = 200;	// To add delay of m_nTimerSpeed*10ms before scrolling
	// now, we're ready to begin... create the window
	m_wndCredits.Create(WS_EX_CLIENTEDGE,WS_VISIBLE|WS_CHILD,IDC_PLACEHOLDER,this);
	m_wndCredits.m_nTimerSpeed = 50; // 
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
