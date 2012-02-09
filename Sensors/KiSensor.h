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
//	Georges GALLERAND
//  Patrice AFFLATET
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// KiSensor.h: interface for the KiSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_KISENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
#define AFX_KISENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OneDeviceSensor.h"
#include "KiSensorPropPage.h"

class CKiSensor : public COneDeviceSensor  
{
public:
	DECLARE_SERIAL(CKiSensor) ;

protected:
	CKiSensorPropPage m_kiSensorPropertiesPage;
 
public:
	CString	m_comPort;
	CString	m_RealComPort;
	UINT	m_timeoutMesure;
	BOOL	m_debugMode;
	BOOL	m_bMeasureRGB;
	BOOL	m_bMeasureWhite;
	UINT	m_nSensorsUsed;
	UINT	m_nInterlaceMode;
	BOOL	m_bFastMeasure;
	BOOL	m_bSingleSensor;
	BOOL	m_bLEDStopped;

public:
	CKiSensor();
	virtual ~CKiSensor();

		// Overriden functions from CSensor
	virtual	void Copy(CSensor * p);	
	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init( BOOL bForSimultaneousMeasures );
	virtual BOOL Release();
	virtual CColor MeasureColor(COLORREF aRGBValue);
	virtual BOOL acquire(char *com_port, int timeout, char command, char *sensVal);

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

	virtual LPCSTR GetStandardSubDir ()	{ return "Etalon_HCFR"; }

	// returns unique sensor identifier (for simultaneous measures: cannot use twice the same sensor on two documents)
	virtual void GetUniqueIdentifier( CString & strId );

protected:
	void decodeKiStr(char *kiStr, int RGB[3]);
	void GetRealComPort ( CString & ComPort );
};

#endif // !defined(AFX_KISENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
