/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
// Copyright (c) 2012 Hcfr project.  All rights reserved.
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

// ArgyllSensor.h: interface for the ArgyllSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ARGYLLSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
#define AFX_ARGYLLSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OneDeviceSensor.h"
#include "Argyllsensorproppage.h"
#include "ArgyllMeterWrapper.h"

class CArgyllSensor : public COneDeviceSensor  
{
public:
    DECLARE_SERIAL(CArgyllSensor) ;

protected:
    CArgyllSensorPropPage m_ArgyllSensorPropertiesPage;
    static bool m_debugMode;

public:
    UINT    m_DisplayType;
    UINT    m_ReadingType;
    UINT    m_meterIndex;
    BOOL    m_HiRes;
private:
    ArgyllMeterWrapper* m_meter;

public:
    CArgyllSensor();
    CArgyllSensor(ArgyllMeterWrapper* meter);
    virtual ~CArgyllSensor();

    // Overriden functions from CSensor
    virtual    void Copy(CSensor * p);    
    virtual void Serialize(CArchive& archive); 

    virtual BOOL Init( BOOL bForSimultaneousMeasures );
    virtual CColor MeasureColor(COLORREF aRGBValue);
    virtual BOOL Release();

    virtual void SetPropertiesSheetValues();
    virtual void GetPropertiesSheetValues();

    virtual LPCSTR GetStandardSubDir ()    { return ( GetConfig()->m_bUseCalibrationFilesOnAllProbes) ? "Calibration Argyll" : ""; }

    void Calibrate();

    virtual BOOL SensorNeedCalibration ();
    virtual BOOL SensorAcceptCalibration ();

    virtual BOOL HasSpectrumCapabilities ( int * pNbBands, int * pMinWaveLength, int * pMaxWaveLength, int * pBandWidth );

    virtual void GetUniqueIdentifier( CString & strId );
    static bool isInDebugMode() {return m_debugMode;}
};

#endif // !defined(AFX_ARGYLLSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
