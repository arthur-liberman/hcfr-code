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

bool CArgyllSensor::m_debugMode = false;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CArgyllSensor, COneDeviceSensor, 1) ;

CArgyllSensor::CArgyllSensor() :
    m_DisplayType(0),
    m_ReadingType(0),
    m_meterIndex(-1),
    m_meter(0)
{
    m_DisplayType = GetConfig()->GetProfileInt("Argyll", "DisplayType", 1);
    m_ReadingType = GetConfig()->GetProfileInt("Argyll", "ReadingType", 0);
    m_debugMode = !!GetConfig()->GetProfileInt("Argyll", "DebugMode", 0);
    m_HiRes = GetConfig()->GetProfileInt("Argyll", "HiRes", 1);

    m_ArgyllSensorPropertiesPage.m_pSensor = this;

    m_pDevicePage = & m_ArgyllSensorPropertiesPage;  // Add Argyll settings page to property sheet

    m_PropertySheetTitle = IDS_ARGYLLSENSOR_PROPERTIES_TITLE;

    SetName("Argyll Meter");
}

CArgyllSensor::CArgyllSensor(ArgyllMeterWrapper* meter) :
    m_DisplayType(0),
    m_ReadingType(0),
    m_meter(meter)
{
    m_DisplayType = GetConfig()->GetProfileInt("Argyll", "DisplayType", 1);
    m_ReadingType = GetConfig()->GetProfileInt("Argyll", "ReadingType", 0);
    m_debugMode = !!GetConfig()->GetProfileInt("Argyll", "DebugMode", 0);
    m_HiRes = GetConfig()->GetProfileInt("Argyll", "HiRes", 1);

    m_ArgyllSensorPropertiesPage.m_pSensor = this;

    m_pDevicePage = & m_ArgyllSensorPropertiesPage;  // Add Argyll settings page to property sheet

    m_PropertySheetTitle = IDS_ARGYLLSENSOR_PROPERTIES_TITLE;

    SetName(CString(m_meter->getMeterName().c_str()));
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
    m_meterIndex = ((CArgyllSensor*)p)->m_meterIndex;
    m_HiRes = ((CArgyllSensor*)p)->m_HiRes;
    if(m_meter >= 0)
    {
        m_meter = ((CArgyllSensor*)p)->m_meter;
        SetName(CString(m_meter->getMeterName().c_str()));
    }
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
        archive << m_meterIndex;
        archive << m_debugMode;
        archive << m_HiRes;
    }
    else
    {
        int version;
        int dummy;
        archive >> version;
        if ( version > 1 )
            AfxThrowArchiveException ( CArchiveException::badSchema );
        archive >> m_DisplayType;
        archive >> m_ReadingType;
        archive >> dummy;
        archive >> m_debugMode;
        archive >> m_HiRes;
    }
}

void CArgyllSensor::SetPropertiesSheetValues()
{
    COneDeviceSensor::SetPropertiesSheetValues();

    m_ArgyllSensorPropertiesPage.m_DisplayType=m_DisplayType;
    m_ArgyllSensorPropertiesPage.m_ReadingType=m_ReadingType;
    m_ArgyllSensorPropertiesPage.m_PortNumber = 0;
    m_ArgyllSensorPropertiesPage.m_DebugMode=m_debugMode;
    m_ArgyllSensorPropertiesPage.m_DebugMode=m_HiRes;
}

void CArgyllSensor::GetPropertiesSheetValues()
{
    COneDeviceSensor::GetPropertiesSheetValues();

    if(m_HiRes != m_ArgyllSensorPropertiesPage.m_HiRes)
    {
        SetModifiedFlag(TRUE);
        m_HiRes = m_ArgyllSensorPropertiesPage.m_HiRes;
        GetConfig () -> WriteProfileInt ( "Argyll", "HiRes", m_HiRes );
        if(m_meter)
        {
            m_meter->setHiResMode(!!m_HiRes);
        }
    }

    if(m_debugMode != !!m_ArgyllSensorPropertiesPage.m_DebugMode) 
    {
        SetModifiedFlag(TRUE);
        m_debugMode = m_ArgyllSensorPropertiesPage.m_DebugMode;
        GetConfig () -> WriteProfileInt ( "Argyll", "DebugMode", m_debugMode?1:0);
    }

    if(m_ReadingType != m_ArgyllSensorPropertiesPage.m_ReadingType) 
    {
        SetModifiedFlag(TRUE);
        m_ReadingType=m_ArgyllSensorPropertiesPage.m_ReadingType;

        GetConfig () -> WriteProfileInt ( "Argyll", "ReadingType", m_ReadingType );
    }
}

BOOL CArgyllSensor::Init( BOOL bForSimultaneousMeasures )
{
    std::string errorDescription;
    if(!m_meter->connectAndStartMeter(errorDescription, (ArgyllMeterWrapper::eReadingType)m_ReadingType))
    {
        MessageBox(NULL, errorDescription.c_str(), "Argyll Meter", MB_OK+MB_ICONHAND);
        delete m_meter;
        m_meter = 0;
        return FALSE;
    }
    m_meter->setHiResMode(!!m_HiRes);
    return TRUE;
}

BOOL CArgyllSensor::Release()
{
    return CSensor::Release();
}

CColor CArgyllSensor::MeasureColor(COLORREF aRGBValue)
{
    if(!m_meter) if(!Init(FALSE)) return noDataColor;

    ArgyllMeterWrapper::eMeterState state(ArgyllMeterWrapper::NEEDS_MANUAL_CALIBRATION);
    while(state != ArgyllMeterWrapper::READY)
    {
        try
        {
            state = m_meter->takeReading();
        }
        catch(std::logic_error&)
        {
            return noDataColor;
        }
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
    if(!m_meter) if(!Init(FALSE)) return;
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
    if(!m_meter) if(!Init(FALSE)) return FALSE;

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


// very basic logging and error handling to override
// the standard argyll verion
// should use whatever log library we end up with
void ArgyllLogMessage(const char* messageType, char *fmt, va_list& args)
{
    if(CArgyllSensor::isInDebugMode())
    {
        FILE *logFile = fopen( GetConfig () -> m_logFileName, "a" );
        fprintf(logFile,"Argyll %s - ", messageType);
        vfprintf(logFile, fmt, args);
        va_end(args);
        fprintf(logFile,"\n");
        fclose(logFile);
    }
}
