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
//      Georges GALLERAND
//      John Adcock
//      Ian C
/////////////////////////////////////////////////////////////////////////////

// ArgyllSensor.cpp: implementation of the Argyll class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ArgyllSensor.h"
#include "SpectralSampleFiles.h"

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
    m_SpectralType(""),
    m_meterIndex(-1),
    m_meter(0),
    m_HiRes(1)
{
    m_ArgyllSensorPropertiesPage.m_pSensor = this;

    m_pDevicePage = & m_ArgyllSensorPropertiesPage;  // Add Argyll settings page to property sheet

    m_PropertySheetTitle = IDS_ARGYLLSENSOR_PROPERTIES_TITLE;

    SetName("Argyll Meter");

    // Retrieve the list of installed ccss files to display
	
    try
    {
        m_spectralSamples = new SpectralSampleFiles;		
    }
    catch (std::logic_error& e)
    {
        MessageBox(NULL, e.what(), "Argyll Meter", MB_OK+MB_ICONHAND);
    }
}

CArgyllSensor::CArgyllSensor(ArgyllMeterWrapper* meter) :
    m_DisplayType(0),
    m_ReadingType(0),
    m_meter(meter)
{
    std::string meterName(m_meter->getMeterName());
    m_DisplayType = GetConfig()->GetProfileInt(meterName.c_str(), "DisplayType", 1);
    m_ReadingType = GetConfig()->GetProfileInt(meterName.c_str(), "ReadingType", 0);
    m_SpectralType = GetConfig()->GetProfileString(meterName.c_str(), "SpectralType", 0);
    m_debugMode = !!GetConfig()->GetProfileInt(meterName.c_str(), "DebugMode", 0);
    m_HiRes = GetConfig()->GetProfileInt(meterName.c_str(), "HiRes", 1);

    if (m_SpectralType == "")
    {
        m_SpectralType = "<None>";
    }
    
    m_ArgyllSensorPropertiesPage.m_pSensor = this;

    m_pDevicePage = & m_ArgyllSensorPropertiesPage;  // Add Argyll settings page to property sheet

    m_PropertySheetTitle = IDS_ARGYLLSENSOR_PROPERTIES_TITLE;

    SetName(CString(meterName.c_str()));

    // Retrieve the list of installed ccss files to display

    try
    {
        m_spectralSamples = new SpectralSampleFiles;		
    }
    catch (std::logic_error& e)
    {
        MessageBox(NULL, e.what(), "Argyll Meter", MB_OK+MB_ICONHAND);
    }
}

CArgyllSensor::~CArgyllSensor()
{
    // we don't own the meter don't delete it

    delete m_spectralSamples;

}

void CArgyllSensor::Copy(CSensor * p)
{
    COneDeviceSensor::Copy(p);

    m_DisplayType = ((CArgyllSensor*)p)->m_DisplayType;
    m_ReadingType = ((CArgyllSensor*)p)->m_ReadingType;
    m_SpectralType = ((CArgyllSensor*)p)->m_SpectralType;
    m_meterIndex = ((CArgyllSensor*)p)->m_meterIndex;
    m_HiRes = ((CArgyllSensor*)p)->m_HiRes;
 
    if(m_meter >= 0)
    {
        m_meter = ((CArgyllSensor*)p)->m_meter;
        SetName(CString(m_meter->getMeterName().c_str()));
    }

    if(m_spectralSamples)
    {
        *m_spectralSamples = *(((CArgyllSensor*)p)->m_spectralSamples);
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
        archive << m_SpectralType;
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
        archive >> m_SpectralType;
        archive >> dummy;
        archive >> m_debugMode;
        archive >> m_HiRes;

        // open whatever the first meter is
        // if we leave here with nothing then 
        // we should ge replaced by the simulated meter
        // in the higher up object
        std::string errorMessage;
        ArgyllMeterWrapper::ArgyllMeterWrappers meters = ArgyllMeterWrapper::getDetectedMeters(errorMessage);
        if(meters.size() > 0)
        {
            m_meter = meters[0];
            SetName(CString(m_meter->getMeterName().c_str()));
        }
    }
}

void CArgyllSensor::SetPropertiesSheetValues()
{
    COneDeviceSensor::SetPropertiesSheetValues();

    m_ArgyllSensorPropertiesPage.m_DisplayType=m_DisplayType;
    m_ArgyllSensorPropertiesPage.m_ReadingType=m_ReadingType;
    m_ArgyllSensorPropertiesPage.m_SpectralType=m_SpectralType;
    m_ArgyllSensorPropertiesPage.m_DebugMode=m_debugMode;
    m_ArgyllSensorPropertiesPage.m_DebugMode=m_HiRes;
    m_ArgyllSensorPropertiesPage.m_MeterName = m_meter->getMeterName().c_str();
    m_ArgyllSensorPropertiesPage.m_HiResCheckBoxEnabled = m_meter->doesSupportHiRes();
}

void CArgyllSensor::GetPropertiesSheetValues()
{
    COneDeviceSensor::GetPropertiesSheetValues();
    std::string meterName(m_meter->getMeterName());

    if(m_debugMode != !!m_ArgyllSensorPropertiesPage.m_DebugMode) 
    {
        SetModifiedFlag(TRUE);
        m_debugMode = m_ArgyllSensorPropertiesPage.m_DebugMode;
        GetConfig()->WriteProfileInt(meterName.c_str(), "DebugMode", m_debugMode?1:0);
    }

    if(m_ReadingType != m_ArgyllSensorPropertiesPage.m_ReadingType ||
        m_SpectralType != m_ArgyllSensorPropertiesPage.m_SpectralType ||
        m_DisplayType != m_ArgyllSensorPropertiesPage.m_DisplayType ||
        m_HiRes != m_ArgyllSensorPropertiesPage.m_HiRes)
    {
        SetModifiedFlag(TRUE);
        m_ReadingType=m_ArgyllSensorPropertiesPage.m_ReadingType;
        m_SpectralType=m_ArgyllSensorPropertiesPage.m_SpectralType;
        m_DisplayType=m_ArgyllSensorPropertiesPage.m_DisplayType;
        m_HiRes = m_ArgyllSensorPropertiesPage.m_HiRes;

        GetConfig()->WriteProfileInt(meterName.c_str(), "ReadingType", m_ReadingType );
        GetConfig()->WriteProfileString(meterName.c_str(), "SpectralType", m_SpectralType );
        GetConfig()->WriteProfileInt(meterName.c_str(), "DisplayType", m_DisplayType );
        GetConfig()->WriteProfileInt(meterName.c_str(), "HiRes", m_HiRes );
        Init(FALSE);
    }
}

BOOL CArgyllSensor::Init( BOOL bForSimultaneousMeasures )
{
    std::string errorDescription;
    if(!m_meter->connectAndStartMeter(errorDescription, (ArgyllMeterWrapper::eReadingType)m_ReadingType))
    {
        MessageBox(NULL, errorDescription.c_str(), "Argyll Meter", MB_OK+MB_ICONHAND);
        m_meter = 0;
        return FALSE;
    }
    m_meter->setHiResMode(!!m_HiRes);
    if(m_DisplayType != 0xFFFFFFFF)
    {
        m_meter->setDisplayType(m_DisplayType);
    }
    
    // Cause the meter to load the user-specified spectral calibration .ccss file
 
    try
    {
        if (m_meter->doesMeterSupportSpectralSamples())
        {
            if (!m_meter->isSpectralSampleLoaded((LPCSTR)m_SpectralType))
            {
                SpectralSample spectralSample = spectralSample;
                (void)m_spectralSamples->getSample(spectralSample, (LPCSTR)m_SpectralType);
                return m_meter->loadSpectralSample(spectralSample);
            }
        }
    }
    catch (std::logic_error& e)
    {
        MessageBox(NULL, e.what(), "Argyll Meter", MB_OK+MB_ICONHAND);
        return FALSE;
    }
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
    if(!Init(FALSE)) return;
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

void CArgyllSensor::GetUniqueIdentifier( CString & strId )
{
    strId = "Argyll Meter";
}

void CArgyllSensor::FillDisplayTypeCombo(CComboBox& comboToFill)
{

    int numDisplayTypes(m_meter->getNumberOfDisplayTypes());

    if(numDisplayTypes > 0)
    {
        for(int i(0); i < numDisplayTypes; ++i)
        {
            comboToFill.AddString(m_meter->getDisplayTypeText(i));
        }
    }
}

void CArgyllSensor::FillSpectralTypeCombo(CComboBox& comboToFill)
{
    if (!m_meter->doesMeterSupportSpectralSamples())
	{
        return;    
	}
	
	comboToFill.AddString("<None>");

    try
    {			
        SpectralSampleFiles::SpectralSamples::const_iterator iter;

        for (iter = m_spectralSamples->getList().begin(); iter != m_spectralSamples->getList().end(); iter++)
        {
            comboToFill.AddString(iter->getDescription());
        }
        comboToFill.EnableWindow((comboToFill.GetCount() != 0)?TRUE:FALSE);
    }
    catch (std::logic_error& e)
    {
        MessageBox(NULL, e.what(), "Argyll Meter", MB_OK+MB_ICONHAND);
    }
    
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

BOOL CArgyllSensor::SensorNeedCalibration()
{
    return FALSE;
}

BOOL CArgyllSensor::SensorAcceptCalibration()
{
    if(GetConfig()->m_bUseCalibrationFilesOnAllProbes)
    {
        return true;
    }
    else
    {
        return m_meter->isColorimeter()?TRUE:FALSE;
    }
}
