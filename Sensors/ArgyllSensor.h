/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
// Copyright (c) 2012-2015 Hcfr project.  All rights reserved.
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
    UINT		m_DisplayType;
    UINT		m_ReadingType;
    CString		m_SpectralType;
    UINT        m_intTime;
    BOOL		m_HiRes;
    BOOL        m_Adapt;
private:
    ArgyllMeterWrapper* m_meter;
    SpectralSampleFiles *m_spectralSamples;

public:
    CArgyllSensor();
    CArgyllSensor(ArgyllMeterWrapper* meter);
    virtual ~CArgyllSensor();

    // Overriden functions from CSensor
    virtual void Copy(CSensor * p);    
    virtual void Serialize(CArchive& archive); 

    virtual BOOL Init( BOOL bForSimultaneousMeasures );
    virtual BOOL Release();

    virtual void SetPropertiesSheetValues();
    virtual void GetPropertiesSheetValues();

    virtual LPCSTR GetStandardSubDir ()    { return "Etalon_Argyll"; }

    void Calibrate();

    virtual void GetUniqueIdentifier( CString & strId );
    static bool isInDebugMode() {return m_debugMode;}
    virtual bool isValid() const {return (m_meter != 0);}
    virtual int ReadingType() const {return m_ReadingType;}
    virtual CString SpectralType() const {return m_SpectralType;}
    void FillDisplayTypeCombo(CComboBox& comboToFill);
    virtual bool isColorimeter() const;
    virtual bool setAvg();
    virtual bool isRefresh() const;

private:
    virtual CColor MeasureColorInternal(const ColorRGBDisplay& aRGBValue);
};

#endif // !defined(AFX_ARGYLLSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
