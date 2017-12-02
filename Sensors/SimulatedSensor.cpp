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
	m_offsetRed=2;
	m_offsetGreen=1;
	m_offsetBlue=3;
	m_doOffsetError=TRUE;
	m_offsetErrorMax=1.0;
	m_doGainError=TRUE;
	m_gainErrorMax=0.01;		
	m_doGammaError=TRUE;
	m_gammaErrorMax=0.07;

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
	m_offsetR=0;//m_offsetRed*(1+(double)rand()/(double)RAND_MAX);
	m_offsetG=0;//m_offsetGreen*(1+(double)rand()/(double)RAND_MAX);
	m_offsetB=0;//m_offsetBlue*(1+(double)rand()/(double)RAND_MAX);
	m_bNW = bForSimultaneousMeasures; //use for turning off diffuse scaling in NW measures
	return TRUE;
}

CColor CSimulatedSensor::MeasureColorInternal(const ColorRGBDisplay& aRGBValue)
{
	ColorRGB simulColor;
	double offset=0.0, gain=1.0 ,gamma;
	double value;
	CColor White = GetColorReference().GetWhite();
	int mode = GetConfig()->m_GammaOffsetType;
	White.SetY(100);
	CColor Black = GetColorReference().GetWhite();
	Black.SetY(GetConfig()->m_TargetMinL);
	double peakY = 10000.;
	if (GetConfig()->m_colorStandard == sRGB)
		mode = 99;
	if  (mode == 7 || mode == 8 || mode == 9)
		mode = 5; //simulate standard PQ curve
	
	//	quantize to 8 or 10 bit video
	double r,g,b;
	if (GetConfig() -> m_bUse10bit)
	{
		r =  floor( (aRGBValue[0]/100. * 219. * 4.) + 0.5 ) / (2.19 * 4.0);
		g =  floor( (aRGBValue[1]/100. * 219. * 4.) + 0.5 ) / (2.19 * 4.0);
		b =  floor( (aRGBValue[2]/100. * 219. * 4.) + 0.5 ) / (2.19 * 4.0);
	}
	else
	{
		r =  floor( (aRGBValue[0]/100. * 219.) + 0.5 ) / 2.19;
		g =  floor( (aRGBValue[1]/100. * 219.) + 0.5 ) / 2.19;
		b =  floor( (aRGBValue[2]/100. * 219.) + 0.5 ) / 2.19;
	}
		

	gamma=GetConfig()->m_GammaRef;

	if (GetConfig()->m_colorStandard == sRGB) mode = 99;

	offset=m_offsetR;
	if(m_doOffsetError)
		offset+=(m_offsetErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);
	if(m_doGainError)
		gain=1+(m_gainErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);
	value=max(r*gain+offset,0.0);
	
	if (  mode >= 4 )
    {
		if (mode == 5)
		{
			if (m_bNW)
				value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode) / 100.;
			else
				value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap) / 100.;

			value = min(value,peakY / 10000.);
		}
		else
			value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);

		simulColor[0] = value;
	} 
	else
		simulColor[0]=(pow(value/100.0,gamma));

	offset=m_offsetG;
	if(m_doOffsetError)
		offset+=(m_offsetErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);
	if(m_doGainError)
		gain=1+(m_gainErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);
	value = max(g*gain+offset,0.0);

	if (  mode >= 4 )
    {
		if (mode == 5)
		{
			if (m_bNW)
				value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode) / 100.;
			else
				value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap) / 100.;
			value = min(value,peakY / 10000.);
		}
		else
			value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);

		simulColor[1] = value;
	} 
	else
		simulColor[1]=(pow(value/100.0,gamma));

	offset=m_offsetB;
	if(m_doOffsetError)
		offset+=(m_offsetErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);
	if(m_doGainError)
		gain=1+(m_gainErrorMax*(double)rand()/(double)RAND_MAX) * (rand() > RAND_MAX/2 ? -1.0 : 1.0);
	value = max(b*gain+offset,0.0);

	if (  mode >= 4 )
    {
		if (mode == 5)
		{
			if (m_bNW)
				value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode) / 100.;
			else
				value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode, GetConfig()->m_DiffuseL, GetConfig()->m_MasterMinL, GetConfig()->m_MasterMaxL, GetConfig()->m_TargetMinL, GetConfig()->m_TargetMaxL,GetConfig()->m_useToneMap) / 100.;
			value = min(value,peakY / 10000.);
		}
		else
			value = getL_EOTF(value / 100., White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);

		simulColor[2] = value;
	} 
	else
		simulColor[2]=(pow(value/100.0,gamma));

	Sleep(50);
	ColorRGB colMeasure(simulColor);
	bool isSpecial = (GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb);

	CColor colSensor(ColorXYZ(colMeasure, isSpecial?CColorReference(HDTV):GetConfig()->m_colorStandard == UHDTV3?CColorReference(UHDTV2):GetColorReference()));
	colSensor.SetX(colSensor.GetX() * (mode==5?10000.:100.));
	colSensor.SetY(colSensor.GetY() * (mode==5?10000.:100.));
	colSensor.SetZ(colSensor.GetZ() * (mode==5?10000.:100.));

	//cap peak white to target maxL
	if (mode == 5)
	{
		double rescale = min(colSensor.GetY(), GetConfig()->m_TargetMaxL)/colSensor.GetY();
		colSensor.SetX(colSensor.GetX() * rescale);
		colSensor.SetY(colSensor.GetY() * rescale);
		colSensor.SetZ(colSensor.GetZ() * rescale);
	}

	//set black to target minL
	else if (aRGBValue[0] == 0. && aRGBValue[1] == 0. && aRGBValue[2] == 0.)
		colSensor.SetY(GetConfig()->m_TargetMinL);


	double	Spectrum[18] = { 0.001, 0.01, 0.1, 0.15, 0.2, 0.4, 0.5, 0.6, 0.7, 1.2, 1.0, 1.1, 0.8, 0.9, 0.6, 0.5, 0.4, 0.15 };
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
