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
//	Georges GALLERAND
//  Patrice AFFLATET
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// KiSensor.cpp: implementation of the KiSensor class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "KiSensor.h"
#include "SerialCom.h"

#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// Authorized firmwares
#define FIRMWARE_MAJOR_VERSION	5
#define FIRMWARE_MINOR_VERSION	0

// First firmware allowing LED full control
#define FIRMWARE_LED_MAJOR_VERSION	5
#define FIRMWARE_LED_MINOR_VERSION	20


static BOOL bVersionValidated = FALSE;
static BOOL bAllowLedMessages = FALSE;


BOOL TestKiDeviceVersion ( LPCSTR szComPort, LPSTR lpszErrorMsg, LPBOOL lpbAllowLedMessages )
{
	CSerialCom port;
	CString	Msg;
	int		nMajor, nMinor;
	char	c[2] = "";
	char	szVersion [ 256 ] = "";
	LPSTR	lpStr;
	BYTE	data=0;
	BOOL	bOk = FALSE;

	if ( lpbAllowLedMessages )
		* lpbAllowLedMessages = bAllowLedMessages;

	if ( bVersionValidated )
		return TRUE;

	lpszErrorMsg [ 0 ] = '\0';
	if(!(port.OpenPort(szComPort)))
	{
		strcpy ( lpszErrorMsg, "Cannot open Communication Port" );
	} 
	else 
	{
		if(!(port.ConfigurePort(115200,8,0,NOPARITY ,ONESTOPBIT )))
		{
			strcpy ( lpszErrorMsg, "Cannot Configure Communication Port" );
			port.ClosePort();
		} 
		else 
		{
			if(!(port.SetCommunicationTimeouts(0,2000,0,0,0)))
			{
				strcpy ( lpszErrorMsg, "Cannot Configure Communication Timeouts" );
				port.ClosePort();
			} 
			else 
			{
				if(!(port.WriteByte(0xFF)))
				{
					strcpy ( lpszErrorMsg, "Cannot Write to Port" );
					port.ClosePort();
				} 
				else 
				{
					while(port.ReadByte(data))
					{
						if (data==13) break;
						sprintf(c, "%c", data);
						strcat(szVersion, c);
					}
					port.ClosePort();
					
					lpStr = strchr ( szVersion, '.' );
					if ( (szVersion [ 0 ] == 'v' || szVersion [ 0 ] == 's') && lpStr )
					{
						nMajor = atoi ( szVersion + 1 );
						nMinor = atoi ( lpStr + 1 );

						if ( nMajor > FIRMWARE_LED_MAJOR_VERSION || ( nMajor == FIRMWARE_LED_MAJOR_VERSION && nMinor >= FIRMWARE_LED_MINOR_VERSION ) )
						{
							bAllowLedMessages = TRUE;

							if ( lpbAllowLedMessages )
								* lpbAllowLedMessages = bAllowLedMessages;
						}

						if ( nMajor == FIRMWARE_MAJOR_VERSION && nMinor >= FIRMWARE_MINOR_VERSION )
						{
							bOk = TRUE;
							bVersionValidated = TRUE;
						}
						else
						{
							Msg.LoadString ( IDS_BADFIRMWARE1 );
							sprintf ( lpszErrorMsg, (LPCSTR)Msg, FIRMWARE_MAJOR_VERSION, FIRMWARE_MINOR_VERSION, szVersion );
						}
					}
					else
					{
						Msg.LoadString ( IDS_BADFIRMWARE2 );
						sprintf ( lpszErrorMsg, (LPCSTR)Msg, szVersion );
					}
				}
			}
		} 
	}
	return bOk;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CKiSensor, COneDeviceSensor, 1) ;

CKiSensor::CKiSensor()
{
	m_timeoutMesure = GetConfig()->GetProfileInt("HCFRSensor","Timeout",40000);
	m_debugMode = FALSE;
	m_comPort = "USB";
	m_RealComPort = "";	// Initialised by INIT

	m_bMeasureRGB = TRUE;
	m_bMeasureWhite = FALSE;
	m_nSensorsUsed = GetConfig()->GetProfileInt("HCFRSensor","Sensor",0);
	m_nInterlaceMode = GetConfig()->GetProfileInt("HCFRSensor","Interlace",0);
	m_bFastMeasure = GetConfig()->GetProfileInt("HCFRSensor","FastMeasure",0);

	m_bSingleSensor = GetConfig () -> GetProfileInt ( "Debug", "SingleSensor", FALSE );
	m_bLEDStopped = FALSE;

	m_kiSensorPropertiesPage.m_pSensor = this;

	m_pDevicePage = & m_kiSensorPropertiesPage;  // Add ki settings page to property sheet

	m_PropertySheetTitle = IDS_KISENSOR_PROPERTIES_TITLE;

	CString str;
	str.LoadString(IDS_KISENSOR_NAME);
	SetName(str);
}


CKiSensor::~CKiSensor()
{

}

void CKiSensor::Copy(CSensor * p)
{
	COneDeviceSensor::Copy(p);
	
	m_timeoutMesure = ((CKiSensor*)p)->m_timeoutMesure;
	m_debugMode = ((CKiSensor*)p)->m_debugMode;
	m_bMeasureRGB = ((CKiSensor*)p)->m_bMeasureRGB;
	m_bMeasureWhite = ((CKiSensor*)p)->m_bMeasureWhite;
	m_nSensorsUsed = ((CKiSensor*)p)->m_nSensorsUsed;
	m_nInterlaceMode = ((CKiSensor*)p)->m_nInterlaceMode;
	m_bFastMeasure = ((CKiSensor*)p)->m_bFastMeasure;
	m_bSingleSensor = ((CKiSensor*)p)->m_bSingleSensor;

	m_comPort = ((CKiSensor*)p)->m_comPort;
	m_RealComPort = ((CKiSensor*)p)->m_RealComPort;
}

void CKiSensor::Serialize(CArchive& archive)
{
	COneDeviceSensor::Serialize(archive) ;

	if (archive.IsStoring())
	{
		int version=2;
		archive << version;
		archive << m_comPort;
		archive << m_timeoutMesure;
		archive << m_debugMode;

		archive << m_bMeasureWhite;
		archive << m_nSensorsUsed;
		archive << m_nInterlaceMode;
		archive << m_bFastMeasure;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 2 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		if ( version == 1 )
		{
			// Old sensor dialog parameters
			CString	cmdMesure;
			UINT	sensorInUse;
			double	nbPeriodCoef;
			double	dureeMesureCoef;
			double	hertzCoef;

			archive >> m_timeoutMesure;
			archive >> cmdMesure;		// dummy
			archive >> m_debugMode;
			archive >> sensorInUse;		// dummy
			archive >> m_comPort;
			archive >> nbPeriodCoef;	// dummy
			archive >> dureeMesureCoef;	// dummy
			archive >> hertzCoef;		// dummy

			// Set default values for new mode
			m_bMeasureWhite = FALSE;
			m_nSensorsUsed = 0;
			m_nInterlaceMode = 0;
			m_bFastMeasure = FALSE;
		}
		else
		{
			// New sensor dialog parameters
			archive >> m_comPort;
			archive >> m_timeoutMesure;
			archive >> m_debugMode;

			archive >> m_bMeasureWhite;
			archive >> m_nSensorsUsed;
			archive >> m_nInterlaceMode;
			archive >> m_bFastMeasure;
		}
	}
}

void CKiSensor::SetPropertiesSheetValues()
{
	COneDeviceSensor::SetPropertiesSheetValues();

	m_kiSensorPropertiesPage.m_timeoutMesure=m_timeoutMesure;
	m_kiSensorPropertiesPage.m_debugMode=m_debugMode;
	m_kiSensorPropertiesPage.m_comPort=m_comPort;
	m_kiSensorPropertiesPage.m_bMeasureWhite=m_bMeasureWhite;
	m_kiSensorPropertiesPage.m_nSensorsUsed=m_nSensorsUsed;
	m_kiSensorPropertiesPage.m_nInterlaceMode=m_nInterlaceMode;
	m_kiSensorPropertiesPage.m_bFastMeasure=m_bFastMeasure;
}

void CKiSensor::GetPropertiesSheetValues()
{
	COneDeviceSensor::GetPropertiesSheetValues();

	if( m_timeoutMesure != m_kiSensorPropertiesPage.m_timeoutMesure ) {
		m_timeoutMesure=m_kiSensorPropertiesPage.m_timeoutMesure;
		GetConfig()->WriteProfileInt("HCFRSensor","Timeout",m_timeoutMesure);
		SetModifiedFlag(TRUE);
	}

	if( m_debugMode != m_kiSensorPropertiesPage.m_debugMode ) {
		m_debugMode=m_kiSensorPropertiesPage.m_debugMode;
		SetModifiedFlag(TRUE);
	}

	if( m_comPort != m_kiSensorPropertiesPage.m_comPort ) {
		m_comPort=m_kiSensorPropertiesPage.m_comPort;
		SetModifiedFlag(TRUE);
	}

	if( m_bMeasureWhite != m_kiSensorPropertiesPage.m_bMeasureWhite ) {
		m_bMeasureWhite=m_kiSensorPropertiesPage.m_bMeasureWhite;
		SetModifiedFlag(TRUE);
	}

	if( m_nSensorsUsed != m_kiSensorPropertiesPage.m_nSensorsUsed ) {
		m_nSensorsUsed=m_kiSensorPropertiesPage.m_nSensorsUsed;
		GetConfig()->WriteProfileInt("HCFRSensor","Sensor",m_nSensorsUsed);
		SetModifiedFlag(TRUE);
	}

	if( m_nInterlaceMode != m_kiSensorPropertiesPage.m_nInterlaceMode ) {
		m_nInterlaceMode=m_kiSensorPropertiesPage.m_nInterlaceMode;
		GetConfig()->WriteProfileInt("HCFRSensor","Interlace",m_nInterlaceMode);
		SetModifiedFlag(TRUE);
	}

	if( m_bFastMeasure != m_kiSensorPropertiesPage.m_bFastMeasure ) {
		m_bFastMeasure=m_kiSensorPropertiesPage.m_bFastMeasure;
		GetConfig()->WriteProfileInt("HCFRSensor","FastMeasure",m_bFastMeasure);
		SetModifiedFlag(TRUE);
	}
}

void CKiSensor::GetRealComPort ( CString & ComPort )
{
	int			nKeyIndex;
	BOOL		bOk = FALSE;
	DWORD		cb, cb2;
	DWORD		dwType;
	HKEY		hKey, hSubKey;
	char		szBuf [ 1024 ];
	char		szBuf2 [ 256 ];
	CString		strCom;
	CStringList	PotentialComList;
	CStringList	ExistingComList;
	POSITION	pos;

	ComPort.Empty ();

	if ( m_comPort == "USB" )
	{
		// Search real COM port name in registry
		// Enumerate com ports for installed HCFR device
		// New USB identifier
		if ( ERROR_SUCCESS == RegOpenKeyEx ( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Enum\\USB\\Vid_04d8&Pid_fe17", 0, KEY_READ, & hKey ) )
		{
			nKeyIndex = 0;
			cb = sizeof ( szBuf );
			while ( ERROR_SUCCESS == RegEnumKeyEx ( hKey, nKeyIndex, szBuf, & cb, NULL, NULL, NULL, NULL ) )
			{
				strcat ( szBuf, "\\Device Parameters" );
				if ( ERROR_SUCCESS == RegOpenKeyEx ( hKey, szBuf, 0, KEY_READ, & hSubKey ) )
				{
					cb = sizeof ( szBuf );
					if ( ERROR_SUCCESS == RegQueryValueEx ( hSubKey, "PortName", NULL, NULL, (LPBYTE) szBuf, & cb ) )
					{
						if ( _strnicmp ( szBuf, "COM", 3 ) == 0 )
							PotentialComList.AddTail ( szBuf );
					}
					RegCloseKey ( hSubKey );
				}
				nKeyIndex ++;
				cb = sizeof ( szBuf );
			}
			RegCloseKey ( hKey );
		}

		// Old USB identifier
		// Enumerate com ports for installed HCFR device
		if ( ERROR_SUCCESS == RegOpenKeyEx ( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Enum\\USB\\Vid_04db&Pid_005b", 0, KEY_READ, & hKey ) )
		{
			nKeyIndex = 0;
			cb = sizeof ( szBuf );
			while ( ERROR_SUCCESS == RegEnumKeyEx ( hKey, nKeyIndex, szBuf, & cb, NULL, NULL, NULL, NULL ) )
			{
				strcat ( szBuf, "\\Device Parameters" );
				if ( ERROR_SUCCESS == RegOpenKeyEx ( hKey, szBuf, 0, KEY_READ, & hSubKey ) )
				{
					cb = sizeof ( szBuf );
					if ( ERROR_SUCCESS == RegQueryValueEx ( hSubKey, "PortName", NULL, NULL, (LPBYTE) szBuf, & cb ) )
					{
						if ( _strnicmp ( szBuf, "COM", 3 ) == 0 )
							PotentialComList.AddTail ( szBuf );
					}
					RegCloseKey ( hSubKey );
				}
				nKeyIndex ++;
				cb = sizeof ( szBuf );
			}
			RegCloseKey ( hKey );
		}
		
		// Enumerate currently active com port, filtered by device type
		if ( ERROR_SUCCESS == RegOpenKeyEx ( HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, & hKey ) )
		{
			nKeyIndex = 0;
			cb = sizeof ( szBuf );
			cb2 = sizeof ( szBuf2 );
			while ( ERROR_SUCCESS == RegEnumValue ( hKey, nKeyIndex, szBuf, & cb, NULL, & dwType, (LPBYTE) szBuf2, & cb2 ) )
			{
				if ( _strnicmp ( szBuf, "\\Device\\USBSER", 14 ) == 0 && dwType == REG_SZ )
				{
					if ( _strnicmp ( szBuf2, "COM", 3 ) == 0 )
						ExistingComList.AddTail ( szBuf2 );
				}
				nKeyIndex ++;
				cb = sizeof ( szBuf );
				cb2 = sizeof ( szBuf2 );
			}
			RegCloseKey ( hKey );
		}

		// Search in active com device list which one is HCFR device
		pos = ExistingComList.GetHeadPosition ();
		while ( pos )
		{
			strCom = ExistingComList.GetNext ( pos );

			if ( PotentialComList.Find ( (LPCTSTR) strCom ) != NULL )
			{
				ComPort = strCom;
				break;
			}
		}
	}
	else
	{
		// Standard COM port defined
		ComPort = m_comPort;
	}
}

BOOL CKiSensor::Init( BOOL bForSimultaneousMeasures )
{
	BOOL		bOk = FALSE;
	BOOL		bLEDControl = FALSE;
	char		szBuf [ 1024 ];

	GetRealComPort ( m_RealComPort );
	
	if ( ! m_RealComPort.IsEmpty () )
		bOk = TRUE;

	if ( bOk )
	{
		// Check sensor version
		bOk = TestKiDeviceVersion ( (LPCTSTR) m_RealComPort, szBuf, & bLEDControl );

		if ( ! bOk )
		{
			// Display error message
			AfxMessageBox ( szBuf, MB_OK + MB_ICONERROR );
		}
		else
		{
			if ( bForSimultaneousMeasures && bLEDControl )
			{
				char mStr[255];
				
				acquire((char *) (LPCTSTR) m_RealComPort, m_timeoutMesure, (char)0x83 , mStr);
				m_bLEDStopped = TRUE;
			} 
		}
	}

	return bOk;
}

BOOL CKiSensor::Release()
{
	if ( m_bLEDStopped )
	{
		char mStr[255];
		
		acquire((char *) (LPCTSTR) m_RealComPort, m_timeoutMesure, (char)0x82 , mStr);
		m_bLEDStopped = FALSE;
	}
	return CSensor::Release();
}

void CKiSensor::GetUniqueIdentifier( CString & strId )
{
	CString	ComPort;

	GetRealComPort ( ComPort );
	if ( ComPort.IsEmpty () )
		strId = m_name;
	else
		strId = m_name + " on " + ComPort;
}

BOOL CKiSensor::acquire(char *com_port, int timeout, char command, char *sensVal)
{
	CSerialCom port;
	char c[2];

	strcpy(c, "");
	strcpy(sensVal, "");
	BYTE data=0;

	if(!(port.OpenPort(com_port))){
		MessageBox(0, "Cannot open Communication Port","Error",MB_OK+MB_ICONERROR);
	} else {
		if(!(port.ConfigurePort(115200,8,0,NOPARITY ,ONESTOPBIT )))
		{
			MessageBox(0, "Cannot Configure Communication Port","Error",MB_OK+MB_ICONERROR);
			port.ClosePort();
		} else {
			if(!(port.SetCommunicationTimeouts(0,timeout,0,0,0)))
			{
				MessageBox(0, "Cannot Configure Communication Timeouts","Error",MB_OK+MB_ICONERROR);
				port.ClosePort();
			} else {
				if(!(port.WriteByte(command)))
				{
					MessageBox(0, "Cannot Write to Port","Error",MB_OK+MB_ICONERROR);
					port.ClosePort();
				} else {
					while(port.ReadByte(data))
					{
						if (data==13) break;
						sprintf(c, "%c", data);
						strcat(sensVal, c);
					}
					port.ClosePort();
					return TRUE;
				}
			}
		} 
	}
	return FALSE;
}

CColor CKiSensor::MeasureColorInternal(COLORREF aRGBValue)
{
	CColor kiColor;
	CColor colMeasure;
	char mStr[255];
	char cmd[1] = "";
	
	// Create command byte
	cmd[0] = ( m_bMeasureRGB & 0x01 ) + ( ( m_bMeasureWhite & 0x01 ) << 1 ) + ( ( m_nSensorsUsed & 0x03 ) << 2 ) + ( ( m_nInterlaceMode & 0x03 ) << 4 ) + ( ( m_bFastMeasure & 0x01 ) << 6 );

	// sensor = 0;
	if( acquire((char *) (LPCTSTR) m_RealComPort, m_timeoutMesure, *cmd , mStr) )
	{
		int RGB[3];
		decodeKiStr(mStr, RGB);
		kiColor[0]= RGB[0];
		kiColor[1]= RGB[1];
		kiColor[2]= RGB[2];
        double defaultSensorToXYZ[3][3] = { {   7.79025E-05,  5.06389E-05,   6.02556E-05  }, 
                                            {   3.08665E-05,  0.000131285,   2.94813E-05  },
                                            {  -9.41924E-07, -4.42599E-05,   0.000271669  } };

        Matrix defaultSensorToXYZMatrix(&defaultSensorToXYZ[0][0],3,3);

        colMeasure = defaultSensorToXYZMatrix * kiColor;
	} 
    else 
	{
		MessageBox(0, "No data from Sensor","Success",MB_OK+MB_ICONINFORMATION);
		return noDataColor;
	}
	return colMeasure;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////


// Sample result : "RGB0 000 030 255 156 089 034 000 030 254 179 042 111 000 030 254 233 033 121"
void CKiSensor::decodeKiStr(char *kiStr, int RGB[3])
{
	RGB[0]= RGB[1]= RGB[2]= 0;

	if ( strlen(kiStr) >= 156 )
	{
		int intRGB[48];
		int	nDiv, nMul;
		int nSensor;
		char dummyStr[6];
		
		sscanf(kiStr, "%4s%1d:%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d%3d",
			&dummyStr, & nSensor, & nDiv, & nMul,
			&intRGB[0],  &intRGB[1],  &intRGB[2],  &intRGB[3],  &intRGB[4],  &intRGB[5], 
			&intRGB[6],  &intRGB[7],  &intRGB[8],  &intRGB[9],  &intRGB[10], &intRGB[11], 
			&intRGB[12], &intRGB[13], &intRGB[14], &intRGB[15], &intRGB[16], &intRGB[17],
			&intRGB[18], &intRGB[19], &intRGB[20], &intRGB[21], &intRGB[22], &intRGB[23], 
			&intRGB[24], &intRGB[25], &intRGB[26], &intRGB[27], &intRGB[28], &intRGB[29], 
			&intRGB[30], &intRGB[31], &intRGB[32], &intRGB[33], &intRGB[34], &intRGB[35], 
			&intRGB[36], &intRGB[37], &intRGB[38], &intRGB[39], &intRGB[40], &intRGB[41], 
			&intRGB[42], &intRGB[43], &intRGB[44], &intRGB[45], &intRGB[46], &intRGB[47]);
		
		int color= 0;
		for(int hue= 0; hue <3; hue++) 
		{
			if ( nSensor > 1 && ! m_bSingleSensor )
			{
				RGB[hue] = (int)( 1000000.0 * 
									(
										(
											( (double) ( ( intRGB[color+4] << 8 ) + intRGB[color+5] ) * (double) nMul )
											/	
											( (double) ( ( intRGB[color+0] << 24 ) + ( intRGB[color+1] << 16 ) + ( intRGB[color+2] << 8 ) + intRGB[color+3] ) / (double) nDiv )
										)
										+
										(
											( (double) ( ( intRGB[color+24+4] << 8 ) + intRGB[color+24+5] ) * (double) nMul )
											/	
											( (double) ( ( intRGB[color+24+0] << 24 ) + ( intRGB[color+24+1] << 16 ) + ( intRGB[color+24+2] << 8 ) + intRGB[color+24+3] ) / (double) nDiv )
										)
									) / 2.0 
								);
			}
			else
			{
				RGB[hue] = (int)( 1000000.0 * 
									(
										( (double) ( ( intRGB[color+4] << 8 ) + intRGB[color+5] ) * (double) nMul )
										/	
										( (double) ( ( intRGB[color+0] << 24 ) + ( intRGB[color+1] << 16 ) + ( intRGB[color+2] << 8 ) + intRGB[color+3] ) / (double) nDiv )
									) 
								);
			}
			color+= 6;
		}
	}

	if (m_debugMode) 
	{
		EnterCriticalSection ( & GetConfig () -> LogFileCritSec );
		CTime theTime = CTime::GetCurrentTime(); 
		CString s = theTime.Format( "%d/%m/%y %H:%M:%S" );		
		FILE *f = fopen ( GetConfig () -> m_logFileName, "a" );
		fprintf(f, "HCFR - %s : %s R:%d G:%d B:%d\n", s, kiStr, RGB[0],RGB[1],RGB[2]);
		fclose(f);
		LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
	}
}
