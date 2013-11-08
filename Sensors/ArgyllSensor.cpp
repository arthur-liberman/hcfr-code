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
    m_SpectralType("Default"),
    m_intTime(1),
    m_meter(0),
    m_HiRes(0)
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
    m_SpectralType = GetConfig()->GetProfileString(meterName.c_str(), "SpectralType", "Default");
    m_intTime = GetConfig()->GetProfileInt(meterName.c_str(), "IntTime", 1);
    m_debugMode = GetConfig()->GetProfileInt(meterName.c_str(), "DebugMode", 0);
    m_HiRes = GetConfig()->GetProfileInt(meterName.c_str(), "HiRes", 0);
    
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
    m_intTime = ((CArgyllSensor*)p)->m_intTime;
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
        int version=3;
        archive << version;
        archive << m_DisplayType;
        archive << m_ReadingType;
        archive << m_SpectralType;
        archive << m_debugMode;
        archive << m_HiRes;
        archive << m_intTime;
        if(m_meter)
        {
            archive << CString(m_meter->getMeterName().c_str());
        }
    }
    else
    {
        int version;
        archive >> version;
        if ( version > 3 )
            AfxThrowArchiveException ( CArchiveException::badSchema );
        archive >> m_DisplayType;
        archive >> m_ReadingType;
        archive >> m_SpectralType;
        if ( version > 2)
            archive >> m_intTime;
        if(version == 1)
        {
            UINT dummy;
            archive >> dummy;
        }
        archive >> m_debugMode;
        archive >> m_HiRes;

        std::string errorMessage;
        ArgyllMeterWrapper::ArgyllMeterWrappers meters = ArgyllMeterWrapper::getDetectedMeters(errorMessage);
        if(version > 1)
        {
            // try and open the same meter we were saved with
            // otherwise exit so that we get the simulated meter
            CString meterName;
            archive >> meterName;
            for(size_t i(0); i < meters.size(); ++i)
            {
                if(meters[i]->getMeterName().c_str() == meterName)
                {
                    m_meter = meters[0];
                    SetName(CString(m_meter->getMeterName().c_str()));
                }
            }
        }
        else
        {
            // if we don't yet have a meter
            // open whatever the first meter is
            // if we leave here with nothing then 
            // we should ge replaced by the simulated meter
            // in the higher up object
            if(meters.size() > 0)
            {
                m_meter = meters[0];
                SetName(CString(m_meter->getMeterName().c_str()));
            }
        }
    }
}

void CArgyllSensor::SetPropertiesSheetValues()
{
    COneDeviceSensor::SetPropertiesSheetValues();

    m_ArgyllSensorPropertiesPage.m_DisplayType=m_DisplayType;
    m_ArgyllSensorPropertiesPage.m_ReadingType=m_ReadingType;
    m_ArgyllSensorPropertiesPage.m_SpectralType=m_SpectralType;
    m_ArgyllSensorPropertiesPage.m_intTime=m_intTime;
    m_ArgyllSensorPropertiesPage.m_DebugMode=m_debugMode;
    m_ArgyllSensorPropertiesPage.m_HiResCheckBoxEnabled = m_meter->doesSupportHiRes();
    m_ArgyllSensorPropertiesPage.m_obTypeEnabled = (m_meter->doesMeterSupportSpectralSamples() || !m_meter->isColorimeter());
    m_ArgyllSensorPropertiesPage.m_intTimeEnabled = (m_meter->getMeterName() == "Xrite i1 DisplayPro, ColorMunki Display");
    m_ArgyllSensorPropertiesPage.m_HiRes=m_HiRes;
    m_ArgyllSensorPropertiesPage.m_MeterName = m_meter->getMeterName().c_str();
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
        m_HiRes != m_ArgyllSensorPropertiesPage.m_HiRes ||
        m_intTime != m_ArgyllSensorPropertiesPage.m_intTime)
    {
        SetModifiedFlag(TRUE);
        m_ReadingType=m_ArgyllSensorPropertiesPage.m_ReadingType;
        m_SpectralType=m_ArgyllSensorPropertiesPage.m_SpectralType;
        m_DisplayType=m_ArgyllSensorPropertiesPage.m_DisplayType;
        m_HiRes = m_ArgyllSensorPropertiesPage.m_HiRes;
        m_intTime=m_ArgyllSensorPropertiesPage.m_intTime;

        GetConfig()->WriteProfileInt(meterName.c_str(), "ReadingType", m_ReadingType );
        GetConfig()->WriteProfileString(meterName.c_str(), "SpectralType", m_SpectralType );
        GetConfig()->WriteProfileInt(meterName.c_str(), "IntTime", m_intTime );
        GetConfig()->WriteProfileInt(meterName.c_str(), "DisplayType", m_DisplayType );
        GetConfig()->WriteProfileInt(meterName.c_str(), "HiRes", m_HiRes );
        Init(TRUE);
    }
}

BOOL CArgyllSensor::Init( BOOL bForSimultaneousMeasures )
{
    std::string errorDescription;
    double i_time=0.0;

    switch (m_intTime)
    {
        case 1:
            i_time = 0.50;
            break;
        case 2:
            i_time = 0.30;
            break;
        case 3:
            i_time = 0.40;
            break;
        case 4:
            i_time = 0.60;
            break;
        case 5:
            i_time = 0.8;
            break;
        case 6:
            i_time = 1.0;
            break;
    }

    if(!m_meter->connectAndStartMeter(errorDescription, (ArgyllMeterWrapper::eReadingType)m_ReadingType, m_SpectralType, CArgyllSensor::isInDebugMode(), i_time, ((m_DisplayType == 1) || CArgyllSensor::isRefresh()) && !(m_DisplayType == 0)  ) )
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

//    if (bForSimultaneousMeasures)
//        MessageBox(NULL, m_meter->getDisplayTypeText(m_meter->getDisplayType()), "Display Type Set", MB_OK);

 //ccss is now loaded through display type
    // Cause the meter to load the user-specified spectral calibration .ccss file
/*    try
    {
        if (m_meter->doesMeterSupportSpectralSamples())
        {
                        MessageBox(NULL,(LPCSTR)m_SpectralType,"test",MB_OK);
           // see if the sample exists, otherwise reset
            // thus the <None> string or translation will cause a reset as required
            if (m_spectralSamples->doesSampleDescriptionExist((LPCSTR)m_SpectralType))
            {
                if (m_meter->currentSpectralSampleDescription() != (LPCSTR)m_SpectralType )
                {
                    const SpectralSample& spectralSample(m_spectralSamples->getSample((LPCSTR)m_SpectralType));
                    m_meter->loadSpectralSample(spectralSample);
                }
                else
                {
                    ; // do nothing - already in correct state
                }
            }
            else
            {
                m_meter->resetSpectralSample();
            }
        }
    }

    catch (std::logic_error& e)
    {
        MessageBox(NULL, e.what(), "Argyll Meter", MB_OK+MB_ICONHAND);
        return FALSE;
    }
    */
    //Alert user if in ambient/lux mode
    if (bForSimultaneousMeasures)
    {
        if (m_meter->getReadingType() == 2)
            MessageBox(NULL, "Ambient mode set, values will be reported in LUX", "Argyll Meter set-up", MB_OK);
        if (m_meter->getReadingType() != m_ReadingType)
        {
            char s1 [128];
            sprintf(s1, "Reading mode not available, defaulting to %s",m_meter->getReadingType()==0?"DISPLAY":(m_meter->getReadingType()==1?"PROJECTOR":"AMBIENT"));
            MessageBox(NULL, s1, "Argyll Meter set-up", MB_OK);
            m_ReadingType = m_meter->getReadingType();
        }   
    }
    return TRUE;
}

BOOL CArgyllSensor::Release()
{
    return CSensor::Release();
}

CColor CArgyllSensor::MeasureColorInternal(const ColorRGBDisplay& aRGBValue)
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
    strId = m_meter->getMeterName().c_str();
}

void CArgyllSensor::FillDisplayTypeCombo(CComboBox& comboToFill)
{

    int numDisplayTypes(m_meter->getNumberOfDisplayTypes());

    if(numDisplayTypes > 1)
    {
        for(int i(0); i < numDisplayTypes; ++i)
        {
            comboToFill.AddString(m_meter->getDisplayTypeText(i));
        }
    }
}
/*
void CArgyllSensor::FillSpectralTypeCombo(CComboBox& comboToFill)
{
    if (!m_meter->doesMeterSupportSpectralSamples())
	{
        return;    
	}
	
	comboToFill.AddString("<None>");

    try
    {
        SpectralSampleFiles::SpectralSampleDescriptions::const_iterator iter;

        for (iter = m_spectralSamples->getDescriptions().begin(); iter != m_spectralSamples->getDescriptions().end(); iter++)
        {
            comboToFill.AddString(iter->c_str());
        }
        comboToFill.EnableWindow((comboToFill.GetCount() != 0)?TRUE:FALSE);
    }
    catch (std::logic_error& e)
    {
        MessageBox(NULL, e.what(), "Argyll Meter", MB_OK+MB_ICONHAND);
    }
    
}
*/

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

bool CArgyllSensor::isColorimeter() const
{
    return m_meter->isColorimeter();
}

bool CArgyllSensor::isRefresh() const
{
    return m_meter->isRefresh();
}
