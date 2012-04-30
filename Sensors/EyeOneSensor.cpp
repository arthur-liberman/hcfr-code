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

// EyeOneSensor.cpp: implementation of the EyeOne class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "EyeOneSensor.h"
#include "SerialCom.h"

// Include for device interface (this device interface is outside GNU GPL license)
#include "devlib\CHCFRDI2.h"

#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// Spectrum characteristics
#define SPECTRUM_WAVELENGTH_MIN			380
#define SPECTRUM_WAVELENGTH_MAX			730
#define SPECTRUM_WAVELENGTH_BANDWIDTH	10
#define SPECTRUM_BANDS					36

HANDLE CEyeOneSensor::m_hPipe = NULL;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CEyeOneSensor, COneDeviceSensor, 1) ;

CEyeOneSensor::CEyeOneSensor()
{
	m_CalibrationMode = GetConfig () -> GetProfileInt ( "EyeOne", "DeviceCalibrationMode", 999 );
	if ( m_CalibrationMode > 4 )
	{
		// No new format calibration mode recorded: try to load old format
		m_CalibrationMode = GetConfig () -> GetProfileInt ( "EyeOne", "CalibrationMode", 1 );
		
		// If old value was "eye one pro", translate it into new format
		if ( m_CalibrationMode == 2 )
			m_CalibrationMode = 3;

		// Store translated value for next time
		GetConfig () -> WriteProfileInt ( "EyeOne", "DeviceCalibrationMode", m_CalibrationMode );
	}

	m_bAmbientLight = GetConfig () -> GetProfileInt ( "EyeOne", "AmbientLight", 0 );
	m_debugMode = FALSE;
	m_bAverageReads = GetConfig () -> GetProfileInt ( "EyeOne", "AverageReads", FALSE );
	m_NbMinutesCalibrationValid = GetConfig () -> GetProfileInt ( "EyeOne", "MinutesCalibrationValid", 0 );
	m_MinRequestedY = GetConfig () -> GetProfileDouble ( "EyeOne", "MinRequestedY", 10.0 );
	m_MaxReadLoops = GetConfig () -> GetProfileInt ( "EyeOne", "MaxReadLoops", 8 );
	m_dwLastCalibrationTime = 0;

	m_EyeOneSensorPropertiesPage.m_pSensor = this;

	m_pDevicePage = & m_EyeOneSensorPropertiesPage;  // Add EyeOne settings page to property sheet

	m_PropertySheetTitle = IDS_I1SENSOR_PROPERTIES_TITLE;

	CString str;
	str.LoadString(IDS_I1SENSOR_NAME);
	SetName(str);
}

CEyeOneSensor::~CEyeOneSensor()
{
}

void CEyeOneSensor::Copy(CSensor * p)
{
	COneDeviceSensor::Copy(p);

	m_CalibrationMode = ((CEyeOneSensor*)p)->m_CalibrationMode;
	m_bAmbientLight = ((CEyeOneSensor*)p)->m_bAmbientLight;
	m_debugMode = ((CEyeOneSensor*)p)->m_debugMode;	
	m_bAverageReads = ((CEyeOneSensor*)p)->m_bAverageReads;
	m_NbMinutesCalibrationValid = ((CEyeOneSensor*)p)->m_NbMinutesCalibrationValid;
}

void CEyeOneSensor::Serialize(CArchive& archive)
{
	COneDeviceSensor::Serialize(archive) ;

	if (archive.IsStoring())
	{
		int version=6;
		archive << version;
		archive << m_CalibrationMode;
		archive << m_debugMode;
		archive << m_bAverageReads;
		archive << m_NbMinutesCalibrationValid;
		archive << m_bAmbientLight;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 6 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_CalibrationMode;
		if ( version > 1 )
			archive >> m_debugMode;

		if ( version > 2 )
			archive >> m_bAverageReads;
		else
			m_bAverageReads = FALSE;

		if ( version > 3 )
			archive >> m_NbMinutesCalibrationValid;
		else
			m_NbMinutesCalibrationValid = 0;

		if ( version > 4 )
			archive >> m_bAmbientLight;
		else
			m_bAmbientLight = FALSE;

		// In version 6, Eye One pro became number 3, and number 2 is now the new "Eye One Display - plasma" mode
		if ( version < 6 && m_CalibrationMode == 2 )
			m_CalibrationMode = 3;
	}
}

void CEyeOneSensor::SetPropertiesSheetValues()
{
	COneDeviceSensor::SetPropertiesSheetValues();

	m_EyeOneSensorPropertiesPage.m_CalibrationMode=m_CalibrationMode;
	m_EyeOneSensorPropertiesPage.m_bAmbientLight=m_bAmbientLight;
	m_EyeOneSensorPropertiesPage.m_debugMode=m_debugMode;
	m_EyeOneSensorPropertiesPage.m_bAverageReads=m_bAverageReads;
	m_EyeOneSensorPropertiesPage.m_NbMinutesCalibrationValid=m_NbMinutesCalibrationValid;
}

void CEyeOneSensor::GetPropertiesSheetValues()
{
	COneDeviceSensor::GetPropertiesSheetValues();

	if( m_CalibrationMode != m_EyeOneSensorPropertiesPage.m_CalibrationMode ) {
		m_CalibrationMode=m_EyeOneSensorPropertiesPage.m_CalibrationMode;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "EyeOne", "DeviceCalibrationMode", m_CalibrationMode );

		if ( m_CalibrationMode >= 3 )
		{
			// Eye One pro cannot have calibration matrix: delete it if present
			m_sensorToXYZMatrix=IdentityMatrix(3);
			m_calibrationTime=0;
		}
	}

	if( m_debugMode != m_EyeOneSensorPropertiesPage.m_debugMode ) {
		m_debugMode=m_EyeOneSensorPropertiesPage.m_debugMode;
		SetModifiedFlag(TRUE);
	}

	if( m_bAverageReads != m_EyeOneSensorPropertiesPage.m_bAverageReads ) {
		m_bAverageReads=m_EyeOneSensorPropertiesPage.m_bAverageReads;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "EyeOne", "AverageReads", m_bAverageReads );
	}

	if( m_NbMinutesCalibrationValid != m_EyeOneSensorPropertiesPage.m_NbMinutesCalibrationValid ) {
		m_NbMinutesCalibrationValid=m_EyeOneSensorPropertiesPage.m_NbMinutesCalibrationValid;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "EyeOne", "MinutesCalibrationValid", m_NbMinutesCalibrationValid );
	}

	if( m_bAmbientLight != m_EyeOneSensorPropertiesPage.m_bAmbientLight ) {
		m_bAmbientLight=m_EyeOneSensorPropertiesPage.m_bAmbientLight;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "EyeOne", "AmbientLight", m_bAmbientLight );
	}
}

void CEyeOneSensor::CreateEyeOnePipe ()
{
	DWORD		dw;
	char		szBuf [ 512 ];
	STARTUPINFO	si;
	PROCESS_INFORMATION pi;

	// Create external process and pipe
	memset ( & si, 0, sizeof ( si ) );
	si.cb = sizeof ( si );
	GetStartupInfo ( & si );

	dw = GetTickCount ();
	sprintf ( szBuf, "CHCFRDX2.exe %u", dw );
	if ( CreateProcess ( NULL, szBuf, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, & si, & pi ) )
	{
		sprintf ( szBuf, "\\\\.\\pipe\\CHCFRDX2_TO_COLORHCFR_PIPE_%08X", dw );
		for ( int i = 0; m_hPipe == NULL && i < 10 ; i ++ )
		{
			Sleep ( 200 );
			m_hPipe = CreateFile ( szBuf, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

			if ( m_hPipe == INVALID_HANDLE_VALUE )
				m_hPipe = NULL;
		}
		
		if ( m_hPipe == NULL )
		{
			TerminateProcess ( pi.hProcess, 1 );
		}
		CloseHandle ( pi.hProcess );
		CloseHandle ( pi.hThread );
	}
}

void CEyeOneSensor::CloseEyeOnePipe ()
{
	if ( m_hPipe )
	{
		DWORD	dw;
		WriteFile ( m_hPipe, "X ", 3, & dw, NULL );
		CloseHandle ( m_hPipe );
		m_hPipe = NULL;
	}
}

BOOL CEyeOneSensor::Init( BOOL bForSimultaneousMeasures )
{
	int			nErrorCode;
	BOOL		bOk = FALSE;
	BOOL		bCalibrate = FALSE;
	BOOL		bNeedCalibration, bCalibrated;
	DWORD		dw;
	CString		Msg, Title;
	char		szBuf [ 512 ] = "";

	if ( m_CalibrationMode >= 3 )
	{
		if ( m_hPipe == NULL )
		{
			// Create external process and pipe
			CreateEyeOnePipe ();
		}

		nErrorCode = DI2ERR_UNKNOWN;
		if ( m_hPipe )
		{
			sprintf ( szBuf, "I%c%c", ( (char)m_CalibrationMode + '0' ), ( m_bAmbientLight ? '1' : '0' ) );
			if ( WriteFile ( m_hPipe, szBuf, 3, & dw, NULL ) )
			{
				if ( ReadFile ( m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL ) )
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
		MessageBox(NULL,Msg,Title,MB_OK+MB_ICONHAND);
	}
	else
	{
		if ( m_CalibrationMode >= 3 )
		{
			bNeedCalibration = FALSE;
			if ( WriteFile ( m_hPipe, "N ", 3, & dw, NULL ) )
			{
				if ( ReadFile ( m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL ) )
					bNeedCalibration = atoi ( szBuf );
			}
		}
		else
			bNeedCalibration = Device2NeedCalibration ();

		if ( bNeedCalibration )
			bCalibrate = TRUE;
		else
		{
			// Request calibration only when validity time is expired
			if ( m_NbMinutesCalibrationValid > 0 && m_dwLastCalibrationTime + ( m_NbMinutesCalibrationValid * 60000 ) < GetTickCount () )
				bCalibrate = TRUE;
		}

		if ( bCalibrate )
		{
			HWND hWnd = GetActiveWindow ();

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
					if ( WriteFile ( m_hPipe, "C ", 3, & dw, NULL ) )
					{
						if ( ReadFile ( m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL ) )
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
					m_dwLastCalibrationTime = GetTickCount ();
					AfxMessageBox ( IDS_CALIBRATE_SUCCESS2, MB_OK | MB_ICONINFORMATION );
					SetActiveWindow ( hWnd );
					BringWindowToTop ( hWnd );
					UpdateWindow ( hWnd );
				}
				else
				{
					Msg.LoadString ( IDS_CALIBRATE_FAILED );
					Msg += "\n";
					Msg += ( szBuf + 2 ); 
					AfxMessageBox ( Msg, MB_OK | MB_ICONHAND );

					// Cannot measure without calibration
					bOk = FALSE;
					if ( m_CalibrationMode >= 3 )
					{
						if ( WriteFile ( m_hPipe, "R ", 3, & dw, NULL ) )
						{
							// retrieve ack but ignore it
							ReadFile ( m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL );
						}
					}
					else
						ReleaseDevice2();
				}
			}
			else
			{
				// Test if calibration request is absolute or relative (request comes from calibration validity timeout)
				if ( bNeedCalibration )
				{
					// Cannot measure without calibration
					bOk = FALSE;
					if ( m_CalibrationMode >= 3 )
					{
						if ( WriteFile ( m_hPipe, "R ", 3, & dw, NULL ) )
						{
							// retrieve ack but ignore it
							ReadFile ( m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL );
						}
					}
					else
						ReleaseDevice2();
				}
			}
		}
	}

	return bOk;
}

BOOL CEyeOneSensor::Release()
{
	DWORD	dw;
	char	szBuf [ 256 ];

	if ( m_CalibrationMode >= 3 )
	{
		if ( WriteFile ( m_hPipe, "R ", 3, & dw, NULL ) )
		{
			// retrieve ack but ignore it
			ReadFile ( m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL );
		}
	}
	else
		ReleaseDevice2();

	return CSensor::Release();
}

CColor CEyeOneSensor::MeasureColor(COLORREF aRGBValue)
{
	int			i, nLoops;
	BOOL		bOk;
	BOOL		bContinue = FALSE;
	BOOL		bSpectrumOk = FALSE;
	DWORD		dw;
	double		x, y, z;
	double		d, xx, yy, zz;
	double		Spectrum[SPECTRUM_BANDS];
	double		FullSpectrum[SPECTRUM_BANDS];
	CColor		EyeOneColor;
	char		szBuf [ 4096 ];
	LPSTR		lpStr;
	
	nLoops = 0;
	do
	{
		if ( m_CalibrationMode >= 3 )
		{
			bOk = FALSE;
			if ( WriteFile ( m_hPipe, "M ", 3, & dw, NULL ) )
			{
				if ( ReadFile ( m_hPipe, szBuf, sizeof ( szBuf ), & dw, NULL ) )
				{
					bOk = atoi ( szBuf );

					if ( bOk )
					{
						lpStr = szBuf + 2;
						sscanf ( lpStr, "%lf,%lf,%lf,%d", & x, & y, & z, & bSpectrumOk );
						if ( bSpectrumOk )
						{
							lpStr = strchr ( lpStr, (int) '|' ) + 1;
							for ( i = 0; lpStr && lpStr [ 0 ] && i < SPECTRUM_BANDS ; i ++ )
							{
								Spectrum [ i ] = atof ( lpStr );
								lpStr = strchr ( lpStr, (int) ',' ) + 1;
							}
						}
					}
				}
			}
		}
		else
			bOk = GetValuesDevice2(& x, & y, & z, Spectrum, & bSpectrumOk );

		if ( bOk )
		{
			if ( nLoops > 0 )
			{
				d = (double) nLoops + 1.0;
				xx = ( ( xx * (double) nLoops ) + x ) / d;
				yy = ( ( yy * (double) nLoops ) + y ) / d;
				zz = ( ( zz * (double) nLoops ) + z ) / d;

				if ( bSpectrumOk )
				{
					for ( i = 0; i < SPECTRUM_BANDS ; i++ )
						FullSpectrum [ i ] = ( ( FullSpectrum [ i ] * (double) nLoops ) + Spectrum [ i ] ) / d;
				}
			}
			else
			{
				xx = x;
				yy = y;
				zz = z;

				if ( bSpectrumOk )
				{
					for ( i = 0; i < SPECTRUM_BANDS ; i++ )
						FullSpectrum [ i ] = Spectrum [ i ];
				}
			}
			nLoops ++;

			bContinue = FALSE;

			if ( m_bAverageReads )
			{
				if ( ( yy * (double) nLoops ) < m_MinRequestedY && nLoops < m_MaxReadLoops )
					bContinue = TRUE;

				if (m_debugMode) 
				{
					EnterCriticalSection ( & GetConfig () -> LogFileCritSec );
					CTime theTime = CTime::GetCurrentTime(); 
					CString s = theTime.Format( "%d/%m/%y %H:%M:%S" );		
					FILE *f = fopen ( GetConfig () -> m_logFileName, "a" );
					fprintf(f, "Eye One - %s : * building value - loop %d * : X:%5.3f Y:%5.3f Z:%5.3f |", s, nLoops, x, y, z );
					if ( bSpectrumOk )
					{
						for ( i = 0; i < SPECTRUM_BANDS ; i++ )
							fprintf(f, " %7.4f", Spectrum[i]);
					}
					fprintf(f, "\n");
					fclose(f);
					LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
				}
			}

			if ( ! bContinue )
			{
				EyeOneColor[0]= xx;
				EyeOneColor[1]= yy;
				EyeOneColor[2]= zz;

				if ( bSpectrumOk )
				{
					// Add spectrum values to measured color
					EyeOneColor.SetSpectrum ( CSpectrum ( SPECTRUM_BANDS, SPECTRUM_WAVELENGTH_MIN, SPECTRUM_WAVELENGTH_MAX, SPECTRUM_WAVELENGTH_BANDWIDTH, FullSpectrum ) );
				}

				if (m_debugMode) 
				{
					EnterCriticalSection ( & GetConfig () -> LogFileCritSec );
					CTime theTime = CTime::GetCurrentTime(); 
					CString s = theTime.Format( "%d/%m/%y %H:%M:%S" );		
					FILE *f = fopen ( GetConfig () -> m_logFileName, "a" );
					fprintf(f, "Eye One - %s : R:%3d G:%3d B:%3d (%d loops) : X:%5.3f Y:%5.3f Z:%5.3f |", s, GetRValue(aRGBValue), GetGValue(aRGBValue), GetBValue(aRGBValue), nLoops, xx, yy, zz );
					if ( bSpectrumOk )
					{
						for ( i = 0; i < SPECTRUM_BANDS ; i++ )
							fprintf(f, " %7.4f", FullSpectrum[i]);
					}
					fprintf(f, "\n");
					fclose(f);
					LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
				}
			}
		}
		else 
		{
			MessageBox(0, "No data from Sensor","Error",MB_OK+MB_ICONINFORMATION);
			return noDataColor;
		}
	} while ( bContinue );

	return EyeOneColor;
}

BOOL CEyeOneSensor::HasSpectrumCapabilities ( int * pNbBands, int * pMinWaveLength, int * pMaxWaveLength, double * pBandWidth )
{
	if ( m_CalibrationMode >= 3 )
	{
		* pNbBands = SPECTRUM_BANDS;
		* pMinWaveLength = SPECTRUM_WAVELENGTH_MIN;
		* pMaxWaveLength = SPECTRUM_WAVELENGTH_MAX;
		* pBandWidth = SPECTRUM_WAVELENGTH_BANDWIDTH;

		return TRUE;
	}

	return FALSE;
}

void CEyeOneSensor::GetUniqueIdentifier( CString & strId )
{
	sprintf ( strId.GetBuffer(64), "%s %s", (LPCSTR) m_name, ( m_CalibrationMode == 3 ? "Pro" : "Display" ) );
	strId.ReleaseBuffer (); 
}



