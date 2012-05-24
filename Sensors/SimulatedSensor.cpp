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
/////////////////////////////////////////////////////////////////////////////

// SimulatedSensor.cpp: implementation of the CSimulatedSensor class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "SimulatedSensor.h"

#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

extern Matrix defaultSensorToXYZMatrix;	// Implemented in Color.cpp

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CSimulatedSensor, COneDeviceSensor, 1) ;

CSimulatedSensor::CSimulatedSensor()
{
	m_offsetRed=12;
	m_offsetGreen=12;
	m_offsetBlue=12;
	m_doOffsetError=TRUE;
	m_offsetErrorMax=5.0;
	m_doGainError=TRUE;
	m_gainErrorMax=0.1;		
	m_doGammaError=TRUE;
	m_gammaErrorMax=0.3;

	m_pDevicePage = & m_simulatedSensorPropertiesPage;  // Add setting page to property sheet

	m_PropertySheetTitle = IDS_SIMULATEDSENSOR_PROPERTIES_TITLE;

	CString str;
	str.LoadString(IDS_SIMULATEDSENSOR_NAME);
	SetName(str);

	m_calibrationTime = 0;
}

CSimulatedSensor::~CSimulatedSensor()
{

}

void CSimulatedSensor::Copy(CSensor * p)
{
	COneDeviceSensor::Copy(p);

	m_offsetRed = ((CSimulatedSensor*)p)->m_offsetRed;
	m_offsetGreen = ((CSimulatedSensor*)p)->m_offsetGreen;
	m_offsetBlue = ((CSimulatedSensor*)p)->m_offsetBlue;
	m_doOffsetError = ((CSimulatedSensor*)p)->m_doOffsetError;
	m_offsetErrorMax = ((CSimulatedSensor*)p)->m_offsetErrorMax;
	m_doGainError = ((CSimulatedSensor*)p)->m_doGainError;
	m_gainErrorMax = ((CSimulatedSensor*)p)->m_gainErrorMax;
	m_doGammaError = ((CSimulatedSensor*)p)->m_doGammaError;
	m_gammaErrorMax = ((CSimulatedSensor*)p)->m_gammaErrorMax;

	m_offsetR = ((CSimulatedSensor*)p)->m_offsetR;
	m_offsetG = ((CSimulatedSensor*)p)->m_offsetG;
	m_offsetB = ((CSimulatedSensor*)p)->m_offsetB;
}

void CSimulatedSensor::Serialize(CArchive& archive)
{
	COneDeviceSensor::Serialize(archive) ;

	if (archive.IsStoring())
	{
		int version=1;
		archive << version;
		archive << m_offsetRed;
		archive << m_offsetGreen;
		archive << m_offsetBlue;
		archive << m_doOffsetError;
		archive << m_doGainError;
		archive << m_gainErrorMax;
		archive << m_doGammaError;
		archive << m_gammaErrorMax;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_offsetRed;
		archive >> m_offsetGreen;
		archive >> m_offsetBlue;
		archive >> m_doOffsetError;
		archive >> m_doGainError;
		archive >> m_gainErrorMax;
		archive >> m_doGammaError;
		archive >> m_gammaErrorMax;
	}
}

void CSimulatedSensor::SetPropertiesSheetValues()
{
	COneDeviceSensor::SetPropertiesSheetValues();

	m_simulatedSensorPropertiesPage.m_offsetRed=m_offsetRed;
	m_simulatedSensorPropertiesPage.m_offsetGreen=m_offsetGreen;
	m_simulatedSensorPropertiesPage.m_offsetBlue=m_offsetBlue;
	m_simulatedSensorPropertiesPage.m_doOffsetError=m_doOffsetError;
	m_simulatedSensorPropertiesPage.m_offsetErrorMax=m_offsetErrorMax;
	m_simulatedSensorPropertiesPage.m_doGainError=m_doGainError;
	m_simulatedSensorPropertiesPage.m_gainErrorMax=m_gainErrorMax;
	m_simulatedSensorPropertiesPage.m_doGammaError=m_doGammaError;
	m_simulatedSensorPropertiesPage.m_gammaErrorMax=m_gammaErrorMax;
}

void CSimulatedSensor::GetPropertiesSheetValues()
{
	COneDeviceSensor::GetPropertiesSheetValues();

	if(m_offsetRed!=m_simulatedSensorPropertiesPage.m_offsetRed)
	{
		m_offsetRed=m_simulatedSensorPropertiesPage.m_offsetRed;
		SetModifiedFlag(TRUE);
	}
	if(m_offsetGreen!=m_simulatedSensorPropertiesPage.m_offsetGreen)
	{
		m_offsetGreen=m_simulatedSensorPropertiesPage.m_offsetGreen;
		SetModifiedFlag(TRUE);
	}
	if(m_offsetBlue!=m_simulatedSensorPropertiesPage.m_offsetBlue)
	{
		m_offsetBlue=m_simulatedSensorPropertiesPage.m_offsetBlue;
		SetModifiedFlag(TRUE);
	}
	if(m_doOffsetError!=m_simulatedSensorPropertiesPage.m_doOffsetError)
	{
		m_doOffsetError=m_simulatedSensorPropertiesPage.m_doOffsetError;
		SetModifiedFlag(TRUE);
	}
	if(m_offsetErrorMax!=m_simulatedSensorPropertiesPage.m_offsetErrorMax)
	{
		m_offsetErrorMax=m_simulatedSensorPropertiesPage.m_offsetErrorMax;
		SetModifiedFlag(TRUE);
	}
	if(m_doGainError!=m_simulatedSensorPropertiesPage.m_doGainError)
	{
		m_doGainError=m_simulatedSensorPropertiesPage.m_doGainError;
		SetModifiedFlag(TRUE);
	}
	if(m_gainErrorMax!=m_simulatedSensorPropertiesPage.m_gainErrorMax)
	{
		m_gainErrorMax=m_simulatedSensorPropertiesPage.m_gainErrorMax;
		SetModifiedFlag(TRUE);
	}
	if(m_doGammaError!=m_simulatedSensorPropertiesPage.m_doGammaError)
	{
		m_doGammaError=m_simulatedSensorPropertiesPage.m_doGammaError;
		SetModifiedFlag(TRUE);
	}
	if(m_gammaErrorMax!=m_simulatedSensorPropertiesPage.m_gammaErrorMax)
	{
		m_gammaErrorMax=m_simulatedSensorPropertiesPage.m_gammaErrorMax;
		SetModifiedFlag(TRUE);
	}
}

BOOL CSimulatedSensor::Init( BOOL bForSimultaneousMeasures )
{
	m_offsetR=m_offsetRed*(1+(double)rand()/(double)RAND_MAX);
	m_offsetG=m_offsetGreen*(1+(double)rand()/(double)RAND_MAX);
	m_offsetB=m_offsetBlue*(1+(double)rand()/(double)RAND_MAX);

	return TRUE;
}

CColor CSimulatedSensor::MeasureColorInternal(const ColorRGBDisplay& aRGBValue)
{
	ColorRGB simulColor;
	double offset=0.0, gain=1.0 ,gamma=2.2;
	double value;

	offset=m_offsetR;
	if(m_doOffsetError)
		offset+=(m_offsetErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);
	if(m_doGainError)
		gain=1+(m_gainErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);
	if(m_doGammaError)
		gamma=2.2+(m_gammaErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);

	value=max(aRGBValue[0]*gain+offset,0.0);
	simulColor[0]=(pow(value/100.0,gamma));

	offset=m_offsetG;
	if(m_doOffsetError)
		offset+=(m_offsetErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1 : 1);
	if(m_doGainError)
		gain=1+(m_gainErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1 : 1);
	if(m_doGammaError)
		gamma=2.2+(m_gammaErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1 : 1);

	value=max(aRGBValue[1]*gain+offset,0);
	simulColor[1]=(pow(value/100.0,gamma));

	offset=m_offsetB;
	if(m_doOffsetError)
		offset+=(m_offsetErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1 : 1);
	if(m_doGainError)
		gain=1+(m_gainErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1 : 1);
	if(m_doGammaError)
		gamma=2.2+(m_gammaErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1 : 1);

	value=max(aRGBValue[2]*gain+offset,0);
	simulColor[2]=(pow(value/100.0,gamma));

	Sleep(200);		// Sleep 200 ms to simulate acquisition

	ColorRGB colMeasure(simulColor);
	
	CColor colSensor(ColorXYZ(colMeasure, GetColorReference()));

	double		Spectrum[18] = { 0.001, 0.01, 0.1, 0.15, 0.2, 0.4, 0.5, 0.6, 0.7, 1.2, 1.0, 1.1, 0.8, 0.9, 0.6, 0.5, 0.4, 0.15 };
	colSensor.SetSpectrum ( CSpectrum ( 18, 380, 730, 20, Spectrum ) );
	
	return colSensor;
}

void CSimulatedSensor::GetUniqueIdentifier( CString & strId ) 
{
	sprintf ( strId.GetBuffer(64), "%s-%08X", (LPCSTR) m_name, (DWORD) this );
	strId.ReleaseBuffer (); 
}

BOOL CSimulatedSensor::HasSpectrumCapabilities ( int * pNbBands, int * pMinWaveLength, int * pMaxWaveLength, double * pBandWidth )
{
	return FALSE;
/*
	* pNbBands = 18;
	* pMinWaveLength = 380;
	* pMaxWaveLength = 730;
	* pBandWidth = 20;

	return TRUE;
*/
}
