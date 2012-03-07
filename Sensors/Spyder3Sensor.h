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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// Spyder3Sensor.h: interface for the Spyder3Sensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SPYDER3SENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
#define AFX_SPYDER3SENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OneDeviceSensor.h"
#include "Spyder3SensorPropPage.h"

class CSpyder3Sensor : public COneDeviceSensor  
{
public:
	DECLARE_SERIAL(CSpyder3Sensor) ;

protected:
	CSpyder3SensorPropPage m_Spyder3SensorPropertiesPage;
 
public:
	UINT	m_ReadTime;
	BOOL	m_bAdjustTime;
	BOOL	m_bAverageReads;
	BOOL	m_debugMode;
	double	m_MinRequestedY;
	UINT	m_MaxReadLoops;

public:
	CSpyder3Sensor();
	virtual ~CSpyder3Sensor();

	// Overriden functions from CSensor 
	virtual	void Copy(CSensor * p);
	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init( BOOL bForSimultaneousMeasures );
	virtual CColor MeasureColor(COLORREF aRGBValue);
	virtual BOOL Release();

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

	virtual LPCSTR GetStandardSubDir ()	{ return GetConfig()->m_bUseCalibrationFilesOnAllProbes ? "Etalon_S2" : ""; }
	virtual BOOL SensorNeedCalibration () { return FALSE; }
	virtual BOOL SensorAcceptCalibration () { return GetConfig()->m_bUseCalibrationFilesOnAllProbes; }
#ifdef USE_NON_FREE_CODE
    virtual bool isValid() const {return false;}
#endif
};

#endif // !defined(AFX_SPYDER3SENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
