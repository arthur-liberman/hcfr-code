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

// Sensor.h: interface for the CSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SENSOR_H__FD0761AA_CBEC_4A38_8A67_ADB0963FBAE4__INCLUDED_)
#define AFX_SENSOR_H__FD0761AA_CBEC_4A38_8A67_ADB0963FBAE4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Color.h"
#include "SensorPropPage.h"

class CGenerator;

class CSensor: public CObject   
{

public:
	DECLARE_SERIAL(CSensor) ;

protected:
	BOOL m_isModified;
	CString m_errorString;
	BOOL m_isMeasureValid;
	Matrix m_sensorToXYZMatrix;
	time_t m_calibrationTime;
	int		m_PropertySheetTitle;
	CSensorPropPage m_SensorPropertiesPage;
	CString m_name;

	CPropertyPageWithHelp * m_pDevicePage;
	CPropertyPageWithHelp * m_pCalibrationPage;

public:
	CSensor();
	virtual ~CSensor();
	virtual	void Copy(CSensor * p);

	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init( BOOL bForSimultaneousMeasures );
	virtual CColor MeasureColor(COLORREF aRGBValue); // need to be overriden
	virtual CColor MeasureGray(double aIRELevel,BOOL bIRE);
	virtual BOOL Release();

	virtual BOOL CalibrateSensor(CGenerator *apGenerator);
	virtual BOOL CalibrateSensor(Matrix & measures, Matrix & references, CColor & WhiteTest, CColor & WhiteRef, CColor & BlackTest, CColor & BlackRef);
	virtual void LoadCalibrationFile(CString & aFileName) { return ; }
	virtual void SaveCalibrationFile() { return ; }

	virtual void SetSensorMatrix(Matrix aMatrix) { m_sensorToXYZMatrix=aMatrix; m_calibrationTime=time(NULL);}
	virtual Matrix GetSensorMatrix() {return m_sensorToXYZMatrix; }

	virtual BOOL IsMeasureValid() {return m_isMeasureValid; }
	virtual void SetMeasureValidity(BOOL isValid) { m_isMeasureValid=isValid; }
	virtual void SetErrorString(CString aString) { m_errorString=aString; }
	virtual CString GetErrorString() { return m_errorString; }

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();
	virtual BOOL Configure();

	virtual BOOL IsModified() { return m_isModified; }
	virtual void SetModifiedFlag( BOOL bModified ) { m_isModified = bModified; }

	virtual LPCSTR GetStandardSubDir ()	{ return ""; }
	virtual BOOL SensorNeedCalibration () { return TRUE; }
	virtual BOOL SensorAcceptCalibration () { return TRUE; }

	CTime GetCalibrationTime() { return SensorAcceptCalibration () ? CTime(m_calibrationTime) : CTime (2000,1,1,0,0,0,-1); }
	BOOL IsCalibrated() { return SensorNeedCalibration () ? ( m_calibrationTime != 0 || ! m_sensorToXYZMatrix.IsIdentity () ) : TRUE; }

	CString GetName() { return m_name; }
	void SetName(CString aStr) { m_name=aStr; } 

	// returns unique sensor identifier (for simultaneous measures: cannot use twice the same sensor on two documents)
	virtual void GetUniqueIdentifier( CString & strId ) { strId = m_name; }

	virtual BOOL HasSpectrumCapabilities ( int * pNbBands, int * pMinWaveLength, int * pMaxWaveLength, int * pBandWidth ) { return FALSE; }
};

#endif // !defined(AFX_SENSOR_H__FD0761AA_CBEC_4A38_8A67_ADB0963FBAE4__INCLUDED_)
