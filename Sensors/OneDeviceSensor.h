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
//	François-Xavier CHABOUD
/////////////////////////////////////////////////////////////////////////////

// OneDeviceSensor.h: interface for the COneDeviceSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ONEDEVICESENSOR_H__88E5A2F7_5D02_4EAF_9895_8507A7E47CDA__INCLUDED_)
#define AFX_ONEDEVICESENSOR_H__88E5A2F7_5D02_4EAF_9895_8507A7E47CDA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Sensor.h"
#include "OneDeviceSensorPropPage.h"

class CCalibrationInfo : public CObject
{
public:
	CCalibrationInfo()	{ m_WhiteTest=noDataColor; m_WhiteRef=noDataColor; m_BlackTest=noDataColor; m_BlackRef=noDataColor; }
	CCalibrationInfo(Matrix & measures, Matrix & references, CColor & WhiteTest, CColor & WhiteRef, CColor & BlackTest, CColor & BlackRef, CString & Author );
	CCalibrationInfo(CCalibrationInfo & other);

	Matrix	m_measures;
	Matrix	m_references;
	CColor	m_WhiteTest;
	CColor	m_WhiteRef;
	CColor	m_BlackTest;
	CColor	m_BlackRef;
	CString	m_Author;

	virtual void Serialize(CArchive& archive); 
	virtual void DisplayAdditivity ( Matrix & sensorToXYZMatrix, BOOL bForHCFRSensor );
	virtual void GetAdditivityInfoText ( CString & strResult, Matrix & sensorToXYZMatrix, BOOL bForHCFRSensor );
};

class COneDeviceSensor : public CSensor  
{
public:
	DECLARE_SERIAL(COneDeviceSensor) ;

protected: 
	float m_calibrationIRELevel;
	BOOL m_doBlackCompensation;
	BOOL m_doVerifyAdditivity;
	BOOL m_showAdditivityResults;
	int m_maxAdditivityErrorPercent;
	CString m_calibrationReferenceName;

	COneDeviceSensorPropPage m_oneDevicePropertiesPage;

	CCalibrationInfo *	m_pCalibrationInfo;

public:
	Matrix m_primariesChromacities; 
	Matrix m_whiteChromacity; 

	CString	m_CalibrationFileName;

public:
	COneDeviceSensor();
	virtual ~COneDeviceSensor();
	virtual	void Copy(CSensor * p);

	CCalibrationInfo * GetCalibrationInfo ()				{ return m_pCalibrationInfo; }
	void SetCalibrationInfo ( CCalibrationInfo * pInfo );

	virtual void Serialize(CArchive& archive); 

	virtual BOOL CalibrateSensor(CGenerator *apGenerator);
	virtual BOOL CalibrateSensor(Matrix & measures, Matrix & references, CColor & WhiteTest, CColor & WhiteRef, CColor & BlackTest, CColor & BlackRef);
	virtual void LoadCalibrationFile(CString & aFileName);
	virtual void SaveCalibrationFile();

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

};

#endif // !defined(AFX_ONEDEVICESENSOR_H__88E5A2F7_5D02_4EAF_9895_8507A7E47CDA__INCLUDED_)
