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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// DataSetDoc.cpp : implementation of the CDataSetDoc class
//

#include "stdafx.h"
#include "ColorHCFR.h"

#include "DataSetDoc.h"

#include "Measure.h"
#include "MainView.h"
#include "DocTempl.h"
#include "CIEChartView.h"
#include "LuminanceHistoView.h"
#include "NearBlackHistoView.h"
#include "NearWhiteHistoView.h"
#include "RGBHistoView.h"
#include "ColorTempHistoView.h"
#include "SatLumHistoView.h"
#include "SatLumShiftView.h"
#include "MeasuresHistoView.h"
#include "MeasuresHistoView.h"
#include "DuplicateDocDlg.h"

#include "GDIGenerator.h"
#include "ManualDVDGenerator.h"
#include "KiGenerator.h"

#include "SimulatedSensor.h"
#include "KiSensor.h"
#include "SpyderIISensor.h"
#include "DTPSensor.h"
#include "EyeOneSensor.h"
#include "MTCSSensor.h"
#include "Spyder3Sensor.h"
#include "ArgyllSensor.h"
#include "SpectralSampleDlg.h"

#include "Matrix.h"
#include "NewDocWizard.h"

#include "Export.h"

#include "MainFrm.h"

#include "RefColorDlg.h"
#include "ScaleSizes.h"

#include "DocEnumerator.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Current file format version. 
// When loading, only versions <= CURRENT_FILE_FORMAT_VERSION are accepted
// Version 1 is for ColorHCFR version <= 1.12 beta 61
// Version 2 is for ColorHCFR version >= 1.12 beta 62
// Version 3 is for ColorHCFR version >= 2.0 beta 144

#define CURRENT_FILE_FORMAT_VERSION	3
#define _S(id) (CString(LPCTSTR(id))) 

extern CPtrList gOpenedFramesList;	// Implemented in MultiFrm.cpp

BOOL	g_bNewDocUseDDEInfo = FALSE;
BOOL	g_bNewDocIsDuplication = FALSE;

CString g_NewDocName;
CString g_ThcFileName;
int		g_NewDocGeneratorID = 0;
int		g_NewDocSensorID = 0;
Matrix	g_NewDocSensorMatrix;

CDataSetDoc * g_DocToDuplicate;


/////////////////////////////////////////////////////////////////////////////
// Background thread control (continuous measures)

// Internal data to handle background execution

       CWinThread*			g_hThread = NULL;
       BOOL				g_bTerminateThread = FALSE;
       CDataSetDoc *	g_pDataDocRunningThread = NULL;	// View running background thread
static BOOL				g_bGDIGeneratorRunning = FALSE;
static BOOL				g_bGeneratorUsesScreenBlanking = FALSE;
static ColorRGBDisplay	g_CurrentColor;
static CRITICAL_SECTION g_CritSec;
static CPtrList			g_MeasuredColorList;
volatile BOOL			g_bInsideBkgndRefresh = FALSE;

// The background thread function
static UINT __cdecl BkgndThreadFunc ( LPVOID lpParameter )
{
    CrashDump useInThisThread;
    try
    {
	    CSensor *		pSensor = g_pDataDocRunningThread-> m_pSensor;	// Assume sensor is initialized
	    CGenerator *	pGenerator = g_pDataDocRunningThread -> m_pGenerator;
    	
	    CColor			measuredColor;
	    CString			Msg;

	    CView *			pView;
	    POSITION		pos;
			    
		pos = g_pDataDocRunningThread -> GetFirstViewPosition ();
		pView = g_pDataDocRunningThread -> GetNextView ( pos );
		int m_d = ((CMainView*)pView)->m_displayMode;

	    do
	    {
		    // Main loop
		    if ( g_bTerminateThread )
		    {
			    // Exit
			    break;
		    }

			if ( g_bGDIGeneratorRunning )
		    {
			    ColorRGBDisplay clr((( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.GetColor ())& 0x00FFFFFF);
			    if ( g_CurrentColor != clr )
			    {
				    g_CurrentColor = clr;
					CGenerator::MeasureType MT = CGenerator::MT_UNKNOWN;							
					if ( m_d == 1 || (m_d < 11 && m_d > 4) )
						MT = CGenerator::MT_ACTUAL;
					pGenerator->DisplayRGBColor(clr, MT);
			    }
		    }

		    // Perform one measure
		    measuredColor=pSensor->MeasureColor(g_CurrentColor);  // don't know the color to measure so default value to mid gray...
		    if ( g_bTerminateThread )
		    {
			    // Exit
			    break;
		    }

		    if(pSensor->IsMeasureValid())
		    {
			    if ( ! g_bTerminateThread )
			    {
				    CColor * pMeasurement = new CColor;
				    *pMeasurement = measuredColor;

				    while ( g_bInsideBkgndRefresh )
					    Sleep ( 50 );

				    EnterCriticalSection (& g_CritSec );
				    g_MeasuredColorList.AddTail ( pMeasurement );
				    LeaveCriticalSection (& g_CritSec );

				    // Post message to first document view (this message is treated by the document object)
				    AfxGetMainWnd () -> PostMessage ( WM_BKGND_MEASURE_READY, (WPARAM) g_pDataDocRunningThread );
			    }
		    }
		    else
		    {
			    // Error: close thread
			    g_hThread = NULL;
			    g_bTerminateThread = FALSE;

			    if ( GetConfig()->m_bContinuousMeasures )
			    {
				    pos = g_pDataDocRunningThread -> GetFirstViewPosition ();
				    while ( pos )
				    {
					    pView = g_pDataDocRunningThread -> GetNextView ( pos );
					    if ( IsWindow ( pView -> m_hWnd ) && pView -> IsKindOf ( RUNTIME_CLASS ( CMainView ) ) )
					    {
						    // Reset button image to the green arrow only when in free measures mode
						    if ( ( (CMainView*) pView ) -> m_displayMode == 2 )
						    {
		 					    ( (CMainView*) pView ) -> m_grayScaleButton.SetBitmaps(IDB_MEAS_CONT,RGB(0,0,0));
							    Msg.LoadString ( IDS_RUNCONTINUOUS );
							    ( (CMainView*) pView ) -> m_grayScaleButton.SetTooltipText(Msg);
						    }
					    }
				    }
			    }
    			
			    if ( g_bGDIGeneratorRunning )
			    {
				    g_pDataDocRunningThread->GetGenerator()->Release();

				    if ( g_bGeneratorUsesScreenBlanking )
				    {
					    g_pDataDocRunningThread->GetGenerator()->m_doScreenBlanking = TRUE;
					    g_bGeneratorUsesScreenBlanking = FALSE;
				    }

				    g_bGDIGeneratorRunning = FALSE;
			    }
    			
			    g_pDataDocRunningThread = NULL;
			    break;
		    }

	    } while ( TRUE );

	    pSensor -> Release ();
	    DeleteCriticalSection (& g_CritSec );
	
    }
    catch(std::exception& e)
    {
        std::cerr << "Exception in measurement thread : " << e.what() << std::endl;
    }
    catch(...)
    {
        std::cerr << "Unexpected Exception in measurement thread" << std::endl;
    }
	return 0;
}

BOOL StartBackgroundMeasures ( CDataSetDoc * pDoc )
{
	BOOL		bOk = FALSE;
	DWORD		dw;
	CString		Msg, Title;
	CView *		pView;
	POSITION	pos;

	if ( pDoc && g_pDataDocRunningThread == NULL && g_hThread == NULL )
	{
		if(pDoc->m_pSensor->Init(FALSE) != TRUE)
		{
			Msg.LoadString ( IDS_ERRINITSENSOR );
			Title.LoadString ( IDS_ERROR );
			GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
			return FALSE;
		}
		
		pDoc->EnsureVisibleViewMeasuresCombo();

		g_CurrentColor = ColorRGBDisplay(0.5, 0.5, 0.5);

		CString str, str2;
		str.LoadString(IDS_GDIGENERATOR_NAME);
		str2 = pDoc->GetGenerator()->GetName();
		pos = pDoc -> GetFirstViewPosition ();
		pView = pDoc -> GetNextView ( pos );
		int m_d = ((CMainView*)pView)->m_displayMode;
		
		if ( str == str2 )
		{
//			if ( ( ((CGDIGenerator*) pDoc->GetGenerator() ) -> IsOnOtherMonitor () || ( (CMainView*) pView ) -> m_displayMode == DISPLAY_madVR) && GetConfig()->m_bContinuousMeasures )
			if ( GetConfig()->m_bDisplayTestColors )
			{
				// Remove screen blanking flag
				g_bGeneratorUsesScreenBlanking = pDoc->GetGenerator()->m_doScreenBlanking;
				pDoc->GetGenerator()->m_doScreenBlanking = FALSE;

				if ( ! pDoc->GetGenerator()->Init() )
				{
					pDoc->m_pSensor->Release ();

					if ( g_bGeneratorUsesScreenBlanking )
					{
						pDoc->GetGenerator()->m_doScreenBlanking = TRUE;
						g_bGeneratorUsesScreenBlanking = FALSE;
					}

					Msg.LoadString ( IDS_ERRINITGENERATOR );
					Title.LoadString ( IDS_ERROR );
					GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
					return FALSE;
				}

				ColorRGBDisplay clr((( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.GetColor ()) & 0x00ffffff);
				if (!clr.isValid())
					clr = ColorRGBDisplay(0.25);
				g_CurrentColor = clr;
				CGenerator::MeasureType MT = CGenerator::MT_UNKNOWN;							
				if ( m_d == 1 || (m_d < 11 && m_d > 4) )
					MT = CGenerator::MT_ACTUAL;
				pDoc->GetGenerator()->DisplayRGBColor(clr,MT);
				Sleep(500);
				
				g_bGDIGeneratorRunning = TRUE;
			}
		}

		// Create the background thread
		g_bTerminateThread = FALSE;
		g_pDataDocRunningThread = pDoc;
		InitializeCriticalSection (& g_CritSec );
		g_hThread = AfxBeginThread (BkgndThreadFunc, & dw );

		if ( g_hThread != NULL )
		{
			// Background thread is running
			if ( GetConfig()->m_bContinuousMeasures )
			{
				pos = g_pDataDocRunningThread -> GetFirstViewPosition ();
				while ( pos )
				{
					pView = g_pDataDocRunningThread -> GetNextView ( pos );
					if ( IsWindow ( pView -> m_hWnd ) && pView -> IsKindOf ( RUNTIME_CLASS ( CMainView ) ) )
					{
						if ( ( (CMainView*) pView ) -> m_displayMode == 2 )
						{
							//( (CMainView*) pView ) -> m_grayScaleButton.SetIcon(IDI_STOP_ICON,24,24);
							( (CMainView*) pView ) -> m_grayScaleButton.SetBitmaps(IDB_MEAS_STOP,RGB(0,0,0));
							Msg.LoadString ( IDS_STOPCONTINUOUS );
							( (CMainView*) pView ) -> m_grayScaleButton.SetTooltipText(Msg);
						}
					}
				}
			}
			bOk = TRUE;
		}
		else
		{
			// Background thread didn't start
			pDoc->m_pSensor->Release ();
			if ( g_bGDIGeneratorRunning )
			{
				pDoc->GetGenerator()->Release();

				if ( g_bGeneratorUsesScreenBlanking )
				{
					pDoc->GetGenerator()->m_doScreenBlanking = TRUE;
					g_bGeneratorUsesScreenBlanking = FALSE;
				}

				g_bGDIGeneratorRunning = FALSE;
			}
			DeleteCriticalSection (& g_CritSec );

			g_pDataDocRunningThread = NULL;
		}
	}

	return bOk;
}

void StopBackgroundMeasures ()
{
	if ( g_hThread != NULL )
	{
		CString		Msg;
		CWaitCursor	wait;

		// Send thread termination request
		g_bTerminateThread = TRUE;

		// Wait for thread ending
		WaitForSingleObject ((HANDLE)(*g_hThread), INFINITE );
		
		// Clean remaining measures
		POSITION pos = g_MeasuredColorList.GetHeadPosition ();
		while ( pos )
		{
			CColor * pMeasurement = ( CColor * ) g_MeasuredColorList.GetNext ( pos );
			delete pMeasurement;
		}
		g_MeasuredColorList.RemoveAll ();

		// Delete thread object
		g_hThread = NULL;
		g_bTerminateThread = FALSE;
	}

	if ( g_pDataDocRunningThread )
	{
		// Check if view still exists.
		if ( GetConfig()->m_bContinuousMeasures )
		{
			CString		Msg;
			CView *		pView;
			POSITION	pos;
			
			Msg.LoadString ( IDS_RUNCONTINUOUS );

			pos = g_pDataDocRunningThread -> GetFirstViewPosition ();
			while ( pos )
			{
				pView = g_pDataDocRunningThread -> GetNextView ( pos );
				if ( IsWindow ( pView -> m_hWnd ) && pView -> IsKindOf ( RUNTIME_CLASS ( CMainView ) ) )
				{
					// Reset button image to the green arrow only when in free measures mode
					if ( ( (CMainView*) pView ) -> m_displayMode == 2 )
					{
						( (CMainView*) pView ) -> m_grayScaleButton.SetBitmaps(IDB_MEAS_CONT,RGB(0,0,0));
						( (CMainView*) pView ) -> m_grayScaleButton.SetTooltipText(Msg);
					}
				}
			}
		}
		
		if ( g_bGDIGeneratorRunning )
		{
			g_pDataDocRunningThread->GetGenerator()->Release();

			if ( g_bGeneratorUsesScreenBlanking )
			{
				g_pDataDocRunningThread->GetGenerator()->m_doScreenBlanking = TRUE;
				g_bGeneratorUsesScreenBlanking = FALSE;
			}

			g_bGDIGeneratorRunning = FALSE;
		}

		g_pDataDocRunningThread = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CDataSetDoc

IMPLEMENT_DYNCREATE(CDataSetDoc, CDocument)

BEGIN_MESSAGE_MAP(CDataSetDoc, CDocument)
	//{{AFX_MSG_MAP(CDataSetDoc)
	ON_COMMAND(IDM_CONFIGURE_SENSOR, OnConfigureSensor)
	ON_COMMAND(IDM_CONFIGURE_SENSOR2, OnConfigureSensor2)
	ON_COMMAND(IDM_CONFIGURE_GENERATOR, OnConfigureGenerator)
	ON_COMMAND(IDM_CHANGE_GENERATOR, OnChangeGenerator)
	ON_COMMAND(IDM_CHANGE_SENSOR, OnChangeSensor)
	ON_COMMAND(IDM_EXPORT_XLS, OnExportXls)
	ON_COMMAND(IDM_EXPORT_CSV, OnExportCsv)
	ON_COMMAND(IDM_EXPORT_PDF, OnExportPdf)
	ON_UPDATE_COMMAND_UI(IDM_EXPORT_XLS, OnUpdateExportXls)
	ON_COMMAND(IDM_CALIBRATION_SIM, OnCalibrationSim)
	ON_COMMAND(IDM_CALIBRATION_MANUAL, OnCalibrationManual)
	ON_COMMAND(IDM_CALIBRATION_EXISTING, OnCalibrationExisting)
	ON_COMMAND(IDM_CALIBRATION_SPECTRAL, OnCalibrationSpectralSample)
	ON_COMMAND(IDM_SIM_GRAYSCALE, OnSimGrayscale)
	ON_COMMAND(IDM_SIM_PRIMARIES, OnSimPrimaries)
	ON_COMMAND(IDM_SIM_SECONDARIES, OnSimSecondaries)
	ON_COMMAND(IDM_SIM_GRAYSCALE_AND_COLORS, OnSimGrayscaleAndColors)
	ON_COMMAND(IDM_PATTERN_ANIM_BLACK, OnPatternAnimBlack)
	ON_COMMAND(IDM_PATTERN_ANIM_WHITE, OnPatternAnimWhite)
	ON_COMMAND(IDM_PATTERN_GRADIENT, OnPatternGradient)
	ON_COMMAND(IDM_PATTERN_GRADIENT2, OnPatternGradient2)
	ON_COMMAND(IDM_PATTERN_RG, OnPatternRG)
	ON_COMMAND(IDM_PATTERN_RB, OnPatternRB)
	ON_COMMAND(IDM_PATTERN_GB, OnPatternGB)
	ON_COMMAND(IDM_PATTERN_RGd, OnPatternRGd)
	ON_COMMAND(IDM_PATTERN_RBd, OnPatternRBd)
	ON_COMMAND(IDM_PATTERN_GBd, OnPatternGBd)
	ON_COMMAND(IDM_PATTERN_BN, OnPatternBN)
	ON_COMMAND(IDM_PATTERN_LRAMP, OnPatternLramp)
	ON_COMMAND(IDM_PATTERN_GRANGER, OnPatternGranger)
	ON_COMMAND(IDM_PATTERN_80, OnPattern80)
	ON_COMMAND(IDM_PATTERN_TV, OnPatternTV)
	ON_COMMAND(IDM_PATTERN_SPECTRUM, OnPatternSpectrum)
	ON_COMMAND(IDM_PATTERN_SRAMP, OnPatternSramp)
	ON_COMMAND(IDM_PATTERN_VSMPTE, OnPatternVSMPTE)
	ON_COMMAND(IDM_PATTERN_ERAMP, OnPatternEramp)
	ON_COMMAND(IDM_PATTERN_TC0, OnPatternTC0)
	ON_COMMAND(IDM_PATTERN_TC1, OnPatternTC1)
	ON_COMMAND(IDM_PATTERN_TC2, OnPatternTC2)
	ON_COMMAND(IDM_PATTERN_TC3, OnPatternTC3)
	ON_COMMAND(IDM_PATTERN_TC4, OnPatternTC4)
	ON_COMMAND(IDM_PATTERN_DR0, OnPatternDR0)
	ON_COMMAND(IDM_PATTERN_DR1, OnPatternDR1)
	ON_COMMAND(IDM_PATTERN_DR2, OnPatternDR2)
	ON_COMMAND(IDM_PATTERN_TC5, OnPatternTC5)
	ON_COMMAND(IDM_PATTERN_ALIGN, OnPatternAlign)
	ON_COMMAND(IDM_PATTERN_SHARP, OnPatternSharp)
	ON_COMMAND(IDM_PATTERN_TV, OnPatternTV)
	ON_COMMAND(IDM_PATTERN_CLIPH, OnPatternClipH)
	ON_COMMAND(IDM_PATTERN_CLIPL, OnPatternClipL)
	ON_COMMAND(IDM_PATTERN_TESTIMG, OnPatternTestimg)
	ON_COMMAND(IDM_SINGLE_MEASUREMENT, OnSingleMeasurement)
	ON_COMMAND(IDM_CONTINUOUS_MEASUREMENT, OnContinuousMeasurement)
	ON_COMMAND(IDM_MEASURE_GRAYSCALE, OnMeasureGrayscale)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_GRAYSCALE, OnUpdateMeasureGrayscale)
	ON_COMMAND(IDM_MEASURE_PRIMARIES, OnMeasurePrimaries)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_PRIMARIES, OnUpdateMeasurePrimaries)
	ON_COMMAND(IDM_MEASURE_SECONDARIES, OnMeasureSecondaries)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SECONDARIES, OnUpdateMeasureSecondaries)
	ON_UPDATE_COMMAND_UI(IDM_CONTINUOUS_MEASUREMENT, OnUpdateContinuousMeasurement)
	ON_COMMAND(IDM_MEASURE_DEFINESCALE, OnMeasureDefinescale)
	ON_COMMAND(IDM_MEASURE_NEARBLACK, OnMeasureNearblack)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_NEARBLACK, OnUpdateMeasureNearblack)
	ON_COMMAND(IDM_MEASURE_NEARWHITE, OnMeasureNearwhite)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_NEARWHITE, OnUpdateMeasureNearwhite)
	ON_COMMAND(IDM_MEASURE_SAT_RED, OnMeasureSatRed)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_RED, OnUpdateMeasureSatRed)
	ON_COMMAND(IDM_MEASURE_SAT_GREEN, OnMeasureSatGreen)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_GREEN, OnUpdateMeasureSatGreen)
	ON_COMMAND(IDM_MEASURE_SAT_BLUE, OnMeasureSatBlue)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_BLUE, OnUpdateMeasureSatBlue)
	ON_COMMAND(IDM_MEASURE_SAT_YELLOW, OnMeasureSatYellow)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_YELLOW, OnUpdateMeasureSatYellow)
	ON_COMMAND(IDM_MEASURE_SAT_CYAN, OnMeasureSatCyan)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_CYAN, OnUpdateMeasureSatCyan)
	ON_COMMAND(IDM_MEASURE_SAT_MAGENTA, OnMeasureSatMagenta)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_MAGENTA, OnUpdateMeasureSatMagenta)
	ON_COMMAND(IDM_MEASURE_SAT_CC24, OnMeasureSatCC24)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_CC24, OnUpdateMeasureSatCC24)
	ON_COMMAND(IDM_MEASURE_CONTRAST, OnMeasureContrast)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_CONTRAST, OnUpdateMeasureContrast)
	ON_COMMAND(IDM_MEASURE_SAT_ALL, OnMeasureSatAll)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_ALL, OnUpdateMeasureSatAll)
	ON_COMMAND(IDM_MEASURE_GRAYSCALE_COLORS, OnMeasureGrayscaleColors)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_GRAYSCALE_COLORS, OnUpdateMeasureGrayscaleColors)
	ON_COMMAND(ID_MEASURES_FULLTILTBOOGIE, OnMeasureFullTiltBoogie)
	ON_UPDATE_COMMAND_UI(ID_MEASURES_FULLTILTBOOGIE, OnUpdateMeasureFullTiltBoogie)
	ON_COMMAND(IDM_SIM_NEARBLACK, OnSimNearblack)
	ON_COMMAND(IDM_SIM_NEARWHITE, OnSimNearwhite)
	ON_COMMAND(IDM_SIM_SAT_RED, OnSimSatRed)
	ON_COMMAND(IDM_SIM_SAT_GREEN, OnSimSatGreen)
	ON_COMMAND(IDM_SIM_SAT_BLUE, OnSimSatBlue)
	ON_COMMAND(IDM_SIM_SAT_YELLOW, OnSimSatYellow)
	ON_COMMAND(IDM_SIM_SAT_CYAN, OnSimSatCyan)
	ON_COMMAND(IDM_SIM_SAT_MAGENTA, OnSimSatMagenta)
	ON_COMMAND(IDM_SIM_SINGLEMEASURE, OnSimSingleMeasurement)
	ON_COMMAND(IDM_MEASURE_SAT_PRIMARIES, OnMeasureSatPrimaries)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_PRIMARIES, OnUpdateMeasureSatPrimaries)
	ON_COMMAND(IDM_MEASURE_SAT_PRIMARIES_SECONDARIES, OnMeasureSatPrimariesSecondaries)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_PRIMARIES_SECONDARIES, OnUpdateMeasureSatPrimariesSecondaries)
	ON_COMMAND(IDM_SAVE_CALIBRATION_FILE, OnSaveCalibrationFile)
	ON_COMMAND(IDM_MANUALLY_EDIT_SENSOR, OnConfigureSensor ) 
	ON_COMMAND(IDM_LOAD_CALIBRATION_FILE, OnLoadCalibrationFile ) 
	ON_UPDATE_COMMAND_UI(IDM_SAVE_CALIBRATION_FILE, OnUpdateSaveCalibrationFile)
	ON_UPDATE_COMMAND_UI(IDM_LOAD_CALIBRATION_FILE, OnUpdateLoadCalibrationFile)
	ON_UPDATE_COMMAND_UI(IDM_MANUALLY_EDIT_SENSOR, OnUpdateLoadCalibrationFile)
	ON_UPDATE_COMMAND_UI(IDM_CALIBRATION_MANUAL, OnUpdateLoadCalibrationFile)
	ON_UPDATE_COMMAND_UI(IDM_CALIBRATION_EXISTING, OnUpdateLoadCalibrationFile)
	ON_UPDATE_COMMAND_UI(IDM_CALIBRATION_SIM, OnUpdateLoadCalibrationFile)
	ON_UPDATE_COMMAND_UI(IDM_CALIBRATION_SPECTRAL, OnUpdateLoadCalibrationFile)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDataSetDoc construction/destruction

CDataSetDoc::CDataSetDoc()
{
	m_pSensor = NULL;
	m_pGenerator = NULL;
	m_SelectedColor = noDataColor;
	m_pWndPos = NULL;
	m_pFramePosInfo = NULL;
}

CDataSetDoc::~CDataSetDoc()
{
	if ( m_pWndPos )
		delete m_pWndPos;
	m_pWndPos = NULL;
}

void CDataSetDoc::UpdateFrameCounts()
{
	// This function is globally identical to CDocument one, except it does not take into account CSubFrame
	
	// walk all frames of views (mark and sweep approach)
	POSITION pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		ASSERT_VALID(pView);
		ASSERT(::IsWindow(pView->m_hWnd));
		if (pView->IsWindowVisible())   // Do not count invisible windows.
		{
			CFrameWnd* pFrame = pView->GetParentFrame();
			if (pFrame != NULL)
				pFrame->m_nWindow = -1;     // unknown
		}
	}

	// now do it again counting the unique ones
	int nFrames = 0;
	pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		ASSERT_VALID(pView);
		ASSERT(::IsWindow(pView->m_hWnd));
		if (pView->IsWindowVisible())   // Do not count invisible windows.
		{
			CFrameWnd* pFrame = pView->GetParentFrame();
			
			// Ignore CSubFrames
			if ( pFrame -> IsKindOf ( RUNTIME_CLASS(CSubFrame) ) )
				pFrame = NULL;

			if (pFrame != NULL && pFrame->m_nWindow == -1)
			{
				ASSERT_VALID(pFrame);
				// not yet counted (give it a 1 based number)
				pFrame->m_nWindow = ++nFrames;
			}
		}
	}

	// lastly walk the frames and update titles (assume same order)
	// go through frames updating the appropriate one
	int iFrame = 1;
	pos = GetFirstViewPosition();
	while (pos != NULL)
	{
		CView* pView = GetNextView(pos);
		ASSERT_VALID(pView);
		ASSERT(::IsWindow(pView->m_hWnd));
		if (pView->IsWindowVisible())   // Do not count invisible windows.
		{
			CFrameWnd* pFrame = pView->GetParentFrame();

			// Ignore CSubFrames
			if ( pFrame -> IsKindOf ( RUNTIME_CLASS(CSubFrame) ) )
				pFrame = NULL;

			if (pFrame != NULL && pFrame->m_nWindow == iFrame)
			{
				ASSERT_VALID(pFrame);
				if (nFrames == 1)
					pFrame->m_nWindow = 0;      // the only one of its kind
				pFrame->OnUpdateFrameTitle(TRUE);
				iFrame++;
			}
		}
	}
	ASSERT(iFrame == nFrames + 1);
}

void CDataSetDoc::CreateSensor(int aID)
{
	CString	Msg, Title;

	if(m_pSensor != NULL)
		delete m_pSensor;

	switch(aID)
	{
		case 0:
				m_pSensor=new CKiSensor();
				break;
		case 1:
				m_pSensor=new CSimulatedSensor();
				break;
#ifdef USE_NON_FREE_CODE
		case 2:
				m_pSensor=new CSpyderIISensor();
				break;
#endif
		case 3:
				m_pSensor=new CDTPSensor();
				break;
#ifdef USE_NON_FREE_CODE
		case 4:
				m_pSensor=new CEyeOneSensor();
				break;
		case 5:
				m_pSensor=new CMTCSSensor();
				break;
		case 6:
				m_pSensor=new CSpyder3Sensor();
				break;
#endif
		default:
            if(aID >= 7 || aID < 0)
            {
				m_pSensor=new CArgyllSensor((ArgyllMeterWrapper*)aID);
            }
            else
            {
				Msg.LoadString ( IDS_UNKNOWNSENSOR1 );
				Title.LoadString ( IDS_ERROR );
				GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
				m_pSensor=new CSimulatedSensor();
            }
	}
}

void CDataSetDoc::CreateGenerator(int aID)
{
	CString	Msg, Title;

	if(m_pGenerator != NULL)
		delete m_pGenerator;

	switch(aID)
	{
		case 0:
			m_pGenerator=new CGDIGenerator();
			break;
		case 1:
			m_pGenerator=new CKiGenerator();
			break;
		case 2:
			m_pGenerator=new CManualDVDGenerator();
			break;
		default:
			Msg.LoadString ( IDS_UNKNOWNGENERATOR1 );
			Title.LoadString ( IDS_ERROR );
			GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
			m_pGenerator=new CGDIGenerator();
	}
}

void CDataSetDoc::DuplicateSensor(CDataSetDoc* pDoc)
{
	CString     Msg, Title;
	CString     type = typeid(*(pDoc->m_pSensor)).name();

	if(m_pSensor != NULL)
		delete m_pSensor;

	if (type == typeid(CKiSensor).name())
		m_pSensor=new CKiSensor();
	else
		if (type == typeid(CSimulatedSensor).name())
			m_pSensor=new CSimulatedSensor();
		else
#ifdef USE_NON_FREE_CODE
			if (type == typeid(CSpyderIISensor).name())
				m_pSensor=new CSpyderIISensor();
			else
#endif
				if (type == typeid(CDTPSensor).name())
					m_pSensor=new CDTPSensor();
				else
#ifdef USE_NON_FREE_CODE
					if (type == typeid(CEyeOneSensor).name())
						m_pSensor=new CEyeOneSensor();
					else
						if (type == typeid(CMTCSSensor).name())
							m_pSensor=new CMTCSSensor();
						else
							if (type == typeid(CSpyder3Sensor).name())
								m_pSensor=new CSpyder3Sensor();
							else
#endif
							    if (type == typeid(CArgyllSensor).name())
								    m_pSensor=new CArgyllSensor();
							    else
							    {
								    Msg.LoadString ( IDS_UNKNOWNSENSOR1 );
								    Title.LoadString ( IDS_ERROR );
								    GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
								    m_pSensor=new CSimulatedSensor();
							    }

	m_pSensor->Copy(pDoc->m_pSensor);
}

void CDataSetDoc::DuplicateGenerator(CDataSetDoc* pDoc)
{
	CString	Msg, Title;
	CString	type = typeid((*pDoc->m_pGenerator)).name();

	if(m_pGenerator != NULL)
		delete m_pGenerator;

 	if (type == typeid(CGDIGenerator).name())
		m_pGenerator=new CGDIGenerator();
	else
		if (type == typeid(CKiGenerator).name())
			m_pGenerator=new CKiGenerator();
		else
			if (type == typeid(CManualDVDGenerator).name())
				m_pGenerator=new CManualDVDGenerator();
			else
			{
				Msg.LoadString ( IDS_UNKNOWNGENERATOR1 );
				Title.LoadString ( IDS_ERROR );
				GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
				m_pGenerator=new CGDIGenerator();
			}
	m_pGenerator->Copy(pDoc->m_pGenerator);
}

BOOL CDataSetDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	if ( g_bNewDocUseDDEInfo )
	{
		if ( ! g_NewDocName.IsEmpty () )
			SetTitle ( (LPCSTR) g_NewDocName );

		CreateGenerator(g_NewDocGeneratorID);
		CreateSensor(g_NewDocSensorID);

		if(m_pSensor==NULL || m_pGenerator==NULL)
			return FALSE;	// Something went wrong during creation

		if ( g_ThcFileName.IsEmpty () )
			m_pSensor->SetSensorMatrix(g_NewDocSensorMatrix);
		else
			m_pSensor->LoadCalibrationFile(g_ThcFileName);
	}
	else
	{
		if (!g_bNewDocIsDuplication)		
		{
			CNewDocWizard propSheet;
	
			if (propSheet.DoModal() == ID_WIZFINISH)
			{
				CreateGenerator(propSheet.m_Page1.GetCurrentID());

				if ( propSheet.m_Page1.GetCurrentID() == 1)
				{
					//HCFR Generator. If no IR profile is selected, display property page
					if (((CKiGenerator*)m_pGenerator)->GetIRProfileName() == "")
						m_pGenerator->Configure();
				}

				CreateSensor(propSheet.m_Page2.GetCurrentID());
				if(m_pSensor==NULL || m_pGenerator==NULL)
					return FALSE;	// Something went wrong during creation

				if ( propSheet.m_Page2.GetCurrentID() < 2)
				{
					// HCFR sensor or simulated sensor,
					if(propSheet.m_Page2.m_sensorTrainingMode == 1)
					{
						m_pSensor->Configure();
					}
					else
						m_pSensor->LoadCalibrationFile(propSheet.m_Page2.m_trainingFileName);
				}
                else if(propSheet.m_Page2.GetCurrentID() > 6)
                {
                    m_pSensor->Configure();
                    if(propSheet.m_Page2.m_sensorTrainingMode != 1)
                    {
                        m_pSensor->LoadCalibrationFile(propSheet.m_Page2.m_trainingFileName);
                    }
                }
				else
				{
					// Sensor not needing calibration
					if(propSheet.m_Page2.m_sensorTrainingMode != 1)
					{
#ifdef USE_NON_FREE_CODE
						if ( propSheet.m_Page2.GetCurrentID() == 4 ) 
						{
							// Eye one. When a calibration file is selected, cannot use Eye One Pro
							if ( ( (CEyeOneSensor*) m_pSensor ) -> m_CalibrationMode == 3 )
							{
								// If Eye One Pro is set, reset to Eye One Display in LCD mode before loading calibration file
								( (CEyeOneSensor*) m_pSensor ) -> m_CalibrationMode = 1;
							}
						}
#endif
						m_pSensor->LoadCalibrationFile(propSheet.m_Page2.m_trainingFileName);
					}
				}
			}
			else	// Default
			{
				return FALSE;
			}
		}
		else	//Document duplication
		{
			CDuplicateDocDlg Dlg;
			if( Dlg.DoModal() == IDOK )
			{
				DuplicateGenerator(g_DocToDuplicate);
				DuplicateSensor(g_DocToDuplicate);

				if (Dlg.m_DuplGrayLevelCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLGRAYLEVEL);
				if (Dlg.m_DuplNearBlackCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLNEARBLACK);
				if (Dlg.m_DuplNearWhiteCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLNEARWHITE);
				if (Dlg.m_DuplPrimariesSatCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLPRIMARIESSAT);
				if (Dlg.m_DuplSecondariesSatCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLSECONDARIESSAT);
				if (Dlg.m_DuplPrimariesColCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLPRIMARIESCOL);
				if (Dlg.m_DuplSecondariesColCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLSECONDARIESCOL);
				if (Dlg.m_DuplContrastCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLCONTRAST);
				if (Dlg.m_DuplInfoCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLINFO);
			}
			else	// Default
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CDataSetDoc serialization

void CDataSetDoc::Serialize(CArchive& ar)
{
	CString	Msg, Title;
	CDataSetWindowPositions * pWndPos;

	StopBackgroundMeasures ();

	if (ar.IsStoring())
	{
		// write the object header first so we can correctly recognise the object type "CMatrixC"
		long header1=0x4F4C4F43;	// header is "COLORHCF" in hexa
		long header2=0x46434852;
		int	 version = CURRENT_FILE_FORMAT_VERSION ;			// serialization version format number

		ar << header1;
		ar << header2;
		ar << version;

		// save Measure
		m_measure.Serialize(ar);

		// save Sensor & Generator
		ar << m_pSensor;
		ar << m_pGenerator;

		// Save views positions
		pWndPos = new CDataSetWindowPositions ( this );
		pWndPos -> Serialize ( ar );
		delete pWndPos;
	}
	else
	{
		// read the object header first so we can correctly recognise the object type "CMatrixC"
		long header1;
		long header2;
		int	 version = 0;						// serialization version format number

		ar >> header1;
		ar >> header2;
		ar >> version;

		if( header1 != 0x4F4C4F43 || header2 != 0x46434852 || version > CURRENT_FILE_FORMAT_VERSION )
		{
			Msg.LoadString ( IDS_INVALIDFILEFORMAT );
			Title.LoadString ( IDS_ERROR );
			GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
			return;
		}

		// Load Measure
		m_measure.Serialize(ar);

		// load Sensor & Generator

		CObject *pObj=ar.ReadObject(NULL);
		if(pObj->GetRuntimeClass()->IsDerivedFrom(RUNTIME_CLASS(CSensor)) )
        {
            m_pSensor=(CSensor *)pObj;
            if(!m_pSensor->isValid())
            {
                delete m_pSensor;
                m_pSensor = new CSimulatedSensor();
            }
        }
		else
		{
			Msg.LoadString ( IDS_UNKNOWNSENSOR2 );
			Title.LoadString ( IDS_ERROR );
			GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
			return;
		}
			
		pObj=ar.ReadObject(NULL);
		if(pObj->GetRuntimeClass()->IsDerivedFrom(RUNTIME_CLASS(CGenerator)) )
			m_pGenerator=(CGenerator *)pObj;
		else
		{
			Msg.LoadString ( IDS_UNKNOWNGENERATOR2 );
			Title.LoadString ( IDS_ERROR );
			GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
			return;
		}

		if ( version >= 3 )
		{
			m_pWndPos = new CDataSetWindowPositions;
			m_pWndPos -> Serialize ( ar );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// CDataSetDoc diagnostics

#ifdef _DEBUG
void CDataSetDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CDataSetDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDataSetDoc commands

void CDataSetDoc::SetModifiedFlag( BOOL bModified )
{
	CDocument::SetModifiedFlag(bModified);

	if(bModified == FALSE)
	{
		if(m_pSensor)
			m_pSensor->SetModifiedFlag(FALSE);
		if(m_pGenerator)
			m_pGenerator->SetModifiedFlag(FALSE);
	}

	// Update title of document views to show that doc was modified (an * is displayed at the end)
	POSITION pos = GetFirstViewPosition();	
	while (pos != NULL)
	{
	  CView* pView = GetNextView(pos);
	  if( pView->GetParent() )
		((CFrameWnd *)(pView->GetParent()))->OnUpdateFrameTitle(TRUE);
	}   
}

BOOL CDataSetDoc::IsModified( )
{
	BOOL docIsModified=CDocument::IsModified();
	if(m_pGenerator)
		docIsModified = docIsModified || m_pGenerator->IsModified();
	if(m_pSensor)
		docIsModified = docIsModified || m_pSensor->IsModified();

	return docIsModified || GetConfig()->m_bSave || GetConfig()->m_bSave2;
}

void CDataSetDoc::OnCloseDocument() 
{
	StopBackgroundMeasures ();

// If reference measure document is closed, clear reference measure
	if (GetDataRef() == this) {	//Ki
		UpdateDataRef(FALSE, this);
		
		AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
	
	delete m_pSensor;
	delete m_pGenerator;

	m_pSensor=NULL;
	m_pGenerator=NULL;

	CDocument::OnCloseDocument();
}

BOOL CDataSetDoc::CanCloseFrame(CFrameWnd* pFrame) 
{
	if ( this == g_pDataDocRunningThread )
		StopBackgroundMeasures ();

	return CDocument::CanCloseFrame(pFrame);
}

void CDataSetDoc::ShowViewOnTop(UINT nMsg)
{
	// TODO: OBSOLETE : remove this function, or put in in CMultiFrame
}


void CDataSetDoc::HideView(UINT nMsg)
{
	// TODO: OBSOLETE : remove this function, or put in in CMultiFrame
}


void CDataSetDoc::ShowAllViews() 
{
	// TODO: OBSOLETE : remove this function, or put in in CMultiFrame
}

void CDataSetDoc::HideAllViews() 
{
	// TODO: OBSOLETE : remove this function, or put in in CMultiFrame
}

void CDataSetDoc::EnsureVisibleViewMeasuresCombo()
{
	// TODO: OBSOLETE : use free measures in CMultiFrame, and use a tabbed button.
}

void CDataSetDoc::MeasureGrayScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureGrayScale(m_pSensor,m_pGenerator, this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_GRAYSCALE);
	}
}

void CDataSetDoc::MeasureGrayScaleAndColors() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureGrayScaleAndColors(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_GRAYSCALEANDCOLORS);
	}
}

void CDataSetDoc::MeasureNearBlackScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureNearBlackScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_NEARBLACK);
	}
}

void CDataSetDoc::MeasureNearWhiteScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureNearWhiteScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_NEARWHITE);
	}
}

void CDataSetDoc::MeasureRedSatScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureRedSatScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_REDSAT);
	}
}

void CDataSetDoc::MeasureGreenSatScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureGreenSatScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_GREENSAT);
	}
}

void CDataSetDoc::MeasureBlueSatScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureBlueSatScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_BLUESAT);
	}
}

void CDataSetDoc::MeasureYellowSatScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureYellowSatScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_YELLOWSAT);
	}
}

void CDataSetDoc::MeasureCyanSatScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureCyanSatScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_CYANSAT);
	}
}

void CDataSetDoc::MeasureMagentaSatScale() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureMagentaSatScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_MAGENTASAT);
	}
}

void CDataSetDoc::MeasureCC24SatScale() 
{
	StopBackgroundMeasures ();
	if(m_measure.MeasureCC24SatScale(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_CC24SAT);
	}
}

void CDataSetDoc::MeasureAllSaturationScales() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureAllSaturationScales(m_pSensor,m_pGenerator,FALSE,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_ALLSATURATIONS);
	}
}

void CDataSetDoc::MeasurePrimarySaturationScales() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasurePrimarySecondarySaturationScales(m_pSensor,m_pGenerator,TRUE,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_ALLSATURATIONS);
	}
}

void CDataSetDoc::MeasurePrimarySecondarySaturationScales() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasurePrimarySecondarySaturationScales(m_pSensor,m_pGenerator,FALSE,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_ALLSATURATIONS);
	}
}

void CDataSetDoc::MeasurePrimaries() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasurePrimaries(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_PRIMARIES);
	}
}

void CDataSetDoc::MeasureSecondaries() 
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureSecondaries(m_pSensor,m_pGenerator,this))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_SECONDARIES);
	}
}

void CDataSetDoc::MeasureContrast()
{
	StopBackgroundMeasures ();

	if(m_measure.MeasureContrast(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_CONTRAST);
	}
}

void CDataSetDoc::AddMeasurement() 
{
	StopBackgroundMeasures ();
    POSITION pos;
    CView* pView;
    pos = this -> GetFirstViewPosition ();
    pView = this -> GetNextView ( pos );
	int m_d;
	Settling=GetConfig()->m_isSettling;
	GetConfig()->m_isSettling = FALSE;
	m_d=((CMainView *)pView)->m_displayMode;
	CGenerator::MeasureType MT = CGenerator::MT_UNKNOWN;							
	if ( m_d == 1 || (m_d < 11 &&  m_d > 4) )
		MT = CGenerator::MT_ACTUAL;

	if(m_measure.AddMeasurement(m_pSensor,m_pGenerator, MT, m_d == 1))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_FREEMEASUREAPPENDED);
	}
	GetConfig()->m_isSettling = Settling;
}


void CDataSetDoc::OnConfigureSensor() 
{
	StopBackgroundMeasures ();
    m_pSensor->SetSensorMatrixMod( m_pSensor->GetSensorMatrix() );
	m_pSensor->Configure();
	if( m_pSensor->IsModified() )
	{
        //unapply old - apply new
        m_measure.ApplySensorAdjustmentMatrix( m_pSensor->GetSensorMatrixMod().GetInverse() );
		m_measure.ApplySensorAdjustmentMatrix(m_pSensor->GetSensorMatrix());
        m_pSensor->SetSensorMatrixMod( Matrix::IdentityMatrix(3) );
		SetModifiedFlag(TRUE);
		UpdateAllViews ( NULL, UPD_EVERYTHING );
		AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );

	}
}

void CDataSetDoc::OnConfigureSensor2() 
{
    POSITION pos;
    CView* pView;
    pos = this -> GetFirstViewPosition ();
    pView = this -> GetNextView ( pos );
    if (m_pSensor->setAvg())
    {
       GetColorApp()->InMeasureMessageBox("Averaging on","Average Button",MB_OK);
       ( (CMainView*) pView )->m_configSensorButton2.SetIcon(IDI_STOP_ICON,18,18);
    }
    else
    {
       GetColorApp()->InMeasureMessageBox("Averaging off","Average Button",MB_OK);
       ( (CMainView*) pView )->m_configSensorButton2.SetIcon(IDI_START_ICON,18,18);
    }


}

void CDataSetDoc::OnConfigureGenerator() 
{
	StopBackgroundMeasures ();

	m_pGenerator->Configure();
	if( m_pGenerator->IsModified() )
		SetModifiedFlag(TRUE);
}


BOOL CDataSetDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;
	
	if(m_pSensor != NULL  && m_pGenerator != NULL )	
	{
		CString string = (CString)lpszPathName;
		CString string2,name;
		int	lastpos,pos = 0;
		int filenamepos = 0;
		int extensionpos = 0;

		if (! string.IsEmpty())
		{
			while (pos != -1)
			{
				lastpos = pos ;
				pos = string.Find('\\', lastpos + 1);
			}
			string2 = string.Right(string.GetLength()-lastpos-1);

			pos = 0 ;
			while (pos != -1)
			{
				lastpos = pos;
				pos = string2.Find('.', lastpos + 1);
			}
			name = string2.Left(lastpos);
			SetTitle(name);
		}
		return TRUE;
	}
	else
		return FALSE;
}

void CDataSetDoc::OnChangeGenerator() 
{
	StopBackgroundMeasures ();

	CPropertySheetWithHelp propSheet;
	CGeneratorSelectionPropPage page;
	propSheet.AddPage(&page);
	propSheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;
	
	CString	Title;
	Title.LoadString ( IDM_CHANGE_GENERATOR );
	propSheet.SetTitle ( Title );

	page.m_generatorChoice=m_pGenerator->GetName(); // default selection is current generator
	if( propSheet.DoModal() == IDOK )
	{
		CreateGenerator(page.GetCurrentID());
		UpdateAllViews(NULL, UPD_GENERATORCONFIG);	// To update generator name displayed
	}
}

void CDataSetDoc::OnChangeSensor() 
{
	CString		Msg, Title;

	StopBackgroundMeasures ();

	Msg.LoadString ( IDS_CONFIRMSENSORCHANGE );
	Title.LoadString ( IDS_SENSOR );
	if(GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONQUESTION | MB_YESNO) != IDYES)
		return;

	CPropertySheetWithHelp propSheet;
	CSensorSelectionPropPage page;
	propSheet.AddPage(&page);
	propSheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	Title.LoadString ( IDM_CHANGE_SENSOR );
	propSheet.SetTitle ( Title );

	page.m_sensorChoice=m_pSensor->GetName(); 	// default selection is current sensor
	if( propSheet.DoModal() == IDOK )
	{
		CreateSensor(page.GetCurrentID());
		if(m_pSensor==NULL)
			return;	// Something went wrong during creation

		if(page.m_sensorTrainingMode != 1)
			m_pSensor->LoadCalibrationFile(page.m_trainingFileName);

		UpdateAllViews(NULL, UPD_SENSORCONFIG);	// To update sensor name displayed
		SetModifiedFlag(TRUE);
	}
}

void CDataSetDoc::OnUpdateExportXls(CCmdUI* pCmdUI) 
{
	static bool isEnabled=IsExcelDriverInsalled();
	pCmdUI->Enable(isEnabled);
}

void CDataSetDoc::OnExportXls() 
{
	StopBackgroundMeasures ();

	CExport exportVar(this,CExport::XLS);
	exportVar.Save();
}

void CDataSetDoc::OnExportPdf() 
{
	StopBackgroundMeasures ();

	CExport exportVar(this,CExport::PDF);
	exportVar.Save();
}

void CDataSetDoc::OnExportCsv() 
{
	StopBackgroundMeasures ();

	CExport exportVar(this,CExport::CSV);
	exportVar.Save();
}

BOOL CDataSetDoc::OnSaveDocument(LPCTSTR lpszPathName) 
{
	// TODO: Add your specialized code here and/or call the base class
	BOOL	return_value = CDocument::OnSaveDocument(lpszPathName);


	if (return_value)
	{
		CString string = (CString)lpszPathName;
		CString string2,name;
		int	lastpos,pos = 0;
		int filenamepos = 0;
		int extensionpos = 0;

		if (! string.IsEmpty())
		{
			while (pos != -1)
			{
				lastpos = pos ;
				pos = string.Find('\\', lastpos + 1);
			}
			string2 = string.Right(string.GetLength()-lastpos-1);

			pos = 0 ;
			while (pos != -1)
			{
				lastpos = pos;
				pos = string2.Find('.', lastpos + 1);
			}
			name = string2.Left(lastpos);
			SetTitle(name);
		}
	}
	return return_value;
}

void CDataSetDoc::OnCalibrationSim() 
{
	CString	Msg, strMsg, Title;
	CDataSetDoc *pDataRef = GetDataRef();

	if ( pDataRef == NULL )
	{
		// No data ref.
		Msg.LoadString ( IDS_SIM_CAL_ERROR1 );
		Title.LoadString ( IDS_ERROR );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
		return;
	}

	if ( pDataRef == this )
	{
		// Ref document cannot be current document
		Msg.LoadString ( IDS_SIM_CAL_ERROR3 );
		Title.LoadString ( IDS_ERROR );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
		return;
	}

	Msg.LoadString ( IDS_RUN_SIM_CALIBRATION );
	if ( IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_YESNO | MB_ICONQUESTION ) )
	{
		// Use special simultaneous mode
		PerformSimultaneousMeasures ( -5 );
        ComputeAdjustmentMatrix();
		// Save file
		strMsg.LoadString ( IDS_SAVECALDATA );
		Title.LoadString ( IDS_CALIBRATION );
		if(GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
			m_pSensor->SaveCalibrationFile();
	}
}

void CDataSetDoc::OnCalibrationExisting() 
{
	CString	Msg, Title;
	CDataSetDoc *pDataRef = GetDataRef();

	if ( pDataRef == NULL )
	{
		// No data ref.
		Msg.LoadString ( IDS_SIM_CAL_ERROR1 );
		Title.LoadString ( IDS_ERROR );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
		return;
	}

	if ( pDataRef == this )
	{
		// Ref document cannot be current document
		Msg.LoadString ( IDS_SIM_CAL_ERROR3 );
		Title.LoadString ( IDS_ERROR );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
		return;
	}

	bool Red = (this->GetMeasure()->GetRedPrimary().isValid() && pDataRef->GetMeasure()->GetRedPrimary().isValid()) ;

	if (!Red)
    {
        //No measurements
		Msg.LoadString ( IDS_SIM_CAL_ERROR4 );
		Title.LoadString ( IDS_ERROR );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
		return;
    }

    ComputeAdjustmentMatrix();
	// Save file
	Msg.LoadString ( IDS_SAVECALDATA );
	Title.LoadString ( IDS_CALIBRATION );
	if(GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
		m_pSensor->SaveCalibrationFile();

}

void CDataSetDoc::OnCalibrationManual()
{
	int		i;
	MSG		Msg;
	BOOL	bEscape, bReturn;
	CColor	measuredColor[4];
	CString	strMsg, Title;

	if ( IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_RUN_MANUAL_CALIBRATION), "Manual Calibration", MB_YESNO | MB_ICONQUESTION ) )
	{
		if(m_pGenerator->Init(5) != TRUE)
		{
			Title.LoadString ( IDS_ERROR );
			strMsg.LoadString ( IDS_ERRINITGENERATOR );
			GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
			return;
		}

		if(m_pSensor->Init(FALSE) != TRUE)
		{
			Title.LoadString ( IDS_ERROR );
			strMsg.LoadString ( IDS_ERRINITSENSOR );
			GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
			m_pGenerator->Release();
			return;
		}

		double primaryIRELevel=100.0;	
		CColor measure;
		
		// Measure red primary 
		for ( i = 0; i < 1 ; i ++ )
		{
			if( m_pGenerator->DisplayRGBColor(ColorRGBDisplay(primaryIRELevel,0,0),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(ColorRGBDisplay(primaryIRELevel,0,0));
				
				while ( ! bEscape && ! bReturn )
				{
					while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
					{
						if ( Msg.message == WM_KEYDOWN )
						{
							if ( Msg.wParam == VK_ESCAPE )
								bEscape = TRUE;
							else if ( Msg.wParam == VK_RETURN )
								bReturn = TRUE;
						}
					}
					Sleep(10);
				}

				if ( bEscape )
				{
					m_pSensor->Release();
					m_pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return;
				}

				if(!m_pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+m_pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						m_pSensor->Release();
						m_pGenerator->Release();
						return;
					}
					if(result == IDRETRY)
						i--;
				}
				else
					measuredColor[0]=measure;
			}
			else
			{
				m_pSensor->Release();
				m_pGenerator->Release();
				return;
			}
		}

		// Measure green primary 
		for ( i = 0; i < 1 ; i ++ )
		{
			if( m_pGenerator->DisplayRGBColor(ColorRGBDisplay(0,primaryIRELevel,0),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(ColorRGBDisplay(0,primaryIRELevel,0));
				
				while ( ! bEscape && ! bReturn )
				{
					while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
					{
						if ( Msg.message == WM_KEYDOWN )
						{
							if ( Msg.wParam == VK_ESCAPE )
								bEscape = TRUE;
							else if ( Msg.wParam == VK_RETURN )
								bReturn = TRUE;
						}
					}
					Sleep(10);
				}

				if ( bEscape )
				{
					m_pSensor->Release();
					m_pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return;
				}

				if(!m_pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+m_pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						m_pSensor->Release();
						m_pGenerator->Release();
						return;
					}
					if(result == IDRETRY)
						i--;
				}
				else
					measuredColor[1]=measure;
			}
			else
			{
				m_pSensor->Release();
				m_pGenerator->Release();
				return;
			}
		}

		// Measure blue primary 
		for ( i = 0; i < 1 ; i ++ )
		{
			if( m_pGenerator->DisplayRGBColor(ColorRGBDisplay(0,0,primaryIRELevel),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(ColorRGBDisplay(0,0,primaryIRELevel));
				
				while ( ! bEscape && ! bReturn )
				{
					while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
					{
						if ( Msg.message == WM_KEYDOWN )
						{
							if ( Msg.wParam == VK_ESCAPE )
								bEscape = TRUE;
							else if ( Msg.wParam == VK_RETURN )
								bReturn = TRUE;
						}
					}
					Sleep(10);
				}

				if ( bEscape )
				{
					m_pSensor->Release();
					m_pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return;
				}

				if(!m_pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+m_pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						m_pSensor->Release();
						m_pGenerator->Release();
						return;
					}
					if(result == IDRETRY)
						i--;
				}
				else
					measuredColor[2]=measure;
			}
			else
			{
				m_pSensor->Release();
				m_pGenerator->Release();
				return;
			}
		}

		// Measure white reference
		for ( i = 0; i < 1 ; i ++ )
		{
			if( m_pGenerator->DisplayRGBColor(ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel));
				
				while ( ! bEscape && ! bReturn )
				{
					while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
					{
						if ( Msg.message == WM_KEYDOWN )
						{
							if ( Msg.wParam == VK_ESCAPE )
								bEscape = TRUE;
							else if ( Msg.wParam == VK_RETURN )
								bReturn = TRUE;
						}
					}
					Sleep(10);
				}

				if ( bEscape )
				{
					m_pSensor->Release();
					m_pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return;
				}

				if(!m_pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+m_pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						m_pSensor->Release();
						m_pGenerator->Release();
						return;
					}
					if(result == IDRETRY)
						i--;
				}
				else
					measuredColor[3]=measure;
			}
			else
			{
				m_pSensor->Release();
				m_pGenerator->Release();
				return;
			}
		}

		m_pSensor->Release();
		m_pGenerator->Release();

        CRefColorDlg dlg;
		if ( dlg.DoModal () == IDOK )
		{
			// Create calibration data
            ColorXYZ measures[3] = {
                                        dlg.m_RedColor.GetXYZValue(),
                                        dlg.m_GreenColor.GetXYZValue(),
                                        dlg.m_BlueColor.GetXYZValue()
                                    };
            ColorXYZ references[3] = {
                                        measuredColor[0].GetXYZValue(),
                                        measuredColor[1].GetXYZValue(),
                                        measuredColor[2].GetXYZValue()
                                    };

            ColorXYZ whiteRef = dlg.m_WhiteColor.GetXYZValue();

            ColorXYZ white(measuredColor[3].GetXYZValue());
            Matrix oldMatrix = m_pSensor->GetSensorMatrix();
            Matrix ConvMatrix = ComputeConversionMatrix (measures, references, white, whiteRef, GetConfig () -> m_bUseOnlyPrimaries );

        	// check that matrix is inversible
	        if ( ConvMatrix.Determinant() != 0.0 )
	        {
		        // Ok: set adjustment matrix
                Matrix newMatrix = ConvMatrix * oldMatrix;
		        m_measure.ApplySensorAdjustmentMatrix( ConvMatrix );
                m_pSensor->SetSensorMatrix(newMatrix);
		        m_pSensor->SetSensorMatrixMod(Matrix::IdentityMatrix(3));
		        SetModifiedFlag(TRUE);
	        }
	        else
	        {
		        GetColorApp()->InMeasureMessageBox( _S(IDS_INVALIDMEASUREMATRIX), "Invalide Matrix", MB_OK | MB_ICONERROR );
	        }

	        AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );

            SetModifiedFlag ();

			UpdateAllViews ( NULL, UPD_EVERYTHING );

			// Save file
			strMsg.LoadString ( IDS_SAVECALDATA );
			Title.LoadString ( IDS_CALIBRATION );
			if(GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
				m_pSensor->SaveCalibrationFile();
		} 
	}
    
}

void CDataSetDoc::OnCalibrationSpectralSample() 
{
	int		i;
	MSG		Msg;
	BOOL	bEscape, bReturn;
	CColor	measuredColor[4];
	CString	strMsg, Title;

	if (m_pSensor->isColorimeter())
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString(IDS_SPECTRAL_SAMPLE_USE_SPECTRO);
		GetColorApp()->InMeasureMessageBox( strMsg, Title ,MB_ICONERROR | MB_OK);  
		return;
	}

	CString displayName = m_pGenerator->GetActiveDisplayName();

	if ( IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_SPECTRAL_SAMPLE_CREATE), "Create Spectral Sample", MB_YESNO | MB_ICONQUESTION ) )
	{
		if(m_pGenerator->Init(5) != TRUE)
		{
			Title.LoadString ( IDS_ERROR );
			strMsg.LoadString ( IDS_ERRINITGENERATOR );
			GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
			return;
		}

		if(m_pSensor->Init(FALSE) != TRUE)
		{
			Title.LoadString ( IDS_ERROR );
			strMsg.LoadString ( IDS_ERRINITSENSOR );
			GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
			m_pGenerator->Release();
			return;
		}

		double primaryIRELevel=100.0;	
		CColor measure;
		
		// Measure red primary 
		for ( i = 0; i < 1 ; i ++ )
		{
			if( m_pGenerator->DisplayRGBColor(ColorRGBDisplay(primaryIRELevel,0,0),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(ColorRGBDisplay(primaryIRELevel,0,0));
				
				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					m_pSensor->Release();
					m_pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return;
				}

				if(!m_pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+m_pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						m_pSensor->Release();
						m_pGenerator->Release();
						return;
					}
					if(result == IDRETRY)
						i--;
				}
				else
					measuredColor[0]=measure;
			}
			else
			{
				m_pSensor->Release();
				m_pGenerator->Release();
				return;
			}
		}

		// Measure green primary 
		for ( i = 0; i < 1 ; i ++ )
		{
			if( m_pGenerator->DisplayRGBColor(ColorRGBDisplay(0,primaryIRELevel,0),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(ColorRGBDisplay(0,primaryIRELevel,0));
				
				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					m_pSensor->Release();
					m_pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return;
				}

				if(!m_pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+m_pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						m_pSensor->Release();
						m_pGenerator->Release();
						return;
					}
					if(result == IDRETRY)
						i--;
				}
				else
					measuredColor[1]=measure;
			}
			else
			{
				m_pSensor->Release();
				m_pGenerator->Release();
				return;
			}
		}

		// Measure blue primary 
		for ( i = 0; i < 1 ; i ++ )
		{
			if( m_pGenerator->DisplayRGBColor(ColorRGBDisplay(0,0,primaryIRELevel),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(ColorRGBDisplay(0,0,primaryIRELevel));
				
				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					m_pSensor->Release();
					m_pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return;
				}

				if(!m_pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+m_pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						m_pSensor->Release();
						m_pGenerator->Release();
						return;
					}
					if(result == IDRETRY)
						i--;
				}
				else
					measuredColor[2]=measure;
			}
			else
			{
				m_pSensor->Release();
				m_pGenerator->Release();
				return;
			}
		}

		// Measure white reference
		for ( i = 0; i < 1 ; i ++ )
		{
			if( m_pGenerator->DisplayRGBColor(ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel));
				
				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					m_pSensor->Release();
					m_pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return;
				}

				if(!m_pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+m_pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						m_pSensor->Release();
						m_pGenerator->Release();
						return;
					}
					if(result == IDRETRY)
						i--;
				}
				else
					measuredColor[3]=measure;
			}
			else
			{
				m_pSensor->Release();
				m_pGenerator->Release();
				return;
			}
		}

		const char* FILE_SEPARATOR = "\\";			
		const char* FILE_PREPEND = "\\color\\";			
		std::string savePath = getenv("APPDATA");
		savePath += FILE_PREPEND;
		CFileDialog fileSaveDialog( FALSE, "ccss", NULL, OFN_HIDEREADONLY, "Colorimeter Calibration Spectral Sample (*.ccss)|*.ccss||" );
		fileSaveDialog.m_ofn.lpstrInitialDir = savePath.c_str();
		CSpectralSampleDlg spectralSampleDialog(displayName);

		if(spectralSampleDialog.DoModal() == IDOK && fileSaveDialog.DoModal() == IDOK)
		{
			savePath = fileSaveDialog.GetFolderPath();
			std::string saveFilename = savePath + FILE_SEPARATOR + (LPCSTR)fileSaveDialog.GetFileName();
			try
			{	
				SpectralSample ss;
				ss.setDisplay(spectralSampleDialog.m_displayName);
				ss.setTech(spectralSampleDialog.m_displayTech);
				ss.setReferenceInstrument(m_pSensor->GetName());
				
				// Ignore return values as any error will throw an exception	

#if defined(LIBHCFR_HAS_MFC) && defined(_DEBUG)

				std::string saveReadings = savePath + (LPCSTR)fileSaveDialog.GetFileTitle() + ".xyz";
				CFile fileReadings;
				if( fileReadings.Open(saveReadings.c_str(), CFile::modeCreate|CFile::modeWrite))
				{
					CArchive ar(&fileReadings, CArchive::store);
					for (int i = 0; i < 4; i++)
					{
						measuredColor[i].Serialize(ar);
					}
					ar.Close();
					fileReadings.Close();
				}
#endif
				(void)ss.createFromMeasurements(measuredColor, 4); 
				(void)ss.Write(saveFilename);
			}
			catch (std::logic_error& e)
			{
				Title.LoadString(IDS_ERROR);
				GetColorApp()->InMeasureMessageBox( e.what(), Title, MB_ICONERROR | MB_OK);
			}
			catch (...)
			{
				Title.LoadString(IDS_ERROR);
				strMsg.LoadString(IDS_SPECTRAL_SAMPLE_UNEXPECTED_EXCEPTION);
				GetColorApp()->InMeasureMessageBox( strMsg, Title, MB_ICONERROR | MB_OK); 
			}
		}
		
		m_pSensor->Release();
		m_pGenerator->Release();
	}
}

BOOL CDataSetDoc::ComputeAdjustmentMatrix() 
{
	BOOL			bOk = FALSE;
	CDataSetDoc *	pDataRef = GetDataRef();
	
	ASSERT ( pDataRef && pDataRef != this && pDataRef -> m_measure.GetBluePrimary ().isValid() && m_measure.GetBluePrimary ().isValid());

        ColorXYZ measures[3] = {
                                    m_measure.GetRedPrimary().GetXYZValue(),
                                    m_measure.GetGreenPrimary().GetXYZValue(),
                                    m_measure.GetBluePrimary().GetXYZValue()
                                };
        ColorXYZ references[3] = {
                                    pDataRef->m_measure.GetRedPrimary().GetXYZValue(),
                                    pDataRef->m_measure.GetGreenPrimary().GetXYZValue(),
                                    pDataRef->m_measure.GetBluePrimary().GetXYZValue()
                                };

    ColorXYZ whiteRef = pDataRef -> m_measure.GetPrimeWhite().GetXYZValue();

    ColorXYZ white = m_measure.GetPrimeWhite().GetXYZValue();

    // check that measure matrix is inversible
    Matrix oldMatrix = m_pSensor->GetSensorMatrix();
    Matrix ConvMatrix = ComputeConversionMatrix (measures, references, white, whiteRef, GetConfig () -> m_bUseOnlyPrimaries );

	// check that matrix is inversible
	if ( ConvMatrix.Determinant() != 0.0 )
	{
		// Ok: set adjustment matrix
        Matrix newMatrix = ConvMatrix * oldMatrix;
		m_measure.ApplySensorAdjustmentMatrix( ConvMatrix );
        m_pSensor->SetSensorMatrix(newMatrix);
		m_pSensor->SetSensorMatrixMod(Matrix::IdentityMatrix(3));
		SetModifiedFlag(TRUE);
		bOk = TRUE;
	}
	else
	{
		        GetColorApp()->InMeasureMessageBox( _S(IDS_INVALIDMEASUREMATRIX), "Invalide Matrix", MB_OK | MB_ICONERROR );
	}
	this->UpdateAllViews ( NULL, UPD_EVERYTHING );
	AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );

	return bOk;
}

void CDataSetDoc::OnSimGrayscale() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( (CString) (LPCTSTR)IDS_MEASUREGRAYSCALE_SIM, "Measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 0 );
}

void CDataSetDoc::OnSimGrayscaleAndColors() 
{
	if ( m_pGenerator -> CanDisplayGrayAndColorsSeries () )
	{
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASUREGRAYANDCOLORS), "Measure",MB_YESNO | MB_ICONQUESTION ) )
			PerformSimultaneousMeasures ( -3 );
	}
	else
	{
		CString	Msg;
		Msg.Format ( IDS_CANNOTMEASUREGRAYANDCOLORS, (LPCSTR) m_pGenerator -> GetName () );
		GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnSimPrimaries() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASUREPRIMARIES_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( -1 );
}

void CDataSetDoc::OnSimSecondaries() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURESECONDARIES_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( -2 );
}

void CDataSetDoc::OnSimNearblack() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURENEARBLACK_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 1 );
}

void CDataSetDoc::OnSimNearwhite() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURENEARWHITE_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 2 );
}

void CDataSetDoc::OnSimSatRed() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURESATRED_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 3 );
}

void CDataSetDoc::OnSimSatGreen() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURESATGREEN_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 4 );
}

void CDataSetDoc::OnSimSatBlue() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURESATBLUE_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 5 );
}

void CDataSetDoc::OnSimSatYellow() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURESATYELLOW_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 6 );
}

void CDataSetDoc::OnSimSatCyan() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURESATCYAN_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 7 );
}

void CDataSetDoc::OnSimSatMagenta() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURESATMAGENTA_SIM), "On measure",  MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 8 );
}

void CDataSetDoc::OnSimSingleMeasurement() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_MEASURESINGLE_SIM), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( -4 );
}

void CDataSetDoc::PerformSimultaneousMeasures ( int nMode )
{
	int				i, j;
	int				NbDocs = 0;
	int				nSteps = 0;
	int				nMaxSteps;
	double			GenIRE [ 256 ];
	BOOL			bIRE [ 256 ];
	LPARAM			lHint = UPD_EVERYTHING;
	BOOL			bOk = TRUE;
	BOOL			bEscape = FALSE;
	CDataSetDoc *	pDocs [ MAX_SIMULTANEOUS_MEASURES ];
	HANDLE			hEvents [ MAX_SIMULTANEOUS_MEASURES ];
	CDocEnumerator	docEnumerator;
	CDataSetDoc *	pDoc;
	CStringList		SensorList;
	CString			strId, strTmp;
	CString			Msg, Title;
	ColorRGBDisplay	GenColors [ 1000 ];
	CGenerator::MeasureType	mType[1000];

	double			dLuxValue;
	BOOL			bUseLuxValues = TRUE;
	double			measuredLux [ 1000 ];
    double          gamma=(GetConfig()->m_useMeasuredGamma)?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);
    MSG				message;

	BOOL (CMeasure::*pValidationFunc) ( BOOL, double * ) = NULL;
	
	memset ( hEvents, 0, sizeof ( hEvents ) );
	memset ( bIRE, 0, sizeof ( bIRE ) );

	switch ( nMode )
	{
		case 1:
			 nSteps = GetMeasure () -> GetNearBlackScaleSize ();
			 nMaxSteps = nSteps;
			 for ( i = 0; i < nSteps ; i ++ )
			 {
				GenIRE [ i ] = i;
				GenColors [ i ] = ColorRGBDisplay(i,i,i);
				mType [ i ] = CGenerator::MT_NEARBLACK;
			 }
			 pValidationFunc = &CMeasure::ValidateBackgroundNearBlack;
			 lHint = UPD_NEARBLACK;
			 break;

		case 2:
			 nSteps = GetMeasure () -> GetNearWhiteScaleSize ();
			 nMaxSteps = nSteps;
			 for ( i = 0; i < nSteps ; i ++ )
			 {
				GenIRE [ i ] = 101-nSteps+i;
				GenColors [ i ] = ColorRGBDisplay(101-nSteps+i,101-nSteps+i,101-nSteps+i);
				mType [ i ] = CGenerator::MT_NEARWHITE;
			 }
			 pValidationFunc = &CMeasure::ValidateBackgroundNearWhite;
			 lHint = UPD_NEARWHITE;
			 break;

		case 3:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_RED;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, true, false, false);
			 pValidationFunc = &CMeasure::ValidateBackgroundRedSatScale;
			 lHint = UPD_REDSAT;
			 break;

		case 4:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_GREEN;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, false, true, false );
			 pValidationFunc = &CMeasure::ValidateBackgroundGreenSatScale;
			 lHint = UPD_GREENSAT;
			 break;

		case 5:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_BLUE;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, false, false, true );
			 pValidationFunc = &CMeasure::ValidateBackgroundBlueSatScale;
			 lHint = UPD_BLUESAT;
			 break;

		case 6:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_YELLOW;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, true, true, false );
			 pValidationFunc = &CMeasure::ValidateBackgroundYellowSatScale;
			 lHint = UPD_YELLOWSAT;
			 break;

		case 7:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_CYAN;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, false, true, true );
			 pValidationFunc = &CMeasure::ValidateBackgroundCyanSatScale;
			 lHint = UPD_CYANSAT;
			 break;

		case 8:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_MAGENTA;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, true, false, true );
			 pValidationFunc = &CMeasure::ValidateBackgroundMagentaSatScale;
			 lHint = UPD_MAGENTASAT;
			 break;

		case 9:
			CGenerator::MeasureType nPattern;
			switch (GetConfig()->m_CCMode)
			{
				case MCD:
				 nPattern=CGenerator::MT_SAT_CC24_MCD;
				 break;
				case GCD:
				nPattern=CGenerator::MT_SAT_CC24_GCD;		
				 break;
				case CMC:
				nPattern=CGenerator::MT_SAT_CC24_GCD;		
				 break;
				case CMS:
				nPattern=CGenerator::MT_SAT_CC24_CMS;		
                nSteps = 19;
				 break;
				case CPS:
				nPattern=CGenerator::MT_SAT_CC24_CPS;		
                nSteps = 19;
				 break;
				case SKIN:
				nPattern=CGenerator::MT_SAT_CC24_GCD;		
				 break;
				case AXIS:
				nPattern=CGenerator::MT_SAT_CC24_GCD;		
				 break;
				case CCSG:
				nPattern=CGenerator::MT_SAT_CC24_CCSG;		
                nSteps = 96;
				 break;
                default:
    			 nSteps = 24;
			}
			 nMaxSteps = nSteps;
			 mType [ 0 ] = nPattern;
			 GenerateCC24Colors (GenColors, GetConfig()->m_CCMode );
			 pValidationFunc = &CMeasure::ValidateBackgroundCC24SatScale;
			 lHint = UPD_CC24SAT;
			 break;

		case -5:
		case -1:
			 mType [ 0 ] = CGenerator::MT_PRIMARY;
			 mType [ 1 ] = CGenerator::MT_PRIMARY;
			 mType [ 2 ] = CGenerator::MT_PRIMARY;
			 mType [ 3 ] = CGenerator::MT_PRIMARY;
			 mType [ 4 ] = CGenerator::MT_PRIMARY;
			 nSteps = ( 3 + GetConfig () -> m_BWColorsToAdd );
			 nMaxSteps = 5;
			 if ( nMode == -5 && nSteps < 4 )
				nSteps = 4;
			 GenColors [ 0 ] = ColorRGBDisplay(100,0,0);		// red
			 GenColors [ 1 ] = ColorRGBDisplay(0,100,0);		// green
			 GenColors [ 2 ] = ColorRGBDisplay(0,0,100);		// blue
			 GenColors [ 3 ] = ColorRGBDisplay(100,100,100);	// white (to control additivity)
			 GenColors [ 4 ] = ColorRGBDisplay(0,0,0);			// black
			 pValidationFunc = &CMeasure::ValidateBackgroundPrimaries;
			 lHint = UPD_PRIMARIES;
			 break;

		case -2:
			 mType [ 0 ] = CGenerator::MT_SECONDARY;
			 mType [ 1 ] = CGenerator::MT_SECONDARY;
			 mType [ 2 ] = CGenerator::MT_SECONDARY;
			 mType [ 3 ] = CGenerator::MT_SECONDARY;
			 mType [ 4 ] = CGenerator::MT_SECONDARY;
			 mType [ 5 ] = CGenerator::MT_SECONDARY;
			 mType [ 6 ] = CGenerator::MT_SECONDARY;
			 mType [ 7 ] = CGenerator::MT_SECONDARY;
			 nSteps = 6 + GetConfig () -> m_BWColorsToAdd;
			 nMaxSteps = 8;
			 GenColors [ 0 ] = ColorRGBDisplay(100,0,0);		// red
			 GenColors [ 1 ] = ColorRGBDisplay(0,100,0);		// green
			 GenColors [ 2 ] = ColorRGBDisplay(0,0,100);		// blue
			 GenColors [ 3 ] = ColorRGBDisplay(100,100,0);		// yellow
			 GenColors [ 4 ] = ColorRGBDisplay(0,100,100);		// cyan
			 GenColors [ 5 ] = ColorRGBDisplay(100,0,100);		// magenta
			 GenColors [ 6 ] = ColorRGBDisplay(100,100,100);	// white (to control additivity)
			 GenColors [ 7 ] = ColorRGBDisplay(0,0,0);			// black
			 pValidationFunc = &CMeasure::ValidateBackgroundSecondaries;
			 lHint = UPD_SECONDARIES;
			 break;

		case -3:
			 nSteps = GetMeasure () -> GetGrayScaleSize ();
			 for (i = 0; i < nSteps ; i ++ )
			 {
				GenIRE [ i ] = ArrayIndexToGrayLevel ( i, nSteps, GetConfig () -> m_bUseRoundDown);
				bIRE [ i ] = GetMeasure () -> m_bIREScaleMode;
				GenColors [ i ] = ColorRGBDisplay(GenIRE[i]);
			 	mType [ i ] = CGenerator::MT_IRE;
			 }

			 GenColors [ nSteps + 0 ] = ColorRGBDisplay(100,0,0);	// red
			 mType [ nSteps + 0 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 1 ] = ColorRGBDisplay(0,100,0);	// green
			 mType [ nSteps + 1 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 2 ] = ColorRGBDisplay(0,0,100);	// blue
			 mType [ nSteps + 2 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 3 ] = ColorRGBDisplay(100,100,0);	// yellow
			 mType [ nSteps + 3 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 4 ] = ColorRGBDisplay(0,100,100);	// cyan
			 mType [ nSteps + 4 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 5 ] = ColorRGBDisplay(100,0,100);	// magenta
			 mType [ nSteps + 5 ] = CGenerator::MT_SECONDARY;
			 nSteps += 6;
			 nMaxSteps = nSteps;
			 pValidationFunc = &CMeasure::ValidateBackgroundGrayScaleAndColors;
			 lHint = UPD_GRAYSCALEANDCOLORS;
			 break;

		case -4:
			 nSteps = 1;
			 nMaxSteps = nSteps;
			 GenColors [ 0 ] = ColorRGBDisplay(( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.GetColor () & 0x00FFFFFF);
			 mType [ 0 ] = CGenerator::MT_UNKNOWN;
			 pValidationFunc = &CMeasure::ValidateBackgroundSingleMeasurement;
			 lHint = UPD_FREEMEASUREAPPENDED;

			 // Do not use luxmeter when performing free measurement
			 bUseLuxValues = FALSE;
			 break;

		case 0:
		default:
			 nSteps = GetMeasure () -> GetGrayScaleSize ();
			 nMaxSteps = nSteps;
			 for (i = 0; i < nSteps ; i ++ )
			 {
				GenIRE [ i ] = (int)ArrayIndexToGrayLevel ( i, nSteps, GetConfig () -> m_bUseRoundDown);
				bIRE [ i ] = GetMeasure () -> m_bIREScaleMode;
				GenColors [ i ] = ColorRGBDisplay(GenIRE[i]);
 			 	mType [ i ] = CGenerator::MT_IRE;
			 }

			 pValidationFunc = &CMeasure::ValidateBackgroundGrayScale;
			 lHint = UPD_GRAYSCALE;
			 break;
	}

	StopBackgroundMeasures ();

	// Register current document as first in the list
	pDocs [ NbDocs ++ ] = this;
	m_pSensor -> GetUniqueIdentifier ( strId );
	SensorList.AddTail ( strId );

	if ( nMode == -5 )
	{
		// Calibration mode (1)
		// take only reference document as secondary document
		pDoc = GetDataRef ();
		if ( pDoc )
			pDocs [ NbDocs ++ ] = pDoc;
		else
		{
			GetColorApp()->InMeasureMessageBox( "Internal error" );
			return;
		}
	}
	else
	{
		while ( ( pDoc = (CDataSetDoc *) docEnumerator.Next () ) != NULL )
		{
			if ( pDoc != this )
			{
				pDoc -> m_pSensor -> GetUniqueIdentifier ( strId );
				if ( ! SensorList.Find ( strId ) )
				{
					if ( NbDocs < MAX_SIMULTANEOUS_MEASURES )
					{
						SensorList.AddTail ( strId );
						pDocs [ NbDocs ++ ] = pDoc;
					}
					else
					{
						bOk = FALSE;
						Msg.Format ( "Too many opened documents ! Cannot perform simultaneous measures on more than %d sensors.\nClose some documents then retry.", MAX_SIMULTANEOUS_MEASURES );
						Title.LoadString ( IDS_ERROR );
						GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
					}
				}
				else
				{
					bOk = FALSE;
					Msg.Format ( IDS_MANYDOCSUSESAMESENSOR, (LPCSTR) strId );
					Title.LoadString ( IDS_ERROR );
					GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
				}
			}
		}
	}
	
	// Ensure there is more than one opened document
	if ( bOk && NbDocs == 1 )
	{
		bOk = FALSE;
		Msg.LoadString ( IDS_ONLYONEDOC );
		Title.LoadString ( IDS_ERROR );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
	}
	
	if ( bOk )
	{
		// Ok, we can perform simultaneous measures. 
		// Try to initialise all documents for simultaneous measures
		for ( i	= 0; bOk && i < NbDocs ; i ++ )
		{
			hEvents [ i ] = pDocs [ i ] -> m_measure.InitBackgroundMeasures ( pDocs [ i ] -> m_pSensor, nSteps );
			if ( hEvents [ i ] == NULL )
			{
				// Initialization failed
				bOk = FALSE;
				Msg.Format ( IDS_CANNOTINITDOC, (LPCSTR) pDocs [ i ] -> GetTitle () );
				Title.LoadString ( IDS_ERROR );
				GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);

				// Cancel background process on already initialized documents
				while ( ( --i ) >= 0 )
				{
					pDocs [ i ] -> m_measure.CancelBackgroundMeasures ();
					hEvents [ i ] = NULL;
				}
				break;
			}
		}
	}


	if ( bOk )
	{
		// All documents are ready for simultaneous measures. Let's go on with current document 
		CGenerator * pGenerator = m_pGenerator;
		
		// Initialize generator for the maximum steps possible (can be different from nSteps only for primary/secondary colors)
		if ( pGenerator -> Init ( nMaxSteps ) )
		{
			for ( i = 0 ; bOk && i < nSteps ; i++ )
			{
				BOOL bresult;
				if ((mType[i] == CGenerator::MT_IRE)||(mType[i] == CGenerator::MT_NEARWHITE)||(mType[i] == CGenerator::MT_NEARWHITE))
					bresult = pGenerator -> DisplayGray ( GenIRE [ i ], mType[i] );
				else
					bresult = pGenerator -> DisplayRGBColor ( GenColors [ i ] , mType[i] );
				if (bresult )
				{
					if ( m_measure.WaitForDynamicIris () )
					{
						// Operation cancelled at user request
						bOk = FALSE;
						bEscape = TRUE;
					}
					else
					{
						if ( bUseLuxValues )
							m_measure.StartLuxMeasure ();

						// Request background measure on all sensors at the same time
						for ( j = 0; j < NbDocs ; j ++ )
						{
							bOk &= pDocs [ j ] -> m_measure.BackgroundMeasureColor ( i, GenColors [ i ] );
							ASSERT ( bOk );
						}

						// Wait for all measures ended
						WaitForMultipleObjects ( NbDocs, hEvents, TRUE, INFINITE );
						
						if ( bUseLuxValues )
						{
							switch ( m_measure.GetLuxMeasure ( & dLuxValue ) )
							{
								case LUX_NOMEASURE:
									 bUseLuxValues = FALSE;
									 break;

								case LUX_OK:
									 measuredLux[i] = dLuxValue;
									 break;

								case LUX_CANCELED:
									 bEscape = TRUE;
									 break;
							}
						}
						// Check if user hit escape key
						while ( PeekMessage ( & message, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
						{
							if ( message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE )
							{
								// Operation cancelled at user request
								bOk = FALSE;
								bEscape = TRUE;
							}
						}
					}
				}
				else
				{
					// Generator error
					bOk = FALSE;
					break;
				}
			}

			pGenerator -> Release ();
		}

		if ( bOk )
		{
			// Validate results on all documents
			for ( i	= 0 ; i < NbDocs ; i ++ )
			{
				// Transfer background results in document results
				( pDocs [ i ] -> m_measure .* pValidationFunc ) ( bUseLuxValues, measuredLux );
				
				pDocs [ i ] -> SetModifiedFlag ( pDocs [ i ] -> m_measure.IsModified () );
			}

			for ( i	= 0 ; i < NbDocs ; i ++ )
			{
				// Update graphical views
				pDocs [ i ] -> UpdateAllViews ( NULL, lHint );
			}
			
			// Clear information window
			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls

		}
		else
		{
			// Operation failed.
			if ( bEscape )
			{
				Msg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( Msg, NULL, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				Title.LoadString ( IDS_ERROR );
				Msg.LoadString ( IDS_ANERROROCCURED );
				GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
			}

			// Cancel all background measures
			for ( i	= 0; i < NbDocs ; i ++ )
			{
				pDocs [ i ] -> m_measure.CancelBackgroundMeasures ();
			}
		}
	}
}

void CDataSetDoc::WaitKey()
{
	BOOL	bKeyTyped = FALSE;
	MSG		Msg;
	
	while ( !bKeyTyped )
	{
		while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_MOUSELAST, TRUE ) )
		{
			if ( Msg.message == WM_KEYDOWN || Msg.message == WM_SYSKEYDOWN)
			{
				if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN || Msg.wParam == VK_CONTROL || Msg.wParam == VK_MENU )
					bKeyTyped = TRUE;
			}
			else if ( Msg.message == WM_TIMER )
			{
				// Dispatch timer message to allow animation run
				DispatchMessage ( &Msg );
			}
		}
		Sleep(10);
	}
}

void CDataSetDoc::OnPatternAnimBlack() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns(FALSE) )
	{
		if ( IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_DISPLAYANIMATEDPATTERN), "On Measure", MB_YESNO | MB_ICONQUESTION ) )
		{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayAnimatedBlack();
			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
		}
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternAnimWhite() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns(FALSE) )
	{
		if ( IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_DISPLAYANIMATEDPATTERN), "On measure", MB_YESNO | MB_ICONQUESTION ) )
		{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayAnimatedWhite();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
		}
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternGradient() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayGradient();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternRB() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayRB();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}
void CDataSetDoc::OnPatternRG() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayRG();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}
void CDataSetDoc::OnPatternGB() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayGB();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}
void CDataSetDoc::OnPatternRBd() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayRBd();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}
void CDataSetDoc::OnPatternRGd() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayRGd();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternGBd() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayGBd();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternLramp() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayLramp();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternGradient2() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayGradient2();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternGranger() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayGranger();
			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPattern80() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->Display80();
			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternTV() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayTV();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternSharp() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplaySharp();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternClipH() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayClipH();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternClipL() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayClipL();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternSpectrum() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplaySpectrum();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternSramp() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplaySramp();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternVSMPTE() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayVSMPTE();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternEramp() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayEramp();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternTC0() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayTC0();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternTC1() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayTC1();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternTC2() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayTC2();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternTC3() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayTC3();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternTC4() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayTC4();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternTC5() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayTC5();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternBN() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns(TRUE) )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayBN();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternDR0() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayDR0();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternDR1() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayDR1();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternDR2() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayDR2();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternAlign() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayAlign();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternTestimg() 
{

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayTestimg();

			WaitKey();
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
	}
	else
	{
		GetColorApp()->InMeasureMessageBox( _S(IDS_CANNOTDISPLAYANIMATION), "On measure", MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnSingleMeasurement()
{
	AddMeasurement();
}

void CDataSetDoc::OnContinuousMeasurement()
{
	if ( g_pDataDocRunningThread && g_pDataDocRunningThread != this )
	{
		// Another document uses background thread: stop it before starting our background thread.
		StopBackgroundMeasures ();
	}
	
	if ( g_pDataDocRunningThread )
	{
		StopBackgroundMeasures ();
		GetConfig()->m_isSettling = Settling;
	}
	else
	{
		Settling=GetConfig()->m_isSettling;
		GetConfig()->m_isSettling = FALSE;
		StartBackgroundMeasures ( this );
	}
}

void CDataSetDoc::OnUpdateContinuousMeasurement(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	int		iImage;
	UINT	nID, nStyle;
	BOOL	bRunning = ( g_pDataDocRunningThread && g_pDataDocRunningThread == this ); 
	
	pCmdUI -> SetCheck ( bRunning ); 

	CToolBar* pToolBar = (CToolBar*) pCmdUI -> m_pOther;
	if ( pToolBar && pToolBar -> m_hWnd )
	{
		pToolBar -> GetButtonInfo ( pCmdUI -> m_nIndex, nID, nStyle, iImage );
		pToolBar -> SetButtonInfo ( pCmdUI -> m_nIndex, nID, nStyle, bRunning ? 6: 5 );
	}
}

void CDataSetDoc::ComputeGammaAndOffset(double * Gamma, double * Offset, int ColorSpace,int ColorIndex,int Size, bool m_bBT1886)
{
	int		nConfigOffsetType = GetConfig() -> m_GammaOffsetType;
	int		nLumCurveMode = GetConfig () -> m_nLuminanceCurveMode;
	double	blacklvl, whitelvl, graylvl, Offset_opt, Gamma_opt;
	BOOL	bIRE = GetMeasure () -> m_bIREScaleMode;

	double * lumlvl = new double [ Size ];
	double * valx = new double [ Size ];
	double * tmpx = new double [ Size ];

	if (ColorSpace == 0) 
	{
		blacklvl=GetMeasure()->GetGray(0).GetRGBValue((GetColorReference()))[ColorIndex];
		whitelvl=GetMeasure()->GetGray(Size-1).GetRGBValue((GetColorReference()))[ColorIndex];
	}
	else if (ColorSpace == 1) 
	{
		blacklvl=GetMeasure()->GetGray(0).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		whitelvl=GetMeasure()->GetGray(Size-1).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
	}
	else if (ColorSpace == 2) 
	{
		blacklvl=GetMeasure()->GetGray(0).GetLuxValue();
		whitelvl=GetMeasure()->GetGray(Size-1).GetLuxValue();
	}
	else
	{
		// ColorSpace == 3: special mode to display in main view
		if ( nLumCurveMode > 0 )
		{
			// When luxmeter values are authorized in curves, use preference for contrast/delta luminance
			blacklvl=GetMeasure()->GetGray(0).GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter);
			whitelvl=GetMeasure()->GetGray(Size-1).GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter);
		}
		else
		{
			// Use only colorimeter values
			blacklvl=GetMeasure()->GetGray(0).GetLuminance();
			whitelvl=GetMeasure()->GetGray(Size-1).GetLuminance();
		}
	}

	if ( nConfigOffsetType == 1 )
		whitelvl = whitelvl - blacklvl;

	for (int i=0; i<Size; i++)
	{	
		if (ColorSpace == 0)
			graylvl=GetMeasure()->GetGray(i).GetRGBValue((GetColorReference()))[ColorIndex];
		else if (ColorSpace == 1)
			graylvl=GetMeasure()->GetGray(i).GetLuxOrLumaValue(GetConfig () -> m_nLuminanceCurveMode);
		else if (ColorSpace == 2) 
			graylvl=GetMeasure()->GetGray(i).GetLuxValue();
		else
		{
			if ( nLumCurveMode > 0 )
				graylvl=GetMeasure()->GetGray(i).GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter);
			else
				graylvl=GetMeasure()->GetGray(i).GetLuminance();
		}

		if ( nConfigOffsetType == 1 )
			graylvl = graylvl - blacklvl;
		lumlvl[i] = graylvl/whitelvl;
	}

	if ( nConfigOffsetType == 1 )
		blacklvl = 0;


	Gamma_opt = 1.0;
	Offset_opt = 0.0;

			
	if ((Size > 2) && (whitelvl > 0))
	{
		switch ( nConfigOffsetType )
		{
		case 0:
		case 1:
			Offset_opt = 0;
			break;
		case 2:
			if(GetConfig()->m_colorStandard == sRGB)
				Offset_opt = 0.055;
			else
				Offset_opt = 0.099;
			break;
		case 3:
			Offset_opt = GetConfig() -> m_manualGOffset;
			break;
		case 4: //optimized, we should replace with bt.1886
			{
				Offset_opt = 0.0;
			}
			break;
		case 5: //optimized, we should replace with bt.1886
			{
				Offset_opt = 0.0;
			}
			break;
		}
			double x, v;

			for (int i=0; i<Size; i++)
			{
				x = ArrayIndexToGrayLevel ( i, Size, GetConfig () -> m_bUseRoundDown);
				v = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown);
				valx[i]=(v+Offset_opt)/(1.0+Offset_opt);
			}

			int	nb = 0;
			double avg = 0;
			for (int i=1; i<Size-1; i++)
			{
				if ( lumlvl[i] > 0.0 )
				{
					int mode = GetConfig()->m_GammaOffsetType;
					if (GetConfig()->m_colorStandard == sRGB) mode = 7;
                    if (m_bBT1886 && !GetConfig()->m_useMeasuredGamma)
                        avg += log(getEOTF(valx[i], GetMeasure()->GetGray(Size -1), GetMeasure()->GetGray(0), GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(valx[i]);
                    else
		    			avg += log(lumlvl[i])/log(valx[i]);
					nb ++;
				}
			}
			Gamma_opt = avg / ( nb ? nb : 1 );
	}
	*Offset = Offset_opt;
	*Gamma = Gamma_opt;

	delete lumlvl;
	delete valx;
	delete tmpx;
}


void CDataSetDoc::OnBkgndMeasureReady()
{
	POSITION	pos;
	CColor *	pMeasurement;
	
	g_bInsideBkgndRefresh = TRUE;

	if ( g_hThread && ! g_bTerminateThread && this == g_pDataDocRunningThread )
	{
		EnterCriticalSection ( & g_CritSec );
		pos = g_MeasuredColorList.GetHeadPosition ();
		POSITION pos2 = g_pDataDocRunningThread -> GetFirstViewPosition ();
		
		while ( pos )
		{
			CView *pView = g_pDataDocRunningThread -> GetNextView ( pos2 );
			int m_d = ((CMainView*)pView)->m_displayMode;
			pMeasurement = ( CColor * ) g_MeasuredColorList.GetNext ( pos );

			m_measure.AppendMeasurements(*pMeasurement, m_d == 1);
			SetModifiedFlag();

			SetSelectedColor ( *pMeasurement );

			UpdateAllViews(NULL,UPD_FREEMEASUREAPPENDED,this);

			delete pMeasurement;
		}
		g_MeasuredColorList.RemoveAll ();
		LeaveCriticalSection ( & g_CritSec );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}

	g_bInsideBkgndRefresh = FALSE;
}

void CDataSetDoc::OnMeasureDefinescale() 
{
	CScaleSizes dlg ( this );

	dlg.DoModal ();
}

void CDataSetDoc::OnMeasureGrayscale() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetGrayScaleSize ();
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNGRAYSCALEON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
	{
		MeasureGrayScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureGrayscale(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CDataSetDoc::OnMeasurePrimaries() 
{
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite && (GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb) )
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_RUNPRIMARIES), "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasurePrimaries();
		
			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasurePrimaries(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CDataSetDoc::OnMeasureSecondaries() 
{
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite && (GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb))
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( _S(IDS_RUNSECONDARIES), "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureSecondaries();
		
			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSecondaries(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}


void CDataSetDoc::OnMeasureNearblack() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetNearBlackScaleSize ();
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
    {
    	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

    	Msg.LoadString ( IDS_RUNNEARBLACKON );
	    TmpStr.Format ( " %d ", nNbPoints );
	    Msg += TmpStr + MsgQueue;
	    if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
	    {
		    MeasureNearBlackScale();

    		SetSelectedColor ( noDataColor );
	    	(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	    }
    }
}

void CDataSetDoc::OnUpdateMeasureNearblack(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_NEARBLACK, GetMeasure () -> GetNearBlackScaleSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureNearwhite() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetNearWhiteScaleSize ();
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
    {
    	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

    	Msg.LoadString ( IDS_RUNNEARWHITEON );
	    TmpStr.Format ( " %d ", nNbPoints );
	    Msg += TmpStr + MsgQueue;
	    if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
	    {
		    MeasureNearWhiteScale();

    		SetSelectedColor ( noDataColor );
	    	(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	    }
    }
}

void CDataSetDoc::OnUpdateMeasureNearwhite(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_NEARWHITE, GetMeasure () -> GetNearWhiteScaleSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatRed() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetSaturationSize ();
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNSATREDON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureRedSatScale();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSatRed(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_RED, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatGreen() 
{
	CString	Msg, MsgQueue, TmpStr;

	int		nNbPoints = GetMeasure () -> GetSaturationSize ();
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNSATGREENON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureGreenSatScale();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSatGreen(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_GREEN, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatBlue() 
{
	CString	Msg, MsgQueue, TmpStr;

	int		nNbPoints = GetMeasure () -> GetSaturationSize ();
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNSATBLUEON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureBlueSatScale();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSatBlue(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_BLUE, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatYellow() 
{
	CString	Msg, MsgQueue, TmpStr;

	int		nNbPoints = GetMeasure () -> GetSaturationSize ();
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNSATYELLOWON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureYellowSatScale();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSatYellow(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_YELLOW, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatCyan() 
{
	CString	Msg, MsgQueue, TmpStr;

	int		nNbPoints = GetMeasure () -> GetSaturationSize ();
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNSATCYANON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureCyanSatScale();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSatCyan(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_CYAN, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatMagenta() 
{
	CString	Msg, MsgQueue, TmpStr;

	int		nNbPoints = GetMeasure () -> GetSaturationSize ();
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNSATMAGENTAON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureMagentaSatScale();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnMeasureSatCC24() 
{
	CString	Msg, MsgQueue, TmpStr;

	BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);
    int		nNbPoints = (GetConfig()->m_CCMode==CCSG?96:isExtPat?GetConfig()->GetCColorsSize():(GetConfig()->m_CCMode==AXIS?71:24));
	if (GetConfig()->m_CCMode==CMS || GetConfig()->m_CCMode==CPS)
		nNbPoints = 19;
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );
		
		Msg.LoadString ( IDS_RUNSATCC24ON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureCC24SatScale();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSatMagenta(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_MAGENTA, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnUpdateMeasureSatCC24(CCmdUI* pCmdUI) 
{
	CGenerator::MeasureType nPattern;
	switch (GetConfig()->m_CCMode)
	{
	case MCD:
		 nPattern=CGenerator::MT_SAT_CC24_MCD;
		 break;
	case GCD:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;		
		 break;
	case CMC:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;		
		 break;
	case CMS:
		 nPattern=CGenerator::MT_SAT_CC24_CMS;		
		 break;
	case CPS:
		 nPattern=CGenerator::MT_SAT_CC24_CPS;		
		 break;
	case SKIN:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;		
		 break;
	case AXIS:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;		
		 break;
	case CCSG:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;		
		 break;
	}

	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( nPattern, (nPattern<16?24:(nPattern==16?96:1000)), TRUE ) );
}

void CDataSetDoc::OnMeasureContrast() 
{
	CString	Msg;
	
	Msg.LoadString ( IDS_RUNCONTRAST );
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
	{
		MeasureContrast();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureContrast(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( TRUE );
}

void CDataSetDoc::OnMeasureSatAll() 
{
	CString	Msg, MsgQueue, TmpStr;

	BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);
    int		nNbPoints = (GetMeasure () -> GetSaturationSize ()) * 6 + (GetConfig()->m_CCMode==CCSG?96:isExtPat?GetConfig()->GetCColorsSize():(GetConfig()->m_CCMode==AXIS?71:24));
	if (GetConfig()->m_CCMode==CMS || GetConfig()->m_CCMode==CPS)
		nNbPoints = GetMeasure () -> GetSaturationSize () * 6 + 19;
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNALLSATON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasureAllSaturationScales();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSatAll(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_ALL, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureGrayscaleColors() 
{
	// TODO: Add your command handler code here
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetGrayScaleSize () + 6 ;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_MEASUREGRAYANDCOLORSON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
	{
		MeasureGrayScaleAndColors();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
	
}

void CDataSetDoc::OnUpdateMeasureGrayscaleColors(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayGrayAndColorsSeries () );
}

void CDataSetDoc::OnMeasureSatPrimaries() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints =( GetMeasure () -> GetSaturationSize () ) * 3;
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNPRIMSATON );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasurePrimarySaturationScales();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnMeasureSatPrimariesSecondaries() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints =( GetMeasure () -> GetSaturationSize () ) * 6;
	int		nCount = GetMeasure () -> GetGrayScaleSize ();
    bool m_YWhite =  GetMeasure () -> GetGray ( nCount - 1 ).isValid();
	if (!m_YWhite)
		GetColorApp()->InMeasureMessageBox("Please run the grayscale measures scan first so that color targets can be calculated.","No grayscale measures found!",MB_OK);
	else
	{
		MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

		Msg.LoadString ( IDS_RUNPRIMSATON2 );
		TmpStr.Format ( " %d ", nNbPoints );
		Msg += TmpStr + MsgQueue;
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
		{
			MeasurePrimarySecondarySaturationScales();

			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}

void CDataSetDoc::OnUpdateMeasureSatPrimaries(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_ALL, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnUpdateMeasureSatPrimariesSecondaries(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_ALL, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureFullTiltBoogie()
{
	CString	Msg, MsgQueue, TmpStr;
	BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);
    int		nNbPoints2 = (GetMeasure () -> GetSaturationSize ()) * 6 + (GetConfig()->m_CCMode==CCSG?96:isExtPat?GetConfig()->GetCColorsSize():(GetConfig()->m_CCMode==AXIS?71:24));
	if (GetConfig()->m_CCMode==CMS || GetConfig()->m_CCMode==CPS)
		nNbPoints2 = GetMeasure () -> GetSaturationSize () * 6 + 19;

	int		nNbPoints = GetMeasure () -> GetGrayScaleSize () + 6 +  GetMeasure () -> GetNearBlackScaleSize () +  GetMeasure () -> GetNearWhiteScaleSize () + nNbPoints2;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

//	Msg.LoadString ( IDS_MEASUREGRAYANDCOLORSON );
	TmpStr.Format ( "Run Gray scale, Near Black/White, Primaries/Secondaries, and Color Checker patterns on  %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == GetColorApp()->InMeasureMessageBox( Msg, "On measure", MB_ICONQUESTION | MB_YESNO ) )
	{
		MeasureGrayScaleAndColors();
		MeasureNearBlackScale();
		MeasureNearWhiteScale();
		MeasureAllSaturationScales();
		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
}

void CDataSetDoc::OnUpdateMeasureFullTiltBoogie(CCmdUI* pCmdUI)
{
	OnUpdateMeasureGrayscaleColors(pCmdUI);
	OnUpdateMeasureNearblack(pCmdUI);
	OnUpdateMeasureNearwhite(pCmdUI);
	OnUpdateMeasureSatAll(pCmdUI);
}

void CDataSetDoc::OnSaveCalibrationFile() 
{
	m_pSensor -> SaveCalibrationFile ();
}

void CDataSetDoc::OnLoadCalibrationFile() 
{
	CString		Msg, Title;

	StopBackgroundMeasures ();

	CPropertySheetWithHelp propSheet;
	CSensorSelectionPropPage page;
	propSheet.AddPage(&page);
	propSheet.m_psh.dwFlags |= PSH_NOAPPLYNOW;

	Title.LoadString ( IDM_LOAD_CALIBRATION_FILE );
	propSheet.SetTitle ( Title );

	page.m_sensorChoice=m_pSensor->GetName(); 	// default selection is current sensor
	if( propSheet.DoModal() == IDOK )
	{

		if(page.m_sensorTrainingMode != 1)
			m_pSensor->LoadCalibrationFile(page.m_trainingFileName);
		m_pSensor->SetSensorMatrixMod(Matrix::IdentityMatrix(3));
		m_measure.ApplySensorAdjustmentMatrix( m_pSensor->GetSensorMatrix() );
		UpdateAllViews ( NULL, UPD_EVERYTHING );
		SetModifiedFlag(TRUE);
	}
}


void CDataSetDoc::OnUpdateLoadCalibrationFile(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pSensor -> IsCalibrated () != 1 );
}

void CDataSetDoc::OnUpdateSaveCalibrationFile(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pSensor -> IsCalibrated () == 1 );
}
