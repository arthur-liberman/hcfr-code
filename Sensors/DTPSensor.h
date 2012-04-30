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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// DTPSensor.h: interface for the DTPSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DTPSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
#define AFX_DTPSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OneDeviceSensor.h"
#include "DTPSensorPropPage.h"

class CDTPSensor : public COneDeviceSensor  
{
public:
	DECLARE_SERIAL(CDTPSensor) ;

protected:
	CDTPSensorPropPage m_DTPSensorPropertiesPage;
	BOOL	m_bNeedInit;

public:
	UINT	m_CalibrationMode;
	UINT	m_timeout;
	BOOL	m_bUseInternalOffsets;
	BOOL	m_debugMode;
	BOOL	m_bAverageReads;
	double	m_MinRequestedY;
	UINT	m_MaxReadLoops;

public:
	CDTPSensor();
	virtual ~CDTPSensor();

	// Overriden functions from CSensor
	virtual	void Copy(CSensor * p);	
	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init( BOOL bForSimultaneousMeasures );
	virtual CColor MeasureColor(COLORREF aRGBValue);
	virtual BOOL Release();

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

	virtual LPCSTR GetStandardSubDir ()	{ return GetConfig()->m_bUseCalibrationFilesOnAllProbes ? "Etalon_DTP94" : ""; }
	virtual BOOL SensorNeedCalibration () { return FALSE; }
	virtual BOOL SensorAcceptCalibration () { return GetConfig()->m_bUseCalibrationFilesOnAllProbes; }

	virtual BOOL SendAndReceive ( LPCSTR lpszCmd, LPSTR lpszResponse, size_t nBufferSize );
};

#endif // !defined(AFX_DTPSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
