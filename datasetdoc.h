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
//	Fran�ois-Xavier CHABOUD
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// DataSetDoc.h : interface of the CDataSetDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_DataSetDoc_H__FDDEAC5A_8B9B_4AFD_BFDE_F48C28BF9899__INCLUDED_)
#define AFX_DataSetDoc_H__FDDEAC5A_8B9B_4AFD_BFDE_F48C28BF9899__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define MAX_SIMULTANEOUS_MEASURES	32

// Update views hint
#define UPD_EVERYTHING					0
#define UPD_PRIMARIES					1
#define UPD_SECONDARIES					2
#define UPD_PRIMARIESANDSECONDARIES		3
#define UPD_GRAYSCALEANDCOLORS			4
#define UPD_GRAYSCALE					5 
#define UPD_NEARBLACK					6 
#define UPD_NEARWHITE					7 
#define UPD_REDSAT						8 
#define UPD_GREENSAT					9 
#define UPD_BLUESAT						10
#define UPD_YELLOWSAT					11
#define UPD_CYANSAT						12
#define UPD_MAGENTASAT					13
#define UPD_ALLSATURATIONS				14
#define UPD_CONTRAST					15
#define UPD_FREEMEASURES				16
#define UPD_SENSORCONFIG				17
#define UPD_GENERATORCONFIG				18
#define UPD_DATAREFDOC					19
#define UPD_REFERENCEDATA				20
#define UPD_SELECTEDCOLOR				21
#define UPD_ARRAYSIZES					22
#define UPD_GENERALREFERENCES			23
#define UPD_FREEMEASUREAPPENDED			24
#define UPD_CC24SAT						25
#define UPD_REALTIME					26

class CColorTempHistoView;
class CRGBHistoView;
class CLuminanceHistoView;
class CNearBlackHistoView;
class CNearWhiteHistoView;
class CCIEChartView;
class CSatLumHistoView;
class CSatLumShiftView;
class CMeasuresHistoView;

class CDataSetWindowPositions;
class CDataSetFrameInfo;

#include "Measure.h"
#include "Sensors\Sensor.h"
#include "Generators\Generator.h"

#define WM_BKGND_MEASURE_READY	WM_USER + 1279

class CDataSetDoc : public CDocument
{
protected: // create from serialization only
	CDataSetDoc();
	DECLARE_DYNCREATE(CDataSetDoc)

public:
	// Attributes used during document opening for window position serialization
	CDataSetWindowPositions *	m_pWndPos;
	CDataSetFrameInfo *			m_pFramePosInfo;
	void DisplayPatternPicture(HMODULE hInst, UINT nIDResource, BOOL bResizePict);

// Attributes
public:
	CMeasure		m_measure;
	CGenerator *	m_pGenerator;
	CSensor *		m_pSensor;

	CColor			m_SelectedColor;
	CColor			m_LastColor;
	// Operations
public:
	void ShowAllViews();

	void MeasureGrayScale();
	void MeasureGrayScaleAndColors();

	void MeasureNearBlackScale();
	void MeasureNearWhiteScale();

	void MeasureRedSatScale();
	void MeasureGreenSatScale();
	void MeasureBlueSatScale();
	void MeasureYellowSatScale();
	void MeasureCyanSatScale();
	void MeasureMagentaSatScale();
	void MeasureCC24SatScale();
	void MeasureAllSaturationScales();
	void MeasurePrimarySaturationScales();
	void MeasurePrimarySecondarySaturationScales();

	void MeasurePrimaries();
	void MeasureSecondaries();
	void MeasureContrast();
	void AddMeasurement();
	bool CheckVideoLevel();
	//void CalibrateSensor(BOOL doUpdateValues=TRUE);

	CMeasure *GetMeasure() { return &m_measure; }
	CGenerator *GetGenerator() { return m_pGenerator; }
	CSensor *GetSensor() { return m_pSensor; }

	void ShowViewOnTop(UINT nMsg);
	void HideView(UINT nMsg);
	void HideAllViews();

	void ComputeGammaAndOffset(double * Gamma, double * Offset, int ColorSpace,int ColorIndex,int Size, bool m_bBT1886);
	BOOL ComputeAdjustmentMatrix();

	void SetSelectedColor ( const CColor & clr )	{ m_SelectedColor = clr; }
	void SetLastColor ( const CColor & clr )	{ m_LastColor = clr; }
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDataSetDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual void OnCloseDocument();
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL CanCloseFrame(CFrameWnd* pFrame);
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	//}}AFX_VIRTUAL

public:
	virtual void SetModifiedFlag( BOOL bModified = TRUE );
	virtual BOOL IsModified( );

// Implementation
public:
	virtual ~CDataSetDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	void OnBkgndMeasureReady();

protected:
	void CreateSensor(int aID);
	void CreateGenerator(int aID);
	void DuplicateSensor(CDataSetDoc * pDoc);
	void DuplicateGenerator(CDataSetDoc * pDoc);
	void WaitKey(int Anim = 0);

	void PerformSimultaneousMeasures ( int nMode );
	
	virtual void UpdateFrameCounts();
	bool Settling;
friend class CMainView;

// Generated message map functions
protected:
	//{{AFX_MSG(CDataSetDoc)
	afx_msg void OnConfigureSensor();
	afx_msg void OnConfigureSensor2();
	afx_msg void OnConfigureGenerator();
	afx_msg void OnChangeGenerator();
	afx_msg void OnChangeSensor();
	afx_msg void OnExportXls();
	afx_msg void OnExportCsv();
	afx_msg void OnExportPdf();
	afx_msg void OnUpdateExportXls(CCmdUI* pCmdUI);
	afx_msg void OnCalibrationSim();
	afx_msg void OnCalibrationManual();
	afx_msg void OnCalibrationExisting();
	afx_msg void OnCalibrationSpectralSample();
	afx_msg void OnSimGrayscale();
	afx_msg void OnSimPrimaries();
	afx_msg void OnSimSecondaries();
	afx_msg void OnSimGrayscaleAndColors();
	afx_msg void OnPatternAnimBlack();
	afx_msg void OnPatternAnimWhite();
	afx_msg void OnPatternGradient();
	afx_msg void OnPatternGradient2();
	afx_msg void OnPatternRG();
	afx_msg void OnPatternRB();
	afx_msg void OnPatternGB();
	afx_msg void OnPatternRGd();
	afx_msg void OnPatternRBd();
	afx_msg void OnPatternGBd();
	afx_msg void OnPatternLramp();
	afx_msg void OnPatternGranger();
	afx_msg void OnPattern80();
	afx_msg void OnPatternTV();
	afx_msg void OnPatternTV2();
	afx_msg void OnPatternSpectrum();
	afx_msg void OnPatternSramp();
	afx_msg void OnPatternTestimg();
	afx_msg void OnPatternISO12233();
	afx_msg void OnPatternNB();
	afx_msg void OnPatternBBCHD();
	afx_msg void OnPatternCROSSl();
	afx_msg void OnPatternCROSSd();
	afx_msg void OnPatternPM5644();
	afx_msg void OnPatternZONE();
	afx_msg void OnPatternVSMPTE();
	afx_msg void OnPatternEramp();
	afx_msg void OnPatternAlign();
	afx_msg void OnPatternAlign2();
	afx_msg void OnPatternUser1();
	afx_msg void OnPatternUser2();
	afx_msg void OnPatternUser3();
	afx_msg void OnPatternUser4();
	afx_msg void OnPatternUser5();
	afx_msg void OnPatternUser6();
	afx_msg void OnPatternTC0();
	afx_msg void OnPatternTC1();
	afx_msg void OnPatternTC2();
	afx_msg void OnPatternTC3();
	afx_msg void OnPatternTC4();
	afx_msg void OnPatternTC5();
	afx_msg void OnPatternBN();
	afx_msg void OnPatternDR0();
	afx_msg void OnPatternDR1();
	afx_msg void OnPatternDR2();
	afx_msg void OnPatternSharp();
	afx_msg void OnPatternClipL();
	afx_msg void OnPatternClipH();
	afx_msg void OnSingleMeasurement();
	afx_msg void OnContinuousMeasurement();
	afx_msg void OnMeasureGrayscale();
	afx_msg void OnUpdateMeasureGrayscale(CCmdUI* pCmdUI);
	afx_msg void OnMeasurePrimaries();
	afx_msg void OnUpdateMeasurePrimaries(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSecondaries();
	afx_msg void OnUpdateMeasureSecondaries(CCmdUI* pCmdUI);
	afx_msg void OnUpdateContinuousMeasurement(CCmdUI* pCmdUI);
	afx_msg void OnMeasureDefinescale();
	afx_msg void OnMeasureNearblack();
	afx_msg void OnUpdateMeasureNearblack(CCmdUI* pCmdUI);
	afx_msg void OnMeasureNearwhite();
	afx_msg void OnUpdateMeasureNearwhite(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSatRed();
	afx_msg void OnUpdateMeasureSatRed(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSatGreen();
	afx_msg void OnUpdateMeasureSatGreen(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSatBlue();
	afx_msg void OnUpdateMeasureSatBlue(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSatYellow();
	afx_msg void OnUpdateMeasureSatYellow(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSatCyan();
	afx_msg void OnUpdateMeasureSatCyan(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSatMagenta();
	afx_msg void OnUpdateMeasureSatMagenta(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSatCC24();
	afx_msg void OnUpdateMeasureSatCC24(CCmdUI* pCmdUI);
	afx_msg void OnMeasureContrast();
	afx_msg void OnUpdateMeasureContrast(CCmdUI* pCmdUI);
	afx_msg void OnMeasureSatAll();
	afx_msg void OnUpdateMeasureSatAll(CCmdUI* pCmdUI);
	afx_msg void OnMeasureGrayscaleColors();
	afx_msg void OnUpdateMeasureGrayscaleColors(CCmdUI* pCmdUI);
	afx_msg void OnSimNearblack();
	afx_msg void OnSimNearwhite();
	afx_msg void OnSimSatRed();
	afx_msg void OnSimSatGreen();
	afx_msg void OnSimSatBlue();
	afx_msg void OnSimSatYellow();
	afx_msg void OnSimSatCyan();
	afx_msg void OnSimSatMagenta();
	afx_msg void OnSimSingleMeasurement();
	afx_msg void OnMeasureSatPrimaries();
	afx_msg void OnUpdateMeasureSatPrimaries(CCmdUI* pCmdUI);
	afx_msg void OnSaveCalibrationFile();
	afx_msg void OnMeasureSatPrimariesSecondaries();
	afx_msg void OnMeasureFullTiltBoogie();
	afx_msg void OnUpdateMeasureFullTiltBoogie(CCmdUI* pCmdUI);
	afx_msg void OnUpdateMeasureSatPrimariesSecondaries(CCmdUI* pCmdUI);
	afx_msg void OnLoadCalibrationFile();
	afx_msg void OnUpdateSaveCalibrationFile(CCmdUI* pCmdUI);
	afx_msg void OnUpdateLoadCalibrationFile(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DataSetDoc_H__FDDEAC5A_8B9B_4AFD_BFDE_F48C28BF9899__INCLUDED_)
