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

// MTCSSensor.h: interface for the MTCSSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MTCSSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
#define AFX_MTCSSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OneDeviceSensor.h"
#include "MTCSSensorPropPage.h"

// Position of high precision matrix in MTCS-C2 EEPROM user memory
#define DI3_USERMEMOFFSET_HIDEFMATRIX	5

class CMTCSSensor : public COneDeviceSensor  
{
public:
	DECLARE_SERIAL(CMTCSSensor) ;

protected:
	CMTCSSensorPropPage m_MTCSSensorPropertiesPage;
 
public:
	UINT	m_nAverageMode;
	UINT	m_ReadTime;
	BOOL	m_bAdjustTime;
	BOOL	m_debugMode;
	UINT	m_nAmpValue;
	UINT	m_nShiftValue;
	Matrix	m_DeviceMatrix;
	Matrix	m_DeviceBlackLevel;
	int		m_nBlackROffset;
	int		m_nBlackGOffset;
	int		m_nBlackBOffset;

public:
	CMTCSSensor();
	virtual ~CMTCSSensor();

	// Overriden functions from CSensor 
	virtual	void Copy(CSensor * p);
	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init( BOOL bForSimultaneousMeasures );
	virtual CColor MeasureColor(COLORREF aRGBValue);
	virtual CColor MeasureBlackLevel(BOOL bUseOffsets);
	virtual BOOL Release();

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

	virtual LPCSTR GetStandardSubDir ()	{ return GetConfig()->m_bUseCalibrationFilesOnAllProbes ? "Etalon_MTCS" : ""; }
	virtual BOOL SensorNeedCalibration () { return FALSE; }
	virtual BOOL SensorAcceptCalibration () { return GetConfig()->m_bUseCalibrationFilesOnAllProbes; }
};

#endif // !defined(AFX_MTCSSENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
