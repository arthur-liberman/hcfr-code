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

// SpyderIISensor.cpp: implementation of the SpyderII class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "SpyderIISensor.h"

// Include for device interface (this device interface is outside GNU GPL license)
#ifdef USE_NON_FREE_CODE
#include "devlib\CHCFRDI1.h"
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

IMPLEMENT_SERIAL(CSpyderIISensor, COneDeviceSensor, 1) ;

CSpyderIISensor::CSpyderIISensor()
{
	m_CalibrationMode = 2;
	m_ReadTime = 300;
	m_bAdjustTime = GetConfig () -> GetProfileInt ( "SpyderII", "AdjustTime", FALSE );
	m_bAverageReads = GetConfig () -> GetProfileInt ( "SpyderII", "AverageReads", FALSE );
	m_debugMode = FALSE;
	m_MinRequestedY = GetConfig () -> GetProfileDouble ( "SpyderII", "MinRequestedY", 10.0 );
	m_MaxReadLoops = GetConfig () -> GetProfileInt ( "SpyderII", "MaxReadLoops", 8 );

	m_SpyderIISensorPropertiesPage.m_pSensor = this;

	m_pDevicePage = & m_SpyderIISensorPropertiesPage;  // Add SpyderII settings page to property sheet

	m_PropertySheetTitle = IDS_S2SENSOR_PROPERTIES_TITLE;

	CString str;
	str.LoadString(IDS_S2SENSOR_NAME);
	SetName(str);
}

CSpyderIISensor::~CSpyderIISensor()
{

}

void CSpyderIISensor::Copy(CSensor * p)
{
	COneDeviceSensor::Copy(p);

	m_CalibrationMode = ((CSpyderIISensor*)p)->m_CalibrationMode;
	m_ReadTime = ((CSpyderIISensor*)p)->m_ReadTime;
	m_bAdjustTime = ((CSpyderIISensor*)p)->m_bAdjustTime;
	m_bAverageReads = ((CSpyderIISensor*)p)->m_bAverageReads;
	m_debugMode = ((CSpyderIISensor*)p)->m_debugMode;	
}

void CSpyderIISensor::Serialize(CArchive& archive)
{
	COneDeviceSensor::Serialize(archive) ;

	if (archive.IsStoring())
	{
		int version=(m_debugMode ? 4 : 3);
		archive << version;
		archive << m_CalibrationMode;
		archive << m_ReadTime;
		archive << m_bAdjustTime;
		archive << m_bAverageReads;
		
		if ( version > 3 )
			archive << m_debugMode;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 4 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_CalibrationMode;
		archive >> m_ReadTime;
		
		if ( version > 1 )
			archive >> m_bAdjustTime;
		else
			m_bAdjustTime = FALSE;

		if ( version > 2 )
			archive >> m_bAverageReads;
		else
			m_bAverageReads = FALSE;

		if ( version > 3 )
			archive >> m_debugMode;
		else
			m_debugMode = FALSE;
	}
}

void CSpyderIISensor::SetPropertiesSheetValues()
{
	COneDeviceSensor::SetPropertiesSheetValues();

	m_SpyderIISensorPropertiesPage.m_CalibrationMode=m_CalibrationMode;
	m_SpyderIISensorPropertiesPage.m_ReadTime=m_ReadTime;
	m_SpyderIISensorPropertiesPage.m_bAdjustTime=m_bAdjustTime;
	m_SpyderIISensorPropertiesPage.m_bAverageReads=m_bAverageReads;
	m_SpyderIISensorPropertiesPage.m_debugMode=m_debugMode;
}

void CSpyderIISensor::GetPropertiesSheetValues()
{
	COneDeviceSensor::GetPropertiesSheetValues();

	if( m_CalibrationMode != m_SpyderIISensorPropertiesPage.m_CalibrationMode ) {
		m_CalibrationMode=m_SpyderIISensorPropertiesPage.m_CalibrationMode;
		SetModifiedFlag(TRUE);
	}

	if( m_ReadTime != m_SpyderIISensorPropertiesPage.m_ReadTime ) {
		m_ReadTime=m_SpyderIISensorPropertiesPage.m_ReadTime;
		SetModifiedFlag(TRUE);
	}

	if( m_bAdjustTime != m_SpyderIISensorPropertiesPage.m_bAdjustTime ) {
		m_bAdjustTime=m_SpyderIISensorPropertiesPage.m_bAdjustTime;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "SpyderII", "AdjustTime", m_bAdjustTime );
	}

	if( m_bAverageReads != m_SpyderIISensorPropertiesPage.m_bAverageReads ) {
		m_bAverageReads=m_SpyderIISensorPropertiesPage.m_bAverageReads;
		SetModifiedFlag(TRUE);

		GetConfig () -> WriteProfileInt ( "SpyderII", "AverageReads", m_bAverageReads );
	}

	if( m_debugMode != m_SpyderIISensorPropertiesPage.m_debugMode ) {
		m_debugMode=m_SpyderIISensorPropertiesPage.m_debugMode;
		SetModifiedFlag(TRUE);
	}

}

BOOL CSpyderIISensor::Init( BOOL bForSimultaneousMeasures )
{
#ifdef USE_NON_FREE_CODE
	int			nErrorCode;
	BOOL		bOk = FALSE;
	CString		Msg, Title;

	nErrorCode = InitDevice1(m_CalibrationMode);

	switch ( nErrorCode )
	{
		case DI1ERR_SUCCESS:
			 bOk = TRUE;
			 break;

		case DI1ERR_NODLL:
			 Msg.LoadString ( IDS_ERRSPYDERDLL1 );
			 break;

		case DI1ERR_INVALIDDLL:
			 Msg.LoadString ( IDS_ERRSPYDERDLL2 );
			 break;

		case DI1ERR_SPYDER:
			 Msg.LoadString ( IDS_SPYDERERROR );
			 break;

		case DI1ERR_UNKNOWN:
		default:
			 Msg = "Unknown error";
			 break;
	}

	if ( ! bOk )
	{
		Title.LoadString(IDS_SPYDER);
		MessageBox(NULL,Msg,Title,MB_OK+MB_ICONHAND);
	}

	return bOk;
#else
    return TRUE;
#endif
}

BOOL CSpyderIISensor::Release()
{
#ifdef USE_NON_FREE_CODE
	ReleaseDevice1();
#endif
	return CSensor::Release();
}

CColor CSpyderIISensor::MeasureColorInternal(const ColorRGBDisplay& aRGBValue)
{
#ifdef USE_NON_FREE_CODE
	UINT		nLoops;
	BOOL		bContinue = FALSE;
    UINT		nAdjustedReadTime;
	double		d;
	double		x, y, z;
	double		xx, yy, zz;
	CColor		SpyderIIColor, colMeasure;
	
	if ( m_bAdjustTime )
	{
		// Retrieve darker component
		d = min ( aRGBValue[0], min (aRGBValue[1], aRGBValue[2]) );

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

	nLoops = 0;
	do
	{
		if ( GetValuesDevice1(nAdjustedReadTime, & x, & y, & z) )
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
					fprintf(f, "Spyder 2 - %s : * building value - loop %d * : X:%5.3f Y:%5.3f Z:%5.3f\n", s, nLoops, x, y, z );
					fclose(f);
					LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
				}
			}

			if ( ! bContinue )
			{
				SpyderIIColor[0]= xx;
				SpyderIIColor[1]= yy;
				SpyderIIColor[2]= zz;

				if (m_debugMode) 
				{
					EnterCriticalSection ( & GetConfig () -> LogFileCritSec );
					CTime theTime = CTime::GetCurrentTime(); 
					CString s = theTime.Format( "%d/%m/%y %H:%M:%S" );		
					FILE *f = fopen ( GetConfig () -> m_logFileName, "a" );
					fprintf(f, "Spyder 2 - %s : R:%3f G:%3f B:%3f (%d loops) : X:%5.3f Y:%5.3f Z:%5.3f\n", s, aRGBValue[0], aRGBValue[1], aRGBValue[2], nLoops, xx, yy, zz );
					fclose(f);
					LeaveCriticalSection ( & GetConfig () -> LogFileCritSec );
				}

				colMeasure = SpyderIIColor;
			}
		}
		else 
		{
			MessageBox(0, "No data from Sensor","Error",MB_OK+MB_ICONINFORMATION);
			return noDataColor;
		}
	} while ( bContinue );

	return colMeasure;
#else
    return noDataColor;
#endif
}

#ifdef USE_NON_FREE_CODE
#pragma comment(lib, "devlib\\CHCFRDI1.lib")
#endif
