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
//	François-Xavier CHABOUD
/////////////////////////////////////////////////////////////////////////////

// SimulatedSensor.h: interface for the CSimulatedSensor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SIMULATEDSENSOR_H__0A1F47AC_62BE_4C6A_8B0B_F00C8F09C40E__INCLUDED_)
#define AFX_SIMULATEDSENSOR_H__0A1F47AC_62BE_4C6A_8B0B_F00C8F09C40E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "OneDeviceSensor.h"
#include "SimulatedSensorPropPage.h"

class CSimulatedSensor : public COneDeviceSensor  
{
	DECLARE_SERIAL(CSimulatedSensor) ;

public:
	CSimulatedSensor();
	virtual ~CSimulatedSensor();

	// Overriden functions from CSensor
	virtual	void Copy(CSensor * p);
	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init( BOOL bForSimultaneousMeasures );

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

	virtual LPCSTR GetStandardSubDir ()	{ return "Etalon_Simulation"; }

	virtual BOOL HasSpectrumCapabilities ( int * pNbBands, int * pMinWaveLength, int * pMaxWaveLength, double * pBandWidth );

	// returns unique sensor identifier (for simultaneous measures: cannot use twice the same sensor on two documents)
	virtual void GetUniqueIdentifier( CString & strId );

	// Settings
	UINT m_offsetRed;
	UINT m_offsetGreen;
	UINT m_offsetBlue;
	BOOL m_doOffsetError;
	double m_offsetErrorMax;
	BOOL m_doGainError;
	double m_gainErrorMax;		
	BOOL m_doGammaError;
	double m_gammaErrorMax;
	BOOL m_bNW;

protected:
	CSimulatedSensorPropPage m_simulatedSensorPropertiesPage;
	double m_offsetR;
	double m_offsetG;
	double m_offsetB;
private:
    virtual CColor MeasureColorInternal(const ColorRGBDisplay& aRGBValue, int displaymode);
};

#endif // !defined(AFX_SIMULATEDSENSOR_H__0A1F47AC_62BE_4C6A_8B0B_F00C8F09C40E__INCLUDED_)
