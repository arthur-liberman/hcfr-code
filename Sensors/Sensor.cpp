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
//	Fran�ois-Xavier CHABOUD
/////////////////////////////////////////////////////////////////////////////

// Sensor.cpp: implementation of the CSensor class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "Sensor.h"
#include "Generator.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

IMPLEMENT_SERIAL(CSensor, CObject, 1) ;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSensor::CSensor()
{
	m_isModified=FALSE;
	m_isMeasureValid=TRUE;
	m_sensorToXYZMatrix=IdentityMatrix(3);

	m_calibrationTime=0;

	m_PropertySheetTitle = IDS_SENSOR_PROPERTIES_TITLE;

	m_pDevicePage = NULL;
	m_pCalibrationPage = NULL;

	SetName("Not defined");  // Needs to be set for real sensors
}

CSensor::~CSensor()
{

}

void CSensor::Copy(CSensor * p)
{
	m_errorString = p->m_errorString;
	m_isMeasureValid = p->m_isMeasureValid;
	m_sensorToXYZMatrix = p->m_sensorToXYZMatrix;
	m_calibrationTime = p->m_calibrationTime;
	m_name = p->m_name;
}

void CSensor::Serialize(CArchive& archive)
{
	CObject::Serialize(archive);
	m_sensorToXYZMatrix.Serialize(archive); 
	if (archive.IsStoring())
	{
		int version=1;
		archive << version;
		archive << m_calibrationTime;
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		archive >> m_calibrationTime;
	}
}

void CSensor::SetPropertiesSheetValues()
{
	m_SensorPropertiesPage.SetMatrix(m_sensorToXYZMatrix);
	m_SensorPropertiesPage.m_calibrationDate=GetCalibrationTime().Format("%#c");
}

void CSensor::GetPropertiesSheetValues()
{
	if(	m_sensorToXYZMatrix != m_SensorPropertiesPage.GetMatrix() )
	{
		m_sensorToXYZMatrix=m_SensorPropertiesPage.GetMatrix();
		SetModifiedFlag(TRUE);
	}
}

BOOL CSensor::Init( BOOL bForSimultaneousMeasures )
{
	return TRUE;
}

BOOL CSensor::Release()
{
	return TRUE;
}

CColor CSensor::MeasureColor(COLORREF aRGBValue)
{
	return noDataColor; 
}

CColor CSensor::MeasureGray(double aLevel, BOOL bIRE)
{
	// by default use pure virtual DisplayRGBColor function
	return MeasureColor(CIRELevel(aLevel,bIRE)); 
}

BOOL CSensor::CalibrateSensor(CGenerator *apGenerator)
{
	return TRUE;
}

BOOL CSensor::CalibrateSensor(Matrix & measures, Matrix & references, CColor & WhiteTest, CColor & WhiteRef, CColor & BlackTest, CColor & BlackRef)
{
	return TRUE;
}

BOOL CSensor::Configure()
{
	CString	str;
	CPropertySheetWithHelp propertySheet;

	str.LoadString(m_PropertySheetTitle);
	propertySheet.SetTitle(str);
	propertySheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	if ( m_pDevicePage )
		propertySheet.AddPage ( m_pDevicePage );
	if ( m_pCalibrationPage && SensorNeedCalibration () )
		propertySheet.AddPage ( m_pCalibrationPage );

	if ( SensorAcceptCalibration () || ! m_sensorToXYZMatrix.IsIdentity () )
		propertySheet.AddPage ( & m_SensorPropertiesPage );

	propertySheet.SetActivePage(0);
	SetPropertiesSheetValues();
	int result=propertySheet.DoModal();
	if(result == IDOK)
		GetPropertiesSheetValues();

	return result==IDOK;
}
