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

// Spyder3Sensor.cpp: implementation of the Spyder3 class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "Spyder3Sensor.h"
#include "SerialCom.h"

// Include for device interface (this device interface is outside GNU GPL license)
#ifdef USE_NON_FREE_CODE
#include "devlib\CHCFRDI4.h"
#endif

#include <math.h>
#include <psapi.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CSpyder3Sensor, COneDeviceSensor, 1) ;

CSpyder3Sensor::CSpyder3Sensor()
{
	m_ReadTime = 3000;
	m_bAdjustTime = GetConfig () -> GetProfileInt ( "Spyder3", "AdjustTime", FALSE );
	m_bAverageReads = GetConfig () -> GetProfileInt ( "Spyder3", "AverageReads", FALSE );
	m_debugMode = FALSE;
	m_MinRequestedY = GetConfig () -> GetProfileDouble ( "Spyder3", "MinRequestedY", 10.0 );
	m_MaxReadLoops = GetConfig () -> GetProfileInt ( "Spyder3", "MaxReadLoops", 8 );

	m_Spyder3SensorPropertiesPage.m_pSensor = this;

	m_pDevicePage = & m_Spyder3SensorPropertiesPage;  // Add Spyder3 settings page to property sheet

	m_PropertySheetTitle = IDS_S3SENSOR_PROPERTIES_TITLE;

	CString str;
	str.LoadString(IDS_S3SENSOR_NAME);
	SetName(str);
}

CSpyder3Sensor::~CSpyder3Sensor()
{

}

void CSpyder3Sensor::Copy(CSensor * p)
{
	COneDeviceSensor::Copy(p);

	m_ReadTime = ((CSpyder3Sensor*)p)->m_ReadTime;
	m_bAdjustTime = ((CSpyder3Sensor*)p)->m_bAdjustTime;
	m_bAverageReads = ((CSpyder3Sensor*)p)->m_bAverageReads;
	m_debugMode = ((CSpyder3Sensor*)p)->m_debugMode;	
}

void CSpyder3Sensor::Serialize(CArchive& archive)
{
	COneDeviceSensor::Serialize(archive) ;

	if (archive.IsStoring())
	{
		int version=1;
		archive << version;
		archive << m_ReadTime;
		archive << m_bAdjustTime;
		archive << m_bAverageReads;
		archive << m_debugMode;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_ReadTime;
		archive >> m_bAdjustTime;
		archive >> m_bAverageReads;
		archive >> m_debugMode;
	}
}

void CSpyder3Sensor::SetPropertiesSheetValues()
{
	COneDeviceSensor::SetPropertiesSheetValues();

	m_Spyder3SensorPropertiesPage.m_ReadTime=m_ReadTime;
	m_Spyder3SensorPropertiesPage.m_bAdjustTime=m_bAdjustTime;
	m_Spyder3SensorPropertiesPage.m_bAverageReads=m_bAverageReads;
	m_Spyder3SensorPropertiesPage.m_debugMode=m_debugMode;
}

void CSpyder3Sensor::GetPropertiesSheetValues()
{
	COneDeviceSensor::GetPropertiesSheetValues();

	if( m_ReadTime != m_Spyder3SensorPropertiesPage.m_ReadTime ) {
		m_ReadTime=m_Spyder3SensorPropertiesPage.m_ReadTime;
		SetModifiedFlag(TRUE);
	}

	if( m_bAdjustTime != m_Spyder3SensorPropertiesPage.m_bAdjustTime ) {
		m_bAdjustTime=m_Spyder3SensorPropertiesPage.m_bAdjustTime;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "Spyder3", "AdjustTime", m_bAdjustTime );
	}

	if( m_bAverageReads != m_Spyder3SensorPropertiesPage.m_bAverageReads ) {
		m_bAverageReads=m_Spyder3SensorPropertiesPage.m_bAverageReads;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "Spyder3", "AverageReads", m_bAverageReads );
	}

	if( m_debugMode != m_Spyder3SensorPropertiesPage.m_debugMode ) {
		m_debugMode=m_Spyder3SensorPropertiesPage.m_debugMode;
		SetModifiedFlag(TRUE);
	}

}

BOOL CSpyder3Sensor::Init( BOOL bForSimultaneousMeasures )
{
#ifdef USE_NON_FREE_CODE
	int			nErrorCode;
	BOOL		bOk = FALSE;
	BOOL		bInitializeDevice = TRUE;
	CString		Msg, Title;

	// Check if Spyder3Utility is running
	HWND hS3UWnd = FindWindow ( "RBWindow", "Screen #0" );
	if ( hS3UWnd )
	{
		DWORD	dwProcessId = 0;
		HANDLE	hProcess;
		char	szBuf [ 256 ];

		// Check if this window is really the Spyder3Utility
		GetWindowThreadProcessId ( hS3UWnd, & dwProcessId );
		
		hProcess = OpenProcess ( PROCESS_ALL_ACCESS, FALSE, dwProcessId );

		GetModuleBaseName ( hProcess, NULL, szBuf, sizeof ( szBuf ) );

		if ( _stricmp ( szBuf, "Spyder3Utility.exe" ) == 0 )
		{
			// Cannot use Spyder 3 when Spyder3Utility is running: propose to terminate the process
			Msg.LoadString ( IDS_SPYDER3UTILITYRUNNING );
			Title.LoadString(IDS_SPYDER3);
			if ( IDYES == MessageBox(NULL,Msg,Title,MB_YESNO+MB_ICONHAND) )
			{
				if ( ! TerminateProcess ( hProcess, 0 ) )
				{
					// Cannot terminate the process
					bInitializeDevice = FALSE;
					MessageBox(NULL,"Error while terminating Spyder3Utility process.\nOperation cancelled.",Title,MB_OK+MB_ICONHAND);
				}
			}
			else
			{
				bInitializeDevice = FALSE;
			}
			
			// Set default error message
			Msg.LoadString ( IDS_SPYDER3ERROR );
		}

		CloseHandle ( hProcess );
		hProcess = NULL;
	}
	
	if ( bInitializeDevice )
	{
		nErrorCode = InitDevice4(bForSimultaneousMeasures);

		switch ( nErrorCode )
		{
			case DI4ERR_SUCCESS:
				 bOk = TRUE;
				 break;

			case DI4ERR_NODLL:
				 Msg.LoadString ( IDS_ERRSPYDER3DLL );
				 break;

			case DI4ERR_INVALIDDLL:
				 Msg.LoadString ( IDS_ERRSPYDER3DLL2 );
				 break;

			case DI4ERR_SPYDER:
				 Msg.LoadString ( IDS_SPYDER3ERROR );
				 break;

			case DI4ERR_UNKNOWN:
			default:
				 Msg = "Unknown error";
				 break;
		}
	}

	if ( ! bOk )
	{
		Title.LoadString(IDS_SPYDER3);
		MessageBox(NULL,Msg,Title,MB_OK+MB_ICONHAND);
	}

	return bOk;
#else
    return TRUE;
#endif
}

BOOL CSpyder3Sensor::Release()
{
#ifdef USE_NON_FREE_CODE
	ReleaseDevice4();
#endif
	return CSensor::Release();
}

CColor CSpyder3Sensor::MeasureColor(COLORREF aRGBValue)
{
#ifdef USE_NON_FREE_CODE
	UINT		nLoops;
	BOOL		bContinue = FALSE;
	UINT		r, g, b;
	UINT		nAdjustedReadTime;
	double		d;
	double		x, y, z;
	double		xx, yy, zz;
	CColor		Spyder3Color, colMeasure;
	
	if ( m_bAdjustTime )
	{
		r = GetRValue ( aRGBValue );
		g = GetGValue ( aRGBValue );
		b = GetBValue ( aRGBValue );

		// Retrieve darker component
		d = (double) min ( r, min (g, b) );

		// Increase read time for dark component readings
		if ( d < 80.0 )
		{
			nAdjustedReadTime = (UINT) ( 3.0 * ( 1.0 - ( d / 80.0 ) ) * (double) m_ReadTime ) + m_ReadTime;
			
			if ( nAdjustedReadTime > 60000 )
				nAdjustedReadTime = 60000;
		}
		else
			nAdjustedReadTime = m_ReadTime;
	}
	else
		nAdjustedReadTime = m_ReadTime;

	nLoops = 0;
	do
	{
		// Time is not in ms, but in 60th of seconds
		if ( GetValuesDevice4(nAdjustedReadTime*60/1000, & x, & y, & z) )
		{
			if ( nLoops > 0 )
			{
				d = (double) nLoops + 1.0;
				xx = ( ( xx * (double) nLoops ) + x ) / d;
				yy = ( ( yy * (double) nLoops ) + y ) / d;
				zz = ( ( zz * (double) nLoops ) + z ) / d;

			}
			else
			{
				xx = x;
				yy = y;
				zz = z;
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
					fprintf(f, "Spyder 3 - %s : * building value - loop %d * : X:%5.3f Y:%5.3f Z:%5.3f\n", s, nLoops, x, y, z );
					fclose(f);
					LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
				}
			}

			if ( ! bContinue )
			{
				Spyder3Color[0]= xx;
				Spyder3Color[1]= yy;
				Spyder3Color[2]= zz;

				if (m_debugMode) 
				{
					EnterCriticalSection ( & GetConfig () -> LogFileCritSec );
					CTime theTime = CTime::GetCurrentTime(); 
					CString s = theTime.Format( "%d/%m/%y %H:%M:%S" );		
					FILE *f = fopen ( GetConfig () -> m_logFileName, "a" );
					fprintf(f, "Spyder 3 - %s : R:%3d G:%3d B:%3d (%d loops) : X:%5.3f Y:%5.3f Z:%5.3f\n", s, GetRValue(aRGBValue), GetGValue(aRGBValue), GetBValue(aRGBValue), nLoops, xx, yy, zz );
					fclose(f);
					LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
				}

				colMeasure.SetSensorValue(Spyder3Color);
			}
		}
		else 
		{
			MessageBox(0, "No data from Sensor","Error",MB_OK+MB_ICONINFORMATION);
			return noDataColor;
		}
	} while ( bContinue );

	return colMeasure.GetSensorValue();
#else
    return noDataColor;
#endif
}

#ifdef USE_NON_FREE_CODE
#pragma comment(lib, "devlib\\CHCFRDI4.lib")
#endif
