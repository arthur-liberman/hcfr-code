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

// DTPSensor.cpp: implementation of the DTPSensor class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DTPSensor.h"
#include "SerialCom.h"

#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

static HMODULE hXds3DLL = NULL;
static __int64	hConnection = 0i64;

static __int64 ( __stdcall *pCreateUSBConnection) ( unsigned long ulDeviceId,const char* szSerialNumber,unsigned long ulProtocol ) = NULL;
static bool ( __stdcall *pConnect) ( __int64 hConnection ) = NULL;
static bool ( __stdcall *pDisconnect) ( __int64 hConnection ) = NULL;	
static void ( __stdcall *pDestroyConnection) ( __int64 hConnection ) = NULL;
static bool ( __stdcall *pSendAndReceive) ( __int64 hConnection,const char* pchSendBuffer,unsigned long ulSendBufferLen,unsigned char uchDeviceID,int iCmdDataMode,unsigned char* pchRcvBuffer,unsigned long ulRcvBufferSize,unsigned long ulTimeout,unsigned long* pulNumBytesRcved,unsigned long* pulStatusCode ) = NULL;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CDTPSensor, COneDeviceSensor, 1) ;

CDTPSensor::CDTPSensor()
{
	m_CalibrationMode = GetConfig()->GetProfileInt("DTPSensor","CalibrationMode",2);
	m_timeout = GetConfig()->GetProfileInt("DTPSensor","Timeout",10000);
	m_debugMode = FALSE;
	m_bAverageReads = GetConfig () -> GetProfileInt ( "DTPSensor", "AverageReads", FALSE );
	m_MinRequestedY = GetConfig () -> GetProfileDouble ( "DTPSensor", "MinRequestedY", 10.0 );
	m_MaxReadLoops = GetConfig () -> GetProfileInt ( "DTPSensor", "MaxReadLoops", 8 );

	m_bUseInternalOffsets = GetConfig()->GetProfileInt("DTPSensor","UseOffsets",1);
	m_bNeedInit = TRUE;

	m_DTPSensorPropertiesPage.m_pSensor = this;

	m_pDevicePage = & m_DTPSensorPropertiesPage;  // Add DTP settings page to property sheet

	m_PropertySheetTitle = IDS_DTPSENSOR_PROPERTIES_TITLE;

	CString str;
	str.LoadString(IDS_DTPSENSOR_NAME);
	SetName(str);
}

CDTPSensor::~CDTPSensor()
{
	if ( hXds3DLL && hConnection )
	{
		pDestroyConnection ( hConnection );
		hConnection = 0i64;
	}

}

void CDTPSensor::Copy(CSensor * p)
{
	COneDeviceSensor::Copy(p);

	m_bNeedInit = ((CDTPSensor*)p)->m_bNeedInit;
	m_CalibrationMode = ((CDTPSensor*)p)->m_CalibrationMode;
	m_timeout = ((CDTPSensor*)p)->m_timeout;
	m_bUseInternalOffsets = ((CDTPSensor*)p)->m_bUseInternalOffsets;
	m_debugMode = ((CDTPSensor*)p)->m_debugMode;	
	m_bAverageReads = ((CDTPSensor*)p)->m_bAverageReads;
}

void CDTPSensor::Serialize(CArchive& archive)
{
	COneDeviceSensor::Serialize(archive) ;

	if (archive.IsStoring())
	{
		int version=3;
		archive << version;
		archive << m_CalibrationMode;
		archive << m_timeout;
		archive << m_bUseInternalOffsets;
		archive << m_debugMode;
		archive << m_bAverageReads;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 3 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_CalibrationMode;
		archive >> m_timeout;
		archive >> m_bUseInternalOffsets;
		
		if ( version > 1 )
			archive >> m_debugMode;


		if ( version > 2 )
			archive >> m_bAverageReads;
		else
			m_bAverageReads = FALSE;

		m_bNeedInit = TRUE;
	}
}

void CDTPSensor::SetPropertiesSheetValues()
{
	COneDeviceSensor::SetPropertiesSheetValues();

	m_DTPSensorPropertiesPage.m_CalibrationMode=m_CalibrationMode;
	m_DTPSensorPropertiesPage.m_timeout=m_timeout;
	m_DTPSensorPropertiesPage.m_bUseInternalOffsets=m_bUseInternalOffsets;
	m_DTPSensorPropertiesPage.m_debugMode=m_debugMode;
	m_DTPSensorPropertiesPage.m_bAverageReads=m_bAverageReads;
}

void CDTPSensor::GetPropertiesSheetValues()
{
	COneDeviceSensor::GetPropertiesSheetValues();

	if( m_CalibrationMode != m_DTPSensorPropertiesPage.m_CalibrationMode ) {
		m_CalibrationMode=m_DTPSensorPropertiesPage.m_CalibrationMode;
		GetConfig()->WriteProfileInt("DTPSensor","CalibrationMode",m_CalibrationMode);
		SetModifiedFlag(TRUE);
	}

	if( m_timeout != m_DTPSensorPropertiesPage.m_timeout ) {
		m_timeout=m_DTPSensorPropertiesPage.m_timeout;
		GetConfig()->WriteProfileInt("DTPSensor","Timeout",m_timeout);
		SetModifiedFlag(TRUE);
	}

	if( m_bUseInternalOffsets != m_DTPSensorPropertiesPage.m_bUseInternalOffsets ) {
		m_bUseInternalOffsets=m_DTPSensorPropertiesPage.m_bUseInternalOffsets;
		GetConfig()->WriteProfileInt("DTPSensor","UseOffsets",m_bUseInternalOffsets);
		SetModifiedFlag(TRUE);
	}

	if( m_debugMode != m_DTPSensorPropertiesPage.m_debugMode ) {
		m_debugMode=m_DTPSensorPropertiesPage.m_debugMode;
		SetModifiedFlag(TRUE);
	}

	if( m_bAverageReads != m_DTPSensorPropertiesPage.m_bAverageReads ) {
		m_bAverageReads=m_DTPSensorPropertiesPage.m_bAverageReads;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "DTPSensor", "AverageReads", m_bAverageReads );
	}

	m_bNeedInit = TRUE;
}

BOOL CDTPSensor::Init( BOOL bForSimultaneousMeasures )
{
	UINT		OldErrMode;
	BOOL		bOk = FALSE;
	char		szBuf [ 256 ];
	CString		Msg, Title;

	Title.LoadString(IDS_DTPSENSOR_NAME);

	if ( hXds3DLL == NULL )
	{
		m_bNeedInit = TRUE;
		OldErrMode = SetErrorMode ( SEM_NOOPENFILEERRORBOX );
		hXds3DLL = LoadLibrary ( "XdsIII.DLL" );
		SetErrorMode ( OldErrMode );
		if ( hXds3DLL )
		{
			pCreateUSBConnection = (__int64 ( __stdcall *) ( unsigned long ulDeviceId,const char* szSerialNumber,unsigned long ulProtocol ) ) GetProcAddress ( hXds3DLL, "CreateUSBConnection" );
			pConnect = (bool ( __stdcall *) ( __int64 hConnection ) ) GetProcAddress ( hXds3DLL, "Connect" );
			pDisconnect = (bool ( __stdcall *) ( __int64 hConnection ) ) GetProcAddress ( hXds3DLL, "Disconnect" );	
			pDestroyConnection = (void ( __stdcall *) ( __int64 hConnection ) ) GetProcAddress ( hXds3DLL, "DestroyConnection" );
			pSendAndReceive = (bool ( __stdcall *) ( __int64,const char*,unsigned long,unsigned char,int,unsigned char*,unsigned long,unsigned long,unsigned long*,unsigned long* )) GetProcAddress ( hXds3DLL, "SendAndReceive" );

			if ( ! pCreateUSBConnection || ! pConnect || ! pDisconnect || ! pDestroyConnection || ! pSendAndReceive )
			{
				FreeLibrary ( hXds3DLL );
				hXds3DLL = NULL;

				Msg.LoadString ( IDS_ERRXDSDLL2 );
				MessageBox(NULL,Msg,Title,MB_OK+MB_ICONHAND);
			}
		}
		else
		{
			Msg.LoadString ( IDS_ERRXDSDLL1 );
			MessageBox(NULL,Msg,Title,MB_OK+MB_ICONHAND);
		}
	}

	if ( hXds3DLL )
	{
		if ( hConnection == 0i64 )
		{
			hConnection = pCreateUSBConnection ( 0xD094, NULL, 1 );
			if ( hConnection == 0i64 )
			{
				Msg.LoadString ( IDS_ERRXDSINIT1 );
				MessageBox(NULL,Msg,Title,MB_OK+MB_ICONHAND);
			}
		}

		if ( hConnection )
		{
			if ( pConnect ( hConnection ) )
			{
				bOk = TRUE;
			}
			else
			{
				// First attempt to connect failed: try a second time
				if ( pConnect ( hConnection ) )
				{
					bOk = TRUE;
				}
				else
				{
					// Error
					Msg.LoadString ( IDS_ERRXDSINIT2 );
					MessageBox(NULL,Msg,Title,MB_OK+MB_ICONHAND);
				}
			}

			if ( bOk && m_bNeedInit )
			{
				// Perform general initializations
				m_bNeedInit = FALSE;

				sprintf ( szBuf, "%02d16CF", m_CalibrationMode );
				SendAndReceive ( szBuf, NULL, 0 );

				// Ensure all values to default
				SendAndReceive ( "0007CF", NULL, 0 );
				SendAndReceive ( "0008CF", NULL, 0 );
				SendAndReceive ( "010aCF", NULL, 0 );
				SendAndReceive ( m_bUseInternalOffsets ? "0117CF" : "0017CF", NULL, 0 );
				SendAndReceive ( "0019CF", NULL, 0 );
			}
		}
	}

	return bOk;
}

BOOL CDTPSensor::Release()
{
	if ( hXds3DLL && hConnection )
		pDisconnect ( hConnection );

	return CSensor::Release();
}

CColor CDTPSensor::MeasureColor(COLORREF aRGBValue)
{
	UINT		nLoops;
	BOOL		bContinue = FALSE;
	LPSTR		lpStr;
	double		x=0.0, y=0.0, z=0.0;
	double		d, xx, yy, zz;
	char		szBuf [ 256 ];
	CColor		DTPColor, colMeasure;
	
	if ( hXds3DLL && hConnection )
	{
		nLoops = 0;
		do
		{
			if ( SendAndReceive ( "RM", szBuf, sizeof(szBuf) ) )
			{
				lpStr = strchr ( szBuf, 'X' );
				if (lpStr) 
					x=atof(lpStr+1);

				lpStr = strchr ( szBuf, 'Y' );
				if (lpStr) 
					y=atof(lpStr+1);

				lpStr = strchr ( szBuf, 'Z' );
				if (lpStr) 
					z=atof(lpStr+1);

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
				}

				if ( ! bContinue )
				{
					DTPColor[0]= xx;
					DTPColor[1]= yy;
					DTPColor[2]= zz;

					colMeasure.SetSensorValue(DTPColor);
				}
			}
			else
			{
				MessageBox(0, "SendAndReceived failed","Error",MB_OK+MB_ICONINFORMATION);
				return noDataColor;
			}
		} while ( bContinue );
	}
	else 
	{
		MessageBox(0, "DTP not connected","Error",MB_OK+MB_ICONINFORMATION);
		return noDataColor;
	}
	
	return colMeasure.GetSensorValue();
}

BOOL CDTPSensor::SendAndReceive ( LPCSTR lpszCmd, LPSTR lpszResponse, size_t nBufferSize )
{
	BOOL	bOk = FALSE;
	DWORD	status;
	DWORD	BytesReceived;
	BYTE	dummy [ 16 ];
	
	if ( pSendAndReceive ( hConnection, lpszCmd, strlen(lpszCmd), 0, 1, ( lpszResponse ? (LPBYTE) lpszResponse : dummy ), lpszResponse ? nBufferSize : sizeof (dummy), m_timeout, & BytesReceived, & status ) )
	{
		bOk = TRUE;
		
		if ( lpszResponse && BytesReceived < nBufferSize )
			lpszResponse [ BytesReceived ] = '\0';
	}

	if (m_debugMode) 
	{
		EnterCriticalSection ( & GetConfig () -> LogFileCritSec );
		CTime theTime = CTime::GetCurrentTime(); 
		CString s = theTime.Format( "%d/%m/%y %H:%M:%S" );		
		FILE *f = fopen ( GetConfig () -> m_logFileName, "a" );
		fprintf(f, "DTP94 - %s : %-6s : %s\n", s, lpszCmd, ( bOk ? ( lpszResponse ? lpszResponse : "succeeded" ) : "failed" ) );
		fclose(f);
		LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
	}


	return bOk;
}




