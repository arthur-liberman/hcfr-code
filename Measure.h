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
/////////////////////////////////////////////////////////////////////////////

// Measure.h: interface for the CMeasure class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MEASURE_H__4A61BBE7_7779_4FCD_90B5_E9F22517DFBD__INCLUDED_)
#define AFX_MEASURE_H__4A61BBE7_7779_4FCD_90B5_E9F22517DFBD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Color.h"
#include "Sensor.h"
#include "Generator.h"

#define	DUPLGRAYLEVEL		0
#define	DUPLNEARBLACK		1
#define	DUPLNEARWHITE		2
#define	DUPLPRIMARIESSAT	3
#define	DUPLSECONDARIESSAT	4
#define	DUPLPRIMARIESCOL	5
#define	DUPLSECONDARIESCOL	6
#define	DUPLCONTRAST		7
#define	DUPLINFO			8
#define DUPLXYZADJUST		9


#define LUX_NOMEASURE	0
#define LUX_OK			1
#define LUX_CANCELED	2


class CMeasure : public CObject  
{
public:
	DECLARE_SERIAL(CMeasure) ;

protected:
	BOOL m_isModified;
	CArray<CColor,CColor> m_primariesArray;
	CArray<CColor,CColor> m_secondariesArray;
	CArray<CColor,CColor> m_grayMeasureArray;
	CArray<CColor,CColor> m_measurementsArray;
	CColor m_OnOffBlack;
	CColor m_OnOffWhite;
	CColor m_AnsiBlack;
	CColor m_AnsiWhite;
	CArray<CColor,CColor> m_nearBlackMeasureArray;
	CArray<CColor,CColor> m_nearWhiteMeasureArray;
	CArray<CColor,CColor> m_redSatMeasureArray;
	CArray<CColor,CColor> m_greenSatMeasureArray;
	CArray<CColor,CColor> m_blueSatMeasureArray;
	CArray<CColor,CColor> m_yellowSatMeasureArray;
	CArray<CColor,CColor> m_cyanSatMeasureArray;
	CArray<CColor,CColor> m_magentaSatMeasureArray;
	CString m_infoStr;
    BOOL	m_bUseAdjustmentMatrix;
	Matrix	m_XYZAdjustmentMatrix;
public:
	CString m_XYZAdjustmentComment;
	BOOL	m_bIREScaleMode;

	// Internal data used by background measures threads (not serialized)
public:
	HANDLE					m_hThread;
	HANDLE					m_hEventRun;
	HANDLE					m_hEventDone;
	BOOL					m_bTerminateThread;
	BOOL					m_bErrorOccurred;
	int						m_nBkMeasureStepCount;
	int						m_nBkMeasureStep;
	COLORREF				m_clrToMeasure;
	CSensor *				m_pBkMeasureSensor;
	CArray<CColor,int> *	m_pBkMeasuredColor;
	int						m_nbMaxMeasurements;

public:
	CMeasure();
	virtual ~CMeasure();

	virtual void Serialize(CArchive& archive); 

	void StartLuxMeasure ();
	UINT GetLuxMeasure ( double * pValue ); 

	void Copy(CMeasure * p,UINT nId);
	BOOL MeasureGrayScale(CSensor *pSensor, CGenerator *pGenerator);
	BOOL MeasureGrayScaleAndColors(CSensor *pSensor, CGenerator *pGenerator);
	CColor GetGray(int i) const;
	void SetGray(int i,const CColor & aColor) {m_grayMeasureArray[i]=aColor; } 
	int GetGrayScaleSize() const { return m_grayMeasureArray.GetSize(); }
	void SetGrayScaleSize(int steps);
	void SetIREScaleMode(BOOL bIRE);
	void SetMeasuredGray(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_grayMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_grayMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }

	BOOL MeasureNearBlackScale(CSensor *pSensor, CGenerator *pGenerator);
	CColor GetNearBlack(int i) const;
	void SetNearBlack(int i,const CColor & aColor) {m_nearBlackMeasureArray[i]=aColor; } 
	int GetNearBlackScaleSize() const { return m_nearBlackMeasureArray.GetSize(); }
	void SetNearBlackScaleSize(int steps);
	void SetMeasuredNearBlack(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_nearBlackMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_nearBlackMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }

	BOOL MeasureNearWhiteScale(CSensor *pSensor, CGenerator *pGenerator);
	CColor GetNearWhite(int i) const;
	void SetNearWhite(int i,const CColor & aColor) {m_nearWhiteMeasureArray[i]=aColor; } 
	int GetNearWhiteScaleSize() const { return m_nearWhiteMeasureArray.GetSize(); }
	void SetNearWhiteScaleSize(int steps);
	void SetMeasuredNearWhite(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_nearWhiteMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_nearWhiteMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }

	BOOL MeasureRedSatScale(CSensor *pSensor, CGenerator *pGenerator);
	BOOL MeasureGreenSatScale(CSensor *pSensor, CGenerator *pGenerator);
	BOOL MeasureBlueSatScale(CSensor *pSensor, CGenerator *pGenerator);
	BOOL MeasureYellowSatScale(CSensor *pSensor, CGenerator *pGenerator);
	BOOL MeasureCyanSatScale(CSensor *pSensor, CGenerator *pGenerator);
	BOOL MeasureMagentaSatScale(CSensor *pSensor, CGenerator *pGenerator);
	BOOL MeasureAllSaturationScales(CSensor *pSensor, CGenerator *pGenerator,BOOL bPrimaryOnly);
	int GetSaturationSize() const { return m_redSatMeasureArray.GetSize(); }
	void SetSaturationSize(int steps);
	CColor GetRedSat(int i) const;
	void SetRedSat(int i,const CColor & aColor) {m_redSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetGreenSat(int i) const;
	void SetGreenSat(int i,const CColor & aColor) {m_greenSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetBlueSat(int i) const;
	void SetBlueSat(int i,const CColor & aColor) {m_blueSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetYellowSat(int i) const;
	void SetYellowSat(int i,const CColor & aColor) {m_yellowSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetCyanSat(int i) const;
	void SetCyanSat(int i,const CColor & aColor) {m_cyanSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	CColor GetMagentaSat(int i) const;
	void SetMagentaSat(int i,const CColor & aColor) {m_magentaSatMeasureArray[i]=aColor; m_isModified=TRUE; } 
	
	void SetMeasuredRedSat(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_redSatMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_redSatMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }
	void SetMeasuredGreenSat(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_greenSatMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_greenSatMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }
	void SetMeasuredBlueSat(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_blueSatMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_blueSatMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }
	void SetMeasuredYellowSat(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_yellowSatMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_yellowSatMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }
	void SetMeasuredCyanSat(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_cyanSatMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_cyanSatMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }
	void SetMeasuredMagentaSat(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_magentaSatMeasureArray[i].SetSensorToXYZMatrix(SensorMatrix); m_magentaSatMeasureArray[i].SetSensorValue(aColor); m_isModified=TRUE; }

	BOOL MeasurePrimaries(CSensor *pSensor, CGenerator *pGenerator);
	CColor GetPrimary(int i) const;
	CColor GetRedPrimary(BOOL bBypassAdjust=FALSE) const;
	CColor GetGreenPrimary(BOOL bBypassAdjust=FALSE) const;
	CColor GetBluePrimary(BOOL bBypassAdjust=FALSE) const;
	void SetRedPrimary(const CColor & aColor) { m_primariesArray[0]=aColor; m_isModified=TRUE; }
	void SetGreenPrimary(const CColor & aColor) { m_primariesArray[1]=aColor; m_isModified=TRUE; }
	void SetBluePrimary(const CColor & aColor) { m_primariesArray[2]=aColor; m_isModified=TRUE; }
	void SetMeasuredPrimary(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_primariesArray[i].SetSensorToXYZMatrix(SensorMatrix); m_primariesArray[i].SetSensorValue(aColor); m_isModified=TRUE; }
 
	BOOL MeasureSecondaries(CSensor *pSensor, CGenerator *pGenerator);
	CColor GetSecondary(int i) const;
	CColor GetYellowSecondary(BOOL bBypassAdjust=FALSE) const;
	CColor GetCyanSecondary(BOOL bBypassAdjust=FALSE) const;
	CColor GetMagentaSecondary(BOOL bBypassAdjust=FALSE) const;
	void SetYellowSecondary(const CColor & aColor) { m_secondariesArray[0]=aColor; m_isModified=TRUE; }
	void SetCyanSecondary(const CColor & aColor) { m_secondariesArray[1]=aColor; m_isModified=TRUE; }
	void SetMagentaSecondary(const CColor & aColor) { m_secondariesArray[2]=aColor; m_isModified=TRUE; }
	void SetMeasuredSecondary(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_secondariesArray[i].SetSensorToXYZMatrix(SensorMatrix); m_secondariesArray[i].SetSensorValue(aColor); m_isModified=TRUE; }

	CColor GetAnsiBlack(BOOL bBypassAdjust=FALSE) const;
	CColor GetAnsiWhite(BOOL bBypassAdjust=FALSE) const;
	void SetAnsiBlack(const CColor & aColor) { m_AnsiBlack=aColor; m_isModified=TRUE; }
	void SetAnsiWhite(const CColor & aColor) { m_AnsiWhite=aColor; m_isModified=TRUE; }

	CColor GetOnOffBlack(BOOL bBypassAdjust=FALSE) const;
	CColor GetOnOffWhite(BOOL bBypassAdjust=FALSE) const;
	void SetOnOffBlack(const CColor & aColor) { m_OnOffBlack=aColor; m_isModified=TRUE; }
	void SetOnOffWhite(const CColor & aColor) { m_OnOffWhite=aColor; m_isModified=TRUE; }

	BOOL MeasureContrast(CSensor *pSensor, CGenerator *pGenerator);
	double GetOnOffContrast ();
	double GetAnsiContrast ();
	double GetContrastMinLum ();
	double GetContrastMaxLum ();
	void DeleteContrast ();
	
	BOOL AddMeasurement(CSensor *pSensor, CGenerator *pGenerator);
	CColor GetMeasurement(int i) const;
	void SetMeasurements(int i,const CColor & aColor) {m_measurementsArray[i]=aColor; m_isModified=TRUE; } 
	void AppendMeasurements(const CColor & aColor);
	int GetMeasurementsSize() const { return m_measurementsArray.GetSize(); }
	void SetMeasurementsSize(int size) { m_measurementsArray.SetSize(size); m_isModified=TRUE; }
	void DeleteMeasurements(int i,int count) { m_measurementsArray.RemoveAt(i,count); m_isModified=TRUE; }
	void SetMeasuredMeasurement(int i, const CColor & aColor, const Matrix & SensorMatrix) { m_measurementsArray.InsertAt(i,noDataColor);m_measurementsArray[i].SetSensorToXYZMatrix(SensorMatrix); m_measurementsArray[i].SetSensorValue(aColor); m_isModified=TRUE; }
	void InsertMeasurement(int i, const CColor & aColor) { m_measurementsArray.InsertAt(i,aColor); m_isModified=TRUE; }
	void FreeMeasurementAppended();

	CString GetInfoString() const { return m_infoStr; }
	void SetInfoString(CString & aStr) { m_infoStr = aStr; } 

	CColor GetRefPrimary(int i) const;
	CColor GetRefSecondary(int i) const;
	CColor GetRefSat(int i, double sat_percent) const;

	BOOL IsModified() { return m_isModified; }
	void SetSensorMatrix(const Matrix & aMatrix, BOOL doPreserveSensorValues=FALSE);
	
	void ResetAdjustmentMatrix() { m_XYZAdjustmentMatrix = IdentityMatrix(3); m_bUseAdjustmentMatrix = FALSE; m_XYZAdjustmentComment.Empty (); }
	void SetAdjustmentMatrix(const Matrix & aMatrix) { m_XYZAdjustmentMatrix = aMatrix; }
	BOOL HasAdjustmentMatrix() const { return ! m_XYZAdjustmentMatrix.IsIdentity (); }
	Matrix GetAdjustmentMatrix() const { return m_XYZAdjustmentMatrix; }
	void EnableAdjustmentMatrix(BOOL bEnable) { m_bUseAdjustmentMatrix = bEnable; }
	BOOL IsAdjustmentMatrixEnabled() const { return m_bUseAdjustmentMatrix && HasAdjustmentMatrix(); }
	CColor ComputeAdjustedColor(const CColor & aColor) const;

	BOOL WaitForDynamicIris ( BOOL bIgnoreEscape = FALSE );

	HANDLE InitBackgroundMeasures ( CSensor *pSensor, int nSteps );
	BOOL BackgroundMeasureColor ( int nCurStep, COLORREF aRGBValue );
	void CancelBackgroundMeasures ();
	BOOL ValidateBackgroundGrayScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundNearBlack ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundNearWhite ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundPrimaries ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundSecondaries ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundRedSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundGreenSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundBlueSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundYellowSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundCyanSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundMagentaSatScale ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundGrayScaleAndColors ( BOOL bUseLuxValues, double * pLuxValues );
	BOOL ValidateBackgroundSingleMeasurement ( BOOL bUseLuxValues, double * pLuxValues );
};

#endif // !defined(AFX_MEASURE_H__4A61BBE7_7779_4FCD_90B5_E9F22517DFBD__INCLUDED_)
