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

#include "Matrix.h"
#include "NewDocWizard.h"

#include "Export.h"

#include "MainFrm.h"

#include "RefColorDlg.h"
#include "ScaleSizes.h"
#include "AdjustMatrixDlg.h"

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
static COLORREF			g_CurrentColor = 0x00000000;
static CRITICAL_SECTION g_CritSec;
static CPtrList			g_MeasuredColorList;
volatile BOOL			g_bInsideBkgndRefresh = FALSE;

// The background thread function

static UINT __cdecl BkgndThreadFunc ( LPVOID lpParameter )
{
    CrashDump useInThisThread;
    try
    {
	    CSensor *		pSensor = g_pDataDocRunningThread -> m_pSensor;	// Assume sensor is initialized
	    CGenerator *	pGenerator = g_pDataDocRunningThread -> m_pGenerator;
    	
	    CColor			measuredColor;
	    CString			Msg;

	    CView *			pView;
	    POSITION		pos;

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
			    COLORREF clr = ( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.GetColor ();
			    clr &= 0x00FFFFFF;
    			
			    if ( g_CurrentColor != clr )
			    {
				    g_CurrentColor = clr;
				    pGenerator->DisplayRGBColor(clr,CGenerator::MT_UNKNOWN);
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
				    pMeasurement -> SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				    pMeasurement -> SetSensorValue(measuredColor);

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
							    //( (CMainView*) pView ) -> m_grayScaleButton.SetIcon(IDI_START_ICON,24,24);
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
		if(!pDoc->m_pSensor->IsCalibrated())
			pDoc->CalibrateSensor(FALSE);

		if(pDoc->m_pSensor->Init(FALSE) != TRUE)
		{
			Msg.LoadString ( IDS_ERRINITSENSOR );
			Title.LoadString ( IDS_ERROR );
			MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
			return FALSE;
		}
		
		pDoc->EnsureVisibleViewMeasuresCombo();

		g_CurrentColor = RGB(128,128,128);

		CString str, str2;
		str.LoadString(IDS_GDIGENERATOR_NAME);
		str2 = pDoc->GetGenerator()->GetName();

		if ( str == str2 )
		{
			if ( ( (CGDIGenerator*) pDoc->GetGenerator() ) -> IsOnOtherMonitor () )
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

				COLORREF clr = ( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.GetColor ();
				clr &= 0x00FFFFFF;
				
				g_CurrentColor = clr;

				pDoc->GetGenerator()->DisplayRGBColor(clr,CGenerator::MT_UNKNOWN);
				
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
						//( (CMainView*) pView ) -> m_grayScaleButton.SetIcon(IDI_START_ICON,24,24);
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
	ON_COMMAND(IDM_CONFIGURE_GENERATOR, OnConfigureGenerator)
	ON_COMMAND(IDM_CALIBRATE_SENSOR, OnCalibrateSensor)
	ON_UPDATE_COMMAND_UI(IDM_CALIBRATE_SENSOR, OnUpdateCalibrateSensor)
	ON_COMMAND(IDM_CHANGE_GENERATOR, OnChangeGenerator)
	ON_COMMAND(IDM_CHANGE_SENSOR, OnChangeSensor)
	ON_COMMAND(IDM_EXPORT_XLS, OnExportXls)
	ON_COMMAND(IDM_EXPORT_CSV, OnExportCsv)
	ON_UPDATE_COMMAND_UI(IDM_EXPORT_XLS, OnUpdateExportXls)
	ON_COMMAND(IDM_CALIBRATION_SIM, OnCalibrationSim)
	ON_UPDATE_COMMAND_UI(IDM_CALIBRATION_SIM, OnUpdateCalibrationSim)
	ON_COMMAND(IDM_CALIBRATION_MANUAL, OnCalibrationManual)
	ON_UPDATE_COMMAND_UI(IDM_CALIBRATION_MANUAL, OnUpdateCalibrationManual)
	ON_COMMAND(IDM_SIM_GRAYSCALE, OnSimGrayscale)
	ON_COMMAND(IDM_SIM_PRIMARIES, OnSimPrimaries)
	ON_COMMAND(IDM_SIM_SECONDARIES, OnSimSecondaries)
	ON_COMMAND(IDM_SIM_GRAYSCALE_AND_COLORS, OnSimGrayscaleAndColors)
	ON_COMMAND(IDM_PATTERN_ANIM_BLACK, OnPatternAnimBlack)
	ON_COMMAND(IDM_PATTERN_ANIM_WHITE, OnPatternAnimWhite)
	ON_COMMAND(IDM_TRAIN_METER, OnTrainMeter)
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
	ON_COMMAND(IDM_MEASURE_CONTRAST, OnMeasureContrast)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_CONTRAST, OnUpdateMeasureContrast)
	ON_COMMAND(IDM_MEASURE_SAT_ALL, OnMeasureSatAll)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_ALL, OnUpdateMeasureSatAll)
	ON_COMMAND(IDM_MEASURE_GRAYSCALE_COLORS, OnMeasureGrayscaleColors)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_GRAYSCALE_COLORS, OnUpdateMeasureGrayscaleColors)
	ON_COMMAND(IDM_SIM_NEARBLACK, OnSimNearblack)
	ON_COMMAND(IDM_SIM_NEARWHITE, OnSimNearwhite)
	ON_COMMAND(IDM_SIM_SAT_RED, OnSimSatRed)
	ON_COMMAND(IDM_SIM_SAT_GREEN, OnSimSatGreen)
	ON_COMMAND(IDM_SIM_SAT_BLUE, OnSimSatBlue)
	ON_COMMAND(IDM_SIM_SAT_YELLOW, OnSimSatYellow)
	ON_COMMAND(IDM_SIM_SAT_CYAN, OnSimSatCyan)
	ON_COMMAND(IDM_SIM_SAT_MAGENTA, OnSimSatMagenta)
	ON_COMMAND(IDM_SIM_SINGLEMEASURE, OnSimSingleMeasurement)
	ON_COMMAND(IDM_RESET_CONVERSION_MATRIX, OnResetConversionMatrix)
	ON_UPDATE_COMMAND_UI(IDM_RESET_CONVERSION_MATRIX, OnUpdateResetConversionMatrix)
	ON_COMMAND(IDM_EDIT_CONVERSION_MATRIX, OnEditConversionMatrix)
	ON_COMMAND(IDM_MEASURE_SAT_PRIMARIES, OnMeasureSatPrimaries)
	ON_UPDATE_COMMAND_UI(IDM_MEASURE_SAT_PRIMARIES, OnUpdateMeasureSatPrimaries)
	ON_COMMAND(IDM_SAVE_CALIBRATION_FILE, OnSaveCalibrationFile)
	ON_UPDATE_COMMAND_UI(IDM_SAVE_CALIBRATION_FILE, OnUpdateSaveCalibrationFile)
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
				MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
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
			MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
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
								    MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
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
				MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
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
					// HCFR sensor or simulated sensor, needing calibration
					if(propSheet.m_Page2.m_sensorTrainingMode == 1)
					{
						if(m_pSensor->Configure())
							CalibrateSensor(FALSE);
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
				if (Dlg.m_DuplXYZCheck)
					m_measure.Copy(&g_DocToDuplicate->m_measure,DUPLXYZADJUST);
			}
			else	// Default
			{
				return FALSE;
			}
		}
	}

	m_measure.SetSensorMatrix(m_pSensor->GetSensorMatrix());

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
			MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
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
			MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
			return;
		}
			
		pObj=ar.ReadObject(NULL);
		if(pObj->GetRuntimeClass()->IsDerivedFrom(RUNTIME_CLASS(CGenerator)) )
			m_pGenerator=(CGenerator *)pObj;
		else
		{
			Msg.LoadString ( IDS_UNKNOWNGENERATOR2 );
			Title.LoadString ( IDS_ERROR );
			MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
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

	return docIsModified;
}

BOOL CDataSetDoc::IsControlledModeActive ()
{
	BOOL			bControlledMode = FALSE;
	CString			strId1, strId2;
	CDataSetDoc *	pRefDoc = GetDataRef ();

	if ( pRefDoc && pRefDoc != this && GetConfig ()->m_bControlledMode && pRefDoc -> m_measure.m_bIREScaleMode == m_measure.m_bIREScaleMode )
	{
		m_pSensor -> GetUniqueIdentifier ( strId1 );
		pRefDoc -> m_pSensor -> GetUniqueIdentifier ( strId2 );
		
		if ( strId1 != strId2 )
			bControlledMode = TRUE;
	}
	
	return bControlledMode;
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

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureGrayScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_GRAYSCALE);
	}
}

void CDataSetDoc::MeasureGrayScaleAndColors() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureGrayScaleAndColors(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_GRAYSCALEANDCOLORS);
	}
}

void CDataSetDoc::MeasureNearBlackScale() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureNearBlackScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_NEARBLACK);
	}
}

void CDataSetDoc::MeasureNearWhiteScale() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureNearWhiteScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_NEARWHITE);
	}
}

void CDataSetDoc::MeasureRedSatScale() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureRedSatScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_REDSAT);
	}
}

void CDataSetDoc::MeasureGreenSatScale() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureGreenSatScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_GREENSAT);
	}
}

void CDataSetDoc::MeasureBlueSatScale() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureBlueSatScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_BLUESAT);
	}
}

void CDataSetDoc::MeasureYellowSatScale() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureYellowSatScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_YELLOWSAT);
	}
}

void CDataSetDoc::MeasureCyanSatScale() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureCyanSatScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_CYANSAT);
	}
}

void CDataSetDoc::MeasureMagentaSatScale() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureMagentaSatScale(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_MAGENTASAT);
	}
}

void CDataSetDoc::MeasureAllSaturationScales() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureAllSaturationScales(m_pSensor,m_pGenerator,FALSE))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_ALLSATURATIONS);
	}
}

void CDataSetDoc::MeasurePrimarySaturationScales() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureAllSaturationScales(m_pSensor,m_pGenerator,TRUE))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_ALLSATURATIONS);
	}
}

void CDataSetDoc::MeasurePrimaries() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasurePrimaries(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_PRIMARIES);
	}
}

void CDataSetDoc::MeasureSecondaries() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureSecondaries(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_SECONDARIES);
	}
}

void CDataSetDoc::MeasureContrast()
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.MeasureContrast(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_CONTRAST);
	}
}

void CDataSetDoc::AddMeasurement() 
{
	StopBackgroundMeasures ();

	if(!m_pSensor->IsCalibrated())
		CalibrateSensor(FALSE);

	if(m_measure.AddMeasurement(m_pSensor,m_pGenerator))
	{
		SetModifiedFlag(m_measure.IsModified());
		UpdateAllViews(NULL, UPD_FREEMEASUREAPPENDED);
	}
}

void CDataSetDoc::CalibrateSensor(BOOL doUpdateValues) 
{
	CString		Msg, Title;
	StopBackgroundMeasures ();

	if (!m_pSensor->SensorNeedCalibration())
		return;

	if(m_pSensor->CalibrateSensor(m_pGenerator) != TRUE)
		return;

	if(m_pSensor->IsModified() != TRUE)  // calibration didn't change anything
		return;

	if(doUpdateValues)
	{
		Msg.LoadString ( IDS_PRESERVESENSORDATA );
		Title.LoadString ( IDS_CALIBRATION );

		if(MessageBox(NULL,Msg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
			m_measure.SetSensorMatrix(m_pSensor->GetSensorMatrix(), TRUE);
		else	// preserve XYZ values
			m_measure.SetSensorMatrix(m_pSensor->GetSensorMatrix(), FALSE);

		SetModifiedFlag(TRUE);
		UpdateAllViews(NULL, UPD_EVERYTHING);
	}

	Msg.LoadString ( IDS_SAVECALDATA );
	Title.LoadString ( IDS_CALIBRATION );
	if(MessageBox(NULL,Msg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
		m_pSensor->SaveCalibrationFile();
}

void CDataSetDoc::OnConfigureSensor() 
{
	StopBackgroundMeasures ();

	m_pSensor->Configure();
	if( m_pSensor->IsModified() )
	{
		m_measure.SetSensorMatrix(m_pSensor->GetSensorMatrix());
		SetModifiedFlag(TRUE);
		UpdateAllViews(NULL, UPD_SENSORCONFIG);
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

void CDataSetDoc::OnCalibrateSensor() 
{
	CalibrateSensor();
}

void CDataSetDoc::OnUpdateCalibrateSensor(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pSensor->SensorNeedCalibration () );
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
	if(MessageBox(NULL,Msg,Title,MB_ICONQUESTION | MB_YESNO) != IDYES)
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

		if(page.m_sensorTrainingMode == 1)
			CalibrateSensor(FALSE);
		else
			m_pSensor->LoadCalibrationFile(page.m_trainingFileName);

		m_measure.SetSensorMatrix(m_pSensor->GetSensorMatrix());
		
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
	CString	Msg, Title;
	CDataSetDoc *pDataRef = GetDataRef();

	if ( pDataRef == NULL )
	{
		// No data ref.
		Msg.LoadString ( IDS_SIM_CAL_ERROR1 );
		Title.LoadString ( IDS_ERROR );
		MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
		return;
	}

	if ( ! m_pSensor -> SensorAcceptCalibration () )
	{
		// Current sensor does not need calibration
		Msg.LoadString ( IDS_SIM_CAL_ERROR2 );
		Title.LoadString ( IDS_ERROR );
		MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
		return;
	}

	if ( pDataRef == this )
	{
		// Ref document cannot be current document
		Msg.LoadString ( IDS_SIM_CAL_ERROR3 );
		Title.LoadString ( IDS_ERROR );
		MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
		return;
	}

	Msg.LoadString ( IDS_RUN_SIM_CALIBRATION );
	if ( IDYES == AfxMessageBox ( Msg, MB_YESNO | MB_ICONQUESTION ) )
	{
		// Use special simultaneous mode
		PerformSimultaneousMeasures ( -1, 1 /* Calibration mode */ );
	}
}

void CDataSetDoc::OnUpdateCalibrationSim(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	//pCmdUI -> Enable ( m_pSensor -> SensorAcceptCalibration () );
}

void CDataSetDoc::OnCalibrationManual() 
{
	int		i;
	MSG		Msg;
	BOOL	bEscape, bReturn;
	CColor	measuredColor[4];
	CString	strMsg, Title;

	if ( ! m_pSensor -> SensorAcceptCalibration () )
	{
		// Current sensor does not need calibration
		strMsg.LoadString ( IDS_SIM_CAL_ERROR2 );
		Title.LoadString ( IDS_ERROR );
		MessageBox(NULL,strMsg,Title,MB_ICONERROR | MB_OK);
		return;
	}

	if ( IDYES == AfxMessageBox ( IDS_RUN_MANUAL_CALIBRATION, MB_YESNO | MB_ICONQUESTION ) )
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
			if( m_pGenerator->DisplayRGBColor(CIRELevel(primaryIRELevel,0,0,FALSE),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(CIRELevel(primaryIRELevel,0,0,FALSE));
				
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
			if( m_pGenerator->DisplayRGBColor(CIRELevel(0,primaryIRELevel,0,FALSE),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(CIRELevel(0,primaryIRELevel,0,FALSE));
				
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
			if( m_pGenerator->DisplayRGBColor(CIRELevel(0,0,primaryIRELevel,FALSE),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(CIRELevel(0,0,primaryIRELevel,FALSE));
				
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
			if( m_pGenerator->DisplayRGBColor(CIRELevel(primaryIRELevel,primaryIRELevel,primaryIRELevel,FALSE),CGenerator::MT_PRIMARY) )
			{
				bEscape = m_measure.WaitForDynamicIris ();
				bReturn = FALSE;

				if ( ! bEscape )
					measure=m_pSensor->MeasureColor(CIRELevel(primaryIRELevel,primaryIRELevel,primaryIRELevel,FALSE));
				
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
			Matrix measures(0.0,3,3);
			Matrix references(0.0,3,3);
			
			references(0,0) = dlg.m_RedColor.GetX();
			references(1,0) = dlg.m_RedColor.GetY();
			references(2,0) = dlg.m_RedColor.GetZ();
			references(0,1) = dlg.m_GreenColor.GetX();
			references(1,1) = dlg.m_GreenColor.GetY();
			references(2,1) = dlg.m_GreenColor.GetZ();
			references(0,2) = dlg.m_BlueColor.GetX();
			references(1,2) = dlg.m_BlueColor.GetY();
			references(2,2) = dlg.m_BlueColor.GetZ();

			measures(0,0) = measuredColor[0].GetX();  
			measures(1,0) = measuredColor[0].GetY();  
			measures(2,0) = measuredColor[0].GetZ();  
			measures(0,1) = measuredColor[1].GetX();
			measures(1,1) = measuredColor[1].GetY();
			measures(2,1) = measuredColor[1].GetZ();
			measures(0,2) = measuredColor[2].GetX(); 
			measures(1,2) = measuredColor[2].GetY(); 
			measures(2,2) = measuredColor[2].GetZ(); 
			
			CColor whiteRef = dlg.m_WhiteColor;
			
			CColor white = measuredColor[3];

			CColor blackRef = noDataColor;
			CColor black = noDataColor;

			// Calibrate sensor
			if ( m_pSensor -> CalibrateSensor ( measures, references, white, whiteRef, black, blackRef ) )
			{
				// Sensor calibrated: transfer sensor matrix to measure object
				m_measure.SetSensorMatrix(m_pSensor->GetSensorMatrix(), TRUE);
				SetModifiedFlag ();

				CColor aColor;

				aColor.SetSensorToXYZMatrix(m_pSensor->GetSensorMatrix());

				aColor.SetSensorValue(measuredColor[0]);
				m_measure.SetRedPrimary (aColor);

				aColor.SetSensorValue(measuredColor[1]);
				m_measure.SetGreenPrimary (aColor);

				aColor.SetSensorValue(measuredColor[2]);
				m_measure.SetBluePrimary (aColor);

				aColor.SetSensorValue(measuredColor[3]);
				m_measure.SetOnOffWhite (aColor);

				UpdateAllViews ( NULL, UPD_EVERYTHING );

				// Save file
				strMsg.LoadString ( IDS_SAVECALDATA );
				Title.LoadString ( IDS_CALIBRATION );
				if(GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
					m_pSensor->SaveCalibrationFile();
			}
		}
	}
}

void CDataSetDoc::OnUpdateCalibrationManual(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	//pCmdUI -> Enable ( m_pSensor -> SensorAcceptCalibration () );
}

BOOL CDataSetDoc::ComputeAdjustmentMatrix() 
{
	BOOL			bOk = FALSE;
	CDataSetDoc *	pDataRef = GetDataRef();
	
	ASSERT ( pDataRef && pDataRef != this && pDataRef -> m_measure.GetBluePrimary () != noDataColor && m_measure.GetBluePrimary () != noDataColor );

	// Create calibration data
	Matrix measures(0.0,3,3);
	Matrix references(0.0,3,3);
	
	references(0,0) = pDataRef -> m_measure.GetRedPrimary ().GetX();
	references(1,0) = pDataRef -> m_measure.GetRedPrimary ().GetY();
	references(2,0) = pDataRef -> m_measure.GetRedPrimary ().GetZ();
	references(0,1) = pDataRef -> m_measure.GetGreenPrimary ().GetX();
	references(1,1) = pDataRef -> m_measure.GetGreenPrimary ().GetY();
	references(2,1) = pDataRef -> m_measure.GetGreenPrimary ().GetZ();
	references(0,2) = pDataRef -> m_measure.GetBluePrimary ().GetX();
	references(1,2) = pDataRef -> m_measure.GetBluePrimary ().GetY();
	references(2,2) = pDataRef -> m_measure.GetBluePrimary ().GetZ();

	measures(0,0) = m_measure.GetRedPrimary (TRUE).GetX();  
	measures(1,0) = m_measure.GetRedPrimary (TRUE).GetY();  
	measures(2,0) = m_measure.GetRedPrimary (TRUE).GetZ();  
	measures(0,1) = m_measure.GetGreenPrimary (TRUE).GetX();
	measures(1,1) = m_measure.GetGreenPrimary (TRUE).GetY();
	measures(2,1) = m_measure.GetGreenPrimary (TRUE).GetZ();
	measures(0,2) = m_measure.GetBluePrimary (TRUE).GetX(); 
	measures(1,2) = m_measure.GetBluePrimary (TRUE).GetY(); 
	measures(2,2) = m_measure.GetBluePrimary (TRUE).GetZ(); 
	
	CColor whiteRef = pDataRef -> m_measure.GetOnOffWhite();
	
	CColor white = m_measure.GetOnOffWhite(TRUE);

	// check that measure matrix is inversible
	if ( measures.Determinant() != 0.0 ) 
	{
		Matrix ConvMatrix = ComputeConversionMatrix (measures, references, white, whiteRef, GetConfig () -> m_bUseOnlyPrimaries );

		// check that matrix is inversible
		if ( ConvMatrix.Determinant() != 0.0 )	
		{
			// Ok: set adjustment matrix
			m_measure.SetAdjustmentMatrix ( ConvMatrix );
			SetModifiedFlag(TRUE);

			if ( white != noDataColor )
			{
				CString	str;
				CColor	blackRef = noDataColor;
				CColor	black = noDataColor;

				CCalibrationInfo info ( measures, references, white, whiteRef, black, blackRef, str );
				info.GetAdditivityInfoText ( m_measure.m_XYZAdjustmentComment, ConvMatrix, FALSE );
			}
			else
			{
				m_measure.m_XYZAdjustmentComment.Empty ();
			}

			bOk = TRUE;
		}
		else
		{
			AfxMessageBox ( IDS_INVALIDMEASUREMATRIX, MB_OK | MB_ICONERROR );
		}
	}
	else
	{
		AfxMessageBox ( IDS_INVALIDMEASUREMATRIX, MB_OK | MB_ICONERROR );
	}

	return bOk;
}

void CDataSetDoc::OnTrainMeter() 
{
	CDataSetDoc * pDataRef = GetDataRef();
	
	if ( pDataRef )
	{
		if ( pDataRef != this ) 
		{
			if ( pDataRef -> m_measure.GetBluePrimary () != noDataColor && m_measure.GetBluePrimary () != noDataColor )
			{
				if ( IDYES == AfxMessageBox ( IDS_CONFIRMTRAINING, MB_YESNO | MB_ICONQUESTION ) )
				{
					// Create calibration data
					if ( ComputeAdjustmentMatrix() )
					{
						m_measure.EnableAdjustmentMatrix ( TRUE );
						UpdateAllViews ( NULL, UPD_EVERYTHING );
						AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );
					}
				} 
			}
			else
			{
				AfxMessageBox ( IDS_TRAININGNEEDPRIMARY, MB_OK | MB_ICONEXCLAMATION );
			}
		}
		else
		{
			AfxMessageBox ( IDS_TRAININGNOTREF, MB_OK | MB_ICONEXCLAMATION );
		}
	}
	else
	{
		AfxMessageBox ( IDS_TRAININGNEEDREF, MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnResetConversionMatrix() 
{
	if ( m_measure.HasAdjustmentMatrix() )
	{
		if ( IDYES == AfxMessageBox ( IDS_CANCELTRAINING, MB_YESNO | MB_ICONQUESTION ) )
		{
			m_measure.ResetAdjustmentMatrix();
			SetModifiedFlag ();

			UpdateAllViews ( NULL, UPD_EVERYTHING );
		}
	}
}

void CDataSetDoc::OnUpdateResetConversionMatrix(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_measure.HasAdjustmentMatrix() );
}

void CDataSetDoc::OnEditConversionMatrix() 
{
	CAdjustMatrixDlg	dlg;

	dlg.m_matrix = m_measure.GetAdjustmentMatrix();
	dlg.m_strComment = m_measure.m_XYZAdjustmentComment;

	if ( dlg.DoModal () == IDOK )
	{
		m_measure.m_XYZAdjustmentComment = dlg.m_strComment;
		m_measure.SetAdjustmentMatrix(dlg.m_matrix);
		m_measure.EnableAdjustmentMatrix ( m_measure.HasAdjustmentMatrix() );
		SetModifiedFlag ();

		UpdateAllViews ( NULL, UPD_EVERYTHING );
		AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );
	}
	
}
void CDataSetDoc::OnSimGrayscale() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASUREGRAYSCALE_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 0 );
}

void CDataSetDoc::OnSimGrayscaleAndColors() 
{
	if ( m_pGenerator -> CanDisplayGrayAndColorsSeries () )
	{
		if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASUREGRAYANDCOLORS, MB_YESNO | MB_ICONQUESTION ) )
			PerformSimultaneousMeasures ( -3 );
	}
	else
	{
		CString	Msg;
		Msg.Format ( IDS_CANNOTMEASUREGRAYANDCOLORS, (LPCSTR) m_pGenerator -> GetName () );
		AfxMessageBox ( Msg, MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnSimPrimaries() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASUREPRIMARIES_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( -1 );
}

void CDataSetDoc::OnSimSecondaries() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURESECONDARIES_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( -2 );
}

void CDataSetDoc::OnSimNearblack() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURENEARBLACK_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 1 );
}

void CDataSetDoc::OnSimNearwhite() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURENEARWHITE_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 2 );
}

void CDataSetDoc::OnSimSatRed() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURESATRED_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 3 );
}

void CDataSetDoc::OnSimSatGreen() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURESATGREEN_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 4 );
}

void CDataSetDoc::OnSimSatBlue() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURESATBLUE_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 5 );
}

void CDataSetDoc::OnSimSatYellow() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURESATYELLOW_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 6 );
}

void CDataSetDoc::OnSimSatCyan() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURESATCYAN_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 7 );
}

void CDataSetDoc::OnSimSatMagenta() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURESATMAGENTA_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( 8 );
}

void CDataSetDoc::OnSimSingleMeasurement() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_MEASURESINGLE_SIM, MB_YESNO | MB_ICONQUESTION ) )
		PerformSimultaneousMeasures ( -4 );
}

void CDataSetDoc::PerformSimultaneousMeasures ( int nMode, UINT nCalibrationOrControlled )
{
	int				i, j;
	int				NbDocs = 0;
	int				nSteps = 0;
	int				nMaxSteps;
	int				GenIRE [ 256 ];
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
	COLORREF		GenColors [ 256 ];
	CGenerator::MeasureType	mType[256];

	double			dLuxValue;
	BOOL			bUseLuxValues = TRUE;
	double			measuredLux [ 256 ];

	MSG				message;

	BOOL (CMeasure::*pValidationFunc) ( BOOL, double * ) = NULL;
	
	// Calibration mode available only when measuring primary colors
	if ( nCalibrationOrControlled == 1 && nMode != -1 )
	{
		AfxMessageBox ( "Internal error" );
		return;
	}

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
				GenColors [ i ] = CIRELevel(i,i,i,FALSE, m_pGenerator->m_b16_235);
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
				GenColors [ i ] = CIRELevel(101-nSteps+i,101-nSteps+i,101-nSteps+i,FALSE, m_pGenerator->m_b16_235);
				mType [ i ] = CGenerator::MT_NEARWHITE;
			 }
			 pValidationFunc = &CMeasure::ValidateBackgroundNearWhite;
			 lHint = UPD_NEARWHITE;
			 break;

		case 3:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_RED;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, TRUE, FALSE, FALSE, m_pGenerator->m_b16_235 );
			 pValidationFunc = &CMeasure::ValidateBackgroundRedSatScale;
			 lHint = UPD_REDSAT;
			 break;

		case 4:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_GREEN;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, FALSE, TRUE, FALSE, m_pGenerator->m_b16_235 );
			 pValidationFunc = &CMeasure::ValidateBackgroundGreenSatScale;
			 lHint = UPD_GREENSAT;
			 break;

		case 5:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_BLUE;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, FALSE, FALSE, TRUE, m_pGenerator->m_b16_235 );
			 pValidationFunc = &CMeasure::ValidateBackgroundBlueSatScale;
			 lHint = UPD_BLUESAT;
			 break;

		case 6:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_YELLOW;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, TRUE, TRUE, FALSE, m_pGenerator->m_b16_235 );
			 pValidationFunc = &CMeasure::ValidateBackgroundYellowSatScale;
			 lHint = UPD_YELLOWSAT;
			 break;

		case 7:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_CYAN;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, FALSE, TRUE, TRUE, m_pGenerator->m_b16_235 );
			 pValidationFunc = &CMeasure::ValidateBackgroundCyanSatScale;
			 lHint = UPD_CYANSAT;
			 break;

		case 8:
			 nSteps = GetMeasure () -> GetSaturationSize ();
			 nMaxSteps = nSteps;
			 mType [ 0 ] = CGenerator::MT_SAT_MAGENTA;
			 GenerateSaturationColors (GetColorReference(), GenColors, nSteps, TRUE, FALSE, TRUE, m_pGenerator->m_b16_235 );
			 pValidationFunc = &CMeasure::ValidateBackgroundMagentaSatScale;
			 lHint = UPD_MAGENTASAT;
			 break;

		case -1:
			 mType [ 0 ] = CGenerator::MT_PRIMARY;
			 mType [ 1 ] = CGenerator::MT_PRIMARY;
			 mType [ 2 ] = CGenerator::MT_PRIMARY;
			 mType [ 3 ] = CGenerator::MT_PRIMARY;
			 mType [ 4 ] = CGenerator::MT_PRIMARY;
			 nSteps = ( 3 + GetConfig () -> m_BWColorsToAdd );
			 nMaxSteps = 5;
			 if ( nCalibrationOrControlled == 1 && nSteps < 4 )
				nSteps = 4;
			 GenColors [ 0 ] = CIRELevel(100,0,0,FALSE, m_pGenerator->m_b16_235);		// red
			 GenColors [ 1 ] = CIRELevel(0,100,0,FALSE, m_pGenerator->m_b16_235);		// green
			 GenColors [ 2 ] = CIRELevel(0,0,100,FALSE, m_pGenerator->m_b16_235);		// blue
			 GenColors [ 3 ] = CIRELevel(100,100,100,FALSE, m_pGenerator->m_b16_235);	// white (to control additivity)
			 GenColors [ 4 ] = CIRELevel(0,0,0,FALSE, m_pGenerator->m_b16_235);			// black
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
			 GenColors [ 0 ] = CIRELevel(100,0,0,FALSE, m_pGenerator->m_b16_235);		// red
			 GenColors [ 1 ] = CIRELevel(0,100,0,FALSE, m_pGenerator->m_b16_235);		// green
			 GenColors [ 2 ] = CIRELevel(0,0,100,FALSE, m_pGenerator->m_b16_235);		// blue
			 GenColors [ 3 ] = CIRELevel(100,100,0,FALSE, m_pGenerator->m_b16_235);		// yellow
			 GenColors [ 4 ] = CIRELevel(0,100,100,FALSE, m_pGenerator->m_b16_235);		// cyan
			 GenColors [ 5 ] = CIRELevel(100,0,100,FALSE, m_pGenerator->m_b16_235);		// magenta
			 GenColors [ 6 ] = CIRELevel(100,100,100,FALSE, m_pGenerator->m_b16_235);	// white (to control additivity)
			 GenColors [ 7 ] = CIRELevel(0,0,0,FALSE, m_pGenerator->m_b16_235);			// black
			 pValidationFunc = &CMeasure::ValidateBackgroundSecondaries;
			 lHint = UPD_SECONDARIES;
			 break;

		case -3:
			 nSteps = GetMeasure () -> GetGrayScaleSize ();
			 for (i = 0; i < nSteps ; i ++ )
			 {
				GenIRE [ i ] = (int)ArrayIndexToGrayLevel ( i, nSteps, GetMeasure () -> m_bIREScaleMode );
				bIRE [ i ] = GetMeasure () -> m_bIREScaleMode;
				GenColors [ i ] = CIRELevel(GenIRE[i],bIRE[i], m_pGenerator->m_b16_235);
			 	mType [ i ] = CGenerator::MT_IRE;
			 }

			 GenColors [ nSteps + 0 ] = CIRELevel(100,0,0,FALSE, m_pGenerator->m_b16_235);	// red
			 mType [ nSteps + 0 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 1 ] = CIRELevel(0,100,0,FALSE, m_pGenerator->m_b16_235);	// green
			 mType [ nSteps + 1 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 2 ] = CIRELevel(0,0,100,FALSE, m_pGenerator->m_b16_235);	// blue
			 mType [ nSteps + 2 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 3 ] = CIRELevel(100,100,0,FALSE, m_pGenerator->m_b16_235);	// yellow
			 mType [ nSteps + 3 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 4 ] = CIRELevel(0,100,100,FALSE, m_pGenerator->m_b16_235);	// cyan
			 mType [ nSteps + 4 ] = CGenerator::MT_SECONDARY;
			 GenColors [ nSteps + 5 ] = CIRELevel(100,0,100,FALSE, m_pGenerator->m_b16_235);	// magenta
			 mType [ nSteps + 5 ] = CGenerator::MT_SECONDARY;
			 nSteps += 6;
			 nMaxSteps = nSteps;
			 pValidationFunc = &CMeasure::ValidateBackgroundGrayScaleAndColors;
			 lHint = UPD_GRAYSCALEANDCOLORS;
			 break;

		case -4:
			 nSteps = 1;
			 nMaxSteps = nSteps;
			 GenColors [ 0 ] = ( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.GetColor () & 0x00FFFFFF;
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
				GenIRE [ i ] = (int)ArrayIndexToGrayLevel ( i, nSteps, GetMeasure () -> m_bIREScaleMode );
				bIRE [ i ] = GetMeasure () -> m_bIREScaleMode;
				GenColors [ i ] = CIRELevel(GenIRE[i],bIRE[i], m_pGenerator->m_b16_235);
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

	if ( nCalibrationOrControlled != 0 )
	{
		// Calibration mode (1) or controlled mode (2)
		// take only reference document as secondary document
		pDoc = GetDataRef ();
		if ( pDoc )
			pDocs [ NbDocs ++ ] = pDoc;
		else
		{
			AfxMessageBox ( "Internal error" );
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
						MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
					}
				}
				else
				{
					bOk = FALSE;
					Msg.Format ( IDS_MANYDOCSUSESAMESENSOR, (LPCSTR) strId );
					Title.LoadString ( IDS_ERROR );
					MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
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
		MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);
	}

	if ( nCalibrationOrControlled != 1 )
	{
		// Ensure all sensors are calibrated
		for ( i	= 0; bOk && i < NbDocs ; i ++ )
		{
			if( ! pDocs [ i ] -> m_pSensor -> IsCalibrated () )
				if ( pDocs [ i ] -> m_pSensor -> CalibrateSensor ( m_pGenerator ) != TRUE )
					bOk = FALSE;
		}
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
				MessageBox(NULL,Msg,Title,MB_ICONERROR | MB_OK);

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
					bresult = pGenerator -> DisplayGray ( GenIRE [ i ], bIRE [ i ], mType[i] );
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

			if ( nCalibrationOrControlled == 2 && ( nMode > -4 && nMode < 0 ) )
			{
				// Primary colors updated: update XYZ conversion matrix
				BOOL bHadAdjustementMatrix = m_measure.HasAdjustmentMatrix();
				if ( ComputeAdjustmentMatrix() )
				{
					if ( ! bHadAdjustementMatrix )
					{
						m_measure.EnableAdjustmentMatrix ( TRUE );
						UpdateAllViews ( NULL, UPD_EVERYTHING, NULL );
						AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );
					}
				}
			}

			for ( i	= 0 ; i < NbDocs ; i ++ )
			{
				// Update graphical views
				pDocs [ i ] -> UpdateAllViews ( NULL, lHint );
			}
			
			// Clear information window
			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls

			if ( nCalibrationOrControlled == 1 )
			{
				// Create calibration data
				Matrix measures(0.0,3,3);
				Matrix references(0.0,3,3);
				
				pDoc = GetDataRef ();
				references(0,0) = pDoc -> m_measure.GetRedPrimary ().GetX();
				references(1,0) = pDoc -> m_measure.GetRedPrimary ().GetY();
				references(2,0) = pDoc -> m_measure.GetRedPrimary ().GetZ();
				references(0,1) = pDoc -> m_measure.GetGreenPrimary ().GetX();
				references(1,1) = pDoc -> m_measure.GetGreenPrimary ().GetY();
				references(2,1) = pDoc -> m_measure.GetGreenPrimary ().GetZ();
				references(0,2) = pDoc -> m_measure.GetBluePrimary ().GetX();
				references(1,2) = pDoc -> m_measure.GetBluePrimary ().GetY();
				references(2,2) = pDoc -> m_measure.GetBluePrimary ().GetZ();

				measures(0,0) = m_measure.GetRedPrimary ().GetSensorValue().GetX();  
				measures(1,0) = m_measure.GetRedPrimary ().GetSensorValue().GetY();  
				measures(2,0) = m_measure.GetRedPrimary ().GetSensorValue().GetZ();  
				measures(0,1) = m_measure.GetGreenPrimary ().GetSensorValue().GetX();
				measures(1,1) = m_measure.GetGreenPrimary ().GetSensorValue().GetY();
				measures(2,1) = m_measure.GetGreenPrimary ().GetSensorValue().GetZ();
				measures(0,2) = m_measure.GetBluePrimary ().GetSensorValue().GetX(); 
				measures(1,2) = m_measure.GetBluePrimary ().GetSensorValue().GetY(); 
				measures(2,2) = m_measure.GetBluePrimary ().GetSensorValue().GetZ(); 
				
				CColor whiteRef = pDoc -> m_measure.GetOnOffWhite();
				CColor blackRef = pDoc -> m_measure.GetOnOffBlack();
				
				CColor white = m_measure.GetOnOffWhite().GetSensorValue();
				CColor black = m_measure.GetOnOffBlack().GetSensorValue();

				// Calibrate sensor
				if ( m_pSensor -> CalibrateSensor ( measures, references, white, whiteRef, black, blackRef ) )
				{
					// Sensor calibrated: transfer sensor matrix to measure object
					m_measure.SetSensorMatrix(m_pSensor->GetSensorMatrix(), TRUE);
					UpdateAllViews ( NULL, UPD_EVERYTHING );

					// Save file
					Msg.LoadString ( IDS_SAVECALDATA );
					Title.LoadString ( IDS_CALIBRATION );
					if(GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
						m_pSensor->SaveCalibrationFile();
				}
			}
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

void CDataSetDoc::OnPatternAnimBlack() 
{
	BOOL	bKeyTyped = FALSE;
	MSG		Msg;

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
		if ( IDYES == AfxMessageBox ( IDS_DISPLAYANIMATEDPATTERN, MB_YESNO | MB_ICONQUESTION ) )
		{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayAnimatedBlack();

			while ( ! bKeyTyped )
			{
				while ( PeekMessage ( & Msg, NULL, WM_KEYFIRST, WM_MOUSELAST, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN )
					{
						if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN )
							bKeyTyped = TRUE;
					}
					else if ( Msg.message == WM_LBUTTONDOWN || Msg.message == WM_RBUTTONDOWN )
					{
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
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
		}
	}
	else
	{
		AfxMessageBox ( IDS_CANNOTDISPLAYANIMATION, MB_OK | MB_ICONEXCLAMATION );
	}
}

void CDataSetDoc::OnPatternAnimWhite() 
{
	BOOL	bKeyTyped = FALSE;
	MSG		Msg;

	if ( m_pGenerator -> CanDisplayAnimatedPatterns() )
	{
		if ( IDYES == AfxMessageBox ( IDS_DISPLAYANIMATEDPATTERN, MB_YESNO | MB_ICONQUESTION ) )
		{
			AfxGetMainWnd () -> EnableWindow ( FALSE );

			m_pGenerator->Init();
			m_pGenerator->DisplayAnimatedWhite();

			while ( ! bKeyTyped )
			{
				while ( PeekMessage ( & Msg, NULL, WM_KEYFIRST, WM_MOUSELAST, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN )
					{
						if ( Msg.wParam == VK_ESCAPE || Msg.wParam == VK_RETURN )
							bKeyTyped = TRUE;
					}
					else if ( Msg.message == WM_LBUTTONDOWN || Msg.message == WM_RBUTTONDOWN )
					{
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
			
			AfxGetMainWnd () -> EnableWindow ( TRUE );
			m_pGenerator->Release();
		}
	}
	else
	{
		AfxMessageBox ( IDS_CANNOTDISPLAYANIMATION, MB_OK | MB_ICONEXCLAMATION );
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
		StopBackgroundMeasures ();
	else
		StartBackgroundMeasures ( this );
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

void CDataSetDoc::ComputeGammaAndOffset(double * Gamma, double * Offset, int ColorSpace,int ColorIndex,int Size)
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
		blacklvl=GetMeasure()->GetGray(0).GetRGBValue(GetColorReference())[ColorIndex];
		whitelvl=GetMeasure()->GetGray(Size-1).GetRGBValue(GetColorReference())[ColorIndex];
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
			// When luxmeter values are authorized in curves, use preference for contrast/delta luma
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
			graylvl=GetMeasure()->GetGray(i).GetRGBValue(GetColorReference())[ColorIndex];
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
/*			if(Offset_opt == 0 && GetConfig()->m_colorStandard == sRGB)
				Offset_opt = 0.055;
			else
				Offset_opt = 0.099;
*/			break;
		case 4:
			{
				int		i;
				double	deltamin = -1;
				double	offsetMin = 0;
				double	offsetMax = 0.200;
				double	offsetStep = 0.01;
				double	gammaMin = 1.5;
				double	gammaMax = 4.0;
				double	gammaStep = 0.1;
				double	offset, offset_plus_one;

				for (i=0; i<Size; i++)
				{
					double x = ArrayIndexToGrayLevel ( i, Size, bIRE );
					tmpx[i] = GrayLevelToGrayProp(x,bIRE);
				}

				for (int nbopt=0; nbopt <2; nbopt ++)
				{
					for (offset = offsetMin; offset < offsetMax; offset += offsetStep)
					{
						offset_plus_one = offset + 1.0;
						for (i=0; i<Size; i++)
						{
							valx[i] = (tmpx[i]+offset) / offset_plus_one;
						}
		
						for (double gamma = gammaMin; gamma < gammaMax; gamma += gammaStep)
						{
							double	sum = 0;
							for (i=0; i<Size; i++)
							{		
								double valy, ecart;
								valy=pow(valx[i], gamma);
								ecart = (lumlvl[i] - valy);
								sum += ecart * ecart;
							}
							sum /= ( Size ? Size : 1 );
							if ((sum < deltamin) || (deltamin < 0))
							{
								deltamin = sum;
								Gamma_opt = gamma;
								Offset_opt = offset;
							}
						}	
					}
					offsetMin = Offset_opt - offsetStep;
					offsetMax = Offset_opt + offsetStep;
					gammaMin = Gamma_opt - gammaStep;
					gammaMax = Gamma_opt + gammaStep;
					offsetStep /= 10.0;
					gammaStep /= 10.0;
				}
			}
			break;
		}

		if ( nConfigOffsetType == 0 || nConfigOffsetType == 1 || nConfigOffsetType == 2 || nConfigOffsetType == 3 )
		{
			double x, v;

			for (int i=0; i<Size; i++)
			{
				x = ArrayIndexToGrayLevel ( i, Size, bIRE );
				v = GrayLevelToGrayProp(x,bIRE);

				valx[i]=(v+Offset_opt)/(1.0+Offset_opt);
			}

			int	nb = 0;
			double avg = 0;
			for (int i=1; i<Size-1; i++)
			{
				if ( lumlvl[i] > 0.0 )
				{
					avg += log(lumlvl[i])/log(valx[i]);
					nb ++;
				}
			}
			Gamma_opt = avg / ( nb ? nb : 1 );
		}
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
		while ( pos )
		{
			pMeasurement = ( CColor * ) g_MeasuredColorList.GetNext ( pos );

			m_measure.AppendMeasurements(*pMeasurement);
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
	int		nNbPoints = GetMeasure () -> GetGrayScaleSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNGRAYSCALEON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 0, 2 /* controlled mode */ );
		else
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
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_RUNPRIMARIES, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( -1, 2 /* controlled mode */ );
		else
			MeasurePrimaries();
		
		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasurePrimaries(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CDataSetDoc::OnMeasureSecondaries() 
{
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( IDS_RUNSECONDARIES, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( -2, 2 /* controlled mode */ );
		else
			MeasureSecondaries();
		
		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureSecondaries(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}


void CDataSetDoc::OnMeasureNearblack() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetNearBlackScaleSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNNEARBLACKON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 1, 2 /* controlled mode */ );
		else
			MeasureNearBlackScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureNearblack(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_NEARBLACK, GetMeasure () -> GetNearBlackScaleSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureNearwhite() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetNearWhiteScaleSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNNEARWHITEON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 2, 2 /* controlled mode */ );
		else
			MeasureNearWhiteScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureNearwhite(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_NEARWHITE, GetMeasure () -> GetNearWhiteScaleSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatRed() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetSaturationSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNSATREDON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 3, 2 /* controlled mode */ );
		else
			MeasureRedSatScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureSatRed(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_RED, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatGreen() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetSaturationSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNSATGREENON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 4, 2 /* controlled mode */ );
		else
			MeasureGreenSatScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureSatGreen(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_GREEN, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatBlue() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetSaturationSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNSATBLUEON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 5, 2 /* controlled mode */ );
		else
			MeasureBlueSatScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureSatBlue(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_BLUE, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatYellow() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetSaturationSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNSATYELLOWON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 6, 2 /* controlled mode */ );
		else
			MeasureYellowSatScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureSatYellow(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_YELLOW, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatCyan() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetSaturationSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNSATCYANON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 7, 2 /* controlled mode */ );
		else
			MeasureCyanSatScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureSatCyan(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_CYAN, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureSatMagenta() 
{
	CString	Msg, MsgQueue, TmpStr;
	int		nNbPoints = GetMeasure () -> GetSaturationSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNSATMAGENTAON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( 8, 2 /* controlled mode */ );
		else
			MeasureMagentaSatScale();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureSatMagenta(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_MAGENTA, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnMeasureContrast() 
{
	CString	Msg;
	
	Msg.LoadString ( IDS_RUNCONTRAST );
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
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
	int		nNbPoints = GetMeasure () -> GetSaturationSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNALLSATON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		MeasureAllSaturationScales();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
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
	int		nNbPoints = GetMeasure () -> GetGrayScaleSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_MEASUREGRAYANDCOLORSON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		if ( IsControlledModeActive () )
			PerformSimultaneousMeasures ( -3, 2 /* controlled mode */ );
		else
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
	int		nNbPoints = GetMeasure () -> GetSaturationSize () - 1;
	
	MsgQueue.LoadString ( IDS_RUNQUEUEWARNING );

	Msg.LoadString ( IDS_RUNPRIMSATON );
	TmpStr.Format ( " %d ", nNbPoints );
	Msg += TmpStr + MsgQueue;
	if ( ! GetConfig()->m_bConfirmMeasures || IDYES == AfxMessageBox ( Msg, MB_ICONQUESTION | MB_YESNO ) )
	{
		MeasurePrimarySaturationScales();

		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CDataSetDoc::OnUpdateMeasureSatPrimaries(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pGenerator -> CanDisplayScale ( CGenerator::MT_SAT_ALL, GetMeasure () -> GetSaturationSize(), TRUE ) );
}

void CDataSetDoc::OnSaveCalibrationFile() 
{
	m_pSensor -> SaveCalibrationFile ();
}

void CDataSetDoc::OnUpdateSaveCalibrationFile(CCmdUI* pCmdUI) 
{
	pCmdUI -> Enable ( m_pSensor -> SensorAcceptCalibration () && m_pSensor -> IsCalibrated () );
}


