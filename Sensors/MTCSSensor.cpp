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

// MTCSSensor.cpp: implementation of the MTCS-C2 sensor class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "MTCSSensor.h"
#include "SerialCom.h"

// Include for device interface (this device interface is outside GNU GPL license)
#ifdef USE_NON_FREE_CODE
#include "devlib\CHCFRDI3.h"
#endif

#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CMTCSSensor, COneDeviceSensor, 1) ;

CMTCSSensor::CMTCSSensor() : m_DeviceBlackLevel (0.0,3,1)
{
	m_nAverageMode = GetConfig () -> GetProfileInt ( "MTCS", "AvgMode", 0 );
	m_ReadTime = GetConfig () -> GetProfileInt ( "MTCS", "ReadTime", 1000 );
	m_bAdjustTime = GetConfig () -> GetProfileInt ( "MTCS", "AdjustTime", FALSE );
	m_debugMode = FALSE;
	
	m_nAmpValue = GetConfig () -> GetProfileInt ( "MTCS", "AmpValue", 8 );
	m_nShiftValue = 6; //GetConfig () -> GetProfileInt ( "MTCS", "ShiftValue", 6 );
	m_DeviceMatrix = IdentityMatrix (3);
	m_nBlackROffset = 0;
	m_nBlackGOffset = 0;
	m_nBlackBOffset = 0;

	m_MTCSSensorPropertiesPage.m_pSensor = this;

	m_pDevicePage = & m_MTCSSensorPropertiesPage;  // Add MTCS-C2 settings page to property sheet

	m_PropertySheetTitle = IDS_MTCSSENSOR_PROPERTIES_TITLE;

	CString str;
	str.LoadString(IDS_MTCSSENSOR_NAME);
	SetName(str);
}

CMTCSSensor::~CMTCSSensor()
{

}

void CMTCSSensor::Copy(CSensor * p)
{
	COneDeviceSensor::Copy(p);

	m_nAverageMode = ((CMTCSSensor*)p)->m_nAverageMode;
	m_ReadTime = ((CMTCSSensor*)p)->m_ReadTime;
	m_bAdjustTime = ((CMTCSSensor*)p)->m_bAdjustTime;
	m_debugMode = ((CMTCSSensor*)p)->m_debugMode;	
	m_nAmpValue = ((CMTCSSensor*)p)->m_nAmpValue;
}

void CMTCSSensor::Serialize(CArchive& archive)
{
	COneDeviceSensor::Serialize(archive) ;

	if (archive.IsStoring())
	{
		int version=1;
		archive << version;
		archive << m_nAverageMode;
		archive << m_ReadTime;
		archive << m_bAdjustTime;
		archive << m_nAmpValue;
		archive << m_debugMode;
	}
	else
	{
		int version, dummy;
		archive >> version;
		if ( version > 2 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_nAverageMode;
		archive >> m_ReadTime;
		archive >> m_bAdjustTime;
		archive >> m_nAmpValue;
		archive >> m_debugMode;
		if ( version == 2 )
			archive >> dummy;	// was nShiftValue, useless -> version 2 cancelled
	}
}

void CMTCSSensor::SetPropertiesSheetValues()
{
	COneDeviceSensor::SetPropertiesSheetValues();

	m_MTCSSensorPropertiesPage.m_ReadTime=m_ReadTime;
	m_MTCSSensorPropertiesPage.m_bAdjustTime=m_bAdjustTime;
	m_MTCSSensorPropertiesPage.m_debugMode=m_debugMode;
}

void CMTCSSensor::GetPropertiesSheetValues()
{
	COneDeviceSensor::GetPropertiesSheetValues();

	if( m_ReadTime != m_MTCSSensorPropertiesPage.m_ReadTime ) {
		m_ReadTime=m_MTCSSensorPropertiesPage.m_ReadTime;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "MTCS", "ReadTime", m_ReadTime );
	}

	if( m_bAdjustTime != m_MTCSSensorPropertiesPage.m_bAdjustTime ) {
		m_bAdjustTime=m_MTCSSensorPropertiesPage.m_bAdjustTime;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "MTCS", "AdjustTime", m_bAdjustTime );
	}

	if( m_debugMode != m_MTCSSensorPropertiesPage.m_debugMode ) {
		m_debugMode=m_MTCSSensorPropertiesPage.m_debugMode;
		SetModifiedFlag(TRUE);
	}

}

BOOL CMTCSSensor::Init( BOOL bForSimultaneousMeasures )
{
#ifdef USE_NON_FREE_CODE
	BOOL		bOk = FALSE;
	int			i;
	int			nErrorCode;
	BOOL		bMatch;
	short		sValues [ 9 ];
	int			nValues [ 9 ];
	CString		Msg, Title;

	nErrorCode = InitDevice3();

	switch ( nErrorCode )
	{
		case DI3ERR_SUCCESS:
			 bOk = TRUE;
			 break;

		case DI3ERR_NODLL:
			 Msg.LoadString ( IDS_ERRMTCSDLL1 );
			 break;

		case DI3ERR_INVALIDDLL:
			 Msg.LoadString ( IDS_ERRMTCSDLL2 );
			 break;

		case DI3ERR_MTCS:
			 Msg.LoadString ( IDS_MTCSERROR );
			 break;

		case DI3ERR_UNKNOWN:
		default:
			 Msg = "Unknown error";
			 break;
	}

	if ( bOk )
	{
		// Load matrix
		if ( GetDevice3Matrix ( sValues ) )
		{
			m_DeviceMatrix(0,0) = (double) sValues [ 0 ] / 1000.0;
			m_DeviceMatrix(0,1) = (double) sValues [ 1 ] / 1000.0;
			m_DeviceMatrix(0,2) = (double) sValues [ 2 ] / 1000.0;
			m_DeviceMatrix(1,0) = (double) sValues [ 3 ] / 1000.0;
			m_DeviceMatrix(1,1) = (double) sValues [ 4 ] / 1000.0;
			m_DeviceMatrix(1,2) = (double) sValues [ 5 ] / 1000.0;
			m_DeviceMatrix(2,0) = (double) sValues [ 6 ] / 1000.0;
			m_DeviceMatrix(2,1) = (double) sValues [ 7 ] / 1000.0;
			m_DeviceMatrix(2,2) = (double) sValues [ 8 ] / 1000.0;

			// Load high precision matrix ( idem normal matrix with coefficients multiplied by 1,000 )
			if ( ReadDevice3UserMemory ( (short*)nValues, DI3_USERMEMOFFSET_HIDEFMATRIX, 18 ) )
			{
				// Check if this matrix matches standard matrix or not
				bMatch = TRUE;
				for ( i = 0; i < 9 ; i ++ )
				{
					// Test if this coefficient is identical, at rounding error 
					if ( abs ( ( nValues [ i ] / 1000 ) - (int) sValues [ i ] ) > 1 )
					{
						bMatch = FALSE;
						break;
					}
				}

				if ( bMatch )
				{
					// That's ok. Take enhanced matrix instead of ordinary one.
					m_DeviceMatrix(0,0) = (double) nValues [ 0 ] / 1000000.0;
					m_DeviceMatrix(0,1) = (double) nValues [ 1 ] / 1000000.0;
					m_DeviceMatrix(0,2) = (double) nValues [ 2 ] / 1000000.0;
					m_DeviceMatrix(1,0) = (double) nValues [ 3 ] / 1000000.0;
					m_DeviceMatrix(1,1) = (double) nValues [ 4 ] / 1000000.0;
					m_DeviceMatrix(1,2) = (double) nValues [ 5 ] / 1000000.0;
					m_DeviceMatrix(2,0) = (double) nValues [ 6 ] / 1000000.0;
					m_DeviceMatrix(2,1) = (double) nValues [ 7 ] / 1000000.0;
					m_DeviceMatrix(2,2) = (double) nValues [ 8 ] / 1000000.0;
				}
			}
			
		}
		else
		{
			bOk = FALSE;
			Msg.LoadString ( IDS_MTCSERROR );
		}
	}

	if ( bOk )
	{
		// Load black level
		if ( GetDevice3BlackLevel ( sValues ) )
		{
			m_DeviceBlackLevel(0,0) = (double) sValues [ 0 ] / 1000.0;
			m_DeviceBlackLevel(1,0) = (double) sValues [ 1 ] / 1000.0;
			m_DeviceBlackLevel(2,0) = (double) sValues [ 2 ] / 1000.0;
		}
		else
		{
			bOk = FALSE;
			Msg.LoadString ( IDS_MTCSERROR );
		}
	}

	if ( bOk )
	{
		// Load shift value
		if ( ReadDevice3UserMemory ( sValues, DI3_USERMEMOFFSET_SHIFT, 1 ) )
		{
			m_nShiftValue = sValues [ 0 ];
			if ( m_nShiftValue < 0 || m_nShiftValue > 6 )
				m_nShiftValue = 6;
		}
	}

	if ( bOk )
	{
		// Load black level offsets
		if ( ReadDevice3UserMemory ( sValues, DI3_USERMEMOFFSET_BLACK_OFFSETS, 3 ) )
		{
			m_nBlackROffset = sValues [ 0 ];
			m_nBlackGOffset = sValues [ 1 ];
			m_nBlackBOffset = sValues [ 2 ];
		}
	}

	if ( ! bOk )
	{
		Title.LoadString(IDS_MTCSSENSOR_NAME);
		MessageBox(NULL,Msg,Title,MB_OK+MB_ICONHAND);
	}

	return bOk;
#else
    return TRUE;
#endif
}

BOOL CMTCSSensor::Release()
{
#ifdef USE_NON_FREE_CODE
	ReleaseDevice3();
#endif
	m_DeviceMatrix = IdentityMatrix (3);
	m_DeviceBlackLevel(0,0)=0.0;
	m_DeviceBlackLevel(1,0)=0.0;
	m_DeviceBlackLevel(2,0)=0.0;
	m_nShiftValue = 6;
	return CSensor::Release();
}

CColor CMTCSSensor::MeasureColor(COLORREF aRGBValue)
{
#ifdef USE_NON_FREE_CODE
	UINT		nTicks;
	UINT		nAdjustedReadTime;
	DWORD		amp, r, g, b;
	double		d;
	CColor		MTCSColor, DeviceColor, colMeasure;
	
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
			
			if ( nAdjustedReadTime > 32000 )
				nAdjustedReadTime = 32000;
		}
		else
			nAdjustedReadTime = m_ReadTime;
	}
	else
		nAdjustedReadTime = m_ReadTime;

	if ( nAdjustedReadTime > 2800 )
		nAdjustedReadTime = 2800;

	if ( GetValuesDevice3 ( m_nAverageMode, m_nAmpValue, m_nShiftValue, nAdjustedReadTime, & amp, & r, & g, & b, & nTicks ) )
	{
		DeviceColor[0]= ((double)r/(double)nTicks) - ( (double)m_nBlackROffset + m_DeviceBlackLevel(0,0) );
		DeviceColor[1]= ((double)g/(double)nTicks) - ( (double)m_nBlackGOffset + m_DeviceBlackLevel(1,0) );
		DeviceColor[2]= ((double)b/(double)nTicks) - ( (double)m_nBlackBOffset + m_DeviceBlackLevel(2,0) );

		MTCSColor = m_DeviceMatrix*DeviceColor;

		if (m_debugMode) 
		{
			EnterCriticalSection ( & GetConfig () -> LogFileCritSec );
			CTime theTime = CTime::GetCurrentTime(); 
			CString s = theTime.Format( "%d/%m/%y %H:%M:%S" );		
			FILE *f = fopen ( GetConfig () -> m_logFileName, "a" );
			fprintf(f, "MTCS-C2 - %s : R:%3d G:%3d B:%3d : amp:%d Ticks:%6d RVal:%6d GVal:%6d BVal:%6d\n", s, GetRValue(aRGBValue), GetGValue(aRGBValue), GetBValue(aRGBValue), amp, nTicks, r, g, b );
			fclose(f);
			LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
		}

		colMeasure.SetSensorValue(MTCSColor);
	}
	else 
	{
		MessageBox(0, "No data from Sensor","Error",MB_OK+MB_ICONINFORMATION);
		return noDataColor;
	}

	return colMeasure.GetSensorValue();
#else
    return noDataColor;
#endif
}

CColor CMTCSSensor::MeasureBlackLevel(BOOL bUseOffsets)
{
	CColor		DeviceColor;
#ifdef USE_NON_FREE_CODE
	UINT		nTicks;
	UINT		nAdjustedReadTime;
	DWORD		amp, r, g, b;
	
	// This function measures black level, with no conversion
	nAdjustedReadTime = 2800;

	if ( GetValuesDevice3 ( m_nAverageMode, m_nAmpValue, m_nShiftValue, nAdjustedReadTime, & amp, & r, & g, & b, & nTicks ) )
	{
		DeviceColor[0]= ((double)r/(double)nTicks);
		DeviceColor[1]= ((double)g/(double)nTicks);
		DeviceColor[2]= ((double)b/(double)nTicks);

		if ( bUseOffsets )
		{
			DeviceColor[0] -= (double)m_nBlackROffset;
			DeviceColor[1] -= (double)m_nBlackGOffset;
			DeviceColor[2] -= (double)m_nBlackBOffset;
		}
	}
	else 
	{
		MessageBox(0, "No data from Sensor","Error",MB_OK+MB_ICONINFORMATION);
		return noDataColor;
	}
#endif
	return DeviceColor;
}

#ifdef USE_NON_FREE_CODE
#pragma comment(lib, "devlib\\CHCFRDI3.lib")
#endif
