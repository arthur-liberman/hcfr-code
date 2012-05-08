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

// KiGenerator.cpp: implementation of the CKiGenerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "KiGenerator.h"
#include "SerialCom.h"

#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define	GRAYLVLNUMBER	11
#define	ADVANCEDGRAYLVLNUMBER	21
#define	SATURATIONLVLNUMBER	5
#define	NEARBLACKLVLNUMBER	5
#define	NEARWHITELVLNUMBER	5

BOOL TestKiDeviceVersion ( LPCSTR szComPort, LPSTR lpszErrorMsg, LPBOOL lpbAllowLedMessages );	// Implemented in KiSensor.cpp

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CKiGenerator,CGenerator,1) ;

CKiGenerator::CKiGenerator()
{
	m_bBeforeFirstDisplay = FALSE;
	m_comPort = "USB";
	m_RealComPort = "";		// Initialised by INIT
	m_IRProfileName = "";

	CString str, str2;
	str.LoadString(IDS_KIGENERATOR_PROPERTIES_TITLE);
	m_propertySheet.SetTitle(str);

	str2.LoadString(IDS_KIGENERATOR_NAME);

	m_bUseIRCodes = GetConfig () -> GetProfileInt ( "Debug", "KiGeneratorUseNext", 1 );

	m_showDialog = GetConfig () -> GetProfileInt ( (LPCSTR) str2, "ShowDialog", 1 );;

	m_NbNextCodeToAdd = 0;

	m_IRProfileName = GetConfig () -> GetProfileString ( (LPCSTR) str2, "IRProfileName", "" );
	CIRProfile IRProfile;
	BOOL result = IRProfile.Load(m_IRProfileName);

	m_NextCodeLength = IRProfile.GetBinaryIRCode (CIRProfile::CT_NEXT, m_NextCode );
	m_OkCodeLength = IRProfile.GetBinaryIRCode (CIRProfile::CT_OK, m_OkCode );
	m_DownCodeLength = IRProfile.GetBinaryIRCode (CIRProfile::CT_DOWN, m_DownCode );
	m_RightCodeLength = IRProfile.GetBinaryIRCode (CIRProfile::CT_RIGHT, m_RightCode );
	m_NavInMenuLatency = IRProfile.m_NavInMenuLatency;
	m_ValidMenuLatency = IRProfile.m_ValidMenuLatency;
	m_NextChapterLatency = IRProfile.m_NextChapterLatency;
	
	m_patternCheckMaxRetryByPattern=GetConfig()->GetProfileInt((LPCSTR) str2,"patternCheckMaxRetryByPattern",0);
	m_patternCheckMaxRetryBySeries=GetConfig()->GetProfileInt((LPCSTR) str2,"patternCheckMaxRetryBySeries",0);
	m_patternCheckMaxThreshold=GetConfig()->GetProfileInt((LPCSTR) str2,"patternCheckMaxThreshold",0);
	m_patternCheckOnGrayscale=GetConfig()->GetProfileInt((LPCSTR) str2,"patternCheckOnGrayscale",0);
	m_patternCheckOnSaturations=GetConfig()->GetProfileInt((LPCSTR) str2,"patternCheckOnSaturations",0);
	m_patternCheckOnPrimaries=GetConfig()->GetProfileInt((LPCSTR) str2,"patternCheckOnPrimaries",0);
	m_patternCheckOnSecondaries=GetConfig()->GetProfileInt((LPCSTR) str2,"patternCheckOnSecondaries",0);

	AddPropertyPage(&m_KiGenePropertiesPage);

	str.LoadString(IDS_KIGENERATOR_NAME);
	SetName(str);
}

CKiGenerator::~CKiGenerator()
{

}

void CKiGenerator::Copy(CGenerator * p)
{
	CGenerator::Copy(p);

	m_comPort = ((CKiGenerator*)p)->m_comPort;
	m_showDialog = ((CKiGenerator*)p)->m_showDialog;
	m_RealComPort = ((CKiGenerator*)p)->m_RealComPort;
	m_bUseIRCodes = ((CKiGenerator*)p)->m_bUseIRCodes;

	m_NextCodeLength = ((CKiGenerator*)p)->m_NextCodeLength;
	m_OkCodeLength = ((CKiGenerator*)p)->m_OkCodeLength;
	m_DownCodeLength = ((CKiGenerator*)p)->m_DownCodeLength;
	m_RightCodeLength = ((CKiGenerator*)p)->m_RightCodeLength;
	m_NavInMenuLatency = ((CKiGenerator*)p)->m_NavInMenuLatency;
	m_ValidMenuLatency = ((CKiGenerator*)p)->m_ValidMenuLatency;
	m_NextChapterLatency = ((CKiGenerator*)p)->m_NextChapterLatency;
	m_bBeforeFirstDisplay = ((CKiGenerator*)p)->m_bBeforeFirstDisplay;

	int i;

	for (i=0; i<m_NextCodeLength; i++){ m_NextCode [i] = ((CKiGenerator*)p)->m_NextCode [i];}
	for (i=0; i<m_OkCodeLength; i++){ m_OkCode [i] = ((CKiGenerator*)p)->m_OkCode [i];}
	for (i=0; i<m_DownCodeLength; i++){ m_DownCode [i] = ((CKiGenerator*)p)->m_DownCode [i];}
	for (i=0; i<m_RightCodeLength; i++){ m_RightCode [i] = ((CKiGenerator*)p)->m_RightCode [i];}
}

void CKiGenerator::Serialize(CArchive& archive)
{
	CGenerator::Serialize(archive) ;
	if (archive.IsStoring())
	{
		int version=3;
		archive << version;
		archive << m_doScreenBlanking;
		archive << m_comPort;
		archive << m_showDialog;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 3 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_doScreenBlanking;
		
		if ( version >= 2 )
			archive >> m_comPort;
		else
			m_comPort = "USB";

		if ( version >= 3 )
			archive >> m_showDialog;
		else
			m_showDialog = TRUE;
	}
}


void CKiGenerator::SetPropertiesSheetValues()
{
	CString	str2;

	CGenerator::SetPropertiesSheetValues();

	m_KiGenePropertiesPage.m_showDialog=m_showDialog;
	m_KiGenePropertiesPage.m_comPort=m_comPort;
	m_KiGenePropertiesPage.m_pGenerator = this;

	str2.LoadString(IDS_KIGENERATOR_NAME);
	m_KiGenePropertiesPage.m_IRProfileFileName = GetConfig () -> GetProfileString ( (LPCSTR) str2, "IRProfileName", "" );

	m_KiGenePropertiesPage.m_patternCheckMaxRetryByPattern=m_patternCheckMaxRetryByPattern;
	m_KiGenePropertiesPage.m_patternCheckMaxRetryBySeries=m_patternCheckMaxRetryBySeries;
	m_KiGenePropertiesPage.m_patternCheckMaxThreshold=m_patternCheckMaxThreshold;
	m_KiGenePropertiesPage.m_patternCheckOnGrayscale=m_patternCheckOnGrayscale;
	m_KiGenePropertiesPage.m_patternCheckOnSaturations=m_patternCheckOnSaturations;
	m_KiGenePropertiesPage.m_patternCheckOnPrimaries=m_patternCheckOnPrimaries;
	m_KiGenePropertiesPage.m_patternCheckOnSecondaries=m_patternCheckOnSecondaries;
}

void CKiGenerator::GetPropertiesSheetValues()
{
	CString	str, str2;

	CGenerator::GetPropertiesSheetValues();

	str2.LoadString(IDS_KIGENERATOR_NAME);

	if( m_showDialog != m_KiGenePropertiesPage.m_showDialog )
	{
		m_showDialog=m_KiGenePropertiesPage.m_showDialog;
		GetConfig () -> WriteProfileInt ( (LPCSTR) str2, "ShowDialog", m_KiGenePropertiesPage.m_showDialog );
		SetModifiedFlag(TRUE);
	}

	if( m_comPort != m_KiGenePropertiesPage.m_comPort )
	{
		m_comPort=m_KiGenePropertiesPage.m_comPort;
		SetModifiedFlag(TRUE);
	}

	m_IRProfileName = m_KiGenePropertiesPage.m_IRProfileFileName;
	str = GetConfig () -> GetProfileString ( (LPCSTR) str2, "IRProfileName", "" );
	if ((m_KiGenePropertiesPage.m_IRProfileFileIsModified)||(m_IRProfileName != str))
	{
		if (m_IRProfileName != str)
			GetConfig () -> WriteProfileString ( (LPCSTR) str2, "IRProfileName", m_KiGenePropertiesPage.m_IRProfileFileName);
		CIRProfile IRProfile;
		BOOL result = IRProfile.Load(m_IRProfileName);
		m_NextCodeLength = IRProfile.GetBinaryIRCode (CIRProfile::CT_NEXT, m_NextCode );
		m_OkCodeLength = IRProfile.GetBinaryIRCode (CIRProfile::CT_OK, m_OkCode );
		m_DownCodeLength = IRProfile.GetBinaryIRCode (CIRProfile::CT_DOWN, m_DownCode );
		m_RightCodeLength = IRProfile.GetBinaryIRCode (CIRProfile::CT_RIGHT, m_RightCode );
		m_NavInMenuLatency = IRProfile.m_NavInMenuLatency;
		m_ValidMenuLatency = IRProfile.m_ValidMenuLatency;
		m_NextChapterLatency = IRProfile.m_NextChapterLatency;
		m_KiGenePropertiesPage.m_IRProfileFileIsModified = FALSE;
	}

	if( m_patternCheckMaxRetryByPattern != m_KiGenePropertiesPage.m_patternCheckMaxRetryByPattern )
	{
		m_patternCheckMaxRetryByPattern=m_KiGenePropertiesPage.m_patternCheckMaxRetryByPattern;
		GetConfig()->WriteProfileInt((LPCSTR) str2,"patternCheckMaxRetryByPattern",m_patternCheckMaxRetryByPattern);
		SetModifiedFlag(TRUE);
	}

	if( m_patternCheckMaxRetryBySeries != m_KiGenePropertiesPage.m_patternCheckMaxRetryBySeries )
	{
		m_patternCheckMaxRetryBySeries=m_KiGenePropertiesPage.m_patternCheckMaxRetryBySeries;
		GetConfig()->WriteProfileInt((LPCSTR) str2,"patternCheckMaxRetryBySeries",m_patternCheckMaxRetryBySeries);
		SetModifiedFlag(TRUE);
	}
	
	if( m_patternCheckMaxThreshold != m_KiGenePropertiesPage.m_patternCheckMaxThreshold )
	{
		m_patternCheckMaxThreshold=m_KiGenePropertiesPage.m_patternCheckMaxThreshold;
		GetConfig()->WriteProfileInt((LPCSTR) str2,"patternCheckMaxThreshold",m_patternCheckMaxThreshold);
		SetModifiedFlag(TRUE);
	}

	if( m_patternCheckOnGrayscale != m_KiGenePropertiesPage.m_patternCheckOnGrayscale )
	{
		m_patternCheckOnGrayscale=m_KiGenePropertiesPage.m_patternCheckOnGrayscale;
		GetConfig()->WriteProfileInt((LPCSTR) str2,"patternCheckOnGrayscale",m_patternCheckOnGrayscale);
		SetModifiedFlag(TRUE);
	}

	if( m_patternCheckOnSaturations != m_KiGenePropertiesPage.m_patternCheckOnSaturations )
	{
		m_patternCheckOnSaturations=m_KiGenePropertiesPage.m_patternCheckOnSaturations;
		GetConfig()->WriteProfileInt((LPCSTR) str2,"patternCheckOnSaturations",m_patternCheckOnSaturations);
		SetModifiedFlag(TRUE);
	}

	if( m_patternCheckOnPrimaries != m_KiGenePropertiesPage.m_patternCheckOnPrimaries )
	{
		m_patternCheckOnPrimaries=m_KiGenePropertiesPage.m_patternCheckOnPrimaries;
		GetConfig()->WriteProfileInt((LPCSTR) str2,"patternCheckOnPrimaries",m_patternCheckOnPrimaries);
		SetModifiedFlag(TRUE);
	}

	if( m_patternCheckOnSecondaries != m_KiGenePropertiesPage.m_patternCheckOnSecondaries )
	{
		m_patternCheckOnSecondaries=m_KiGenePropertiesPage.m_patternCheckOnSecondaries;
		GetConfig()->WriteProfileInt((LPCSTR) str2,"patternCheckOnSecondaries",m_patternCheckOnSecondaries);
		SetModifiedFlag(TRUE);
	}
}

BOOL CKiGenerator::Init(UINT nbMeasure)
{
	BOOL bOk = InitRealComPort();

	m_bBeforeFirstDisplay = TRUE;
	m_patternRetry = m_patternCheckMaxRetryByPattern;
	m_totalPatternRetry = m_patternCheckMaxRetryBySeries;
	m_minDif = (double)m_patternCheckMaxThreshold/100;
	m_retryList ="";
	m_NbNextCodeToAdd = nbMeasure;
		
	if ( bOk )
	{
		// Check sensor version
		char	szBuf [ 512 ];
		bOk = TestKiDeviceVersion ( (LPCTSTR) m_RealComPort, szBuf, NULL );

		if ( ! bOk )
		{
			// Display error message
			AfxMessageBox ( szBuf, MB_OK | MB_ICONERROR );
		}
	}

	if ( bOk )
	{
		if ( m_bUseIRCodes && m_NextCodeLength == 0 )
		{
			// No next code: cannot proceed
			CString	Msg;
			Msg.LoadString ( IDS_NONEXTCHAPTERCODE );
			AfxMessageBox ( Msg, MB_OK | MB_ICONERROR );
			bOk = FALSE;
		}
	}

	if ( bOk )
		bOk = CGenerator::Init(nbMeasure);

	return bOk;
}

BOOL CKiGenerator::InitRealComPort()
{
	int			nKeyIndex;
	BOOL		bOk = FALSE;
	DWORD		cb, cb2;
	DWORD		dwType;
	HKEY		hKey, hSubKey;
	char		szBuf [ 256 ];
	char		szBuf2 [ 256 ];
	CString		strCom;
	CStringList	PotentialComList;
	CStringList	ExistingComList;
	POSITION	pos;

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
				m_RealComPort = strCom;
				bOk = TRUE;
				break;
			}
		}
	}
	else
	{
		// Standard COM port defined
		m_RealComPort = m_comPort;
		bOk = TRUE;
	}

	return bOk;
}

BOOL CKiGenerator::Release(INT nbNext)
{
	m_bBeforeFirstDisplay = FALSE;

	if (nbNext >=0)
		m_NbNextCodeToAdd = nbNext;

	if ( m_bUseIRCodes )
	{
		// Send a "next" code to return directly to menu
		char mStr[255];
		
		if ( m_NbNextCodeToAdd > 0 )
			AfxGetApp () -> BeginWaitCursor ();

		for ( int i = 0; i <= m_NbNextCodeToAdd ; i ++ )
		{
			SendIRCode (1000, m_NextCode, m_NextCodeLength, mStr);
			
			// No latency on last code emitted
			if ( i < m_NbNextCodeToAdd )
				Sleep ( min(500,m_NextChapterLatency) );
		}

		if ( m_NbNextCodeToAdd > 0 )
			AfxGetApp () -> EndWaitCursor ();
	}
	
	m_NbNextCodeToAdd = 0;
	
	return CGenerator::Release();
}

BOOL CKiGenerator::ChangePatternSeries()
{
	BOOL Flag = FALSE;
	m_bBeforeFirstDisplay = TRUE;

	if ( m_bUseIRCodes )
	{
		// Send "next" code to return to menu
		char mStr[255];
		
		if (SendIRCode (1000, m_NextCode, m_NextCodeLength, mStr) )
		{
			Sleep ( m_NextChapterLatency
				);
			Flag = TRUE;
		}
		AfxGetApp () -> EndWaitCursor ();
	}
	else
		Flag = TRUE;
			
	return Flag;
}

BOOL CKiGenerator::DisplayGray(double aLevel, BOOL bIRE, MeasureType nPatternType, BOOL bChangePattern)
{
	CString str, Msg, Title;
	BOOL	Flag = FALSE;
	BOOL	bContrast = FALSE;
	BOOL	bNearBlack = FALSE;
	BOOL	bNearWhite = FALSE;
	BOOL	bExistNavigationCodes = FALSE;
	char	mStr[255];
	char	m[1] = "";

	if ( m_NbNextCodeToAdd > 0 )
		m_NbNextCodeToAdd --;

	m_lastPatternInfo = (UINT)aLevel;
	
	Title.LoadString ( IDS_INFORMATION );

	if ( m_bUseIRCodes )
	{
		// Current communication protocol
		if ( m_OkCodeLength > 0 && m_DownCodeLength > 0 && m_RightCodeLength > 0 )
			bExistNavigationCodes = TRUE;

		if ( m_bBeforeFirstDisplay )
		{
			m_bBeforeFirstDisplay = FALSE;
			
			switch ( nPatternType )
			{
				case MT_UNKNOWN: 
				case MT_ACTUAL:
					 // DDE: consider IRE

				case MT_IRE:
					 break;

				case MT_CONTRAST:
					 bContrast = TRUE;
					 break;

				case MT_NEARBLACK:
					 bNearBlack = TRUE;
					 break;

				case MT_NEARWHITE:
					 bNearWhite = TRUE;
					 break;

				case MT_PRIMARY:
				case MT_SECONDARY:
				case MT_SAT_RED:
				case MT_SAT_GREEN:
				case MT_SAT_BLUE:
				case MT_SAT_YELLOW:
				case MT_SAT_CYAN:
				case MT_SAT_MAGENTA:
				case MT_CALIBRATE:
				case MT_SAT_ALL:
					 // Cannot occur here
					 ASSERT(0);
					 return FALSE;
			}

			if ( bExistNavigationCodes )
			{
				if ( m_showDialog )
				{
					if (((nMeasureNumber == ADVANCEDGRAYLVLNUMBER)&&(!bContrast)) || bNearBlack || bNearWhite)
						Msg.LoadString ( IDS_DVDPOS11 );
					else
						Msg.LoadString ( IDS_DVDPOS1 );
					if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
						return FALSE;

					if(m_doScreenBlanking)
					{
						m_blankingWindow.BringWindowToTop ();	// refresh black screen
						m_blankingWindow.RedrawWindow ();
					}
				}

				if ( bContrast )
				{
					// Move to exact color
					for ( int i = 0 ; i < 3 ; i ++ )
					{
						if ( SendIRCode (1000, m_DownCode, m_DownCodeLength, mStr) ) 
							Sleep ( m_NavInMenuLatency );
					}
				}
				else if (bNearBlack || bNearWhite)
				{
						// Move from Gray selection to near black pattern
					if ( SendIRCode (1000, m_DownCode, m_DownCodeLength, mStr) ) 
						Sleep ( m_NavInMenuLatency );

					if ( bNearWhite )
					{
					// Near white pattern are right primary saturations
						if ( SendIRCode (1000, m_RightCode, m_DownCodeLength, mStr) ) 
						Sleep ( m_NavInMenuLatency );
					}
				}

				if ( SendIRCode (1000, m_OkCode, m_OkCodeLength, mStr) ) 
				{
					Sleep ( m_ValidMenuLatency );
					Flag = TRUE;
				}
			}
			else if ( m_showDialog )
			{
				if ( bNearBlack )
				{
					Msg.LoadString ( IDS_DVDPOS2 );
					if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
						return FALSE;
				}
				else if ( bNearWhite )
				{
					Msg.LoadString ( IDS_DVDPOS3 );
					if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
						return FALSE;
				}
				else if ( bContrast )
				{
					Msg.LoadString ( IDS_DVDPOS4 );
					if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
						return FALSE;
				}
				else
				{
					if (nMeasureNumber == ADVANCEDGRAYLVLNUMBER)
						Msg.LoadString ( IDS_DVDPOS12 );
					else
						Msg.LoadString ( IDS_DVDPOS5 );
					if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
						return FALSE;
				}

				if(m_doScreenBlanking)
				{
					m_blankingWindow.BringWindowToTop ();	// refresh black screen
					m_blankingWindow.RedrawWindow ();
				}

				Flag = TRUE;
			}
			else
				Flag = TRUE;

		}
		else
		{
			if (bChangePattern)
			{
				// Next color
				if ( SendIRCode (1000, m_NextCode, m_NextCodeLength, mStr) ) 
				{
					Sleep ( m_NextChapterLatency );
					Flag = TRUE;
				}
			}
			else
				Flag = TRUE;
		}
	}
	else
	{		
		// Philips DVD 963 SA HCFR-modified built in protocol
		str.Format("Mire '%g IRE'",aLevel);
	
		m[0] = (char) ( 0x90 + ( (int)aLevel / 10 ) );
		if ( setMire(500, *m, mStr) ) 
			Flag = TRUE;

		if ( Flag )
		{
			if(m_showDialog && ! m_bUseIRCodes)
			{
				if ( GetColorApp()->InMeasureMessageBox(str,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK )
				{
					if(m_doScreenBlanking)
					{
						m_blankingWindow.BringWindowToTop ();	// refresh black screen
						m_blankingWindow.RedrawWindow ();
					}

					return TRUE;
				}
				else
					return FALSE;
			}
			else
				return TRUE;
		}
	}

	return Flag;
}

BOOL CKiGenerator::DisplayRGBColor( COLORREF clr  ,MeasureType nPatternType, UINT nPatternInfo, BOOL bChangePattern,BOOL bSilentMode)
{
	BOOL	foundColor=TRUE;
	CString str;
	CString str2;
	CString	Msg, Title;
	BOOL	Flag = FALSE;
	BOOL	bFirstSecondary = FALSE;
	BOOL	bFirstSaturationPrimary = FALSE;
	BOOL	bFirstSaturationSecondary = FALSE;
	BOOL	bCalibration = FALSE;
	BOOL	bFreeColor = FALSE;
	BOOL	bExistNavigationCodes = FALSE;
	int		nFirstSaturationMenuPos = 0;
	char	mStr[255];
	char	m[1] = "";

	if ( m_NbNextCodeToAdd > 0 )
		m_NbNextCodeToAdd --;

	m_lastColor = clr;
	m_lastPatternInfo = nPatternInfo;
	
	Title.LoadString ( IDS_INFORMATION );

	if ( m_bUseIRCodes )
	{
		// Current communication protocol
		if ( m_OkCodeLength > 0 && m_DownCodeLength > 0 && m_RightCodeLength > 0 )
			bExistNavigationCodes = TRUE;

		if ( m_bBeforeFirstDisplay )
		{
			m_bBeforeFirstDisplay = FALSE;
			
			switch ( nPatternType )
			{
				case MT_UNKNOWN:
				case MT_ACTUAL:
					 // Free measure or DDE
					 bFreeColor = TRUE;
					 break;

				case MT_PRIMARY:
					 str.LoadString ( IDS_PRIMARYCOLORS );
					 break;

				case MT_SECONDARY:
					 str.LoadString ( IDS_SECONDARYCOLORS );
					 bFirstSecondary = TRUE;
					 break;

				case MT_SAT_RED:
					 str.LoadString ( IDS_SATRED2 );
					 bFirstSaturationPrimary = TRUE;
					 nFirstSaturationMenuPos = 2;
					 break;

				case MT_SAT_GREEN:
					 str.LoadString ( IDS_SATGREEN2 );
					 bFirstSaturationPrimary = TRUE;
					 nFirstSaturationMenuPos = 3;
					 break;

				case MT_SAT_BLUE:
					 str.LoadString ( IDS_SATBLUE2 );
					 bFirstSaturationPrimary = TRUE;
					 nFirstSaturationMenuPos = 4;
					 break;

				case MT_SAT_YELLOW:
					 str.LoadString ( IDS_SATYELLOW2 );
					 bFirstSaturationSecondary = TRUE;
					 nFirstSaturationMenuPos = 2;
					 break;

				case MT_SAT_CYAN:
					 str.LoadString ( IDS_SATCYAN2 );
					 bFirstSaturationSecondary = TRUE;
					 nFirstSaturationMenuPos = 3;
					 break;

				case MT_SAT_MAGENTA:
					 str.LoadString ( IDS_SATMAGENTA2 );
					 bFirstSaturationSecondary = TRUE;
					 nFirstSaturationMenuPos = 4;
					 break;

				case MT_CALIBRATE:
					 bCalibration = TRUE;
					 break;

				case MT_IRE:
				case MT_NEARBLACK:
				case MT_NEARWHITE:
				case MT_CONTRAST:
				case MT_SAT_ALL:
					 // Cannot occur here
					 ASSERT(0);
					 return FALSE;
			}
			
			if ( bFreeColor )
			{
				if ( m_showDialog && !bSilentMode)
				{
					Msg.LoadString ( IDS_CONFIRMCOLOR );
 					if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
						return FALSE;

					if(m_doScreenBlanking)
					{
						m_blankingWindow.BringWindowToTop ();	// refresh black screen
						m_blankingWindow.RedrawWindow ();
					}
				}
				Flag = TRUE;
			}
			else
			{
				if ( bExistNavigationCodes )
				{
					if ( bCalibration )
					{
						if ( m_showDialog && !bSilentMode)
						{
							Msg.LoadString ( IDS_DVDPOS1 );
							if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title, MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
								return FALSE;

							if(m_doScreenBlanking)
							{
								m_blankingWindow.BringWindowToTop ();	// refresh black screen
								m_blankingWindow.RedrawWindow ();
							}

							// Move from 0 IRE to first primary color
							if ( SendIRCode (1000, m_DownCode, m_DownCodeLength, mStr) ) 
								Sleep ( m_NavInMenuLatency );
						}
					}
					else if ( bFirstSaturationPrimary || bFirstSaturationSecondary )
					{
						if ( m_showDialog && !bSilentMode)
						{
								Msg.LoadString ( IDS_DVDPOS11 );
								if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title, MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
									return FALSE;

							if(m_doScreenBlanking)
							{
								m_blankingWindow.BringWindowToTop ();	// refresh black screen
								m_blankingWindow.RedrawWindow ();
							}
						}

						// Move from Gray selection to saturation pattern
						for ( int i = 0 ; i < nFirstSaturationMenuPos ; i ++ )
						{
							if ( SendIRCode (1000, m_DownCode, m_DownCodeLength, mStr) ) 
								Sleep ( m_NavInMenuLatency );
						}
						if ( bFirstSaturationSecondary )
						{
							// Secondary saturations are right primary saturations
							if ( SendIRCode (1000, m_RightCode, m_DownCodeLength, mStr) ) 
								Sleep ( m_NavInMenuLatency );
						}
					}
					else
					{
						if ( m_showDialog && !bSilentMode)
						{
							Msg.LoadString ( IDS_DVDPOS1 );
							if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title, MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
								return FALSE;

							if(m_doScreenBlanking)
							{
								m_blankingWindow.BringWindowToTop ();	// refresh black screen
								m_blankingWindow.RedrawWindow ();
							}
						}

						// Move from 0 IRE to first primary color
						if ( SendIRCode (1000, m_DownCode, m_DownCodeLength, mStr) ) 
							Sleep ( m_NavInMenuLatency );

						if ( bFirstSecondary )
						{
							// Secondary colors are just under primary colors
							if ( SendIRCode (1000, m_DownCode, m_DownCodeLength, mStr) ) 
								Sleep ( m_NavInMenuLatency );
						}
					}

					if ( SendIRCode (1000, m_OkCode, m_OkCodeLength, mStr) ) 
					{
						Sleep ( m_ValidMenuLatency );
						Flag = TRUE;
					}
				}
				else if ( m_showDialog )
				{
					// No navigation code: first image requested
					if ( bCalibration )
					{
						Msg.LoadString ( IDS_DVDPOS9 );
						if ( IDCANCEL == GetColorApp()->InMeasureMessageBox(Msg,Title, MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
							return FALSE;
					}
					else
					{
						Msg.LoadString ( IDS_DVDPOS10 );
						str2.Format ( Msg, (LPCSTR) str );
 						if ( IDCANCEL == GetColorApp()->InMeasureMessageBox((LPCSTR) str2, Title,MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) )
							return FALSE;
					}

					if(m_doScreenBlanking)
					{
						m_blankingWindow.BringWindowToTop ();	// refresh black screen
						m_blankingWindow.RedrawWindow ();
					}

					Flag = TRUE;
				}
			}
		}
		else
		{
			if (bChangePattern)
			{
				// Next color
				if ( SendIRCode (1000, m_NextCode, m_NextCodeLength, mStr) ) 
				{
					Sleep ( m_NextChapterLatency );
					Flag = TRUE;
				}
			}
			else
				Flag = TRUE;
		}

		return Flag;
	}
	else
	{
		// Philips DVD 963 SA HCFR-modified built in protocol
		if(GetRValue(clr) == 255 && GetGValue(clr) == 0 && GetBValue(clr) == 0)
		{
			m[0] = (char) 0x9B;
			str.Format("Mire 'Primaire Rouge'");
		}
		else if(GetRValue(clr) == 0 && GetGValue(clr) == 255 && GetBValue(clr) == 0) 
		{
			m[0] = (char) 0x9C;
			str.Format("Mire 'primaire Vert'");
		}
		else if(GetRValue(clr) == 0 && GetGValue(clr) == 0 && GetBValue(clr) == 255)
		{
			m[0] = (char) 0x9D;
			str.Format("Mire 'primaire Bleu'"); 
		}
		else if ( GetRValue(clr) == 255 && GetGValue(clr) == 255 && GetBValue(clr) == 0 )
		{
			bFirstSecondary = TRUE;
			m[0] = (char) 0x9E;
			str.Format("Mire 'secondaire Jaune'"); 
		}
		else if ( GetRValue(clr) == 0 && GetGValue(clr) == 255 && GetBValue(clr) == 255 )
		{
			m[0] = (char) 0x9F;
			str.Format("Mire 'secondaire Cyan'"); 
		}
		else if ( GetRValue(clr) == 255 && GetGValue(clr) == 0 && GetBValue(clr) == 255 )
		{
			m[0] = (char) 0xA0;
			str.Format("Mire 'secondaire Magenta'"); 
		}
		else
		{
			// Unknown color
			foundColor = FALSE;
		}

		if ( foundColor ) 
		{
			if ( setMire(500, *m, mStr) ) 
				Flag = TRUE;
		}
		else
			return FALSE;

		if ( Flag ) 
		{
			if ( m_showDialog )
			{
				if ( GetColorApp()->InMeasureMessageBox(str, "Information",MB_ICONINFORMATION | MB_OKCANCEL | MB_TOPMOST) == IDOK )
				{
					if(m_doScreenBlanking)
					{
						m_blankingWindow.BringWindowToTop ();	// refresh black screen
						m_blankingWindow.RedrawWindow ();
					}
					return TRUE;
				}
				else
					return FALSE;
			}
			else
				return TRUE;
		}
		return TRUE;
	} 
}

BOOL CKiGenerator::DisplayAnsiBWRects(BOOL bInvert)
{
	BOOL	Flag = FALSE;
	char	mStr[255];

	if ( m_NbNextCodeToAdd > 0 )
		m_NbNextCodeToAdd --;

	// Simply send Next Color IR code
	if ( SendIRCode (1000, m_NextCode, m_NextCodeLength, mStr) ) 
	{
		Sleep ( m_NextChapterLatency );
		Flag = TRUE;
	}
	
	return Flag;
}

BOOL CKiGenerator::CanDisplayAnsiBWRects()
{
	return m_bUseIRCodes;	  
}

BOOL CKiGenerator::CanDisplayGrayAndColorsSeries()
{
	if ( m_OkCodeLength > 0 && m_DownCodeLength > 0 && m_RightCodeLength > 0 )
		return m_bUseIRCodes;
	else
		return FALSE;
}

BOOL CKiGenerator::CanDisplayScale ( MeasureType nScaleType, int nbLevels, BOOL bMute )
{
	BOOL	returnvalue = TRUE;

	switch ( nScaleType )
	{
		case MT_IRE:
			if ((nbLevels != GRAYLVLNUMBER)&&(nbLevels != ADVANCEDGRAYLVLNUMBER))
				returnvalue = FALSE;
			break;
		
		case MT_NEARBLACK:
			if (nbLevels != NEARBLACKLVLNUMBER)
				returnvalue = FALSE;
			break;
		
		case MT_NEARWHITE:
			if (nbLevels != NEARWHITELVLNUMBER)
				returnvalue = FALSE;
			break;
		
		case MT_SAT_RED:
		case MT_SAT_GREEN:
		case MT_SAT_BLUE:
		case MT_SAT_YELLOW:
		case MT_SAT_CYAN:
		case MT_SAT_MAGENTA:
			if (nbLevels != SATURATIONLVLNUMBER)
				returnvalue = FALSE;
			break;
		
		case MT_SAT_ALL:
			if ((nbLevels != SATURATIONLVLNUMBER) || ! ( m_OkCodeLength > 0 && m_DownCodeLength > 0 && m_RightCodeLength > 0 ))
				returnvalue = FALSE;
			break;

		default:
			break;
	}			

	if ( ! bMute && ! returnvalue )
	{
		CString		Msg, Title;
		Title.LoadString ( IDS_ERROR );
		Msg.LoadString ( IDS_ERRLEVELNUMBER );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
	}
	return	returnvalue;
}


BOOL CKiGenerator::HasPatternChanged( MeasureType nScaleType,CColor previousColor,CColor lastColor)
{
	CColor pRGBColor,lRGBColor;
	float	redDif,greenDif,blueDif;
		
	pRGBColor=previousColor.GetRGBValue(GetColorReference());
	lRGBColor=lastColor.GetRGBValue(GetColorReference());

	float a = (float)pRGBColor[0];
	float b = (float)pRGBColor[1];
	float c = (float)pRGBColor[2];

	float g = (float)abs(pRGBColor[0]);
	float h = (float)abs(pRGBColor[1]);
	float i = (float)abs(pRGBColor[2]);


	float d = (float)lRGBColor[0];
	float e = (float)lRGBColor[1];
	float f = (float)lRGBColor[2];

	if (pRGBColor[0] != 0)
	{
		redDif = (float)((lRGBColor[0] - pRGBColor[0])/pRGBColor[0]);
		if (redDif < 0)
			redDif = -redDif;
	}
	else
	{
		if (lRGBColor[0] == 0)
			redDif = 0;
		else
			redDif = 1;
	}

	if (pRGBColor[1] != 0)
	{
		greenDif = (float)((lRGBColor[1] - pRGBColor[1])/pRGBColor[1]);
		if (greenDif < 0)
			greenDif = -greenDif;
	}
	else
	{
		if (lRGBColor[1] == 0)
			greenDif = 0;
		else
			greenDif = 1;
	}

		if (pRGBColor[2] != 0)
	{
		blueDif = (float)((lRGBColor[2] - pRGBColor[2])/pRGBColor[2]);
		if (blueDif < 0)
			blueDif = -blueDif;
	}
	else
	{
		if (lRGBColor[2] == 0)
			blueDif = 0;
		else
			blueDif = 1;
	}

switch ( nScaleType )
		{
			case MT_IRE:
				if (!m_patternCheckOnGrayscale)
					return	TRUE;
				break;
			case 		MT_PRIMARY:
				if (!m_patternCheckOnPrimaries)
					return	TRUE;
				break;
			case 		MT_SECONDARY:
				if (!m_patternCheckOnSecondaries)
					return	TRUE;
				break;
			case MT_SAT_RED:
			case MT_SAT_GREEN:
			case MT_SAT_BLUE:
			case MT_SAT_YELLOW:
			case MT_SAT_CYAN:
			case MT_SAT_MAGENTA:
				if (!m_patternCheckOnSaturations)
					return	TRUE;

				break;
			case MT_NEARBLACK:
			case MT_NEARWHITE:
			case MT_CONTRAST:
			case MT_CALIBRATE:
			case MT_SAT_ALL:
			default:
				return	TRUE;
				break;
		}			
		
	if ((redDif < m_minDif) && (greenDif < m_minDif) && (blueDif < m_minDif)&&(m_patternRetry > 0)&&(m_totalPatternRetry>0))
	{
		if (m_patternRetry == m_patternCheckMaxRetryByPattern)
		{
			CString	str,str2;

			switch ( nScaleType )
			{
				case MT_IRE:
					str2.LoadString ( IDS_GRAYIRE );
					str.Format(str2,m_lastPatternInfo);
					break;
				case MT_SAT_RED:
					str2.LoadString ( IDS_REDSATPERCENT );
					str.Format(str2,m_lastPatternInfo);
					break;
				case MT_SAT_GREEN:
					str2.LoadString ( IDS_GREENSATPERCENT );
					str.Format(str2,m_lastPatternInfo);
					break;
				case MT_SAT_BLUE:
					str2.LoadString ( IDS_BLUESATPERCENT );
					str.Format(str2,m_lastPatternInfo);
					break;
				case MT_SAT_YELLOW:
					str2.LoadString ( IDS_YELLOWSATPERCENT );
					str.Format(str2,m_lastPatternInfo);
					break;
				case MT_SAT_CYAN:
					str2.LoadString ( IDS_CYANSATPERCENT );
					str.Format(str2,m_lastPatternInfo);
					break;
				case MT_SAT_MAGENTA:
					str2.LoadString ( IDS_MAGENTASATPERCENT );
					str.Format(str2,m_lastPatternInfo);
					break;
				case MT_PRIMARY:
				case MT_SECONDARY:
					if((GetRValue(m_lastColor) > GetGValue(m_lastColor)) && (GetRValue(m_lastColor) > GetBValue(m_lastColor)))
						str.LoadString ( IDS_REDPRIMARY );
					else if((GetGValue(m_lastColor) > GetRValue(m_lastColor)) && (GetGValue(m_lastColor) > GetBValue(m_lastColor)))
						str.LoadString ( IDS_GREENPRIMARY );
					else if((GetBValue(m_lastColor) > GetRValue(m_lastColor)) && (GetBValue(m_lastColor) > GetGValue(m_lastColor)))
						str.LoadString ( IDS_BLUEPRIMARY );
					else if((GetRValue(m_lastColor) == GetGValue(m_lastColor)) && (GetRValue(m_lastColor) > GetBValue(m_lastColor)))
						str.LoadString ( IDS_YELLOWSECONDARY );
					else if((GetGValue(m_lastColor) == GetBValue(m_lastColor)) && (GetGValue(m_lastColor) > GetRValue(m_lastColor)))
						str.LoadString ( IDS_CYANSECONDARY );
					else if((GetRValue(m_lastColor) == GetBValue(m_lastColor)) && (GetRValue(m_lastColor) > GetGValue(m_lastColor)))
						str.LoadString ( IDS_MAGENTASECONDARY );
					break;
				default:
					str ="";

			}			
			m_retryList += str;
			m_retryList += "\n";
		}
		m_patternRetry --;
		m_totalPatternRetry --;
		return	FALSE;
	}
	else
	{
		m_patternRetry = m_patternCheckMaxRetryByPattern;
		return	TRUE;
	}
}

CString	CKiGenerator::GetRetryMessage ()
{
	CString str,str2;
	str2.LoadString ( IDS_PATTERN_RETRY );
	str.Format(str2,m_retryList);
	return str;
}



BOOL CKiGenerator::setMire(int timeout, char command, char *sensVal)
{
	CSerialCom port;
	char c[2];

	strcpy(c, "");
	strcpy(sensVal, "");
	BYTE data=0;

	if(!(port.OpenPort((LPSTR)(LPCSTR)m_RealComPort)))
	{
		GetColorApp()->InMeasureMessageBox("Cannot open Communication Port","Error",MB_OK+MB_ICONERROR);
	} 
	else 
	{
		if(!(port.ConfigurePort(115200,8,0,NOPARITY ,ONESTOPBIT )))
		{
			GetColorApp()->InMeasureMessageBox("Cannot Configure Communication Port","Error",MB_OK+MB_ICONERROR);
			port.ClosePort();
		} 
		else 
		{
			if(!(port.SetCommunicationTimeouts(0,timeout,0,0,0)))
			{
				GetColorApp()->InMeasureMessageBox("Cannot Configure Communication Timeouts","Error",MB_OK+MB_ICONERROR);
				port.ClosePort();
			} 
			else 
			{
				if(!(port.WriteByte(command)))
				{
					GetColorApp()->InMeasureMessageBox("Cannot Write to Port","Error",MB_OK+MB_ICONERROR);
					port.ClosePort();
				} 
				else 
				{
					while(port.ReadByte(data))
					{
						if (data==10) break;
						sprintf(c, "%c", data);
						strcat(sensVal, c);
					}
					//GetColorApp()->InMeasureMessageBox(sensVal,"Success",MB_OK+MB_ICONINFORMATION);
					port.ClosePort();
					return TRUE;
				}
			}
		} 
	}
	return FALSE;
}

BOOL CKiGenerator::SendIRCode(int timeout, LPBYTE Code, int CodeLength, char *sensVal)
{
	CSerialCom port;
	char c[2];

	strcpy(c, "");
	strcpy(sensVal, "");
	BYTE data=0;

	if ( CodeLength > 0 )
	{
		if(!(port.OpenPort((LPSTR)(LPCSTR)m_RealComPort)))
		{
			GetColorApp()->InMeasureMessageBox("Cannot open Communication Port","Error",MB_OK+MB_ICONERROR);
		} 
		else 
		{
			if(!(port.ConfigurePort(115200,8,0,NOPARITY ,ONESTOPBIT )))
			{
				GetColorApp()->InMeasureMessageBox("Cannot Configure Communication Port","Error",MB_OK+MB_ICONERROR);
				port.ClosePort();
			} 
			else 
			{
				if(!(port.SetCommunicationTimeouts(0,timeout,0,0,0)))
				{
					GetColorApp()->InMeasureMessageBox("Cannot Configure Communication Timeouts","Error",MB_OK+MB_ICONERROR);
					port.ClosePort();
				} 
				else 
				{
					if(!(port.WriteByte(0x80)))
					{
						GetColorApp()->InMeasureMessageBox("Cannot Write to Port","Error",MB_OK+MB_ICONERROR);
						port.ClosePort();
					} 
					else 
					{
						for ( int i = 0; i < CodeLength; i ++ )
						{
							if(!(port.WriteByte(Code[i])))
							{
								GetColorApp()->InMeasureMessageBox("Cannot Write to Port","Error",MB_OK+MB_ICONERROR);
								port.ClosePort();
								return FALSE;
							}
						}

						while(port.ReadByte(data))
						{
							if (data==10) break;
							sprintf(c, "%c", data);
							strcat(sensVal, c);
						}
						//GetColorApp()->InMeasureMessageBox(sensVal,"Success",MB_OK+MB_ICONINFORMATION);
						port.ClosePort();
						return TRUE;
					}
				}
			} 
		}
	}
	return FALSE;
}
CString	CKiGenerator::AcquireIRCode ()
{
	BYTE		data=0;
	BYTE		lobyte, hibyte;
	WORD		w;
	BOOL		bLow;
	BOOL		bFirstByte;
	CSerialCom	port;
	CString		strCode;
	char		szBuf [ 8 ];

	if(!(port.OpenPort((LPSTR)(LPCSTR)m_RealComPort)))
	{
		GetColorApp()->InMeasureMessageBox("Cannot open Communication Port","Error",MB_OK+MB_ICONERROR);
	} 
	else 
	{
		if(!(port.ConfigurePort(115200,8,0,NOPARITY ,ONESTOPBIT )))
		{
			GetColorApp()->InMeasureMessageBox("Cannot Configure Communication Port","Error",MB_OK+MB_ICONERROR);
			port.ClosePort();
		} 
		else 
		{
			if(!(port.SetCommunicationTimeouts(0,12000,0,0,0)))
			{
				GetColorApp()->InMeasureMessageBox("Cannot Configure Communication Timeouts","Error",MB_OK+MB_ICONERROR);
				port.ClosePort();
			} 
			else 
			{
				if(!(port.WriteByte(0x81)))
				{
					GetColorApp()->InMeasureMessageBox("Cannot Write to Port","Error",MB_OK+MB_ICONERROR);
					port.ClosePort();
				} 
				else 
				{
					bLow = TRUE;
					bFirstByte = TRUE;
					while(port.ReadByte(data))
					{
						if ( bFirstByte )
						{
							port.SetCommunicationTimeouts(0,200,0,0,0);
							bFirstByte = FALSE;
						}

						if ( bLow )
						{
							lobyte = data;
							bLow = FALSE;
						}
						else
						{
							hibyte = data;
							bLow = TRUE;

							w = ( hibyte << 8 ) + lobyte;
							sprintf ( szBuf, "%04X", w );

							if ( ! strCode.IsEmpty () )
								strCode += " ";
							
							strCode += szBuf;
						}
					}

					//GetColorApp()->InMeasureMessageBox(sensVal,"Success",MB_OK+MB_ICONINFORMATION);
					port.ClosePort();
				}
			}
		} 
	}

	return strCode;
}
