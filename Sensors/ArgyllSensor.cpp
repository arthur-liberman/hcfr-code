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
//  Georges GALLERAND
//  John Adcock
/////////////////////////////////////////////////////////////////////////////

// ArgyllSensor.cpp: implementation of the Argyll class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ArgyllSensor.h"

#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CArgyllSensor, COneDeviceSensor, 1) ;

CArgyllSensor::CArgyllSensor() :
    m_meter(0)
{
    m_DisplayType = GetConfig()->GetProfileInt("Argyll", "DisplayType", 1);
    m_ReadingType = GetConfig()->GetProfileInt("Argyll", "ReadingType", 0);
    m_PortNumber = GetConfig()->GetProfileInt("Argyll", "PortNumber", 1);
    m_DebugMode = GetConfig()->GetProfileInt("Argyll", "DebugMode", 0);

    m_ArgyllSensorPropertiesPage.m_pSensor = this;

    m_pDevicePage = & m_ArgyllSensorPropertiesPage;  // Add Argyll settings page to property sheet

    m_PropertySheetTitle = IDS_ARGYLLSENSOR_PROPERTIES_TITLE;

    SetName("Argyll Meter");
}

CArgyllSensor::~CArgyllSensor()
{
    delete m_meter;
}

void CArgyllSensor::Copy(CSensor * p)
{
    COneDeviceSensor::Copy(p);

    m_DisplayType = ((CArgyllSensor*)p)->m_DisplayType;
    m_ReadingType = ((CArgyllSensor*)p)->m_ReadingType;
    m_PortNumber = ((CArgyllSensor*)p)->m_PortNumber;
    m_DebugMode = ((CArgyllSensor*)p)->m_DebugMode;
}

void CArgyllSensor::Serialize(CArchive& archive)
{
    COneDeviceSensor::Serialize(archive) ;

    if (archive.IsStoring())
    {
        int version=1;
        archive << version;
        archive << m_DisplayType;
        archive << m_ReadingType;
        archive << m_PortNumber;
        archive << m_DebugMode;
    }
    else
    {
        int version;
        archive >> version;
        if ( version > 1 )
            AfxThrowArchiveException ( CArchiveException::badSchema );
        archive >> m_DisplayType;
        archive >> m_ReadingType;
        archive >> m_PortNumber;
        archive >> m_DebugMode;
    }
}

void CArgyllSensor::SetPropertiesSheetValues()
{
    COneDeviceSensor::SetPropertiesSheetValues();

    m_ArgyllSensorPropertiesPage.m_DisplayType=m_DisplayType;
    m_ArgyllSensorPropertiesPage.m_ReadingType=m_ReadingType;
    m_ArgyllSensorPropertiesPage.m_PortNumber=m_PortNumber - 1;
    m_ArgyllSensorPropertiesPage.m_DebugMode=m_DebugMode;
}

void CArgyllSensor::GetPropertiesSheetValues()
{
    COneDeviceSensor::GetPropertiesSheetValues();

    if( m_DisplayType != m_ArgyllSensorPropertiesPage.m_DisplayType ||
        m_ReadingType != m_ArgyllSensorPropertiesPage.m_ReadingType ||
        m_PortNumber != m_ArgyllSensorPropertiesPage.m_PortNumber ||
        m_DebugMode != m_ArgyllSensorPropertiesPage.m_DebugMode) 
    {
        SetModifiedFlag(TRUE);
        m_DisplayType=m_ArgyllSensorPropertiesPage.m_DisplayType;
        m_ReadingType=m_ArgyllSensorPropertiesPage.m_ReadingType;
        m_PortNumber=m_ArgyllSensorPropertiesPage.m_PortNumber + 1;
        m_DebugMode=m_ArgyllSensorPropertiesPage.m_DebugMode;

        GetConfig () -> WriteProfileInt ( "Argyll", "DisplayType", m_DisplayType );
        GetConfig () -> WriteProfileInt ( "Argyll", "ReadingType", m_ReadingType );
        GetConfig () -> WriteProfileInt ( "Argyll", "PortNumber", m_PortNumber );
        GetConfig () -> WriteProfileInt ( "Argyll", "DebugMode", m_DebugMode );

        if(m_meter)
        {
            delete m_meter;
            m_meter = NULL;
        }
    }

}

BOOL CArgyllSensor::Init( BOOL bForSimultaneousMeasures )
{
    if(!m_meter)
    {
        m_meter = new ArgyllMeterWrapper(ArgyllMeterWrapper::AUTODETECT, 
                                            (ArgyllMeterWrapper::eDisplayType)m_DisplayType,
                                            (ArgyllMeterWrapper::eReadingType)m_ReadingType,
                                            m_PortNumber);
        std::string errorDescription;
        if(!m_meter->connectAndStartMeter(errorDescription))
        {
            MessageBox(NULL, errorDescription.c_str(), "Argyll Meter", MB_OK+MB_ICONHAND);
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CArgyllSensor::Release()
{
    return CSensor::Release();
}

CColor CArgyllSensor::MeasureColor(COLORREF aRGBValue)
{
    if(!m_meter) Init(FALSE);

    ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
    while(state != ArgyllMeterWrapper::READY)
    {
        state = m_meter->takeReading();
        if(state == ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION)
        {
            Calibrate();
        }
        if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
        {
            MessageBox(NULL, m_meter->getIncorrectPositionInstructions().c_str(), "Incorrect Position", MB_OK+MB_ICONHAND);
        }
    }
    return m_meter->getLastReading();
}

void CArgyllSensor::Calibrate()
{
    if(!m_meter) Init(FALSE);
    if(!m_meter->doesMeterSupportCalibration()) return;

    ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
    while(state != ArgyllMeterWrapper::READY)
    {
        std::string meterInstructions(m_meter->getCalibrationInstructions());
        if(meterInstructions.empty())
        {
            break;
        }
        MessageBox(NULL, meterInstructions.c_str(), "Calibration Instructions", MB_OK);
        state = m_meter->calibrate();
        if(state == ArgyllMeterWrapper::INCORRECT_POSITION)
        {
            MessageBox(NULL, m_meter->getIncorrectPositionInstructions().c_str(), "Incorrect Position", MB_OK+MB_ICONHAND);
        }
    }
    MessageBox(NULL, "Device is now calibrated.  If the device requires it return to the correct measurement position.", "Calibration Complete", MB_OK);
}

BOOL CArgyllSensor::SensorNeedCalibration () 
{
    if(!m_meter) Init(FALSE);

    return m_meter->doesMeterSupportCalibration();
}

BOOL CArgyllSensor::SensorAcceptCalibration ()
{
    return FALSE;
}


BOOL CArgyllSensor::HasSpectrumCapabilities ( int * pNbBands, int * pMinWaveLength, int * pMaxWaveLength, int * pBandWidth )
{
    return FALSE;
}

void CArgyllSensor::GetUniqueIdentifier( CString & strId )
{
    strId = "Argyll Meter";
}
