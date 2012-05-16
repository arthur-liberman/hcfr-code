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

// SpyderIISensor.h: interface for the SpyderIISensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SPYDERIISENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
#define AFX_SPYDERIISENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OneDeviceSensor.h"
#include "SpyderIISensorPropPage.h"

class CSpyderIISensor : public COneDeviceSensor  
{
public:
	DECLARE_SERIAL(CSpyderIISensor) ;

protected:
	CSpyderIISensorPropPage m_SpyderIISensorPropertiesPage;
 
public:
	UINT	m_CalibrationMode;
	UINT	m_ReadTime;
	BOOL	m_bAdjustTime;
	BOOL	m_bAverageReads;
	BOOL	m_debugMode;
	double	m_MinRequestedY;
	UINT	m_MaxReadLoops;

public:
	CSpyderIISensor();
	virtual ~CSpyderIISensor();

	// Overriden functions from CSensor 
	virtual	void Copy(CSensor * p);
	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init( BOOL bForSimultaneousMeasures );
	virtual BOOL Release();

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

	virtual LPCSTR GetStandardSubDir ()	{ return "Etalon_S3"; }
#ifdef USE_NON_FREE_CODE
    virtual bool isValid() const {return false;}
#endif
private:
    virtual CColor MeasureColorInternal(COLORREF aRGBValue);
};

#endif // !defined(AFX_SPYDERIISENSOR_H__1493C213_6A02_44C5_8EB7_55B469092E14__INCLUDED_)
