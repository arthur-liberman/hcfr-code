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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// EyeOneSensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "EyeOneSensor.h"
#include "EyeOneSensorPropPage.h"

// Include for device interface (this device interface is outside GNU GPL license)
#include "devlib\CHCFRDI2.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifdef USE_NON_FREE_CODE

/////////////////////////////////////////////////////////////////////////////
// CEyeOneSensorPropPage property page

IMPLEMENT_DYNCREATE(CEyeOneSensorPropPage, CPropertyPageWithHelp)

CEyeOneSensorPropPage::CEyeOneSensorPropPage() : CPropertyPageWithHelp(CEyeOneSensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CEyeOneSensorPropPage)
	m_CalibrationMode = 1;
	m_bAmbientLight = FALSE;
	m_debugMode = FALSE;
	m_bAverageReads = FALSE;
	m_NbMinutesCalibrationValid = 0;
	//}}AFX_DATA_INIT
}

CEyeOneSensorPropPage::~CEyeOneSensorPropPage()
{
}

void CEyeOneSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CEyeOneSensorPropPage)
	DDX_Control(pDX, IDC_AMBIENTLIGHT, m_AmbientCheck);
	DDX_CBIndex(pDX, IDC_S2SENSOR_MODE_COMBO, m_CalibrationMode);
	DDX_Check(pDX, IDC_AMBIENTLIGHT, m_bAmbientLight);
	DDX_Check(pDX, IDC_KISENSOR_DEBUG_CB, m_debugMode);
	DDX_Check(pDX, IDC_CHECK_AVG_MEASURES, m_bAverageReads);
	DDX_Text(pDX, IDC_EDIT_MINUTES, m_NbMinutesCalibrationValid);
	DDV_MinMaxInt(pDX, m_NbMinutesCalibrationValid, 0, 120);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CEyeOneSensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CEyeOneSensorPropPage)
	ON_BN_CLICKED(IDC_DTP_BLACKLEVEL, OnBlackLevel)
	ON_CBN_SELCHANGE(IDC_S2SENSOR_MODE_COMBO, OnSelchangeModeCombo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CEyeOneSensorPropPage message handlers
/////////////////////////////////////////////////////////////////////////////

void CEyeOneSensorPropPage::OnBlackLevel() 
{
	int			nErrorCode;
	BOOL		bOk = FALSE;
	BOOL		bCalibrated;
	DWORD		dw;
	CString		Msg, Title;
	char		szBuf [ 512 ] = "";

	UpdateData(TRUE);

	if ( m_CalibrationMode >= 3 )
	{
		if ( m_pSensor -> m_hPipe == NULL )
		{
			// Create external process and pipe
			m_pSensor -> CreateEyeOnePipe ();
		}

		nErrorCode = DI2ERR_UNKNOWN;
		if ( m_pSensor -> m_hPipe )
		{
			sprintf ( szBuf, "I%c%c", ( (char)m_CalibrationMode + '0' ), ( m_bAmbientLight ? '1' : '0' ) );
			if ( WriteFile ( m_pSensor -> m_hPipe, szBuf, 3, & dw, NULL ) )
			{
				if ( ReadFile ( m_pSensor -> m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL ) )
				{
					nErrorCode = atoi ( szBuf );
					LPSTR lpStr = szBuf + 2;
					memmove ( szBuf, szBuf + 2, strlen ( szBuf + 1 ) );
				}
				else
					szBuf [ 0 ] = '\0';
			}
			else
				szBuf [ 0 ] = '\0';
		}
		else
			szBuf [ 0 ] = '\0';
	}
	else
	{
		nErrorCode = InitDevice2 ( m_CalibrationMode, m_bAmbientLight, szBuf );
	}

	switch ( nErrorCode )
	{
		case DI2ERR_SUCCESS:
			 bOk = TRUE;
			 break;

		case DI2ERR_NODLL:
			 Msg.LoadString ( IDS_ERREYEONEDLL );
			 break;

		case DI2ERR_INVALIDDLL:
			 Msg.LoadString ( IDS_ERREYEONEDLL2 );
			 break;

		case DI2ERR_NOTCONNECTED:
			 Msg.LoadString ( IDS_EYEONEERROR );
			 break;

		case DI2ERR_EYEONE:
			 Msg.LoadString ( IDS_EYEONEERROR );
			 break;

		case DI2ERR_UNKNOWN:
		default:
			 Msg = "Unknown error";
			 break;
	}

	if ( ! bOk )
	{
		Title.LoadString(IDS_EYEONE);
		if ( szBuf [ 0 ] != '\0' )
		{
			Msg += "\n";
			Msg += szBuf;
		}
		MessageBox(Msg,Title,MB_OK+MB_ICONHAND);
	}
	else
	{
		Msg.LoadString ( IDS_EYEONE_CALIBRATE );
		if ( IDYES == AfxMessageBox ( Msg, MB_YESNO | MB_ICONQUESTION ) )
		{
			RECT	rect;
			CWnd	WhiteWnd;
			
			switch ( m_CalibrationMode )
			{
				case 0:	// CRT
					 Msg.LoadString ( IDS_EYEONE );
					 SetRect ( & rect, 0, 0, 400, 400 );
					 WhiteWnd.CreateEx ( WS_EX_TOOLWINDOW | WS_EX_TOPMOST, AfxRegisterWndClass ( CS_NOCLOSE, 0, (HBRUSH) ::GetStockObject ( WHITE_BRUSH ), NULL ), (LPCSTR) Msg, WS_VISIBLE, rect, NULL, 0, NULL );
					 WhiteWnd.BringWindowToTop ();
					 WhiteWnd.UpdateWindow ();
					 Msg.LoadString ( IDS_CALIBRATE_I1DISPLAY_CRT );
					 break;

				case 1:	// LCD
				case 2:	// Plasma
					 Msg.LoadString ( IDS_CALIBRATE_I1DISPLAY_LCD );
					 break;

				case 3:	// Eye One Pro
					 Msg.LoadString ( IDS_CALIBRATE_I1PRO );
					 break;
			}
			
			AfxMessageBox ( Msg, MB_OK );
			
			szBuf [ 2 ] = '\0';
			if ( m_CalibrationMode >= 3 )
			{
				bCalibrated = FALSE;
				if ( WriteFile ( m_pSensor -> m_hPipe, "C ", 3, & dw, NULL ) )
				{
					if ( ReadFile ( m_pSensor -> m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL ) )
						bCalibrated = atoi ( szBuf );
				}
			}
			else
			{
				if ( m_CalibrationMode == 0 )
					AfxGetApp () -> BeginWaitCursor ();

				bCalibrated = CalibrateDevice2 ( szBuf + 2 );

				if ( m_CalibrationMode == 0 )
					AfxGetApp () -> EndWaitCursor ();
			}

			if ( bCalibrated )
			{
				m_pSensor -> m_dwLastCalibrationTime = GetTickCount ();
				AfxMessageBox ( IDS_CALIBRATE_SUCCESS, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				Msg.LoadString ( IDS_CALIBRATE_FAILED );
				Msg += "\n";
				Msg += ( szBuf + 2 ); 
				AfxMessageBox ( Msg, MB_OK | MB_ICONHAND );
			}
		}
		if ( m_CalibrationMode >= 3 )
		{
			if ( WriteFile ( m_pSensor -> m_hPipe, "R ", 3, & dw, NULL ) )
			{
				// retrieve ack but ignore it
				ReadFile ( m_pSensor -> m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL );
			}
		}
		else
			ReleaseDevice2();
	}
}


UINT CEyeOneSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_EYEONE;
}

void CEyeOneSensorPropPage::OnSelchangeModeCombo() 
{
	// TODO: Add your control notification handler code here
	UpdateData ( TRUE );
	
	if ( m_CalibrationMode >= 3 )
	{
		m_bAmbientLight = FALSE;
		m_AmbientCheck.SetCheck ( FALSE );
		m_AmbientCheck.EnableWindow ( FALSE );
	}
	else
		m_AmbientCheck.EnableWindow ( TRUE );
}

BOOL CEyeOneSensorPropPage::OnSetActive() 
{
	// TODO: Add your specialized code here and/or call the base class
	BOOL bRet = CPropertyPageWithHelp::OnSetActive();

	if ( m_CalibrationMode >= 3 )
	{
		m_bAmbientLight = FALSE;
		m_AmbientCheck.SetCheck ( FALSE );
		m_AmbientCheck.EnableWindow ( FALSE );
	}
	else
		m_AmbientCheck.EnableWindow ( TRUE );
	
	return bRet;
}
#endif
