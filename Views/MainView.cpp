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

// MainView.cpp : implementation of the CMainView class
//
#include "stdafx.h"
#include "ColorHCFR.h"

#include "DataSetDoc.h"
#include "DocTempl.h"
#include "MainView.h"
#include "Tools/GridCtrl/GridCtrl.h"
#include "MainFrm.h"
#include "MultiFrm.h"
#include "GDIGenerator.h"
#include "LuminanceHistoView.h"
#include "NearBlackHistoView.h"
#include "GammaHistoView.h"
#include "RGBHistoView.h"
#include "ColorTempHistoView.h"
#include "CIEChartView.h"
#include "MeasuresHistoView.h"
#include "SpectrumDlg.h"
#include "../ColorHCFRConfig.h"
#include "../Measure.h"

#include "DocEnumerator.h"	//Ki
#include <math.h>
#include <algorithm>
#include <vector>
#include <afxpriv.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define WM_SET_USER_INFO_POST_INIT	WM_USER+99

#define		LAYOUT_LEFT			0
#define		LAYOUT_RIGHT		1
#define		LAYOUT_TOP			0
#define		LAYOUT_BOTTOM		1
#define		LAYOUT_TOP_OFFSET	2

struct SCtrlLayout
{
	int		m_nCtrlID;
	int		m_LeftMode;
	int		m_RightMode;
	int		m_TopMode;
	int		m_BottomMode;
};

struct SCtrlInitPos
{
 public:
	HWND				m_hWnd;
	RECT				m_Rect;
	const SCtrlLayout *	m_pLayout;
};

static const SCtrlLayout g_CtrlLayout [] = {
{ IDC_PARAM_GROUP						, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_GRAYSCALESTEPS_COMBOMODE			, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_EDITGRID_CHECK					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_SPIN_VIEW							, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_GRAYSCALE_GROUP					, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP_OFFSET	}, 
{ IDC_VALUES_STATIC						, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP_OFFSET	}, 
{ IDC_GRAYSCALE_GRID					, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP_OFFSET	}, 
{ IDC_MEASUREGRAYSCALE_BUTTON			, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_DELETEGRAYSCALE_BUTTON			, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
																												
{ IDC_DISPLAY_GROUP						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP_OFFSET	},
{ IDC_SENSORRGB_RADIO					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_RGB_RADIO							, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_XYZ_RADIO							, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_XYZ_RADIO2						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_XYY_RADIO 						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
																												
{ IDC_SENSOR_GROUP						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_SENSORNAME_STATIC					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_SENSORNAME_STATIC2					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDM_CONFIGURE_SENSOR					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,			LAYOUT_TOP_OFFSET			},
{ IDM_CONFIGURE_SENSOR2					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,			LAYOUT_TOP_OFFSET			},
{ IDC_GENERATOR_GROUP					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_GENERATORNAME_STATIC				, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDM_CONFIGURE_GENERATOR				, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,			LAYOUT_TOP_OFFSET			},
{ IDC_DATAREF_GROUP						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_DATAREF_CHECK						, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
{ IDC_ADJUSTXYZ_CHECK					, LAYOUT_RIGHT,	LAYOUT_RIGHT,	LAYOUT_TOP,			LAYOUT_TOP			},
																												
{ IDC_SELECTION_GROUP					, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_COLORDATA_STATIC					, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_COLORDATA_GRID					, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_VIEW_GROUP 						, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_INFO_DISPLAY						, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_STATIC_VIEW						, LAYOUT_LEFT,	LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,	LAYOUT_BOTTOM		},
{ IDC_STATIC_RGBLEVELS					, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_RGBLEVELS							, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_STATIC_TARGET						, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_TARGET							, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_STATIC_DATA						, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_RGBLEVELS2						, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_TARGET2							, LAYOUT_LEFT,	LAYOUT_LEFT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	},
{ IDC_ANSICONTRAST_PATTERN_TEST_BUTTON  , LAYOUT_RIGHT, LAYOUT_RIGHT,	LAYOUT_TOP_OFFSET,	LAYOUT_TOP_OFFSET	}

};

                    char*  PatName[96]={
                    "White",
                    "6J",
                    "5F",
                    "6I",
                    "6K",
                    "5G",
                    "6H",
                    "5H",
                    "7K",
                    "6G",
                    "5I",
                    "6F",
                    "8K",
                    "5J",
                    "Black",
                    "2B",
                    "2C",
                    "2D",
                    "2E",
                    "2F",
                    "2G",
                    "2H",
                    "2I",
                    "2J",
                    "2K",
                    "2L",
                    "2M",
                    "3B",
                    "3C",
                    "3D",
                    "3E",
                    "3F",
                    "3G",
                    "3H",
                    "3I",
                    "3J",
                    "3K",
                    "3L",
                    "3M",
                    "4B",
                    "4C",
                    "4D",
                    "4E",
                    "4F",
                    "4G",
                    "4H",
                    "4I",
                    "4J",
                    "4K",
                    "4L",
                    "4M",
                    "5B",
                    "5C",
                    "5D",
                    "5K",
                    "5L",
                    "5M",
                    "6B",
                    "6C",
                    "6D",
                    "6L",
                    "6M",
                    "7B",
                    "7C",
                    "7D",
                    "7E",
                    "7F",
                    "7G",
                    "7H",
                    "7I",
                    "7J",
                    "7L",
                    "7M",
                    "8B",
                    "8C",
                    "8D",
                    "8E",
                    "8F",
                    "8G",
                    "8H",
                    "8I",
                    "8J",
                    "8L",
                    "8M",
                    "9B",
                    "9C",
                    "9D",
                    "9E",
                    "9F",
                    "9G",
                    "9H",
                    "9I",
                    "9J",
                    "9K",
                    "9L",
                    "9M" };
                    char*  PatNameCMS[19]={
						"White",
						"Black",
						"2E",
						"2F",
						"2K",
						"5D",
						"7E",
						"7F",
						"7G",
						"7H",
						"7I",
						"7J",
						"8D",
						"8E",
						"8F",
						"8G",
						"8H",
						"8I",
						"8J" };
                    char*  PatNameCPS[19]={
						"White",
						"D7",
						"D8",
						"E7",
						"E8",
						"F7",
						"F8",
						"G7",
						"G8",
						"H7",
						"H8",
						"I7",
						"I8",
						"J7",
						"J8",
						"CP-Light",
						"CP-Dark",
						"Dark Skin",
						"Light Skin" };
                    char*  PatNameAXIS[71]={
						"Black",
						"White 10",
						"White 20",
						"White 30",
						"White 40",
						"White 50",
						"White 60",
						"White 70",
						"White 80",
						"White 90",
						"White 100",
						"Red 10",
						"Red 20",
						"Red 30",
						"Red 40",
						"Red 50",
						"Red 60",
						"Red 70",
						"Red 80",
						"Red 90",
						"Red 100",
						"Green 10",
						"Green 20",
						"Green 30",
						"Green 40",
						"Green 50",
						"Green 60",
						"Green 70",
						"Green 80",
						"Green 90",
						"Green 100",
						"Blue 10",
						"Blue 20",
						"Blue 30",
						"Blue 40",
						"Blue 50",
						"Blue 60",
						"Blue 70",
						"Blue 80",
						"Blue 90",
						"Blue 100", 
						"Cyan 10",
						"Cyan 20",
						"Cyan 30",
						"Cyan 40",
						"Cyan 50",
						"Cyan 60",
						"Cyan 70",
						"Cyan 80",
						"Cyan 90",
						"Cyan 100", 
						"Magenta 10",
						"Magenta 20",
						"Magenta 30",
						"Magenta 40",
						"Magenta 50",
						"Magenta 60",
						"Magenta 70",
						"Magenta 80",
						"Magenta 90",
						"Magenta 100", 
						"Yellow 10",
						"Yellow 20",
						"Yellow 30",
						"Yellow 40",
						"Yellow 50",
						"Yellow 60",
						"Yellow 70",
						"Yellow 80",
						"Yellow 90",
						"Yellow 100"
					};


/////////////////////////////////////////////////////////////////////////////
// CMainView

IMPLEMENT_DYNCREATE(CMainView, CFormView)

BEGIN_MESSAGE_MAP(CMainView, CFormView)
	//{{AFX_MSG_MAP(CMainView)
	ON_BN_CLICKED(IDC_XYZ_RADIO, OnXyzRadio)
	ON_BN_CLICKED(IDC_SENSORRGB_RADIO, OnSensorrgbRadio)
	ON_BN_CLICKED(IDC_RGB_RADIO, OnRgbRadio)
	ON_BN_CLICKED(IDC_XYZ_RADIO2, OnXyz2Radio)
	ON_BN_CLICKED(IDC_XYY_RADIO, OnxyYRadio)
	ON_CBN_SELCHANGE(IDC_GRAYSCALESTEPS_COMBOMODE, OnSelchangeComboMode)
	ON_BN_CLICKED(IDC_EDITGRID_CHECK, OnEditgridCheck)
	ON_BN_CLICKED(IDC_DATAREF_CHECK, OnDatarefCheck)
	ON_BN_CLICKED(IDC_ADJUSTXYZ_CHECK, OnAdjustXYZCheck)
	ON_BN_CLICKED(IDC_INIT_BUTTON, OnInitDefaults)
	ON_WM_ERASEBKGND()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED(IDC_MEASUREGRAYSCALE_BUTTON, OnMeasureGrayScale)
	ON_BN_CLICKED(IDC_DELETEGRAYSCALE_BUTTON, OnDeleteGrayscale)
	ON_WM_SIZE()
	ON_CBN_SELCHANGE(IDC_INFO_DISPLAY, OnSelchangeInfoDisplay)
	ON_COMMAND(IDM_HELP, OnHelp)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_VIEW, OnDeltaposSpinView)
	ON_BN_CLICKED(IDC_ANSICONTRAST_PATTERN_TEST_BUTTON, OnAnsiContrastPatternTestButton)
	ON_EN_CHANGE(IDC_INFO_VIEW, OnChangeInfosEdit)
	//}}AFX_MSG_MAP
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CFormView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CFormView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CFormView::OnFilePrintPreview)


	ON_NOTIFY(GVN_BEGINLABELEDIT, IDC_GRAYSCALE_GRID, OnGrayScaleGridBeginEdit)
	ON_NOTIFY(GVN_ENDLABELEDIT, IDC_GRAYSCALE_GRID, OnGrayScaleGridEndEdit)
	ON_NOTIFY(GVN_SELCHANGED, IDC_GRAYSCALE_GRID, OnGrayScaleGridEndSelChange)
	ON_MESSAGE(WM_SET_USER_INFO_POST_INIT, OnSetUserInfoPostInitialUpdate)
END_MESSAGE_MAP()


// Tool functions and variables implemented in DataSetDoc.cpp
BOOL StartBackgroundMeasures ( CDataSetDoc * pDoc );
void StopBackgroundMeasures ();
extern CDataSetDoc *	g_pDataDocRunningThread;
extern BOOL				g_bTerminateThread;
extern CWinThread*			g_hThread;

/////////////////////////////////////////////////////////////////////////////
// CMainView construction/destruction

CMainView::CMainView()
	: CFormView(CMainView::IDD)
{
    //{{AFX_DATA_INIT(CMainView)
	m_datarefCheckButton = FALSE;
	//}}AFX_DATA_INIT

	m_displayMode = 0;
	m_infoDisplay = 0;
	m_nSizeOffset = 0;
	m_bPositionsInit = FALSE;
	m_dwInitialUserInfo = 0;
	last_minCol = 1;
	minCol = 1;
	
	m_SelectedColor = noDataColor;

	m_pGrayScaleGrid = NULL;
	m_pSelectedColorGrid = NULL;
	m_pBgBrush= new CBrush(FxGetMenuBgColor());

	m_pInfoWnd = NULL;

	m_displayType=GetConfig()->GetProfileInt("MainView","Display type",HCFR_xyY_VIEW);
	dEavg_gs=0;
	dEmax_gs=0;
	dEavg_cc=0;
	dEmax_cc=0;
	dEavg_sr=0;
	dEmax_sr=0;
	dEavg_sg=0;
	dEmax_sg=0;
	dEavg_sb=0;
	dEmax_sb=0;
	dEavg_sy=0;
	dEmax_sy=0;
	dEavg_sc=0;
	dEmax_sc=0;
	dEavg_sm=0;
	dEmax_sm=0;
	dEavg=0.;
	dLavg=0.;
	dCavg=0.;
	dHavg=0.;
	dEmax=0.;
	dEcnt=0;
	dE10=0.;
	dE10min=0.;
	m_userBlack = FALSE;
	m_oldBlackGS = noDataColor;
	m_oldBlackNB = noDataColor;
	GetConfig()->m_bSave = FALSE;
	GetConfig()->m_bSave2 = FALSE;
	isSelectedWhiteY = FALSE;
}

CMainView::~CMainView()
{
	if(m_pGrayScaleGrid)
		delete m_pGrayScaleGrid;

	if(m_pSelectedColorGrid)
		delete m_pSelectedColorGrid;

	if ( m_pInfoWnd )
		if ( m_infoDisplay < 3 )
			delete m_pInfoWnd;

	delete m_pBgBrush;

	GetConfig()->WriteProfileInt("MainView","Display type",m_displayType);

	POSITION pos = m_CtrlInitPos.GetHeadPosition ();
	while ( pos )
	{
		SCtrlInitPos * pCtrlPos = (SCtrlInitPos *) m_CtrlInitPos.GetNext ( pos );
		delete pCtrlPos;
	}
	m_CtrlInitPos.RemoveAll ();
}

void CMainView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMainView)
	DDX_Control(pDX, IDC_GRAYSCALESTEPS_COMBOMODE, m_comboMode);
	DDX_Control(pDX, IDC_GRAYSCALE_GROUP, m_grayScaleGroup);
	DDX_Control(pDX, IDC_SENSOR_GROUP, m_sensorGroup);
	DDX_Control(pDX, IDC_GENERATOR_GROUP, m_generatorGroup);
	DDX_Control(pDX, IDC_DATAREF_GROUP, m_datarefGroup);
	DDX_Control(pDX, IDC_DISPLAY_GROUP, m_displayGroup);
	DDX_Control(pDX, IDC_PARAM_GROUP, m_paramGroup);
	DDX_Control(pDX, IDC_SELECTION_GROUP, m_selectGroup);
	DDX_Control(pDX, IDC_VIEW_GROUP, m_viewGroup);
	DDX_Control(pDX, IDC_INFO_DISPLAY, m_comboDisplay);
	DDX_Control(pDX, IDC_EDITGRID_CHECK, m_editCheckButton);
	DDX_Check(pDX, IDC_DATAREF_CHECK, m_datarefCheckButton);
	DDX_Control(pDX, IDC_ADJUSTXYZ_CHECK, m_AdjustXYZCheckButton);
	DDX_Control(pDX, IDC_MEASUREGRAYSCALE_BUTTON, m_grayScaleButton);
	DDX_Control(pDX, IDC_DELETEGRAYSCALE_BUTTON, m_grayScaleDeleteButton);
	DDX_Control(pDX, IDM_CONFIGURE_SENSOR, m_configSensorButton);
	DDX_Control(pDX, IDM_CONFIGURE_SENSOR2, m_configSensorButton2);
	DDX_Control(pDX, IDM_CONFIGURE_GENERATOR, m_configGeneratorButton);
	DDX_Control(pDX, IDC_VALUES_STATIC, m_valuesStatic);
	DDX_Control(pDX, IDC_COLORDATA_STATIC, m_colordataStatic);
	DDX_Text(pDX, IDC_GENERATORNAME_STATIC, m_generatorName);
	DDX_Text(pDX, IDC_SENSORNAME_STATIC, m_sensorName);
	DDX_Control(pDX, IDC_TARGET, m_TargetStatic);
	DDX_Control(pDX, IDC_RGBLEVELS, m_RGBLevelsStatic);
	DDX_Control(pDX, IDC_STATIC_RGBLEVELS, m_RGBLevelsLabel);
	DDX_Control(pDX, IDC_ANSICONTRAST_PATTERN_TEST_BUTTON, m_testAnsiPatternButton);
	//}}AFX_DATA_MAP
}

void CMainView::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();	// called first to initialize dialog elements

	GetDlgItem ( IDC_STATIC_VIEW ) -> ShowWindow ( SW_HIDE );

	m_displayMode = 0;
	m_comboMode.SetCurSel ( m_displayMode );	// Echelle de gris
	
	m_comboDisplay.SetCurSel ( 0 );
	OnSelchangeInfoDisplay ();

    // doesn't really make sense to see sensor values
	if ( m_displayType == HCFR_SENSORRGB_VIEW )
		m_displayType = HCFR_xyY_VIEW;

	GetDlgItem ( IDC_SENSORRGB_RADIO ) -> EnableWindow ( FALSE );

	CheckDlgButton(IDC_XYZ_RADIO, m_displayType == HCFR_XYZ_VIEW ? BST_CHECKED : BST_UNCHECKED);  
	CheckDlgButton(IDC_SENSORRGB_RADIO, m_displayType == HCFR_SENSORRGB_VIEW ? BST_CHECKED : BST_UNCHECKED);  
	CheckDlgButton(IDC_RGB_RADIO, m_displayType == HCFR_RGB_VIEW ? BST_CHECKED : BST_UNCHECKED);  
	CheckDlgButton(IDC_XYZ_RADIO2, m_displayType == HCFR_xyz2_VIEW ? BST_CHECKED : BST_UNCHECKED);  
	CheckDlgButton(IDC_XYY_RADIO, m_displayType == HCFR_xyY_VIEW ? BST_CHECKED : BST_UNCHECKED);  

	// Set title of view
	CString title;
	title.LoadString(IDS_DATASETVIEW_NAME);
	GetParent()->SetWindowText(GetDocument()->GetTitle() + ": " + title);

    if (m_pGrayScaleGrid == NULL)             
    {
 	    m_pGrayScaleGrid = new CGridCtrl;     // Create the Gridctrl object
        if (!m_pGrayScaleGrid ) return;

		m_pGrayScaleGrid -> SetHScrollAlwaysVisible ( TRUE );
		m_pGrayScaleGrid -> SetVScrollAlwaysVisible ( TRUE );

		CRect rect;
        m_valuesStatic.GetWindowRect(&rect);	// size control to m_valuesStatic control size
		ScreenToClient(&rect);
        m_pGrayScaleGrid->Create(rect, this, IDC_GRAYSCALE_GRID,WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL );		// Create the Gridctrl window
		m_pGrayScaleGrid->ShowScrollBar ( SB_BOTH );
	}

	if (m_pSelectedColorGrid == NULL)
	{
 	    m_pSelectedColorGrid = new CGridCtrl;     // Create the Gridctrl object
        if (!m_pSelectedColorGrid ) return;

		m_pSelectedColorGrid -> SetVScrollAlwaysVisible ( TRUE );

		CRect rect;
        m_colordataStatic.GetWindowRect(&rect);	// size control to m_colordataStatic control size
		ScreenToClient(&rect);
        m_pSelectedColorGrid->Create(rect, this, IDC_COLORDATA_GRID,WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL);		// Create the Gridctrl window
	}

	InitButtons();
	InitGroups();
	InitGrid();
	InitSelectedColorGrid();

	UpdateData(FALSE);

	UpdateGrid();
	if(m_pGrayScaleGrid->GetColumnCount() > 12)	// Needed after data is set to get correctly sized cells when scrollbar is present
	{
		m_pGrayScaleGrid->ExpandRowsToFit(FALSE);
		m_pGrayScaleGrid->AutoSizeColumns();
	}

	RECT	Rect;

	if ( m_Target.m_hWnd == NULL )
	{
		m_TargetStatic.GetWindowRect ( & Rect );
		m_TargetStatic.ShowWindow ( SW_HIDE );
		ScreenToClient ( & Rect );
		m_Target.Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this, IDC_TARGET2, NULL );
		m_Target.m_pRefColor = & m_SelectedColor;
		m_Target.m_pDocument = GetDocument();

		m_RGBLevelsStatic.GetWindowRect ( & Rect );
		m_RGBLevelsStatic.ShowWindow ( SW_HIDE );
		ScreenToClient ( & Rect );
		m_RGBLevels.Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this, IDC_RGBLEVELS2, NULL );
		m_RGBLevels.m_pRefColor = & m_SelectedColor;
		m_RGBLevels.m_pDocument = GetDocument();
	}

	RefreshSelection ();

	GetParentFrame()->RecalcLayout(); 
	
	// Do not resize parent window when opening a document containing window positions
	if ( ! GetDocument () -> m_pFramePosInfo )
		ResizeParentToFit();

	// Initialise controls positions
	m_InitialWindowSize.x = 0;
	m_InitialWindowSize.y = 0;
	for ( int i = 0; i < sizeof ( g_CtrlLayout ) / sizeof ( g_CtrlLayout [ 0 ] ) ; i ++ )
	{
		HWND			hWnd;
		SCtrlInitPos *	pCtrlPos;

		GetDlgItem ( g_CtrlLayout [ i ].m_nCtrlID, & hWnd );
		if ( hWnd )
		{
			pCtrlPos = new SCtrlInitPos;
			pCtrlPos -> m_hWnd = hWnd;
			pCtrlPos -> m_pLayout = & g_CtrlLayout [ i ];
			::GetWindowRect ( hWnd, & pCtrlPos -> m_Rect );
			::ScreenToClient ( m_hWnd, (LPPOINT) & pCtrlPos -> m_Rect.left );
			::ScreenToClient ( m_hWnd, (LPPOINT) & pCtrlPos -> m_Rect.right );
			m_CtrlInitPos.AddTail ( pCtrlPos );

			if ( m_InitialWindowSize.x < pCtrlPos -> m_Rect.right + 3 )
				m_InitialWindowSize.x = pCtrlPos -> m_Rect.right + 3;

			if ( m_InitialWindowSize.y < pCtrlPos -> m_Rect.bottom + 3 )
				m_InitialWindowSize.y = pCtrlPos -> m_Rect.bottom + 3;
		}
	}
	
	m_OriginalRect.left = 0;
	m_OriginalRect.top = 0;
	m_OriginalRect.right = m_InitialWindowSize.x;
	m_OriginalRect.bottom = m_InitialWindowSize.y;
	m_bPositionsInit = TRUE;
	
	( (CMultiFrame *) GetParentFrame () ) -> m_MinSize2.x = m_InitialWindowSize.x + ( GetSystemMetrics ( SM_CXSIZEFRAME ) * 2 );
	( (CMultiFrame *) GetParentFrame () ) -> m_MinSize2.y = m_InitialWindowSize.y + GetSystemMetrics ( SM_CYCAPTION ) + GetSystemMetrics ( SM_CYSIZEFRAME ) + 6 - 105;
	( (CMultiFrame *) GetParentFrame () ) -> m_bUseMinSize2 = TRUE;

	OnSize ( 0, 0, 0 );

	if ( m_dwInitialUserInfo != 0 )
	{
		// Define initial view status after global initial update
		PostMessage ( WM_SET_USER_INFO_POST_INIT );
	}
}

LRESULT CMainView::OnSetUserInfoPostInitialUpdate(WPARAM wParam, LPARAM lParam)
{
	if ( m_dwInitialUserInfo != 0 )
	{
		// Set m_displayMode
		m_comboMode.SetCurSel ( m_dwInitialUserInfo & 0x003F );
		OnSelchangeComboMode();

		// Set m_displayType
        // this doesn't work and means there is a mismtach between what's shown in the
        // radio buttons and the grid 
        ///\todo fix this
		//SendMessage ( WM_COMMAND, IDC_XYZ_RADIO + ( ( m_dwInitialUserInfo >> 6 ) & 0x000F ) );

		// Set m_infoDisplay
		m_comboDisplay.SetCurSel ( ( m_dwInitialUserInfo >> 10 ) & 0x003F );
		OnSelchangeInfoDisplay();

		// Set m_nSizeOffset
		if ( m_nSizeOffset != ( ( m_dwInitialUserInfo >> 16 ) & 0x00FF ) )
		{
			m_nSizeOffset = ( m_dwInitialUserInfo >> 16 ) & 0x00FF;
			( (CMultiFrame *) GetParentFrame () ) -> EnsureMinimumSize ();
			InvalidateRect ( NULL );
			OnSize ( 0, 0, 0 );
		}

		if ( m_infoDisplay >= 3 && m_pInfoWnd != NULL )
		{
			// Info display is a sub view
			DWORD	dwSubViewUserInfo = ( ( m_dwInitialUserInfo >> 24 ) & 0x00FF );
		
			( (CSavingView *) ( ( (CSubFrame *) m_pInfoWnd ) -> GetActiveView () ) ) -> SetUserInfo ( dwSubViewUserInfo );
		}

		m_dwInitialUserInfo = 0;
	}

	return 0;
}

void CMainView::AddColorToGrid(const ColorTriplet& color, GV_ITEM& Item, const char* format)
{
    Item.strText.Format(format, color[0]);
    ++Item.row;
    m_pSelectedColorGrid->SetItem(&Item);
    Item.strText.Format(format, color[1]);
    ++Item.row;
    m_pSelectedColorGrid->SetItem(&Item);
    Item.strText.Format(format, color[2]);
    ++Item.row;
    m_pSelectedColorGrid->SetItem(&Item);
}

void CMainView::RefreshSelection(bool b_minCol, bool inMeasure)
{
	int		i, aColorTemp;
	double	YWhite = 1.0;
	CColor	aColor;
	GV_ITEM Item;
	CString	str;
	
	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.nFormat = DT_RIGHT;
	Item.row = 0;
	Item.col = 1;
	if (b_minCol)
		minCol = m_pGrayScaleGrid -> GetSelectedCellRange().IsValid()?m_pGrayScaleGrid -> GetSelectedCellRange().GetMinCol():-1;

	if (m_displayMode <= 11 &&  m_displayMode != 2)  
    {
        int size=GetDocument()->GetMeasure()->GetGrayScaleSize();
        if (m_displayMode == 3)
            size = 101;
        else if (m_displayMode == 4)
        size = -1 * GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
		if (m_displayMode > 4 && m_displayMode < 12)
			size=GetDocument()->GetMeasure()->GetSaturationSize();

		if (inMeasure)
		{
			if (minCol > 1)
			    m_RGBLevels.Refresh(last_minCol, m_displayMode, size);
			else
				m_RGBLevels.Refresh(minCol, m_displayMode, size);
		}
		else
		    m_RGBLevels.Refresh(minCol, m_displayMode, size);

		if (last_minCol != minCol && minCol > 0)
			last_minCol = minCol;
		m_Target.Refresh(GetDocument()->GetGenerator()->m_b16_235,  last_minCol, size, m_displayMode, GetDocument(), FALSE);
		if (inMeasure)
			m_Target.Refresh(GetDocument()->GetGenerator()->m_b16_235,  last_minCol - 1, size, m_displayMode, GetDocument(), TRUE);
    }

	if(m_SelectedColor.isValid())
	{
		// Retrieve measured white luminance to compute exact delta E, Lab and LCH values
		if ( GetDocument() -> GetMeasure () -> GetOnOffWhite ().isValid() )
			YWhite = GetDocument() -> GetMeasure () -> GetOnOffWhite () [ 1 ]; //onoff white is always grayscale white

		Item.strText.Format("%.3f",m_SelectedColor.GetLuminance());
		Item.row = 0;
		m_pSelectedColorGrid->SetItem(&Item);

        if (GetDocument()->m_pSensor->ReadingType() == 2)
            Item.strText.Format("%.4f",m_SelectedColor.GetLuminance() / 10.764);
        else
            Item.strText.Format("%.4f",m_SelectedColor.GetLuminance()*.29188558);
		Item.row = 1;
		m_pSelectedColorGrid->SetItem(&Item);

		aColorTemp = m_SelectedColor.GetXYZValue().GetColorTemp(GetColorReference());
		
		if ( aColorTemp < 1500 )
		{
			Item.strText = _T("< 1500");
			Item.row = 2;
			m_pSelectedColorGrid->SetItem(&Item);
		}
		else if ( aColorTemp > 12000 )
		{
			Item.strText= _T("> 12000");
			Item.row = 2;
			m_pSelectedColorGrid->SetItem(&Item);
		}
		else
		{
			Item.strText.Format ( "%d", aColorTemp );
			Item.row = 2;
			m_pSelectedColorGrid->SetItem(&Item);
		}

        AddColorToGrid(m_SelectedColor.GetXYZValue(), Item, "%.3f");
        AddColorToGrid(m_SelectedColor.GetRGBValue((GetColorReference())), Item, "%.3f");
        AddColorToGrid(m_SelectedColor.GetxyYValue(), Item, "%.3f");
        AddColorToGrid(m_SelectedColor.GetxyzValue(), Item, "%.3f");
        AddColorToGrid(m_SelectedColor.GetLabValue(YWhite, GetColorReference()), Item, "%.1f");
        AddColorToGrid(m_SelectedColor.GetLCHValue(YWhite, GetColorReference()), Item, "%.1f");
	}
	else
	{
		for(i=0;i<21;i++)
		{
			Item.row = i;
			Item.strText="";
			m_pSelectedColorGrid->SetItem(&Item);
		}
	}
	m_pSelectedColorGrid->Invalidate();

	if ( m_pInfoWnd )
	{
		switch ( m_infoDisplay )
		{
			case 0:	// Edit
				 break;

			case 1: // target
                if (m_displayMode <= 11 &&  m_displayMode != 2)
                {
                    int size=GetDocument()->GetMeasure()->GetGrayScaleSize();
                    if (m_displayMode == 3)
                        size = 101;
                    else if (m_displayMode == 4)
                        size = -1 * GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
					if (m_displayMode > 4 && m_displayMode < 12)
						size=GetDocument()->GetMeasure()->GetSaturationSize();
					( ( CTargetWnd * ) m_pInfoWnd ) -> m_pRefColor = & m_SelectedColor;
                    ( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_minCol, size, m_displayMode, GetDocument(), FALSE);
					if (inMeasure)
	                    ( ( CTargetWnd * ) m_pInfoWnd ) -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  last_minCol - 1, size, m_displayMode, GetDocument(), TRUE);
                }
				break;

			case 2:	// spectrum
				 ( ( CSpectrumWnd * ) m_pInfoWnd ) -> Refresh ();
				 break;
		}
	}
}

void CMainView::InitGrid()
{
	if(m_pGrayScaleGrid==NULL)
		return;

	CDataSetDoc *	pDataRef = GetDataRef();

	if ( pDataRef == GetDocument () )
		pDataRef = NULL;

   	int size;
	int nRows;
	BOOL bHasLuxValues = FALSE;
	BOOL bHasLuxDelta = FALSE;
	BOOL bIRE = GetDocument()->GetMeasure()->m_bIREScaleMode;

	if ( m_displayMode == 0 )
	{
   		size = GetDocument()->GetMeasure()->GetGrayScaleSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetGrayScaleSize() != size )
			pDataRef = NULL;

		if ( pDataRef && pDataRef->GetMeasure()->m_bIREScaleMode != bIRE )
			pDataRef = NULL;

		if ( size )
			bHasLuxValues = GetDocument()->GetMeasure()->GetGray(0).HasLuxValue ();

		bHasLuxDelta = bHasLuxValues;
	}
	else if ( m_displayMode == 1 )
	{
		size = 8;
		bHasLuxValues = GetDocument()->GetMeasure()->GetRedPrimary().HasLuxValue ();
		if ( bHasLuxValues )
		{
			if ( GetDocument()->GetMeasure()->GetOnOffWhite().isValid() )
				bHasLuxDelta = TRUE;
		}
	}
	else if ( m_displayMode == 2 )
	{
		size = GetDocument()->GetMeasure()->GetMeasurementsSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetMeasurementsSize() != size )
			pDataRef = NULL;
	}
	else if ( m_displayMode == 3 )
	{
		size=GetDocument()->GetMeasure()->GetNearBlackScaleSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetNearBlackScaleSize() != size )
			pDataRef = NULL;
		if ( size )
			bHasLuxValues = GetDocument()->GetMeasure()->GetNearBlack(0).HasLuxValue ();
	}
	else if ( m_displayMode == 4 )
	{
		size=GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetNearWhiteScaleSize() != size )
			pDataRef = NULL;
		if ( size )
			bHasLuxValues = GetDocument()->GetMeasure()->GetNearWhite(0).HasLuxValue ();
	}
	else if ( m_displayMode > 4 && m_displayMode < 11 )
	{
		size=GetDocument()->GetMeasure()->GetSaturationSize();
		if ( pDataRef && pDataRef->GetMeasure()->GetSaturationSize() != size )
			pDataRef = NULL;
		if ( size )
		{
			switch ( m_displayMode )
			{
				case 5:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetRedSat(0).HasLuxValue ();
					 break;

				case 6:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetGreenSat(0).HasLuxValue ();
					 break;

				case 7:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetBlueSat(0).HasLuxValue ();
					 break;

				case 8:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetYellowSat(0).HasLuxValue ();
					 break;

				case 9:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetCyanSat(0).HasLuxValue ();
					 break;

				case 10:
					 bHasLuxValues = GetDocument()->GetMeasure()->GetMagentaSat(0).HasLuxValue ();
					 break;

			}
		}
	}
	else if ( m_displayMode == 11)
	{
		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);
        if (isExtPat)
            size = (GetConfig()->GetCColorsSize());		
        else
            size = (GetConfig()->m_CCMode == CCSG?96:(GetConfig()->m_CCMode == AXIS?71:24));
			if (GetConfig()->m_CCMode==CMS || GetConfig()->m_CCMode==CPS)
				size = 19;
		bHasLuxValues = GetDocument()->GetMeasure()->GetCC24Sat(0).HasLuxValue ();
	}
	else if ( m_displayMode == 12 )
	{
		size = 6;
		pDataRef = NULL;
		bHasLuxValues = GetDocument()->GetMeasure()->GetOnOffWhite().HasLuxValue ();
	}

	if ( m_displayMode == 12 )
		nRows = 3;
	else if ( ! pDataRef )
		nRows = 5;
	else
		nRows = 7;

	if ( m_displayMode <= 1 || (m_displayMode >= 3 && m_displayMode <=11) )
		nRows ++;

	if ( bHasLuxValues )
	{
		if ( bHasLuxDelta )
			nRows += 2;
		else
			nRows ++;
	}

	m_pGrayScaleGrid->SetFixedRowCount(1);
	m_pGrayScaleGrid->SetFixedColumnCount(1);

    m_pGrayScaleGrid->SetRowCount(nRows + 1);     
    m_pGrayScaleGrid->SetColumnCount(size+1);
	
	m_pGrayScaleGrid->SetFixedColumnSelection(TRUE);
	m_pGrayScaleGrid->SetFixedRowSelection(FALSE);
	m_pGrayScaleGrid->SetTrackFocusCell(TRUE);
	m_pGrayScaleGrid->SetEditable(m_editCheckButton.GetCheck());
	m_pGrayScaleGrid->EnableDragAndDrop(m_editCheckButton.GetCheck());
	m_pGrayScaleGrid->SetDoubleBuffering(TRUE);

	m_pGrayScaleGrid->SetDefCellMargin(3);

	// Set the font to bold
	CFont* pFont = m_pGrayScaleGrid->GetFont();
	LOGFONT lf;
	pFont->GetLogFont(&lf);
	lf.lfWeight=FW_BOLD;
	m_pGrayScaleGrid->SetItemFont(0,0, &lf); // Set the font to bold

	// set column label
	GV_ITEM Item;
	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.row = 0;
	Item.col = 0;
	Item.nFormat = DT_CENTER|DT_WORDBREAK;
	
	switch ( m_displayMode )
	{
		case 0:
			 Item.strText = ( bIRE ? "IRE" : GetConfig()->m_PercentGray );
			 break;
		case 1:
			 Item.strText="Color";
			 break;
		case 2:
		case 3:
		case 4:
			 Item.strText=GetConfig()->m_PercentGray;
			 break;

		case 11:
			 Item.strText="Color Checker";
			 break;
		case 12:
			 Item.strText=" ";
			 break;

		default:
			 Item.strText="% Sat";
			 break;
	}

	m_pGrayScaleGrid->SetItem(&Item);

    int i;

	// set columns labels 
	for(i=0;i<size;i++)
	{
		Item.row = 0;
		Item.col = i+1;
		Item.nFormat = DT_CENTER|DT_WORDBREAK;
		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);

		switch ( m_displayMode )
		{
			case 0:
				 if ( bIRE && i==0 )
					Item.strText.Format("%.1f",ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown ));
				 else
					Item.strText.Format("%d", (int) floor(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown )+0.5) );
				 break;

			case 1:
				 switch ( i )
				 {
					case 0:
						Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6a:IDS_RED);
						 break;

					case 1:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6b:IDS_GREEN);
						 break;

					case 2:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6c:IDS_BLUE);
						 break;

					case 3:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6d:IDS_YELLOW);
						 break;

					case 4:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6e:IDS_CYAN);
						 break;

					case 5:
						 Item.strText.LoadString((GetColorReference().m_standard==CC6)?IDS_CC6f:IDS_MAGENTA);
						 break;

					case 6:
						 Item.strText.LoadString(IDS_WHITE);
						 break;

					case 7:
						 Item.strText.LoadString(IDS_BLACK);
						 break;
				 }
				 break;

			case 2:
				 Item.strText.Format("%d",Item.col);
				 break;

			case 3:
				 Item.strText.Format("%d",i);
				 break;

			case 4:
				 Item.strText.Format("%d",i + 101 - size);
				 break;

			case 11:
                 if (GetConfig()->m_CCMode == CCSG)
                 {
                    Item.strText.SetString(PatName[i]);
                 } 
                 else if (GetConfig()->m_CCMode == CMS)
                 {
                    Item.strText.SetString(PatNameCMS[i]);
                 }
                 else if (GetConfig()->m_CCMode == CPS)
                 {
                    Item.strText.SetString(PatNameCPS[i]);
                 }
                 else if (GetConfig()->m_CCMode == AXIS)
                 {
                    Item.strText.SetString(PatNameAXIS[i]);
                 }
                 else if (isExtPat)
                 {
                     char aBuf[50];
					 std::string name = GetConfig()->GetCColorsN(i);
                     sprintf(aBuf,"%s", name.c_str());
                     Item.strText.SetString(aBuf);
                 }
                 else {
				 switch ( i )
				 {
					 case 0:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_1a:(GetConfig()->m_CCMode == SKIN?IDS_CC_1b:IDS_CC_1));
						break;

					 case 1:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_2a:(GetConfig()->m_CCMode == SKIN?IDS_CC_2b:IDS_CC_2));
						break;

					 case 2:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_3a:(GetConfig()->m_CCMode == SKIN?IDS_CC_3b:IDS_CC_3));
						break;

					 case 3:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_4a:(GetConfig()->m_CCMode == SKIN?IDS_CC_4b:IDS_CC_4));
						break;

					 case 4:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_5a:(GetConfig()->m_CCMode == SKIN?IDS_CC_5b:IDS_CC_5));
						break;

					 case 5:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_6a:(GetConfig()->m_CCMode == SKIN?IDS_CC_6b:IDS_CC_6));
						break;

					 case 6:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_7a:(GetConfig()->m_CCMode == SKIN?IDS_CC_7b:IDS_CC_7));
						break;

					 case 7:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_8a:(GetConfig()->m_CCMode == SKIN?IDS_CC_8b:IDS_CC_8));
						break;

					 case 8:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_9a:(GetConfig()->m_CCMode == SKIN?IDS_CC_9b:IDS_CC_9));
						break;

					 case 9:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_10a:(GetConfig()->m_CCMode == SKIN?IDS_CC_10b:IDS_CC_10));
						break;

					 case 10:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_11a:(GetConfig()->m_CCMode == SKIN?IDS_CC_11b:IDS_CC_11));
						break;

					 case 11:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_12a:(GetConfig()->m_CCMode == SKIN?IDS_CC_12b:IDS_CC_12));
						break;

					 case 12:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_13a:(GetConfig()->m_CCMode == SKIN?IDS_CC_13b:IDS_CC_13));
						break;

					 case 13:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_14a:(GetConfig()->m_CCMode == SKIN?IDS_CC_14b:IDS_CC_14));
						break;

					 case 14:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_15a:(GetConfig()->m_CCMode == SKIN?IDS_CC_15b:IDS_CC_15));
						break;

					 case 15:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_16a:(GetConfig()->m_CCMode == SKIN?IDS_CC_16b:IDS_CC_16));
						break;

					 case 16:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_17a:(GetConfig()->m_CCMode == SKIN?IDS_CC_17b:IDS_CC_17));
						break;

					 case 17:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_18a:(GetConfig()->m_CCMode == SKIN?IDS_CC_18b:IDS_CC_18));
						break;

					 case 18:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_19a:(GetConfig()->m_CCMode == SKIN?IDS_CC_19b:IDS_CC_19));
						break;

					 case 19:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_20a:(GetConfig()->m_CCMode == SKIN?IDS_CC_20b:IDS_CC_20));
						break;

					 case 20:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_21a:(GetConfig()->m_CCMode == SKIN?IDS_CC_21b:IDS_CC_21));
						break;

					 case 21:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_22a:(GetConfig()->m_CCMode == SKIN?IDS_CC_22b:IDS_CC_22));
						break;

					 case 22:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_23a:(GetConfig()->m_CCMode == SKIN?IDS_CC_23b:IDS_CC_23));
						break;

					 case 23:
						Item.strText.LoadString(GetConfig()->m_CCMode == CMC?IDS_CC_24a:(GetConfig()->m_CCMode == SKIN?IDS_CC_24b:IDS_CC_24));
						break;
            		 default:
				        Item.strText.LoadString(IDS_CC_24a);
                 }
                 }
				 break;

			case 12:
				 switch ( i )
				 {
					case 0:
						 Item.strText.LoadString(IDS_BLACK);
						 break;

					case 1:
						 Item.strText.LoadString(IDS_WHITE);
						 break;

					case 2:
						 Item.strText="ANSI 1";
						 break;

					case 3:
						 Item.strText="ANSI 2";
						 break;

					case 4:
					case 5:
						 Item.strText.Empty ();
						 break;
				 }
				 break;

			default:
				 Item.strText.Format("%d",i*100/(size-1));
				 break;
		}

		m_pGrayScaleGrid->SetItem(&Item);
		m_pGrayScaleGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold

		for ( int i2 = 4; i2 <= nRows ; i2++ )
		{
			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, ( (i2&1) ? RGB(240,240,240) : RGB(224,224,224) ) );
			m_pGrayScaleGrid->SetItemState ( i2, i+1, m_pGrayScaleGrid->GetItemState(i2,i+1) | GVIS_READONLY );
        }

		if (m_displayMode < 13)
        {
            int i2=0;
            double step = 120.0 / size;
            m_pGrayScaleGrid->SetItemFgColour(i2, i+1, RGB(10,10,10));
            m_pGrayScaleGrid->SetItemBkColour(i2, i+1, RGB(240,240,240));
            switch (m_displayMode)
            {
            case 0:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(i*step+130,i*step+130,i*step+130) );
                break;
            case 1:
                switch(i+1)
                {
                case 1:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,200,200) );
                        break;
                case 2:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(200,240,200) );
                        break;
                case 3:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(200,200,240) );
                        break;
                case 4:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,240,200) );
                        break;
                case 5:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(200,240,240) );
                        break;
                case 6:
            			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,200,240) );
                        break;
                }
            break;
            case 2:
            case 3:
            case 4:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(i*step+130,i*step+130,i*step+130) );
                break;
            case 5:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,240-i*step*2,240-i*step*2) );
                break;
            case 6:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240-i*step*2,240,240-i*step*2) );
                break;
            case 7:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240-i*step*2,240-i*step*2,240) );
                break;
            case 8:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,240,240-i*step*2) );
                break;
            case 9:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240-i*step*2,240,240) );
                break;
            case 10:
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(240,240-i*step*2,240) );
                break;
            case 11:
                CColor s_clr;
                ColorRGB r_clr;
                double inten;
                s_clr=GetDocument()->GetMeasure()->GetRefCC24Sat(i);     
                r_clr=s_clr.GetRGBValue((GetColorReference().m_standard == UHDTV3?CColorReference(UHDTV2):GetColorReference()));
				r_clr[0]=(min(max(r_clr[0],0),1));
				r_clr[1]=(min(max(r_clr[1],0),1));
				r_clr[2]=(min(max(r_clr[2],0),1));
                inten = s_clr.GetLuminance();//GetConfig()->m_CCMode==USER?s_clr.GetLuminance():GetColorReference().GetCC24ReferenceLuma(i,GetConfig()->m_CCMode);
    			m_pGrayScaleGrid->SetItemBkColour ( i2, i+1, RGB(pow(r_clr[0],1.0/2.2)*255,pow(r_clr[1],1.0/2.2)*255,pow(r_clr[2],1.0/2.2)*255) );
                if (inten < 0.4)
                    m_pGrayScaleGrid->SetItemFgColour(i2, i+1, RGB(240,240,240));
                else
                    m_pGrayScaleGrid->SetItemFgColour(i2, i+1, RGB(10,10,10));
            break;
            }
        }
	}

	if ( size >= 5 )
	{
		Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;

		Item.col = 5;
		for ( i = 0 ; i < 3 + bHasLuxValues; i++ )
		{
			Item.row = i+1;
			if ( m_displayMode == 12 )
			{
				Item.strText = ( i==0 ? "ON/OFF:" : ( i==1 ? "ANSI:" : "" ) );
				m_pGrayScaleGrid->SetItem(&Item);
				m_pGrayScaleGrid->SetItemBkColour ( Item.row,Item.col, RGB(224,224,224) );
				m_pGrayScaleGrid->SetItemState ( Item.row,Item.col, m_pGrayScaleGrid->GetItemState(Item.row,Item.col) | GVIS_READONLY );
			}
			else if ( i < 3 )
			{
				m_pGrayScaleGrid->SetItemBkColour ( Item.row,Item.col, RGB(255,255,255) );
//				m_pGrayScaleGrid->SetItemState ( Item.row,Item.col, m_pGrayScaleGrid->GetItemState(Item.row,Item.col) & (~GVIS_READONLY) );
			}
		}

		if ( size >= 6 )
		{
			Item.col = 6;
			Item.strText.Empty ();
			for ( i = 0 ; i < 3 + bHasLuxValues; i++ )
			{
				Item.row = i+1;
				if ( m_displayMode == 12 )
				{
					m_pGrayScaleGrid->SetItem(&Item);
					m_pGrayScaleGrid->SetItemState ( Item.row,Item.col, m_pGrayScaleGrid->GetItemState(Item.row,Item.col) | GVIS_READONLY );
				}
				else if ( i < 3 )
				{
				m_pGrayScaleGrid->SetItemBkColour ( Item.row,Item.col, RGB(255,255,255) );
//					m_pGrayScaleGrid->SetItemState ( Item.row,Item.col, m_pGrayScaleGrid->GetItemState(Item.row,Item.col) & (~GVIS_READONLY) );
				}
			}
		}

		Item.nFormat = DT_CENTER|DT_WORDBREAK;
	}

	// Set row labels
	for(i=0;i<nRows;i++)
	{
		Item.row = i+1;
		Item.col = 0;
		m_pGrayScaleGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold
		
		if ( bHasLuxValues && i == nRows - ( 1 + bHasLuxDelta ) )
		{
			if ( GetConfig () -> m_bUseImperialUnits )
				Item.strText = "Ft-cd";
			else
				Item.strText = "Lux";
		}
		else if ( bHasLuxValues && bHasLuxDelta && i == nRows - 1 )
		{
			Item.strText = "delta Y / lux";
		}
		else
			Item.strText = GetGridRowLabel(i);
		
		m_pGrayScaleGrid->SetItem(&Item);
	}

	m_pGrayScaleGrid->AutoSizeColumn(0);
	m_pGrayScaleGrid -> AutoSizeRows ();

	int width = m_pGrayScaleGrid -> GetColumnWidth ( 0 );
	for ( i = 1 ; i <= size ; i ++ )
		m_pGrayScaleGrid -> SetColumnWidth ( i, width * 11 / 10 );

	if ( m_displayMode != 2 && size < 12 ) 
	{
		// No scrollbars needed : use all the place for cells
		m_pGrayScaleGrid->ExpandColumnsToFit(FALSE);
	}

	if  (GetConfig()->GetProfileDouble("References","Use Black Level",0)) //store/retrieve real black measurements
	{
		if (!m_userBlack)
		{
			m_oldBlackGS = GetDocument()->GetMeasure()->GetGray(0);
			m_oldBlackNB = GetDocument()->GetMeasure()->GetNearBlack(0);
			double Yblack = GetConfig()->GetProfileDouble("References","Manual Black Level",0);
			GetDocument()->GetMeasure()->SetGray(0, ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883));
			GetDocument()->GetMeasure()->SetNearBlack(0, ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883)) ;
			GetDocument()->GetMeasure()->SetOnOffBlack(ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883)) ;
			m_userBlack = TRUE;
		}
	}
	else
	{
		if (m_userBlack)
		{
			GetDocument()->GetMeasure()->SetGray(0, m_oldBlackGS);
			GetDocument()->GetMeasure()->SetNearBlack(0, m_oldBlackNB) ;
			GetDocument()->GetMeasure()->SetOnOffBlack(m_oldBlackGS) ;
			m_userBlack = FALSE;
		}
	}

	if ( (m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 12) && GetConfig()->m_userBlack)
	{
    		m_pGrayScaleGrid->SetItemBkColour ( 1, 1, RGB(255,218,185) );
    		m_pGrayScaleGrid->SetItemBkColour ( 2, 1, RGB(255,218,185) );
    		m_pGrayScaleGrid->SetItemBkColour ( 3, 1, RGB(255,218,185) );
    		m_pGrayScaleGrid->SetItemFgColour ( 1, 1, RGB(0,10,185) );
    		m_pGrayScaleGrid->SetItemFgColour ( 2, 1, RGB(0,10,185) );
    		m_pGrayScaleGrid->SetItemFgColour ( 3, 1, RGB(0,10,185) );
	} else
	{
   			m_pGrayScaleGrid->SetItemBkColour ( 1, 1, RGB(255,255,255) );
   			m_pGrayScaleGrid->SetItemBkColour ( 2, 1, RGB(255,255,255) );
   			m_pGrayScaleGrid->SetItemBkColour ( 3, 1, RGB(255,255,255) );
   			m_pGrayScaleGrid->SetItemFgColour ( 1, 1, RGB(0,0,0) );
   			m_pGrayScaleGrid->SetItemFgColour ( 2, 1, RGB(0,0,0) );
   			m_pGrayScaleGrid->SetItemFgColour ( 3, 1, RGB(0,0,0) );
	}

	
	OnEditgridCheck();
}

void CMainView::InitSelectedColorGrid()
{
	if(m_pSelectedColorGrid==NULL)
		return;

	m_pSelectedColorGrid->SetFixedColumnCount(1);

    m_pSelectedColorGrid->SetRowCount(21);
    m_pSelectedColorGrid->SetColumnCount(2);
	
	m_pSelectedColorGrid->SetFixedColumnSelection(FALSE);
	m_pSelectedColorGrid->SetFixedRowSelection(FALSE);
	m_pSelectedColorGrid->SetTrackFocusCell(TRUE);
	m_pSelectedColorGrid->SetEditable(FALSE);
	m_pSelectedColorGrid->EnableDragAndDrop(FALSE);
	m_pSelectedColorGrid->SetDoubleBuffering(FALSE);

	m_pSelectedColorGrid->SetDefCellMargin(3);

	// Set the font to bold
	CFont* pFont = m_pSelectedColorGrid->GetFont();
	LOGFONT lf;
	pFont->GetLogFont(&lf);
	lf.lfWeight=FW_BOLD;

	// Set row labels
	GV_ITEM Item;
	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.nFormat = DT_CENTER|DT_WORDBREAK;

    char * RowLabels [] = { "Y cd/m²", "Y ftL", "T°", "X", "Y", "Z", "R", "G", "B", "x", "y", "Y", "x", "y", "z", "L", "a", "b", "L", "C", "H"};
            
    if (GetDocument()->m_pSensor->ReadingType() == 2)
    {
			RowLabels [0] = "T lux";
	        RowLabels [1] = "Y ft-c";
    }

    for(int i=0;i<21;i++)
	{
		Item.row = i;
		Item.col = 0;
		m_pSelectedColorGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold
		Item.strText=RowLabels[i];
		m_pSelectedColorGrid->SetItem(&Item);

		if ( (i/3)&1 )
			m_pSelectedColorGrid->SetItemBkColour(Item.row,1,RGB(224,224,224));
	}

	m_pSelectedColorGrid->ExpandColumnsToFit(TRUE);
	m_pSelectedColorGrid->AutoSizeColumn(0);
	m_pSelectedColorGrid->ExpandColumnsToFit(TRUE);

    m_pSelectedColorGrid->AutoSizeRows();
}

/////////////////////////////////////////////////////////////////////////////
// CMainView printing

BOOL CMainView::OnPreparePrinting(CPrintInfo* pInfo)
{
 	// default preparation
	return DoPreparePrinting(pInfo);
}

void CMainView::OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo)
{
    if (m_pGrayScaleGrid)             
		m_pGrayScaleGrid->OnBeginPrinting(pDC,pInfo);
}

void CMainView::OnEndPrinting(CDC* pDC, CPrintInfo* pInfo)
{
    if (m_pGrayScaleGrid)             
		m_pGrayScaleGrid->OnEndPrinting(pDC,pInfo);
}

void CMainView::OnPrint(CDC* pDC, CPrintInfo* pInfo)
{
    if (m_pGrayScaleGrid)             
		m_pGrayScaleGrid->OnPrint(pDC,pInfo);
}

/////////////////////////////////////////////////////////////////////////////
// CMainView diagnostics

#ifdef _DEBUG
void CMainView::AssertValid() const
{
	CFormView::AssertValid();
}

void CMainView::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}

CDataSetDoc* CMainView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDataSetDoc)));
	return (CDataSetDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainView message handlers

void CMainView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	int		nForceMode = -1;
	double	dContrast;
	InitSelectedColorGrid();
	CFrameWnd * pFrame = (CFrameWnd *)(AfxGetApp()->m_pMainWnd);
//	if (pFrame)
//		pFrame->GetActiveFrame()->ActivateFrame();
	pFrame->OnUpdateFrameMenu(NULL);
	// TODO: add a general option for that ?
	if ( 1 )
	{
		// Adjust grid selection if necessary
		switch ( lHint )
		{
			case UPD_PRIMARIES:
			case UPD_SECONDARIES:
			case UPD_PRIMARIESANDSECONDARIES:
				 if ( m_displayMode != 1 )
					nForceMode = 1;
				 break;

			case UPD_GRAYSCALEANDCOLORS:
				 if ( m_displayMode != 0 && m_displayMode != 1 )
					nForceMode = 0;
				 break;

			case UPD_GRAYSCALE:
				 if ( m_displayMode != 0 )
					nForceMode = 0;
				 break;

			case UPD_NEARBLACK:
				 if ( m_displayMode != 3 )
					nForceMode = 3;
				 break;

			case UPD_NEARWHITE:
				 if ( m_displayMode != 4 )
					nForceMode = 4;
				 break;

			case UPD_REDSAT:
				 if ( m_displayMode != 5 )
					nForceMode = 5;
				 break;

			case UPD_GREENSAT:
				 if ( m_displayMode != 6 )
					nForceMode = 6;
				 break;

			case UPD_BLUESAT:
				 if ( m_displayMode != 7 )
					nForceMode = 7;
				 break;

			case UPD_YELLOWSAT:
				 if ( m_displayMode != 8 )
					nForceMode = 8;
				 break;

			case UPD_CYANSAT:
				 if ( m_displayMode != 9 )
					nForceMode = 9;
				 break;

			case UPD_MAGENTASAT:
				 if ( m_displayMode != 10 )
					nForceMode = 10;
				 break;

			case UPD_CC24SAT:
				 if ( m_displayMode != 11 )
					nForceMode = 11;
				 else
					 UpdateGrid();
				 break;

			case UPD_ALLSATURATIONS:
				 if ( m_displayMode < 5 || m_displayMode > 11 )
					nForceMode = 5;
				 break;

			case UPD_CONTRAST:
				 if ( m_displayMode != 12 )
					nForceMode = 12;
				 break;
		}

		if ( nForceMode >= 0 )
		{
			m_comboMode.SetCurSel ( nForceMode );
			OnSelchangeComboMode();
		}
	}
	CColor	MeasuredColor=noDataColor;

	int	n = GetDocument()->GetMeasure()->GetMeasurementsSize();

	if ( n > 0 )
		MeasuredColor=GetDocument()->GetMeasure()->GetMeasurement(n-1);

	if ( lHint == UPD_FREEMEASUREAPPENDED && pHint == g_pDataDocRunningThread && g_hThread && ! g_bTerminateThread )
	{
		// Optimized version for continuous measures: update only measurement grid
		if (m_displayMode == 0 )
		{
			// Gray/Primary colors may have been updated during free measures
			if ( last_minCol >= 1 )// && ( MeasuredColor.GetDeltaxy(GetColorReference().GetWhite(), GetColorReference()) < 0.03 || last_minCol == 1 ) )
			{
				GetDocument()->GetMeasure()->SetGray(last_minCol - 1, MeasuredColor);
				if (last_minCol == 1)
					GetDocument()->GetMeasure()->SetOnOffBlack(MeasuredColor);
				if (last_minCol == GetDocument()->GetMeasure()->GetGrayScaleSize ()    )
					GetDocument()->GetMeasure()->SetOnOffWhite(MeasuredColor);
			}
 			
		}
		UpdateGrid();		
		UpdateMeasurementsAfterBkgndMeasure ();
	}
	else if ( lHint >= UPD_REALTIME ) //optimized for realtime
	{
		last_minCol = GetDocument()->GetMeasure()->m_currentIndex;
		minCol = last_minCol;

		if (m_displayMode != (lHint - UPD_REALTIME)) //need to change to correct sequence
		{
			m_displayMode = (lHint - UPD_REALTIME);
			m_comboMode.SetCurSel (m_displayMode);
			OnSelchangeComboMode();
			minCol = 1;
			last_minCol = minCol;
		}

		UpdateGrid();
		RefreshSelection(FALSE);
		
		if ( m_pInfoWnd ) //in case colorchecker slot is updated
		{
				m_pInfoWnd ->SetWindowTextA(GetDocument()->GetMeasure()->GetInfoString());
				m_pInfoWnd -> Invalidate ();
				m_pInfoWnd -> UpdateWindow();			
		}
	}
	else
	{
		// Normal OnUpdate
		CFormView::OnUpdate(pSender, lHint, pHint);

		if ( m_displayType == HCFR_SENSORRGB_VIEW )
		{
			m_displayType = HCFR_xyY_VIEW;
			CheckDlgButton(IDC_XYZ_RADIO, BST_CHECKED);  
			CheckDlgButton(IDC_SENSORRGB_RADIO, BST_UNCHECKED);  
		}

		GetDlgItem ( IDC_SENSORRGB_RADIO ) -> EnableWindow ( FALSE );


		if ( ( lHint >= UPD_EVERYTHING && lHint <= UPD_FREEMEASURES ) || lHint == UPD_ARRAYSIZES || lHint == UPD_GENERALREFERENCES || lHint == UPD_DATAREFDOC || lHint == UPD_REFERENCEDATA )
		{
			InitGrid(); // to update row labels (if colorReference setting has changed, or if lux values appeared)
			UpdateGrid();
		}
		
		if ( lHint == UPD_FREEMEASUREAPPENDED )
		{
			if ( m_displayMode == 0 )
			{
				// Gray/Primary colors may have been updated during free measures
				if (last_minCol >= 1 ) // ( (MeasuredColor.GetDeltaxy( GetColorReference().GetWhite(), GetColorReference()) < 0.03) || last_minCol == 1 ) )
				{
					GetDocument()->GetMeasure()->SetGray(last_minCol - 1, MeasuredColor);
					if (last_minCol == 1)
						GetDocument()->GetMeasure()->SetOnOffBlack(MeasuredColor);
					if (last_minCol == GetDocument()->GetMeasure()->GetGrayScaleSize ()    )
						GetDocument()->GetMeasure()->SetOnOffWhite(MeasuredColor);
				}
			}
			UpdateGrid();
			UpdateMeasurementsAfterBkgndMeasure ();
		}

		if ( lHint == UPD_EVERYTHING || lHint == UPD_CONTRAST )
		{
			// TODO: Check this
			dContrast = GetDocument()->GetMeasure()->GetOnOffContrast ();


			dContrast = GetDocument()->GetMeasure()->GetAnsiContrast ();

			double dMinLum;
			dMinLum = GetDocument()->GetMeasure()->GetContrastMinLum ();

			double dMaxLum;
			dMaxLum = GetDocument()->GetMeasure()->GetContrastMaxLum ();
		}

		if ( lHint == UPD_EVERYTHING || lHint == UPD_SENSORCONFIG )
		{
			m_sensorName=GetDocument()->m_pSensor->GetName();
		}

		if ( lHint == UPD_EVERYTHING || lHint == UPD_GENERATORCONFIG )
		{
			m_generatorName=GetDocument()->m_pGenerator->GetName();
			m_testAnsiPatternButton.EnableWindow(GetDocument()->m_pGenerator->CanDisplayAnsiBWRects());
		}

		if ( lHint >= UPD_EVERYTHING && lHint <= UPD_FREEMEASURES )
		{
			// Ki : if current dataset is reference measure, update all views of all documents except current.
			if (GetDataRef() == GetDocument()) 
			{

				CDocEnumerator docEnumerator;
				CDocument* pDoc;
				while ((pDoc=docEnumerator.Next())!=NULL) 
				{
					if (GetDataRef() != pDoc)
						pDoc->UpdateAllViews(NULL, UPD_REFERENCEDATA);
				}
				RefreshSelection ();
				AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
			}
		}

		if ( lHint == UPD_SELECTEDCOLOR )
		{
			RefreshSelection ();
		}

		// Change background color
		if ( lHint == UPD_EVERYTHING || lHint == UPD_DATAREFDOC )
		{
			if (GetDataRef() == GetDocument()) {

				m_grayScaleGroup.SetHilighted(1);
				m_sensorGroup.SetHilighted(1);
				m_generatorGroup.SetHilighted(1);
				m_datarefGroup.SetHilighted(1);
				m_displayGroup.SetHilighted(1);
				m_paramGroup.SetHilighted(1);
				m_selectGroup.SetHilighted(1);
				m_viewGroup.SetHilighted(1);

				CDocEnumerator docEnumerator;
				CDocument* pDoc;
				while ((pDoc=docEnumerator.Next())!=NULL) {
					if (GetDataRef() != pDoc)
						pDoc->UpdateAllViews(NULL, UPD_REFERENCEDATA);
				}
				RefreshSelection ();
				AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
			}
			else 
			{
				if ( GetDocument()->m_pSensor->IsCalibrated() == 1 ) 
				{ 
					m_grayScaleGroup.SetHilighted(2);
					m_sensorGroup.SetHilighted(2);
					m_generatorGroup.SetHilighted(2);
					m_datarefGroup.SetHilighted(2);
					m_displayGroup.SetHilighted(2);
					m_paramGroup.SetHilighted(2);
					m_selectGroup.SetHilighted(2);
					m_viewGroup.SetHilighted(2);
				}
				else
				{
					m_grayScaleGroup.SetHilighted(0);
					m_sensorGroup.SetHilighted(0);
					m_generatorGroup.SetHilighted(0);
					m_datarefGroup.SetHilighted(0);
					m_displayGroup.SetHilighted(0);
					m_paramGroup.SetHilighted(0);
					m_selectGroup.SetHilighted(0);
					m_viewGroup.SetHilighted(0);
				}
			}

			//Update checkbox value
			if (GetDataRef() == GetDocument())
				m_datarefCheckButton = TRUE;
			else
				m_datarefCheckButton = FALSE;
		}
		
		m_AdjustXYZCheckButton.EnableWindow ( GetDocument()->m_pSensor->IsCalibrated () > 0 );
		m_AdjustXYZCheckButton.SetCheck ( GetDocument()->m_pSensor->IsCalibrated () == 1 );

		if ( GetDocument()->m_pSensor->IsCalibrated () == 1 || m_displayType == HCFR_xyz2_VIEW )
		{
			if ( m_editCheckButton.GetCheck () )
			{
				m_editCheckButton.SetCheck(FALSE);
				OnEditgridCheck ();
			}
			m_editCheckButton.EnableWindow(FALSE);
		}
		else
		{
			m_editCheckButton.EnableWindow(TRUE);
		}

		UpdateData(FALSE);
	}
}


CString CMainView::GetItemText(CColor & aMeasure, double YWhite, CColor & aReference, CColor & aRefDocColor, double YWhiteRefDoc, int aComponentNum, int nCol, double Offset)
{
	CString str;
	if(aMeasure.isValid() || ( aComponentNum == 7 || ( aComponentNum == 5 && ( GetDataRef() == NULL || GetDataRef() == GetDocument () ) ) ))
	{
		if ( aComponentNum < 3 )
		{
			switch(m_displayType)
			{
				case HCFR_SENSORRGB_VIEW:
				case HCFR_XYZ_VIEW:
    					str.Format("%.3f",aMeasure.GetXYZValue()[aComponentNum]);
					break;
				case HCFR_RGB_VIEW:
					str.Format("%.3f",aMeasure.GetRGBValue((GetColorReference().m_standard == UHDTV3?CColorReference(UHDTV2):GetColorReference()))[aComponentNum]);
					break;
				case HCFR_xyz2_VIEW:
					if (aMeasure.GetY() == 0)
						str.Format("%s","nan");
					else
						str.Format("%.4f",aMeasure.GetxyzValue()[aComponentNum]);
					break;
				case HCFR_xyY_VIEW:
                    if (aComponentNum < 2)
						if (aMeasure.GetY() == 0)
							str.Format("%s","nan");
						else
    						str.Format("%.4f",aMeasure.GetxyYValue()[aComponentNum]);
                    else
						if (aMeasure.GetY() == 0)
							str.Format("%s","0");
						else
	    					str.Format("%.3f",aMeasure.GetxyYValue()[aComponentNum]);
					break;
			}
			if ( str == "-99999.990" ) // Printed FX_NODATA value, coming from partially updated noDataColor
				str.Empty ();
		}
		else if ( aComponentNum == 3 )
		{
			COLORREF clr;
			double dE, dL, dC, dH;
			if ( aReference.isValid() )
				if (m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4)
					if ( nCol > 1 || m_displayMode == 4 )
					{
						double Intensity=GetConfig()->GetProfileInt("GDIGenerator","Intensity",100) / 100.;
                        if (m_displayMode == 0)
                        {
							str.Format("%.1f",aMeasure.GetDeltaE ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight ) );
       						dE=aMeasure.GetDeltaE ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight );
       						dL=aMeasure.GetDeltaLCH ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight, dC, dH );
                        }
                        else
                        {
                            str.Format("%.1f",aMeasure.GetDeltaE ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, true, m_displayMode == 3 ? 1:GetConfig()->gw_Weight ) );
    						dE=aMeasure.GetDeltaE ( YWhite, aReference,  1.0, GetColorReference(), GetConfig()->m_dE_form, true, m_displayMode == 3 ? 1:GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight );
    						dL=aMeasure.GetDeltaLCH ( YWhite, aReference,  1.0, GetColorReference(), GetConfig()->m_dE_form, true, m_displayMode == 3 ? 1:GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight, dC, dH );
                        }

						dEavg+=(isNan(dE)?dEavg:dE);
						dLavg+=(isNan(dL)?dLavg:dL);
						dCavg+=(isNan(dC)?dCavg:dC);
						dHavg+=(isNan(dH)?dHavg:dH);

                        if (dE > dEmax)
                            dEmax = dE;
						if (GetConfig()->m_dE_form == 0)
							clr = (dE<3.0?RGB(175,255,175):(dE<5?RGB(255,255,175):RGB(255,175,175)));
						else
							clr = (dE<2.0?RGB(175,255,175):(dE<3?RGB(255,255,175):RGB(255,175,175)));
                        if (GetConfig()->doHighlight)
                            m_pGrayScaleGrid->SetItemBkColour(4, nCol, clr);
						m_pGrayScaleGrid -> SetItemFont ( 4, nCol, m_pGrayScaleGrid->GetItemFont(0,0) ); // Set the font to bold
						dEcnt++;
					}
					else
						str.Empty ();
				else
				{
					double dL, dH, dC;
					str.Format("%.1f",aMeasure.GetDeltaE ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, false, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight ) );
					dE=aMeasure.GetDeltaE ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, false, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight );
					dL=aMeasure.GetDeltaLCH ( YWhite, aReference, 1.0, GetColorReference(), GetConfig()->m_dE_form, false, GetConfig()->m_GammaOffsetType == 5?3:GetConfig()->gw_Weight, dC, dH );
                    dEvector.push_back(isNan(dE)?dEavg:dE);
                    dLvector.push_back(isNan(dL)?dLavg:dL);
                    dCvector.push_back(isNan(dC)?dCavg:dC);
                    dHvector.push_back(isNan(dH)?dHavg:dH);
                    dEavg+=(isNan(dE)?dEavg:dE);
					dLavg+=(isNan(dL)?dLavg:dL);
					dCavg+=(isNan(dC)?dCavg:dC);
					dHavg+=(isNan(dH)?dHavg:dH);
                    if (dE > dEmax)
                        dEmax = dE;
					if (GetConfig()->m_dE_form == 0)
						clr = (dE<3.0?RGB(175,255,175):(dE<5?RGB(255,255,175):RGB(255,175,175)));
					else
						clr = (dE<2.0?RGB(175,255,175):(dE<3?RGB(255,255,175):RGB(255,175,175)));
                    if (GetConfig()->doHighlight)
                        m_pGrayScaleGrid->SetItemBkColour(4, nCol, clr);
					m_pGrayScaleGrid -> SetItemFont ( 4, nCol, m_pGrayScaleGrid->GetItemFont(0,0) ); // Set the font to bold
					dEcnt++;
				}
			else
				str.Empty ();
		}
		else if ( aComponentNum == 4 )
		{
			if ( aReference.isValid() && (nCol > 1 || ( m_displayMode != 0 && m_displayMode !=3)) )
				str.Format("%.4f",aMeasure.GetDeltaxy ( aReference, GetColorReference()) );
			else
				str.Empty ();
		}
		else if ( aComponentNum == 5 && (nCol > 1 || ( m_displayMode != 0 && m_displayMode != 3)) )
		{
			if ( aRefDocColor.isValid() )
					str.Format("%.1f",aMeasure.GetDeltaE ( YWhite, aRefDocColor, YWhiteRefDoc, GetColorReference(), GetConfig()->m_dE_form, m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4, m_displayMode == 3?1:GetConfig()->gw_Weight ) );
			else
				str.Empty ();
		}
		else if ( aComponentNum == 6 )
		{
			if ( aRefDocColor.isValid() && (nCol > 1 || ( m_displayMode != 0 && m_displayMode !=3)) )
				str.Format("%.4f",aMeasure.GetDeltaxy ( aRefDocColor, GetColorReference()) );
			else
				str.Empty ();
		}
		else
			str.Empty ();

		if ( aComponentNum == 7 || ( aComponentNum == 5 && ( GetDataRef() == NULL || GetDataRef() == GetDocument () ) ) )
		{
			if ( m_displayMode == 0 || m_displayMode == 4 || m_displayMode == 3 )
			{
				// Display reference gamma Y
				str.Empty();

				int nGrayScaleSize = GetDocument()->GetMeasure()->GetGrayScaleSize ();
				int size=GetDocument()->GetMeasure()->GetNearBlackScaleSize();
				if (m_displayMode == 4)
					size=GetDocument()->GetMeasure()->GetNearWhiteScaleSize();

				if ( nCol > 1 && nCol < m_displayMode == 0?nGrayScaleSize:size )
				{
					CColor White = GetDocument()->GetMeasure()->GetOnOffWhite();
					CColor Black;
					if (m_displayMode != 3)
						Black = GetDocument()->GetMeasure()->GetOnOffBlack();
					else
						Black = GetDocument()->GetMeasure()->GetNearBlack(0);

					if ( White.isValid() && Black.isValid())
					{
						double x, valx, valy;
						double yblack = 0.0;
						BOOL bIRE = GetDocument()->GetMeasure()->m_bIREScaleMode;
						
						if ( GetConfig ()->m_GammaOffsetType == 1 )
							yblack = Black.GetY();
						if (m_displayMode == 0)
							x = ArrayIndexToGrayLevel ( nCol - 1, nGrayScaleSize, GetConfig () -> m_bUseRoundDown );
						else if (m_displayMode == 3)
							valx = GrayLevelToGrayProp ( (double)(nCol - 1), GetConfig () -> m_bUseRoundDown );
						else if (m_displayMode == 4)
							valx = GrayLevelToGrayProp ( (double)(nCol - 1 + 101 - size) , GetConfig () -> m_bUseRoundDown );

						int mode = GetConfig()->m_GammaOffsetType;
						if (GetConfig()->m_colorStandard == sRGB) mode = 8;
						if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
						{
							if (m_displayMode == 0)
								valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown);
							if (mode == 5)
	                            valy = getEOTF(valx,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode) * 100.;
							else
	                            valy = getEOTF(valx,White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode) * White.GetY();

							str.Format ( "%.3f", valy );
						}
						else
						{
							if (m_displayMode == 0)
		    					valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+Offset)/(1.0+Offset);
	    					valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
		    				str.Format ( "%.3f", yblack + ( valy * ( White.GetY () - yblack ) ) );
						}
					}
				}
			}
			else
			{
				// Display primary/secondary/saturations colors delta luminance
				int	    nCol2 = nCol, satsize=GetDocument()->GetMeasure()->GetSaturationSize();;
				double  RefLuma [1000], sat=double (nCol)/ double (satsize-1);
//                CColor White = GetDocument() -> GetMeasure () -> GetGray ( GetDocument()->GetMeasure()->GetGrayScaleSize() - 1 );
                CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
	            CColor Black = GetDocument() -> GetMeasure () -> GetGray ( 0 );
				CColor satcolor;
				// Retrieve color luminance coefficients matching actual reference

				switch (m_displayMode)
				{
					case 1: 
						RefLuma [ 0 ] = GetDocument()->GetMeasure()->GetRefPrimary(0).GetLuminance();
						RefLuma [ 1 ] = GetDocument()->GetMeasure()->GetRefPrimary(1).GetLuminance();
						RefLuma [ 2 ] = GetDocument()->GetMeasure()->GetRefPrimary(2).GetLuminance();
						RefLuma [ 3 ] = GetDocument()->GetMeasure()->GetRefSecondary(0).GetLuminance();
						RefLuma [ 4 ] = GetDocument()->GetMeasure()->GetRefSecondary(1).GetLuminance();
						RefLuma [ 5 ] = GetDocument()->GetMeasure()->GetRefSecondary(2).GetLuminance();
						break ;
					case 5:
						satcolor = GetDocument()->GetMeasure()->GetRefSat(0, sat, (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
                        RefLuma [nCol - 1] = satcolor.GetLuminance();
						break;
					case 6:
                        satcolor = GetDocument()->GetMeasure()->GetRefSat(1, sat, (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
                        RefLuma [nCol - 1] = satcolor.GetLuminance();
						break;
					case 7:                                                
                        satcolor = GetDocument()->GetMeasure()->GetRefSat(2, sat, (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
                        RefLuma [nCol - 1] = satcolor.GetLuminance();
						break;
					case 8:
                        satcolor = GetDocument()->GetMeasure()->GetRefSat(3, sat, (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
                        RefLuma [nCol - 1] = satcolor.GetLuminance();
						break;
					case 9:
                        satcolor = GetDocument()->GetMeasure()->GetRefSat(4, sat, (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
                        RefLuma [nCol - 1] = satcolor.GetLuminance();
						break;
					case 10:
                        satcolor = GetDocument()->GetMeasure()->GetRefSat(5, sat, (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
                        RefLuma [nCol - 1] = satcolor.GetLuminance();
						break;
					case 11:
                        RefLuma [nCol -1] = aReference.GetLuminance();
						break;
				}

				CColor white = GetDocument()->GetMeasure()->GetPrimeWhite();

				if (!white.isValid() || m_displayMode == 0 || m_displayMode == 2 || m_displayMode == 3)
					white = GetDocument() -> GetMeasure () ->GetOnOffWhite();
				if ( m_displayMode > 4 && (GetConfig()->m_colorStandard == HDTVa || GetConfig()->m_colorStandard == HDTVb || GetConfig()->m_colorStandard==UHDTV3))
					white = GetDocument() -> GetMeasure () ->GetOnOffWhite();

				//special case check if user has done a primaries run at less than 100%, use grayscale white instead for colorchecker
				if (GetDocument()->GetMeasure()->GetOnOffWhite().isValid())
					if ((GetDocument()->GetMeasure()->GetPrimeWhite()[1] / GetDocument()->GetMeasure()->GetOnOffWhite()[1] < 0.9) && m_displayMode == 11)
						white = GetDocument() -> GetMeasure () ->GetOnOffWhite();

				if ( nCol2 < ( (m_displayMode > 11 || m_displayMode < 5) ? 7 : 1001) && white.isValid() && white.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter) > 0.0001 )
    		    {
					if (aMeasure.isValid())
					{
					double d = aMeasure.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter) / white.GetPreferedLuxValue(GetConfig () -> m_bPreferLuxmeter);
					if ( fabs ( ( RefLuma [ nCol2 - 1 ] - d ) / RefLuma [ nCol2 - 1 ] ) < 0.001 )
						str = "=";
					else if ( d < RefLuma [ nCol2 - 1 ] )
						str.Format("-%.1f %%", 100.0 * ( RefLuma [ nCol2 - 1 ] - d ) / RefLuma [ nCol2 - 1 ] );
					else
						str.Format("+%.1f %%", 100.0 * ( d - RefLuma [ nCol2 - 1 ] ) / RefLuma [ nCol2 - 1 ] );
					}
				}
				else
					str.Empty();
			}
		}
	}
	else
		str.Empty ();

	return str;
}

LPSTR CMainView::GetGridRowLabel(int aComponentNum)
{
	switch(aComponentNum)
	{
		case 0:
			switch(m_displayType)
			{
				case HCFR_SENSORRGB_VIEW:
				case HCFR_XYZ_VIEW:
					return "X";
					break;
				case HCFR_RGB_VIEW:
					switch(GetConfig()->m_colorStandard)
					{
						case SDTV:
							return "R601";
							break;
						case UHDTV:
							return "RDCI-P3";
							break;
						case UHDTV2:
							return "R2020";
							break;
						case UHDTV3:
							return "R2020P3";
							break;
						case HDTV:
							return "R709";
							break;
						case sRGB:
							return "R709";
							break;
						case PALSECAM:
							return "Rpal";
							break;
						case HDTVa:
							return m_displayMode==1?"R709(75%)":"R709";
							break;
						case CC6:
							return m_displayMode==1?"RCC6":"R709";
							break;
						case HDTVb:
							return m_displayMode==1?"R709(OPT-Plasma)":"R709";
							break;
						case CUSTOM:
							return "RCUSTOM";
							break;
						default:
							return "R?";
							break;
					}
				break;
				case HCFR_xyY_VIEW:
				case HCFR_xyz2_VIEW:
					return "x";
					break;
				default:
					return "?";
			}
			break;
		case 1:
			switch(m_displayType)
			{
				case HCFR_SENSORRGB_VIEW:
				case HCFR_XYZ_VIEW:
					return "Y";
					break;
				case HCFR_RGB_VIEW:
					switch(GetConfig()->m_colorStandard)
					{
						case SDTV:
							return "G601";
							break;
						case HDTV:
							return "G709";
							break;
						case UHDTV:
							return "GDCI-P3";
							break;
						case UHDTV2:
							return "G2020";
							break;
						case UHDTV3:
							return "G2020P3";
							break;
						case sRGB:
							return "G709";
							break;
						case PALSECAM:
							return "Gpal";
							break;
						case HDTVa:
							return m_displayMode==1?"G709(75%)":"G709";
							break;
						case CC6:
							return m_displayMode==1?"GCC6":"GCC6";
							break;
						case HDTVb:
							return m_displayMode==1?"G709(OPT-Plasma)":"G709";
							break;
						case CUSTOM:
							return "GCUSTOM";
							break;
						default:
							return "G?";
							break;
					}
				break;
				case HCFR_xyY_VIEW:
				case HCFR_xyz2_VIEW:
					return "y";
					break;
				default:
					return "?";
			}
			break;
		case 2:
			switch(m_displayType)
			{
				case HCFR_SENSORRGB_VIEW:
				case HCFR_XYZ_VIEW:
					return "Z";
					break;

				case HCFR_RGB_VIEW:
					switch(GetConfig()->m_colorStandard)
					{
						case SDTV:
							return "B601";
							break;
						case HDTV:
							return "B709";
							break;
						case UHDTV:
							return "BDCI-P3";
							break;
						case UHDTV2:
							return "B2020";
							break;
						case UHDTV3:
							return "B2020P3";
							break;
						case sRGB:
							return "B709";
							break;
						case PALSECAM:
							return "Bpal";
							break;
						case HDTVa:
							return m_displayMode==1?"B709(75%)":"B709";
							break;
						case CC6:
							return m_displayMode==1?"B709(CC6)":"B709";
							break;
						case HDTVb:
							return m_displayMode==1?"B709(OPT-Plasma)":"B709";
							break;
						case CUSTOM:
							return "BCUSTOM";
							break;
						default:
							return "B?";
							break;
					}
				break;
				case HCFR_xyz2_VIEW:
					return "z";
					break;
				case HCFR_xyY_VIEW:
					return "Y";
					break;
				default:
					return "?";
			}
			break;

		case 3:

			return "Delta E";
			break;

		case 4:
			return "delta xy";
			break;

		case 5:
		 	if ( GetDataRef() && GetDataRef() != GetDocument () )
				return "dE / ref";
			else
				return ( m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4 ? "Y target" : "delta luminance" );
			break;

		case 6:
			return "xy / ref";
			break;

		case 7:
			return ( m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4 ? "Y target" : "delta luminance" );
			break;
	}

	return "Undef";
}

void CMainView::UpdateGrid()
{

	if (m_pGrayScaleGrid)
	{
		CColor			aColor;
		CColor			refColor = GetColorReference().GetWhite();
		CColor			refDocColor = noDataColor;
		CColor			refLuxColor = noDataColor;
		BOOL			bAddedCol = FALSE;
		BOOL			bSpecialRef = FALSE;
		BOOL			bIRE = GetDocument()->GetMeasure()->m_bIREScaleMode;
		double			YWhiteOnOff = -1.0;
		double			YWhiteGray = -1.0;
		double			YWhitePrime = -1.0;
		double			YWhiteOnOffRefDoc = -1.0;
		double			YWhitePrimeRefDoc = -1.0;
		double			YWhiteGrayRefDoc = -1.0;
		double			YWhite = -1.0;
		double			YWhiteRefDoc = -1.0;
		double			Gamma,Offset = 0.0;
		COLORREF		clrSpecial1=RGB(128,128,128), clrSpecial2=RGB(128,128,128);
		CDataSetDoc *	pDataRef = GetDataRef();

		GV_ITEM Item;
		Item.mask = GVIF_TEXT|GVIF_FORMAT;
		Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
		if  (m_userBlack)
		{
			double Yblack = GetConfig()->GetProfileDouble("References","Manual Black Level",0);
			GetDocument()->GetMeasure()->SetGray(0, ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883));
			GetDocument()->GetMeasure()->SetNearBlack(0, ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883)) ;
			GetDocument()->GetMeasure()->SetOnOffBlack(ColorXYZ(Yblack*.95047,Yblack,Yblack*1.0883)) ;
		}

		// update values
		int nRows = 5;
		int	nCount;
		BOOL bHasLuxValues = FALSE;
		BOOL bHasLuxDelta = FALSE;
					
		dEavg = 0.0, dEmax=0.0;
		dEcnt = 0; dE10=0.0;
		dLavg = 0.0, dCavg = 0.0; dHavg = 0.0;

		if ( pDataRef == GetDocument () )
			pDataRef = NULL;

		// Retrieve measured white luminance to compute exact delta E, Lab and LCH values
		if ( GetDocument() -> GetMeasure () -> GetOnOffWhite ().isValid() )
			YWhiteOnOff = GetDocument() -> GetMeasure () -> GetOnOffWhite () [ 1 ];
		if ( GetDocument() -> GetMeasure () -> GetPrimeWhite ().isValid() )
			YWhitePrime = GetDocument() -> GetMeasure () -> GetPrimeWhite () [ 1 ];
		else
			YWhitePrime = YWhiteOnOff;

		nCount = GetDocument() -> GetMeasure () -> GetGrayScaleSize ();
		if ( GetDocument() -> GetMeasure () -> GetGray ( nCount - 1 ).isValid() )
			YWhiteGray = GetDocument() -> GetMeasure () -> GetGray ( nCount - 1 ) [ 1 ];

		if ( pDataRef )
		{
			int refCount;
			if ( pDataRef -> GetMeasure () -> GetOnOffWhite ().isValid() )
				YWhiteOnOffRefDoc = pDataRef -> GetMeasure () -> GetOnOffWhite () [ 1 ];
			if ( pDataRef -> GetMeasure () -> GetPrimeWhite ().isValid() )
				YWhitePrimeRefDoc = pDataRef -> GetMeasure () -> GetPrimeWhite () [ 1 ];

			refCount = pDataRef -> GetMeasure () -> GetGrayScaleSize ();
			if (refCount == nCount)
			{
				nCount = pDataRef -> GetMeasure () -> GetGrayScaleSize ();
				if ( pDataRef -> GetMeasure () -> GetGray ( nCount - 1 ).isValid() )
					YWhiteGrayRefDoc = pDataRef -> GetMeasure () -> GetGray ( nCount - 1 ) [ 1 ];
			}
			else
				pDataRef = NULL;

		}

		// Retrieve gamma and offset in case user has modified
        Gamma = GetConfig()->m_GammaRef;
        GetConfig()->m_GammaAvg = Gamma; //targets can be reference power law or modified for user average gamma, BT.1886 handled separately
        if ( nCount && GetDocument()->GetMeasure()->GetGray(0).isValid() )
		{
            GetDocument()->ComputeGammaAndOffset(&Gamma, &Offset, 1, 1, nCount, false);
//			Gamma = floor(Gamma * 100) / 100;
		}

        if (GetConfig()->m_useMeasuredGamma)
			GetConfig()->m_GammaAvg = (Gamma<1?2.2:floor((Gamma+.005)*100.)/100.);

        GetConfig()->SetPropertiesSheetValues();

		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);
		switch ( m_displayMode )
		{
			case 0:
				 YWhite = YWhiteGray;
				 YWhiteRefDoc = YWhiteGrayRefDoc;

				 nCount = GetDocument()->GetMeasure()->GetGrayScaleSize();
				 if ( pDataRef && pDataRef->GetMeasure()->GetGrayScaleSize() != nCount )
					pDataRef = NULL;

				 if ( pDataRef && pDataRef->GetMeasure()->m_bIREScaleMode != bIRE )
					pDataRef = NULL;

				 if ( nCount )
					bHasLuxValues = GetDocument()->GetMeasure()->GetGray(0).HasLuxValue ();
				 
				 bHasLuxDelta = bHasLuxValues;
				 if ( bHasLuxDelta )
					refLuxColor = GetDocument()->GetMeasure()->GetGray(nCount-1);

				 break;

 			case 1:
  				YWhite = YWhitePrime;
				YWhiteRefDoc = YWhitePrimeRefDoc;
				 nCount = 8;
				 bHasLuxValues = GetDocument()->GetMeasure()->GetRedPrimary().HasLuxValue ();
				 if ( bHasLuxValues )
				 {
					if ( GetDocument()->GetMeasure()->GetOnOffWhite().isValid() )
					{
						bHasLuxDelta = TRUE;
						refLuxColor = GetDocument()->GetMeasure()->GetOnOffWhite();
					}
				 }
				 break;

			case 2:
				 nCount = GetDocument()->GetMeasure()->GetMeasurementsSize();
				 if ( pDataRef && pDataRef->GetMeasure()->GetMeasurementsSize() != nCount )
					 pDataRef = NULL;
				 break;

			case 3:
				 YWhite = YWhiteGray;
				 YWhiteRefDoc = YWhiteGrayRefDoc;
				 nCount = GetDocument()->GetMeasure()->GetNearBlackScaleSize();
				 if ( pDataRef && pDataRef->GetMeasure()->GetNearBlackScaleSize() != nCount )
					pDataRef = NULL;

				 if ( nCount )
					bHasLuxValues = GetDocument()->GetMeasure()->GetNearBlack(0).HasLuxValue ();
				 break;

			case 4:
				 YWhite = YWhiteGray;
				 YWhiteRefDoc = YWhiteGrayRefDoc;
				 nCount = GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
				 if ( pDataRef && pDataRef->GetMeasure()->GetNearWhiteScaleSize() != nCount )
					pDataRef = NULL;

				 if ( nCount )
					bHasLuxValues = GetDocument()->GetMeasure()->GetNearWhite(0).HasLuxValue ();
				 break;

			case 12:
				 nCount = 4;
				 nRows = 3;
				 pDataRef = NULL;
				 bHasLuxValues = GetDocument()->GetMeasure()->GetOnOffWhite().HasLuxValue ();
				 break;

			default:
				 bool isSpecial = (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb || GetConfig()->m_colorStandard==UHDTV3);
				 YWhite = isSpecial?YWhiteOnOff:YWhitePrime;
				 YWhiteRefDoc = isSpecial?YWhiteOnOffRefDoc:YWhitePrimeRefDoc;

				 //special case check if user has done a less than 100% primaries run and use grayscale white instead for colorchecker
				if (GetDocument()->GetMeasure()->GetOnOffWhite().isValid())
					if ((YWhitePrime / YWhiteOnOff < 0.9) && m_displayMode == 11)
					{
						YWhite = YWhiteOnOff;
						YWhiteRefDoc = YWhiteOnOffRefDoc;
					}

				 if (m_displayMode != 11) 
				 {
					 nCount = GetDocument()->GetMeasure()->GetSaturationSize();
					 if ( pDataRef && pDataRef->GetMeasure()->GetSaturationSize() != nCount )
						 pDataRef = NULL;
				 }
				 else
				 {
                     if (isExtPat)
                         nCount = GetConfig()->GetCColorsSize();
                     else
                        nCount = GetConfig()->m_CCMode==CCSG?96:GetConfig()->m_CCMode==CMS||GetConfig()->m_CCMode==CPS?19:(GetConfig()->m_CCMode==AXIS?71:24);
				 }
				 
				 if ( nCount )
				 {
					switch ( m_displayMode )
					{
						case 5:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetRedSat(0).HasLuxValue ();
							 break;

						case 6:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetGreenSat(0).HasLuxValue ();
							 break;

						case 7:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetBlueSat(0).HasLuxValue ();
							 break;

						case 8:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetYellowSat(0).HasLuxValue ();
							 break;

						case 9:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetCyanSat(0).HasLuxValue ();
							 break;

						case 10:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetMagentaSat(0).HasLuxValue ();
							 break;

						case 11:
							 bHasLuxValues = GetDocument()->GetMeasure()->GetCC24Sat(0).HasLuxValue ();
							 break;
					}
				 }
				 break;
		}
		
		if ( pDataRef )
			nRows = 7;

		if ( m_displayMode <= 1 || (m_displayMode >= 3 && m_displayMode <=11) )
			nRows ++;

		if ( bHasLuxValues )
		{
			if ( bHasLuxDelta )
				nRows += 2;
			else
				nRows ++;
		}
        
		dEvector.clear();
        dLvector.clear();
        dCvector.clear();
        dHvector.clear();

		for( int j = 0 ; j < nCount ; j ++ )
		{
            int i = GetDocument() -> GetMeasure () -> GetGrayScaleSize ();
            ColorxyY tmpColor(GetColorReference().GetWhite());
			switch ( m_displayMode )
			{
				case 0:
                     double valy;
					 aColor = GetDocument()->GetMeasure()->GetGray(j);
					 if ( pDataRef )
					 {
						refDocColor = pDataRef->GetMeasure()->GetGray(j);
					 }
					 // Determine Reference Y luminance for Delta E calculus
					 if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
					 {
						// Compute reference Luminance regarding actual offset and reference gamma 
                        // fixed to use correct gamma predicts
                        // and added option to assume perfect gamma
						double x = ArrayIndexToGrayLevel ( j, nCount, GetConfig () -> m_bUseRoundDown );
            		    CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
	                	CColor Black = GetDocument() -> GetMeasure () -> GetGray ( 0 );
						int mode = GetConfig()->m_GammaOffsetType;
						if (GetConfig()->m_colorStandard == sRGB) mode = 8;

						if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
			            {
                            double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown);
                            valy = getEOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			            }
			            else
			            {
				            double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+Offset)/(1.0+Offset);
				            valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
							if (mode == 1) //black compensation target
								valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			            }

						if (mode  == 5)
							tmpColor[2] = valy * 100. / YWhite;
						else
							tmpColor[2] = valy;

                        if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
								tmpColor[2] = aColor [ 1 ] / YWhite; //perfect gamma

						refColor.SetxyYValue(tmpColor);
					 }
					 else
					 {
	                    YWhite = aColor [ 1 ];
						if ( pDataRef )
							YWhiteRefDoc = refDocColor [ 1 ];
					 }
					 break;

				case 1:
					 if ( j < 3 )
					 {
						aColor = GetDocument()->GetMeasure()->GetPrimary(j);
						refColor = GetDocument()->GetMeasure()->GetRefPrimary(j);
						if ( pDataRef )
							refDocColor = pDataRef->GetMeasure()->GetPrimary(j);
					 }
					 else if ( j < 6 )
					 {
						aColor = GetDocument()->GetMeasure()->GetSecondary(j-3);
						refColor = GetDocument()->GetMeasure()->GetRefSecondary(j-3);
						if ( pDataRef )
							refDocColor = pDataRef->GetMeasure()->GetSecondary(j-3);
					 }
					 else if ( j == 6 )
					 {
			            ColorxyY tmpColor(GetColorReference().GetWhite());
						aColor = GetDocument()->GetMeasure()->GetPrimeWhite();
						refColor.SetxyYValue(tmpColor);
						refDocColor.SetxyYValue(tmpColor);
//						refColor = noDataColor;
//						refDocColor = noDataColor;
					 }
					 else if ( j == 7 )
					 {
						aColor = GetDocument()->GetMeasure()->GetOnOffBlack();
						refColor = noDataColor;
						refDocColor = noDataColor;
					 }
					 else
					 {
						ASSERT(0);
						aColor = noDataColor;
						refColor = noDataColor;
						refDocColor = noDataColor;
					 }
					 break;

				case 2:
					 aColor = GetDocument()->GetMeasure()->GetMeasurement(j);
					 if( m_pGrayScaleGrid -> GetColumnCount() <= j+1 )
					 {
						CString	str;
						str.Format("%d",j+1);
						m_pGrayScaleGrid -> InsertColumn(str);
						m_pGrayScaleGrid -> SetItemFont ( 0, j+1, m_pGrayScaleGrid->GetItemFont(0,j) ); // Set the font to bold

						m_pGrayScaleGrid->SetItemState ( 4, j+1, m_pGrayScaleGrid->GetItemState(4,j+1) | GVIS_READONLY );

						m_pGrayScaleGrid->SetItemState ( 5, j+1, m_pGrayScaleGrid->GetItemState(5,j+1) | GVIS_READONLY );

						bAddedCol = TRUE;
					 }
					 
					 bSpecialRef = TRUE;
					 //assume white 1st
					 if ( aColor.GetDeltaxy ( GetColorReference().GetWhite(), GetColorReference() ) < 0.05 )
 					 {
						bSpecialRef = FALSE;
						refColor = GetColorReference().GetWhite();
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(0), GetColorReference() ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefPrimary(0);
						clrSpecial1 = RGB(255,192,192);
						clrSpecial2 = RGB(255,224,224);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(1), GetColorReference() ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefPrimary(1);
						clrSpecial1 = RGB(192,255,192);
						clrSpecial2 = RGB(224,255,224);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(2), GetColorReference() ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefPrimary(2);
						clrSpecial1 = RGB(192,192,255);
						clrSpecial2 = RGB(224,224,255);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(0), GetColorReference() ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefSecondary(0);
						clrSpecial1 = RGB(255,255,192);
						clrSpecial2 = RGB(255,255,224);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(1), GetColorReference() ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefSecondary(1);
						clrSpecial1 = RGB(192,255,255);
						clrSpecial2 = RGB(224,255,255);
					 }
					 else if ( aColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(2), GetColorReference() ) < 0.05 )
					 {
						refColor = GetDocument()->GetMeasure()->GetRefSecondary(2);
						clrSpecial1 = RGB(255,192,255);
						clrSpecial2 = RGB(255,224,255);
					 }
					 
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetMeasurement(j);
					 else
						refDocColor = noDataColor;
					 break;

				case 3:
					 aColor = GetDocument()->GetMeasure()->GetNearBlack(j);
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetNearBlack(j);
//                     double valy;
					 // Determine Reference Y luminance for Delta E calculus
					 if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
					 {
						// Compute reference Luminance regarding actual offset and reference gamma 
                        // fixed to use correct gamma predicts
                        // and added option to assume perfect gamma
						double x = ArrayIndexToGrayLevel ( j, 101, GetConfig () -> m_bUseRoundDown );
            		    CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
						CColor Black = GetDocument() -> GetMeasure () -> GetNearBlack(0);
						int mode = GetConfig()->m_GammaOffsetType;
						if (GetConfig()->m_colorStandard == sRGB) mode = 8;

						if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
			            {
                            double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown);
                            valy = getEOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			            }
			            else
			            {
				            double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+Offset)/(1.0+Offset);
				            valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
							if (mode == 1) //black compensation target
								valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			            }

						if (mode  == 5)
							tmpColor[2] = valy * 100. / YWhite;
						else
							tmpColor[2] = valy;

                        if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
								tmpColor[2] = aColor [ 1 ] / YWhite; //perfect gamma

						refColor.SetxyYValue(tmpColor);
					 }
					 else
					 {
	                    YWhite = aColor [ 1 ];
						if ( pDataRef )
							YWhiteRefDoc = refDocColor [ 1 ];
					 }
					 break;

				case 4:
					 aColor = GetDocument()->GetMeasure()->GetNearWhite(j);
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetNearWhite(j);
//                     double valy;
					 // Determine Reference Y luminance for Delta E calculus
					 if ( GetConfig ()->m_dE_gray > 0 || GetConfig ()->m_dE_form == 5 )
					 {
						// Compute reference Luminance regarding actual offset and reference gamma 
                        // fixed to use correct gamma predicts
                        // and added option to assume perfect gamma
						double x = ArrayIndexToGrayLevel ( 101 - nCount + j, 101, GetConfig () -> m_bUseRoundDown );
            		    CColor White = GetDocument() -> GetMeasure () -> GetOnOffWhite();
	                	CColor Black = GetDocument() -> GetMeasure () -> GetGray ( 0 );
						int mode = GetConfig()->m_GammaOffsetType;
						if (GetConfig()->m_colorStandard == sRGB) mode = 8;

						if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
			            {
                            double valx = GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown);
                            valy = getEOTF(valx, White, Black, GetConfig()->m_GammaRel, GetConfig()->m_Split, mode);
			            }
			            else
			            {
				            double valx=(GrayLevelToGrayProp(x, GetConfig () -> m_bUseRoundDown)+Offset)/(1.0+Offset);
				            valy=pow(valx, GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef));
							if (mode == 1) //black compensation target
								valy = (Black.GetY() + ( valy * ( YWhite - Black.GetY() ) )) / YWhite;
			            }

						if (mode  == 5)
							tmpColor[2] = valy * 100. / YWhite;
						else
							tmpColor[2] = valy;

                        if (GetConfig ()->m_dE_gray == 2 || GetConfig ()->m_dE_form == 5 )
								tmpColor[2] = aColor [ 1 ] / YWhite; //perfect gamma

						refColor.SetxyYValue(tmpColor);
					 }
					 else
					 {
	                    YWhite = aColor [ 1 ];
						if ( pDataRef )
							YWhiteRefDoc = refDocColor [ 1 ];
					 }
					 break;

				case 5:
					 aColor = GetDocument()->GetMeasure()->GetRedSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(0,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));

					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetRedSat(j);
					 break;

				case 6:
					 aColor = GetDocument()->GetMeasure()->GetGreenSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(1,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));

					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetGreenSat(j);
					 break;

				case 7:
					 aColor = GetDocument()->GetMeasure()->GetBlueSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(2,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));

					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetBlueSat(j);
					 break;

				case 8:
					 aColor = GetDocument()->GetMeasure()->GetYellowSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(3,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));

					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetYellowSat(j);
					 break;

				case 9:
					 aColor = GetDocument()->GetMeasure()->GetCyanSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(4,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));
					 
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetCyanSat(j);
					 break;

				case 10:
					 aColor = GetDocument()->GetMeasure()->GetMagentaSat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefSat(5,(double)j/(double)(nCount-1), (GetConfig()->m_colorStandard==HDTVa||GetConfig()->m_colorStandard==HDTVb));

					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetMagentaSat(j);
					 break;

				case 11:
					 aColor = GetDocument()->GetMeasure()->GetCC24Sat(j);
					 refColor = GetDocument()->GetMeasure()->GetRefCC24Sat(j);
					 if ( pDataRef )
						refDocColor = pDataRef->GetMeasure()->GetCC24Sat(j);
					 break;

				case 12:
					 refColor = noDataColor;
					 refDocColor = noDataColor;
					 switch ( j )
					 {
						case 0:
							 aColor = GetDocument()->GetMeasure()->GetOnOffBlack();
							 break;

						case 1:
							 aColor = GetDocument()->GetMeasure()->GetOnOffWhite();
							 break;

						case 2:
							 aColor = GetDocument()->GetMeasure()->GetAnsiBlack();
							 break;

						case 3:
							 aColor = GetDocument()->GetMeasure()->GetAnsiWhite();
							 break;
					 }
			}

			for( int i = 0 ; i < nRows ; i ++ )
			{
				Item.row = i+1;
				Item.col = j+1;
				
				if ( bHasLuxValues && i == nRows - ( 1 + bHasLuxDelta ) )
				{
					if ( aColor.isValid() && aColor.HasLuxValue () )
					{
						if ( GetConfig () -> m_bUseImperialUnits )
							Item.strText.Format ( "%.5g", aColor.GetLuxValue () * 0.0929 );
						else
							Item.strText.Format ( "%.5g", aColor.GetLuxValue () );
					}
					else
						Item.strText.Empty ();
				}
				else if ( bHasLuxValues && bHasLuxDelta && i == nRows - 1 )
				{
					if ( aColor.isValid() )
					{
						double dRef = refLuxColor.GetY() / refLuxColor.GetLuxValue ();
						double dColor = aColor.GetY() / aColor.GetLuxValue ();

						if ( fabs ( dRef ) < 0.000000001 )
							Item.strText.Empty ();
						else if ( fabs ( ( dRef - dColor ) / dRef ) < 0.001 )
						{
							if ( j == nCount - ( 1 + m_displayMode ) )
								Item.strText = "Ref";
							else
								Item.strText = "=";
						}
						else if ( dColor < dRef )
							Item.strText.Format("-%.1f %%", 100.0 * ( dRef - dColor ) / dRef );
						else
							Item.strText.Format("+%.1f %%", 100.0 * ( dColor - dRef ) / dRef );
					}
					else
						Item.strText.Empty ();
				}
				else
				{
					Item.strText = GetItemText ( aColor, YWhite, refColor, refDocColor, YWhiteRefDoc, i, j+1, Offset );
				}
				
				m_pGrayScaleGrid->SetItem(&Item);

				if ( bSpecialRef && i >= 3 )
				{
					m_pGrayScaleGrid->SetItemBkColour ( i+1, j+1, ( i&1 ? clrSpecial1 : clrSpecial2 ) );
				}
			}
		}
		
		if ( bAddedCol )
		{
			int width = m_pGrayScaleGrid -> GetColumnWidth ( 0 );
			m_pGrayScaleGrid -> SetColumnWidth ( m_pGrayScaleGrid->GetColumnCount() - 1, width * 11 / 10 );
		}

		if ( m_displayMode == 12 )
			UpdateContrastValuesInGrid ();

		m_pGrayScaleGrid->Refresh();
		
		if ( m_displayMode == 0 || m_displayMode == 3 || m_displayMode == 4)
		{
			// Gray scale mode: update group box title
			CString	Msg, Tmp;

			Msg.LoadString ( IDS_GRAYSCALE );
			m_grayScaleGroup.SetText ( Msg );

			if (GetDocument()->GetMeasure()->GetGray(0).isValid())
			{
				char	szBuf [ 256 ];

                if (m_displayMode == 0)
                {
    				Tmp.LoadString ( IDS_GAMMAAVERAGE );
	    			Msg += " ( ";
		    		Msg += Tmp;
			    	sprintf ( szBuf, ": %.2f, ", Gamma );
				    Msg += szBuf;					
				    Tmp.LoadString ( IDS_CONTRAST );
				    Msg += Tmp;
				    if ( GetDocument()->GetMeasure()->GetGray(0).GetXYZValue()[1] > 0.0001 )
				    {
					    sprintf ( szBuf, ": %.0f:1 )", GetDocument()->GetMeasure()->GetOnOffWhite()[1] / GetDocument()->GetMeasure()->GetGray(0).GetXYZValue()[1] );
					    Msg += szBuf;
				    }
					else if ( GetDocument()->GetMeasure()->GetGray(0).GetXYZValue()[1] == 0 )
					{
					    sprintf ( szBuf, ": %s:1 )", "Infinity" );
					    Msg += szBuf;
					}
				    else
				    {
					    Msg += ": ???:1 )";
				    }
                }

   			    if ( dEcnt > 0 )
				{
					dEavg_gs = dEavg / dEcnt;
					dEmax_gs = dEmax;
					CString dEform;
                    float a=2.0,b=3.0;
					Tmp.LoadString ( IDS_DELTAEAVERAGE );
					Msg += " ( ";
					Msg += Tmp;
					sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f", dEavg / dEcnt, dLavg / dEcnt, dCavg / dEcnt, dHavg / dEcnt, dEmax  );
					Msg += szBuf;
					switch (GetConfig()->m_dE_form)
					{
					case 0:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 1:
						{
						dEform = " [CIE76(ab)] )";
						break;
						}
					case 2:
						{
						dEform = " [CIE94] )";
						break;
						}
					case 3:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 4:
						{
						dEform = " [CMC(1:1)] )";
						break;
						}
					case 5:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					}
					Msg += dEform;
                    if (m_displayMode == 3 || m_displayMode == 4)
                        dEform = m_displayMode == 3?" [Absolute Y w/o gamma, dark adapted]":" [Absolute Y w/o gamma]";
                    else
    					dEform = GetConfig()->m_dE_gray==0?" [Relative Y]":(GetConfig ()->m_dE_gray == 1?" [Absolute Y w/gamma]":" [Absolute Y w/o gamma]");
					Msg += dEform;
                    if (GetConfig()->doHighlight)
					    m_grayScaleGroup.SetBorderColor (dEavg / dEcnt < a ? RGB(0,230,0):(dEavg / dEcnt < b?RGB(230,230,0):RGB(230,0,0)));
				}
			}

			m_grayScaleGroup.SetText ( Msg );
		} else if ( m_displayMode == 1 )
		{
			CString	Msg, Tmp;;
			Msg.LoadString ( IDS_SECONDARYCOLORS );
			m_grayScaleGroup.SetText ( Msg );
			if (GetDocument()->GetMeasure()->GetRedPrimary().isValid() && dEcnt > 0 )
		    {
				char	szBuf [ 256 ];
				CString dEform;
				float a=2.0, b=3;
				Tmp.LoadString ( IDS_DELTAEAVERAGE );
				Msg += " ( ";
				Msg += Tmp;
				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f", dEavg / dEcnt, dLavg / dEcnt, dCavg / dEcnt, dHavg / dEcnt, dEmax  );
				Msg += szBuf;					
					switch (GetConfig()->m_dE_form)
					{
					case 0:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 1:
						{
						dEform = " [CIE76(ab)] )";
						break;
						}
					case 2:
						{
						dEform = " [CIE94] )";
						break;
						}
					case 3:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 4:
						{
						dEform = " [CMC(1:1)] )";
						break;
						}
					case 5:
						{
						dEform = " [CIE2000] )";
						break;
						}
					}
					Msg += dEform;
                    if (GetConfig()->doHighlight)
                        m_grayScaleGroup.SetBorderColor (dEavg / dEcnt < a ? RGB(0,230,0):(dEavg / dEcnt < b?RGB(230,230,0):RGB(230,0,0)));
			}
			m_grayScaleGroup.SetText ( Msg );
		} else if ( m_displayMode > 4 && m_displayMode < 11 )
		{
			CString	Msg, Tmp;;
			Msg.LoadString ( IDS_SATURATIONCOLORS );
			m_grayScaleGroup.SetText ( Msg );
			if (GetDocument()->GetMeasure()->GetRedSat(0).isValid() && dEcnt > 0 )
		    {
				switch(m_displayMode)
				{
				case 5:
					dEavg_sr = dEavg / dEcnt;
					dEmax_sr = dEmax;
					break;
				case 6:
					dEavg_sg = dEavg / dEcnt;
					dEmax_sg = dEmax;
					break;
				case 7:
					dEavg_sb = dEavg / dEcnt;
					dEmax_sb = dEmax;
					break;
				case 8:
					dEavg_sy = dEavg / dEcnt;
					dEmax_sy = dEmax;
					break;
				case 9:
					dEavg_sc = dEavg / dEcnt;
					dEmax_sc = dEmax;
					break;
				case 10:
					dEavg_sm = dEavg / dEcnt;
					dEmax_sm = dEmax;
					break;
				}
				char	szBuf [ 256 ];
				CString dEform;
				float a=2.0, b=3;
				Tmp.LoadString ( IDS_DELTAEAVERAGE );
				Msg += " ( ";
				Msg += Tmp;
				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f", dEavg / dEcnt, dLavg / dEcnt, dCavg / dEcnt, dHavg / dEcnt, dEmax  );
				Msg += szBuf;					
					switch (GetConfig()->m_dE_form)
					{
					case 0:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 1:
						{
						dEform = " [CIE76(ab)] )";
						break;
						}
					case 2:
						{
						dEform = " [CIE94] )";
						break;
						}
					case 3:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 4:
						{
						dEform = " [CMC(1:1)] )";
						break;
						}
					case 5:
						{
						dEform = " [CIE2000] )";
						break;
						}
					}
					Msg += dEform;
                    if (GetConfig()->doHighlight)
					    m_grayScaleGroup.SetBorderColor (dEavg / dEcnt < a ? RGB(0,230,0):(dEavg / dEcnt < b?RGB(230,230,0):RGB(230,0,0)));
			}
			m_grayScaleGroup.SetText ( Msg );
		} else if (m_displayMode == 11)
		{
			CString	Msg, Tmp;
			BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);
			Msg.LoadString ( IDS_CC24COLORS );
			Msg += " - ";
			Msg += (GetConfig()->m_CCMode == GCD?"Classic GCD":(GetConfig()->m_CCMode==MCD?"Classic MCD":(GetConfig()->m_CCMode==SKIN?"Pantone skin tones":(GetConfig()->m_CCMode==CCSG?"CalMan SG":isExtPat?GetConfig()->GetCColorsN(-1).c_str():(GetConfig()->m_CCMode==CMS?"CalMAN SG skin tones":(GetConfig()->m_CCMode==CPS?"ChromaPure skin tones":(GetConfig()->m_CCMode==CMC?"Classic CalMAN":"RGB Luminance Ramps")))))));
			m_grayScaleGroup.SetText ( Msg );
			if (GetDocument()->GetMeasure()->GetCC24Sat(0).isValid() && dEcnt > 0 )
		    {
				char	szBuf [ 256 ];
				CString dEform;
				dEavg_cc = dEavg / dEcnt;
				dEmax_cc = dEmax;
				float a = 2.0, b = 3;
				Tmp.LoadString ( IDS_DELTAEAVERAGE );
				Msg += " ( ";
				Msg += Tmp;
				if (GetConfig()->GetCColorsSize() >= 96 )
                {
                    vector<double>::iterator max;
                    max = max_element( dEvector.begin(), dEvector.end() );
                    double maxv = *max ;
                    int pos = distance(dEvector.begin(), max);
                    std::sort ( dEvector.begin(), dEvector.end() );
                    for ( unsigned i = (int) (dEvector.size() - (dEvector.size() / 10 + 1)); i < dEvector.size(); i++ ) dE10+=(float)dEvector[i];
					dE10min = dEvector[(int) (dEvector.size() - (dEvector.size() / 10 + 1))];
                    dE10 = dE10 / (int) ( (dEvector.size() / 10 + 1) );
                    char aBuf[10];
                    sprintf(aBuf,"Color %d",pos+1);
					dEmax_cc = maxv;

					if (GetConfig()->m_CCMode == CCSG )
        				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f[%s], worst 10%%: %.2f", dEavg / dEcnt, dLavg / dEcnt, dCavg / dEcnt, dHavg / dEcnt, maxv, PatName[pos], dE10 );
                    else
        				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f[%s], worst 10%%: %.2f", dEavg / dEcnt, dLavg / dEcnt, dCavg / dEcnt, dHavg / dEcnt, maxv, aBuf, dE10 );
                }
                else
				{
					dE10min=0.;
    				sprintf ( szBuf, ": %.2f [%.2f,%.2f,%.2f] max: %.2f", dEavg / dEcnt, dLavg / dEcnt, dCavg / dEcnt, dHavg / dEcnt, dEmax);
				}
                    dEvector.clear();
                    dLvector.clear();
                    dCvector.clear();
                    dHvector.clear();
				    Msg += szBuf;					
					switch (GetConfig()->m_dE_form)
					{
					case 0:
						{
						dEform = " [CIE76(uv)] )";
						a=3.0;
						b=5;
						break;
						}
					case 1:
						{
						dEform = " [CIE76(ab)] )";
						break;
						}
					case 2:
						{
						dEform = " [CIE94] )";
						break;
						}
					case 3:
						{
						dEform = " [CIE2000] )";
						break;
						}
					case 4:
						{
						dEform = " [CMC(1:1)] )";
						break;
						}
					case 5:
						{
						dEform = " [CIE2000] )";
						break;
						}
					}
					Msg += dEform;
                    if (GetConfig()->doHighlight)
					    m_grayScaleGroup.SetBorderColor (dEavg / dEcnt < a ? RGB(0,230,0):(dEavg / dEcnt < b?RGB(230,230,0):RGB(230,0,0)));
			}
			m_grayScaleGroup.SetText ( Msg );
		}
	}
}

void CMainView::UpdateContrastValuesInGrid ()
{
	GV_ITEM Item;

	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;

	Item.col = 6;
	Item.row = 1;

	if ( GetDocument()->GetMeasure()->GetOnOffContrast () > 0.0 )
		Item.strText.Format ( "%.0f:1", GetDocument()->GetMeasure()->GetOnOffContrast () );
	else
		if ( GetDocument()->GetMeasure()->GetOnOffContrast () == -1 )
			Item.strText.Format ("%s","Infinite");
		else
			Item.strText.Format ("%s","undefined");

	m_pGrayScaleGrid->SetItem(&Item);


	Item.row = 2;

	if ( GetDocument()->GetMeasure()->GetAnsiContrast () > 0.0 )
		Item.strText.Format ( "%.0f:1", GetDocument()->GetMeasure()->GetAnsiContrast () );
	else
		if ( GetDocument()->GetMeasure()->GetOnOffContrast () == -1 )
			Item.strText.Format ("%s","Infinite");
		else
			Item.strText.Format ("%s","undefined");

	m_pGrayScaleGrid->SetItem(&Item);
}

void CMainView::OnGrayScaleGridBeginEdit(NMHDR *pNotifyStruct,LRESULT* pResult)
{
    NM_GRIDVIEW *	pItem = (NM_GRIDVIEW*) pNotifyStruct;

    // Check that cell is valid
	if ( pItem->iColumn < 1 || pItem->iRow > 3)
		return;

	// Get document XYZ value
	CColor aColorMeasure;
	
	switch ( m_displayMode )
	{
		case 0:
			 aColorMeasure=GetDocument()->GetMeasure()->GetGray(pItem->iColumn-1);
			 if (pItem->iColumn == GetDocument()->GetMeasure()->GetGrayScaleSize())
				isSelectedWhiteY = TRUE;
			 break;

		case 1:
			 if ( pItem->iColumn < 4 )
				aColorMeasure=GetDocument()->GetMeasure()->GetPrimary(pItem->iColumn-1);
			 else if ( pItem->iColumn < 7 )
				aColorMeasure=GetDocument()->GetMeasure()->GetSecondary(pItem->iColumn-4);
			 else if ( pItem->iColumn == 7 )
			 {
				aColorMeasure=GetDocument()->GetMeasure()->GetPrimeWhite();
				isSelectedWhiteY = TRUE;
			 }
			 else if ( pItem->iColumn == 8 )
				aColorMeasure=GetDocument()->GetMeasure()->GetOnOffBlack();
			 else
			 {
				ASSERT(0);
				aColorMeasure=noDataColor;
			 }
			 break;

		case 2:
			 aColorMeasure=GetDocument()->GetMeasure()->GetMeasurement(pItem->iColumn-1);
			 break;

		case 3:
			 aColorMeasure=GetDocument()->GetMeasure()->GetNearBlack(pItem->iColumn-1);
			 break;

		case 4:
			 aColorMeasure=GetDocument()->GetMeasure()->GetNearWhite(pItem->iColumn-1);
			 break;

		case 5:
			 aColorMeasure=GetDocument()->GetMeasure()->GetRedSat(pItem->iColumn-1);
			 break;

		case 6:
			 aColorMeasure=GetDocument()->GetMeasure()->GetGreenSat(pItem->iColumn-1);
			 break;

		case 7:
			 aColorMeasure=GetDocument()->GetMeasure()->GetBlueSat(pItem->iColumn-1);
			 break;

		case 8:
			 aColorMeasure=GetDocument()->GetMeasure()->GetYellowSat(pItem->iColumn-1);
			 break;

		case 9:
			 aColorMeasure=GetDocument()->GetMeasure()->GetCyanSat(pItem->iColumn-1);
			 break;

		case 10:
			 aColorMeasure=GetDocument()->GetMeasure()->GetMagentaSat(pItem->iColumn-1);
			 break;

		case 11:
			 aColorMeasure=GetDocument()->GetMeasure()->GetCC24Sat(pItem->iColumn-1);
			 break;

		case 12:
			 switch ( pItem->iColumn )
			 {
				case 1:
					 aColorMeasure=GetDocument()->GetMeasure()->GetOnOffBlack();
					 break;

				case 2:
					 aColorMeasure=GetDocument()->GetMeasure()->GetOnOffWhite();
					 break;

				case 3:
					 aColorMeasure=GetDocument()->GetMeasure()->GetAnsiBlack();
					 break;

				case 4:
					 aColorMeasure=GetDocument()->GetMeasure()->GetAnsiWhite();
					 break;
			 }
			 break;
	}
	
	ColorTriplet aColor = ColorXYZ();

	if ( aColorMeasure.isValid() )
	{
		// Get color data from XYZ value 
		switch(m_displayType)
		{
			case HCFR_SENSORRGB_VIEW:
				isSelectedWhiteY = FALSE;
				break;
			case HCFR_XYZ_VIEW:
				aColor=aColorMeasure.GetXYZValue();
				if (isSelectedWhiteY)
				{
					if (pItem->iRow == 2)
						isSelectedWhiteY = TRUE;
					else
						isSelectedWhiteY = FALSE;
				}
				break;
			case HCFR_RGB_VIEW:
				aColor=aColorMeasure.GetRGBValue((GetColorReference().m_standard == UHDTV3?CColorReference(UHDTV2):GetColorReference()));
				isSelectedWhiteY = FALSE;
				break;
			case HCFR_xyY_VIEW:
				aColor=aColorMeasure.GetxyYValue();
				if (isSelectedWhiteY)
				{
					if (pItem->iRow == 3)
						isSelectedWhiteY = TRUE;
					else
						isSelectedWhiteY = FALSE;
				}
				break;
		}
	}

	// Retrieve correct row value
	CString aNewStr;

	if ( aColor.isValid() )
	{
		double aVal;
		aVal = aColor[pItem->iRow-1];

		if ( aVal != FX_NODATA )
			aNewStr.Format ( "%f", aVal );
		else
			isSelectedWhiteY = FALSE;
		m_pGrayScaleGrid->SetItemText(pItem->iRow,pItem->iColumn,aNewStr);
	}
	else
		isSelectedWhiteY = FALSE;
}

void CMainView::OnGrayScaleGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult)
{
	LPARAM			lHint = UPD_EVERYTHING;
    NM_GRIDVIEW *	pItem = (NM_GRIDVIEW*) pNotifyStruct;

    // Check that cell is valid
	if ( pItem->iColumn < 1 || pItem->iRow > 3)
		return;

	CString aNewStr=m_pGrayScaleGrid->GetItemText(pItem->iRow,pItem->iColumn);
	aNewStr.Replace(",",".");	// replace decimal separator if necessary
 	double aVal;
	BOOL bAcceptChange = !aNewStr.IsEmpty() && sscanf(aNewStr,"%lf",&aVal) && (m_displayType != HCFR_xyz2_VIEW);
	if(bAcceptChange)	// update value in document
	{
		// Get document XYZ value
		CColor aColorMeasure;
		
		switch ( m_displayMode )
		{
			case 0:
				 aColorMeasure=GetDocument()->GetMeasure()->GetGray(pItem->iColumn-1);
				 break;

			case 1:
				 if ( pItem->iColumn < 4 )
					aColorMeasure=GetDocument()->GetMeasure()->GetPrimary(pItem->iColumn-1);
				 else if ( pItem->iColumn < 7 )
					aColorMeasure=GetDocument()->GetMeasure()->GetSecondary(pItem->iColumn-4);
				 else if ( pItem->iColumn == 7 )
					aColorMeasure=GetDocument()->GetMeasure()->GetPrimeWhite();
				 else if ( pItem->iColumn == 8 )
					aColorMeasure=GetDocument()->GetMeasure()->GetOnOffBlack();
				 else
				 {
					ASSERT(0);
					aColorMeasure=noDataColor;
				 }
				 break;

			case 2:
				 aColorMeasure=GetDocument()->GetMeasure()->GetMeasurement(pItem->iColumn-1);
				 break;

			case 3:
				 aColorMeasure=GetDocument()->GetMeasure()->GetNearBlack(pItem->iColumn-1);
				 break;

			case 4:
				 aColorMeasure=GetDocument()->GetMeasure()->GetNearWhite(pItem->iColumn-1);
				 break;

			case 5:
				 aColorMeasure=GetDocument()->GetMeasure()->GetRedSat(pItem->iColumn-1);
				 break;

			case 6:
				 aColorMeasure=GetDocument()->GetMeasure()->GetGreenSat(pItem->iColumn-1);
				 break;

			case 7:
				 aColorMeasure=GetDocument()->GetMeasure()->GetBlueSat(pItem->iColumn-1);
				 break;

			case 8:
				 aColorMeasure=GetDocument()->GetMeasure()->GetYellowSat(pItem->iColumn-1);
				 break;

			case 9:
				 aColorMeasure=GetDocument()->GetMeasure()->GetCyanSat(pItem->iColumn-1);
				 break;

			case 10:
				 aColorMeasure=GetDocument()->GetMeasure()->GetMagentaSat(pItem->iColumn-1);
				 break;

			case 11:
				 aColorMeasure=GetDocument()->GetMeasure()->GetCC24Sat(pItem->iColumn-1);
				 break;

			case 12:
				 switch ( pItem->iColumn )
				 {
					case 1:
						 aColorMeasure=GetDocument()->GetMeasure()->GetOnOffBlack();
						 break;

					case 2:
						 aColorMeasure=GetDocument()->GetMeasure()->GetOnOffWhite();
						 break;

					case 3:
						 aColorMeasure=GetDocument()->GetMeasure()->GetAnsiBlack();
						 break;

					case 4:
						 aColorMeasure=GetDocument()->GetMeasure()->GetAnsiWhite();
						 break;
				 }
				 break;
		}
		
		if ( !aColorMeasure.isValid() )
		{
			// No color value: build a color from reference white
			aColorMeasure = GetColorReference().GetWhite ();
		}
		//allow user to scale all measurements to new white luminance
		if (isSelectedWhiteY && GetColorApp()->InMeasureMessageBox("Scale all measurements to new white Y?","Scale all",MB_ICONQUESTION | MB_YESNO) == IDYES) 
		{
			double fact = aVal / aColorMeasure.GetY();
			CColor aNewColor;
			int nMeasures = GetDocument()->GetMeasure()->GetGrayScaleSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetGray(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetGray(i, aNewColor);
				}
			}
			for (int i = 0; i <= 2; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetPrimary(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetPrimary(i, aNewColor);
				}

				aNewColor = GetDocument()->GetMeasure()->GetSecondary(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetSecondary(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetMeasurementsSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetMeasurement(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetMeasurements(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetSaturationSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetRedSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetRedSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetBlueSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetBlueSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetGreenSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetGreenSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetYellowSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetYellowSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetCyanSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetCyanSat(i, aNewColor);
				}
				aNewColor = GetDocument()->GetMeasure()->GetMagentaSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetMagentaSat(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetCC24MasterSaturationSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetCC24MasterSat(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetCC24MasterSat(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetNearBlackScaleSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetNearBlack(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetNearBlack(i, aNewColor);
				}
			}
			nMeasures = GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
			for (int i = 0; i < nMeasures; i++)
			{
				aNewColor = GetDocument()->GetMeasure()->GetNearWhite(i);
				if (aNewColor.isValid())
				{
					aNewColor.SetX(fact * aNewColor.GetX());
					aNewColor.SetY(fact * aNewColor.GetY());
					aNewColor.SetZ(fact * aNewColor.GetZ());
					GetDocument()->GetMeasure()->SetNearWhite(i, aNewColor);
				}
			}

			aNewColor = GetDocument()->GetMeasure()->GetPrimeWhite();
			aNewColor.SetX(fact * aNewColor.GetX());
			aNewColor.SetY(fact * aNewColor.GetY());
			aNewColor.SetZ(fact * aNewColor.GetZ());
			GetDocument()->GetMeasure()->SetPrimeWhite(aNewColor);

			aNewColor = GetDocument()->GetMeasure()->GetOnOffWhite();
			aNewColor.SetX(fact * aNewColor.GetX());
			aNewColor.SetY(fact * aNewColor.GetY());
			aNewColor.SetZ(fact * aNewColor.GetZ());
			GetDocument()->GetMeasure()->SetOnOffWhite(aNewColor);

			aNewColor = GetDocument()->GetMeasure()->GetAnsiWhite();
			aNewColor.SetX(fact * aNewColor.GetX());
			aNewColor.SetY(fact * aNewColor.GetY());
			aNewColor.SetZ(fact * aNewColor.GetZ());
			GetDocument()->GetMeasure()->SetAnsiWhite(aNewColor);

			aNewColor = GetDocument()->GetMeasure()->GetAnsiBlack();
			aNewColor.SetX(fact * aNewColor.GetX());
			aNewColor.SetY(fact * aNewColor.GetY());
			aNewColor.SetZ(fact * aNewColor.GetZ());
			GetDocument()->GetMeasure()->SetAnsiBlack(aNewColor);
			isSelectedWhiteY = FALSE;
			lHint = UPD_EVERYTHING;
		} else //normal edit
		{

		int nsize=GetDocument()->GetMeasure()->GetGrayScaleSize();
		ColorTriplet aColor = ColorXYZ();

		// Get color data from XYZ value 
		switch(m_displayType)
		{
			case HCFR_XYZ_VIEW:
				aColor=aColorMeasure.GetXYZValue();
				break;
			case HCFR_SENSORRGB_VIEW:
				aColor=aColorMeasure.GetXYZValue();
				break;
			case HCFR_RGB_VIEW:
				aColor=aColorMeasure.GetRGBValue((GetColorReference().m_standard == UHDTV3?CColorReference(UHDTV2):GetColorReference()));
				break;
			case HCFR_xyY_VIEW:
				aColor=aColorMeasure.GetxyYValue();
				break;
		}

		// change the correct row value
		aColor[pItem->iRow-1]=aVal;

		// Convert back color data to XYZ
		switch(m_displayType)
		{
			case HCFR_XYZ_VIEW:
				aColorMeasure.SetXYZValue(ColorXYZ(aColor));
				break;
			case HCFR_SENSORRGB_VIEW:
				aColorMeasure.SetXYZValue(ColorXYZ(aColor));
				break;
			case HCFR_RGB_VIEW:
				aColorMeasure.SetRGBValue(ColorRGB(aColor), (GetColorReference().m_standard == UHDTV3?CColorReference(UHDTV2):GetColorReference()));
				break;
			case HCFR_xyY_VIEW:
				aColorMeasure.SetxyYValue(ColorxyY(aColor));
				break;
		}
		
		// Update document XYZ value
		switch ( m_displayMode )
		{
			case 0:
				 GetDocument()->GetMeasure()->SetGray(pItem->iColumn-1,aColorMeasure);
				 if (pItem->iColumn == 1)
					 GetDocument()->GetMeasure()->SetOnOffBlack(aColorMeasure);
				 if (pItem->iColumn == nsize)
					 GetDocument()->GetMeasure()->SetOnOffWhite(aColorMeasure);
				 lHint = UPD_GRAYSCALE;
				 break;

			case 1:
				 switch ( pItem->iColumn )
				 {
					case 1:
						 GetDocument()->GetMeasure()->SetRedPrimary(aColorMeasure);
						 lHint = UPD_PRIMARIES;
						 break;

					case 2:
						 GetDocument()->GetMeasure()->SetGreenPrimary(aColorMeasure);
						 lHint = UPD_PRIMARIES;
						 break;

					case 3:
						 GetDocument()->GetMeasure()->SetBluePrimary(aColorMeasure);
						 lHint = UPD_PRIMARIES;
						 break;

					case 4:
						 GetDocument()->GetMeasure()->SetYellowSecondary(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;

					case 5:
						 GetDocument()->GetMeasure()->SetCyanSecondary(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;

					case 6:
						 GetDocument()->GetMeasure()->SetMagentaSecondary(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;

					case 7:
						 GetDocument()->GetMeasure()->SetPrimeWhite(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;

					case 8:
						 GetDocument()->GetMeasure()->SetOnOffBlack(aColorMeasure);
						 lHint = UPD_SECONDARIES;
						 break;
				 }
				 break;

			case 2:
				 GetDocument()->GetMeasure()->SetMeasurements(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_FREEMEASURES;
				 break;

			case 3:
				 GetDocument()->GetMeasure()->SetNearBlack(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_NEARBLACK;
				 break;

			case 4:
				 GetDocument()->GetMeasure()->SetNearWhite(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_NEARWHITE;
				 break;

			case 5:
				 GetDocument()->GetMeasure()->SetRedSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_REDSAT;
				 break;

			case 6:
				 GetDocument()->GetMeasure()->SetGreenSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_GREENSAT;
				 break;

			case 7:
				 GetDocument()->GetMeasure()->SetBlueSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_BLUESAT;
				 break;

			case 8:
				 GetDocument()->GetMeasure()->SetYellowSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_YELLOWSAT;
				 break;

			case 9:
				 GetDocument()->GetMeasure()->SetCyanSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_CYANSAT;
				 break;

			case 10:
				 GetDocument()->GetMeasure()->SetMagentaSat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_MAGENTASAT;
				 break;

			case 11:
				 GetDocument()->GetMeasure()->SetCC24Sat(pItem->iColumn-1,aColorMeasure);
				 lHint = UPD_CC24SAT;
				 break;

			case 12:
				 switch ( pItem->iColumn )
				 {
					case 1:
						 GetDocument()->GetMeasure()->SetOnOffBlack(aColorMeasure);
						 break;

					case 2:
						 GetDocument()->GetMeasure()->SetOnOffWhite(aColorMeasure);
						 break;

					case 3:
						 GetDocument()->GetMeasure()->SetAnsiBlack(aColorMeasure);
						 break;

					case 4:
						 GetDocument()->GetMeasure()->SetAnsiWhite(aColorMeasure);
						 break;
				 }
				 lHint = UPD_CONTRAST;
				 break;
		}
		}

		GetDocument()->SetModifiedFlag(TRUE);
		GetDocument()->UpdateAllViews(NULL, lHint);	// Called with NULL to update current view too (format correctly values)
		
		switch ( m_displayMode )
		{
			case 0:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetGray(pItem->iColumn-1) );
				 break;

			case 1:
				 if ( pItem->iColumn < 4 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetPrimary(pItem->iColumn-1) );
				 else if ( pItem->iColumn < 7 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetSecondary(pItem->iColumn-4) );
				 else if ( pItem->iColumn == 7 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetPrimeWhite() );
				 else if ( pItem->iColumn == 8 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffBlack() );
				 else
				 {
					ASSERT(0);
					SetSelectedColor ( noDataColor );
				 }
				 break;

			case 2:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetMeasurement(pItem->iColumn-1) );
				 break;

			case 3:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetNearBlack(pItem->iColumn-1) );
				 break;

			case 4:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetNearWhite(pItem->iColumn-1) );
				 break;

			case 5:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetRedSat(pItem->iColumn-1) );
				 break;

			case 6:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetGreenSat(pItem->iColumn-1) );
				 break;

			case 7:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetBlueSat(pItem->iColumn-1) );
				 break;

			case 8:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetYellowSat(pItem->iColumn-1) );
				 break;

			case 9:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetCyanSat(pItem->iColumn-1) );
				 break;

			case 10:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetMagentaSat(pItem->iColumn-1) );
				 break;

			case 11:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetCC24Sat(pItem->iColumn-1) );
				 break;

			case 12:
				 switch ( pItem->iColumn )
				 {
					case 1:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffBlack() );
						 break;

					case 2:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffWhite() );
						 break;

					case 3:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetAnsiBlack() );
						 break;

					case 4:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetAnsiWhite() );
						 break;
				 }
				 UpdateContrastValuesInGrid ();
				 break;
		}

		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}

    *pResult = (bAcceptChange)? 0 : -1;
}

void CMainView::OnGrayScaleGridEndSelChange(NMHDR *pNotifyStruct,LRESULT* pResult)
{
	// Clear selection in other grids: only one grid with selection at time
	int maxCol=m_pGrayScaleGrid->GetSelectedCellRange().GetMaxCol();
	int minCol=m_pGrayScaleGrid->GetSelectedCellRange().GetMinCol();
	if( maxCol != minCol || minCol < 1 )
		SetSelectedColor ( noDataColor );	// if more than one column selected => no data selected t avoid confusion
	else
	{
		switch ( m_displayMode )
		{
			case 0:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetGray(minCol-1) );
				 break;

			case 1:
				 if ( minCol < 4 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetPrimary(minCol-1) );
				 else if ( minCol < 7 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetSecondary(minCol-4) );
				 else if ( minCol == 7 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetPrimeWhite() );
				 else if ( minCol == 8 )
					SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffBlack() );
				 else
				 {
					ASSERT(0);
					SetSelectedColor ( noDataColor );
				 }
				 break;

			case 2:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetMeasurement(minCol-1) );
				 break;

			case 3:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetNearBlack(minCol-1) );
				 break;

			case 4:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetNearWhite(minCol-1) );
				 break;

			case 5:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetRedSat(minCol-1) );
				 break;

			case 6:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetGreenSat(minCol-1) );
				 break;

			case 7:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetBlueSat(minCol-1) );
				 break;

			case 8:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetYellowSat(minCol-1) );
				 break;

			case 9:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetCyanSat(minCol-1) );
				 break;

			case 10:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetMagentaSat(minCol-1) );
				 break;

			case 11:
				 SetSelectedColor ( GetDocument()->GetMeasure()->GetCC24Sat(minCol-1) );
				 break;

			case 12:
				 switch ( minCol )
				 {
					case 1:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffBlack() );
						 break;

					case 2:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetOnOffWhite() );
						 break;

					case 3:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetAnsiBlack() );
						 break;

					case 4:
						 SetSelectedColor ( GetDocument()->GetMeasure()->GetAnsiWhite() );
						 break;
				 }
				 break;
		}

		if (IsDlgButtonChecked(IDC_EDITGRID_CHECK)!=BST_CHECKED)
			m_pGrayScaleGrid->SetSelectedRange(1,minCol,3,minCol,FALSE);	// Select entire column
	}
	GetDocument()->UpdateAllViews(this, UPD_SELECTEDCOLOR);
	(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
}

void CMainView::OnXyzRadio() 
{
	m_editCheckButton.EnableWindow ( ! m_AdjustXYZCheckButton.GetCheck () );
	m_displayType=HCFR_XYZ_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnSensorrgbRadio() 
{
	m_editCheckButton.EnableWindow ( ! m_AdjustXYZCheckButton.GetCheck () );
	m_displayType=HCFR_SENSORRGB_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnRgbRadio() 
{
	m_editCheckButton.EnableWindow ( ! m_AdjustXYZCheckButton.GetCheck () );
	m_displayType=HCFR_RGB_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnXyz2Radio() 
{
	CheckDlgButton ( IDC_EDITGRID_CHECK, FALSE );
	m_editCheckButton.EnableWindow ( FALSE );
	m_displayType=HCFR_xyz2_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnxyYRadio() 
{
	m_editCheckButton.EnableWindow ( ! m_AdjustXYZCheckButton.GetCheck () );
	m_displayType=HCFR_xyY_VIEW;
	InitGrid();	// to update row labels
	UpdateGrid();
}

void CMainView::OnEditgridCheck() 
{
	BOOL isEnabled=m_editCheckButton.GetCheck();
	m_pGrayScaleGrid->SetEditable(isEnabled);
	m_pGrayScaleGrid->EnableDragAndDrop(isEnabled);
}

void CMainView::OnSelchangeComboMode() 
{
	// TODO: Add your control notification handler code here
	int	nNewMode = m_comboMode.GetCurSel ();
	CString	Msg, MsgAdd;

	StopBackgroundMeasures ();

	m_displayMode = nNewMode;

	if ( m_displayMode == 12 )
		m_testAnsiPatternButton.ShowWindow ( SW_SHOW );
	else
		m_testAnsiPatternButton.ShowWindow ( SW_HIDE );

	MsgAdd.LoadString ( IDS_CTRLCLICK_SIM );

	switch ( m_displayMode )
	{
		case 0:
			 Msg.LoadString ( IDS_GRAYSCALE );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASUREGRAYSCALE );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_MEAS_GRAYS,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETEGRAYSCALE );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 1:
			 Msg.LoadString ( IDS_SECONDARYCOLORS );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESECONDARIES );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_MEAS_PRIM_SEC,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETESECONDARIES );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 2:
			 Msg.LoadString ( IDS_FREEMEASURES );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( GetConfig()->m_bContinuousMeasures?IDS_RUNCONTINUOUS:IDS_ONEMEASURE );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps((GetConfig()->m_bContinuousMeasures?IDB_MEAS_CONT:IDB_CAMERA_BITMAP),RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETEALLMEASURES );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 3:
			 Msg.LoadString ( IDS_NEARBLACK );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURENEARBLACK );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_NEARBLACK,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETENEARBLACK );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 4:
			 Msg.LoadString ( IDS_NEARWHITE );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURENEARWHITE );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_NEARWHITE,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETENEARWHITE );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 5:
			 Msg.LoadString ( IDS_SATRED );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATRED );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_SATRED,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETESATRED );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 6:
			 Msg.LoadString ( IDS_SATGREEN );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATGREEN );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_SATGREEN,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETESATGREEN );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 7:
			 Msg.LoadString ( IDS_SATBLUE );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATBLUE );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_SATBLUE,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETESATBLUE );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 8:
			 Msg.LoadString ( IDS_SATYELLOW );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATYELLOW );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_SATYELLOW,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETESATYELLOW );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 9:
			 Msg.LoadString ( IDS_SATCYAN );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATCYAN );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_SATCYAN,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETESATCYAN );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 10:
			 Msg.LoadString ( IDS_SATMAGENTA );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATMAGENTA );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_SATMAGENTA,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETESATMAGENTA );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 11:
			 Msg.LoadString ( IDS_SATCC24 );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURESATCC24 );
			 Msg += "\r\n";
			 Msg += MsgAdd;
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_SATCC24,RGB(0,0,0));
			 Msg.LoadString ( IDS_DELETESATCC24 );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;

		case 12:
			 Msg.LoadString ( IDS_CONTRAST );
			 m_grayScaleGroup.SetText ( Msg );
			 Msg.LoadString ( IDS_MEASURECONTRAST );
			 m_grayScaleButton.SetTooltipText(Msg);
		 	 m_grayScaleButton.SetBitmaps(IDB_CONTRAST,RGB(255,0,0));
			 Msg.LoadString ( IDS_DELETECONTRAST );
			 m_grayScaleDeleteButton.SetTooltipText(Msg);
			 break;
	}

	InitGrid();
	UpdateGrid();
	
	if ( m_pGrayScaleGrid->GetSelectedCellRange().IsValid () )
	{
		m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
		m_pGrayScaleGrid->SetFocusCell(-1,-1);
		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}
}

void CMainView::OnMeasureGrayScale() 
{
	if ( GetKeyState ( VK_CONTROL ) < 0 )
	{
		switch ( m_displayMode )
		{
			case 0:
				 GetDocument()->OnSimGrayscale();
				 break;

			case 1:
				 GetDocument()->OnSimSecondaries();
				 break;

			case 2:
				 GetDocument()->OnSimSingleMeasurement();
				 break;

			case 3:
				 GetDocument()->OnSimNearblack();
				 break;

			case 4:
				 GetDocument()->OnSimNearwhite();
				 break;

			case 5:
				 GetDocument()->OnSimSatRed();
				 break;

			case 6:
				 GetDocument()->OnSimSatGreen();
				 break;

			case 7:
				 GetDocument()->OnSimSatBlue();
				 break;

			case 8:
				 GetDocument()->OnSimSatYellow();
				 break;

			case 9:
				 GetDocument()->OnSimSatCyan();
				 break;

			case 10:
				 GetDocument()->OnSimSatMagenta();
				 break;
		}
	}
	else
	{
		switch ( m_displayMode )
		{
			case 0:
				 GetDocument()->OnMeasureGrayscale();
				 break;

			case 1:
				 GetDocument()->OnMeasureSecondaries();
				 break;

			case 2:
				 if ( GetConfig()->m_bContinuousMeasures )
					GetDocument()->OnContinuousMeasurement();
				 else
					GetDocument()->OnSingleMeasurement();
				 break;

			case 3:
				 GetDocument()->OnMeasureNearblack();
				 break;

			case 4:
				 GetDocument()->OnMeasureNearwhite();
				 break;

			case 5:
				 GetDocument()->OnMeasureSatRed();
				 break;

			case 6:
				 GetDocument()->OnMeasureSatGreen();
				 break;

			case 7:
				 GetDocument()->OnMeasureSatBlue();
				 break;

			case 8:
				 GetDocument()->OnMeasureSatYellow();
				 break;

			case 9:
				 GetDocument()->OnMeasureSatCyan();
				 break;

			case 10:
				 GetDocument()->OnMeasureSatMagenta();
				 break;

			case 11:
				 GetDocument()->OnMeasureSatCC24();
				 break;

			case 12:
				 GetDocument()->OnMeasureContrast();
				 break;
		}
	}
}

void CMainView::OnDeleteGrayscale() 
{
	BOOL	bSelectionOnly = FALSE;
	CString	Msg, Title;
	LPARAM	lHint = UPD_EVERYTHING;

	Msg.LoadString ( IDS_CONFIRMDELETE );
	Title.LoadString ( IDS_CALIBRATION );
	
	if ( m_displayMode == 2 )
	{
		// Special case: free measurements can be deleted by selection or totally
		if(m_pGrayScaleGrid->GetSelectedCount() == 0)
		{
			// No selection: take all
			Msg.LoadString ( IDS_CONFIRMDELETEALL );
		}
		else
		{
			// Have a selection
			bSelectionOnly = TRUE;
			Msg.LoadString ( IDS_CONFIRMDELETESELECTION );
		}
	}

	if(GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONQUESTION | MB_YESNO) == IDYES)
	{
		int	j;
		CCPatterns cPat = GetConfig()->m_CCMode;
		BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);
		switch ( m_displayMode )
		{
			case 0:
				 for(j=0;j<GetDocument()->GetMeasure()->GetGrayScaleSize();j++)
					GetDocument()->GetMeasure()->SetGray(j,noDataColor);
				 lHint = UPD_GRAYSCALE;
				 break;

			case 1:
				 GetDocument()->GetMeasure()->SetRedPrimary(noDataColor);
				 GetDocument()->GetMeasure()->SetGreenPrimary(noDataColor);
				 GetDocument()->GetMeasure()->SetBluePrimary(noDataColor);
				 GetDocument()->GetMeasure()->SetYellowSecondary(noDataColor);
				 GetDocument()->GetMeasure()->SetCyanSecondary(noDataColor);
				 GetDocument()->GetMeasure()->SetMagentaSecondary(noDataColor);
				 GetDocument()->GetMeasure()->SetPrimeWhite(noDataColor);
				 GetDocument()->GetMeasure()->SetOnOffBlack(GetDocument()->GetMeasure()->GetOnOffBlack());
				 lHint = UPD_PRIMARIESANDSECONDARIES;
				 break;

			case 2:
				 if ( bSelectionOnly )
				 {
					CCellRange	selRange=m_pGrayScaleGrid->GetSelectedCellRange();
					GetDocument()->GetMeasure()->DeleteMeasurements(selRange.GetMinCol()-1,selRange.GetMaxCol()-selRange.GetMinCol()+1);
				 }
				 else
				 {
					GetDocument()->GetMeasure()->SetMeasurementsSize(0);
				 }
				 lHint = UPD_FREEMEASURES;
				 break;

			case 3:
				 for(j=0;j<GetDocument()->GetMeasure()->GetNearBlackScaleSize();j++)
					GetDocument()->GetMeasure()->SetNearBlack(j,noDataColor);
				 lHint = UPD_NEARBLACK;
				 break;

			case 4:
				 for(j=0;j<GetDocument()->GetMeasure()->GetNearWhiteScaleSize();j++)
					GetDocument()->GetMeasure()->SetNearWhite(j,noDataColor);
				 lHint = UPD_NEARWHITE;
				 break;

			case 5:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetRedSat(j,noDataColor);
				 lHint = UPD_REDSAT;
				 break;

			case 6:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetGreenSat(j,noDataColor);
				 lHint = UPD_GREENSAT;
				 break;

			case 7:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetBlueSat(j,noDataColor);
				 lHint = UPD_BLUESAT;
				 break;

			case 8:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetYellowSat(j,noDataColor);
				 lHint = UPD_YELLOWSAT;
				 break;

			case 9:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetCyanSat(j,noDataColor);
				 lHint = UPD_CYANSAT;
				 break;

			case 10:
				 for(j=0;j<GetDocument()->GetMeasure()->GetSaturationSize();j++)
					GetDocument()->GetMeasure()->SetMagentaSat(j,noDataColor);
				 lHint = UPD_MAGENTASAT;
				 break;

			case 11:
                if ( isExtPat )
                {
    				for(j=0;j<GetConfig()->GetCColorsSize() ;j++)
	    				GetDocument()->GetMeasure()->SetCC24Sat(j,noDataColor);
	 				GetConfig()->GetCColors();
					UpdateGrid();
               }
                else
                {
                    for(j=0;j< (cPat == CCSG?96:(cPat == AXIS?71:(cPat == CMS||cPat == CPS?19:24))) ;j++)
	    				GetDocument()->GetMeasure()->SetCC24Sat(j,noDataColor);
                }
				 lHint = UPD_CC24SAT;
				 break;

			case 12:
				 GetDocument()->GetMeasure()->DeleteContrast();
				 lHint = UPD_CONTRAST;
				 break;
		}
		InitGrid();
		UpdateGrid();
		GetDocument()->UpdateAllViews(NULL, lHint);
		GetDocument()->SetModifiedFlag(TRUE);
		if ( m_pGrayScaleGrid->GetSelectedCellRange().IsValid () )
		{
			m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
			m_pGrayScaleGrid->SetFocusCell(-1,-1);
			SetSelectedColor ( noDataColor );
			(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
		}
	}
}


void CMainView::UpdateMeasurementsAfterBkgndMeasure ()
{
	CColor	MeasuredColor=noDataColor;

	int	n = GetDocument()->GetMeasure()->GetMeasurementsSize();

	if ( n > 0 )
		MeasuredColor=GetDocument()->GetMeasure()->GetMeasurement(n-1);

	if ( m_displayMode == 2 && n > 0 ) //this updates freemeasures grid
	{
		// Update grid only when in measurement mode
		CColor			refColor = GetColorReference().GetWhite();
		BOOL			bAddedCol = FALSE;
		BOOL			bSpecialRef = FALSE;
		COLORREF		clrSpecial1=RGB(128,128,128), clrSpecial2=RGB(128,128,128);
		CDataSetDoc *	pDataRef = GetDataRef();

		GV_ITEM Item;

		if ( pDataRef == GetDocument () )
			pDataRef = NULL;

		if ( pDataRef && pDataRef->GetMeasure()->GetMeasurementsSize() != n )
			pDataRef = NULL;

		if ( m_pGrayScaleGrid->GetColumnCount()-1==n)
		{
			// Remove first column and reset column headers
			m_pGrayScaleGrid -> DeleteColumn ( 1 );
			Item.mask = GVIF_TEXT;
			Item.row = 0;
			for ( Item.col = 1; Item.col < m_pGrayScaleGrid->GetColumnCount() ; Item.col ++ )
			{
				Item.strText.Format("%d",Item.col);
				m_pGrayScaleGrid->SetItem(&Item);
			}
		}

		Item.mask = GVIF_TEXT|GVIF_FORMAT;
		Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;

		CString	str;
		str.Format("%d",n);
		m_pGrayScaleGrid -> InsertColumn(str);
		m_pGrayScaleGrid -> SetItemFont ( 0, n, m_pGrayScaleGrid->GetItemFont(0,n-1) ); // Set the font to bold

		m_pGrayScaleGrid->SetItemState ( 4, n, m_pGrayScaleGrid->GetItemState(4,n) | GVIS_READONLY );
		m_pGrayScaleGrid->SetItemBkColour ( 4, n, RGB(224,224,224) );

		m_pGrayScaleGrid->SetItemState ( 5, n, m_pGrayScaleGrid->GetItemState(5,n) | GVIS_READONLY );
		m_pGrayScaleGrid->SetItemBkColour ( 5, n, RGB(240,240,240) );

		if ( pDataRef )
		{
			m_pGrayScaleGrid->SetItemState ( 6, n, m_pGrayScaleGrid->GetItemState(6,n) | GVIS_READONLY );
			m_pGrayScaleGrid->SetItemBkColour ( 6, n, RGB(224,224,224) );

			m_pGrayScaleGrid->SetItemState ( 7, n, m_pGrayScaleGrid->GetItemState(7,n) | GVIS_READONLY );
			m_pGrayScaleGrid->SetItemBkColour ( 7, n, RGB(240,240,240) );
		}

		bSpecialRef = TRUE;
		if ( MeasuredColor.GetDeltaxy ( GetColorReference().GetWhite(), GetColorReference() ) < 0.05 )
 		{
			bSpecialRef = FALSE;
			refColor = GetColorReference().GetWhite();
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(0), GetColorReference() ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefPrimary(0);
			clrSpecial1 = RGB(255,192,192);
			clrSpecial2 = RGB(255,224,224);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(1), GetColorReference() ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefPrimary(1);
			clrSpecial1 = RGB(192,255,192);
			clrSpecial2 = RGB(224,255,224);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefPrimary(2), GetColorReference() ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefPrimary(0);
			clrSpecial1 = RGB(192,192,255);
			clrSpecial2 = RGB(224,224,255);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(0), GetColorReference() ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefSecondary(0);
			clrSpecial1 = RGB(255,255,192);
			clrSpecial2 = RGB(255,255,224);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(1), GetColorReference() ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefSecondary(1);
			clrSpecial1 = RGB(192,255,255);
			clrSpecial2 = RGB(224,255,255);
		}
		else if ( MeasuredColor.GetDeltaxy ( GetDocument()->GetMeasure()->GetRefSecondary(2), GetColorReference() ) < 0.05 )
		{
			refColor = GetDocument()->GetMeasure()->GetRefSecondary(2);
			clrSpecial1 = RGB(255,192,255);
			clrSpecial2 = RGB(255,224,255);
		}

		CColor refDocColor;

		if ( pDataRef )
			refDocColor = pDataRef->GetMeasure()->GetMeasurement(n-1);
		else
			refDocColor = noDataColor;

		double YWhite = GetDocument() -> GetMeasure () -> GetPrimeWhite () [ 1 ];

		for( int i = 0 ; i < ( pDataRef ? 7 : 5 ) ; i ++ )
		{
			Item.row = i+1;
			Item.col = n;
			Item.strText = GetItemText ( MeasuredColor, YWhite, refColor, refDocColor, -1.0, i, n, 0.0 );
			
			m_pGrayScaleGrid->SetItem(&Item);

			if ( bSpecialRef && i >= 3 )
			{
				m_pGrayScaleGrid->SetItemBkColour ( i+1, n, ( i&1 ? clrSpecial1 : clrSpecial2 ) );
			}
		}

		ASSERT ( n == m_pGrayScaleGrid->GetColumnCount()-1 );

		int width = m_pGrayScaleGrid -> GetColumnWidth ( 0 );
		m_pGrayScaleGrid -> SetColumnWidth ( n, width * 11 / 10 );

		m_pGrayScaleGrid->SetSelectedRange(1,n,3,n,FALSE);	// select inserted cell
		m_pGrayScaleGrid->EnsureVisible(0,n);
		m_pGrayScaleGrid->RedrawColumn(n);
	}
	else
	{
		m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
		m_pGrayScaleGrid->SetFocusCell(-1,-1);
	}

	SetSelectedColor ( MeasuredColor );
}

void CMainView::InitButtons()
{
	CString	Msg, Msg2;

	Msg.LoadString ( IDS_CONFIGURESENSOR );
	if (GetConfig()->isHighDPI)
		m_configSensorButton.SetIcon(IDI_SETTINGS_ICON,32,32);
	else
		m_configSensorButton.SetIcon(IDI_SETTINGS_ICON,18,18);
	Msg2.LoadString ( IDS_CONFIGURESENSOR2 );
	if (GetConfig()->isHighDPI)
		m_configSensorButton2.SetIcon(IDI_START_ICON,28,28);
	else
		m_configSensorButton2.SetIcon(IDI_START_ICON,16,16);
	m_configSensorButton.SetFont(GetFont());
	m_configSensorButton2.SetFont(GetFont());
	m_configSensorButton.EnableBalloonTooltip();
	m_configSensorButton.SetTooltipText(Msg);
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_configSensorButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_configSensorButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_configSensorButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
	m_configSensorButton.SizeToContent();
	m_configSensorButton2.EnableBalloonTooltip();
	m_configSensorButton2.SetTooltipText(Msg2);
	m_configSensorButton2.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton2.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton2.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_configSensorButton2.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_configSensorButton2.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_configSensorButton2.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_configSensorButton2.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_configSensorButton2.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
	m_configSensorButton2.SizeToContent();
//	m_configSensorButton.DrawTransparent(TRUE);

	Msg.LoadString ( IDS_CONFIGUREGENERATOR );
	if (GetConfig()->isHighDPI)
		m_configGeneratorButton.SetIcon(IDI_SETTINGS_ICON,32,32);
	else
		m_configGeneratorButton.SetIcon(IDI_SETTINGS_ICON,18,18);
	m_configGeneratorButton.SetFont(GetFont());
	m_configGeneratorButton.EnableBalloonTooltip();
	m_configGeneratorButton.SetTooltipText(Msg);
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_configGeneratorButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_configGeneratorButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_configGeneratorButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
	m_configGeneratorButton.SizeToContent();
//	m_configGeneratorButton.DrawTransparent(TRUE);

	Msg.LoadString ( IDS_MEASUREGRAYSCALE );
	Msg2.LoadString ( IDS_CTRLCLICK_SIM );
	Msg += "\r\n";
	Msg += Msg2;
	m_grayScaleButton.SetBitmaps(IDB_MEAS_GRAYS,RGB(0,0,0));
	m_grayScaleButton.DrawFlatFocus(FALSE);
	m_grayScaleButton.EnableBalloonTooltip();
	m_grayScaleButton.SetTooltipText(Msg);
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_grayScaleButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_grayScaleButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_grayScaleButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
//	m_grayScaleButton.DrawTransparent(TRUE);

	Msg.LoadString ( IDS_DELETEGRAYSCALE );
//	m_grayScaleDeleteButton.SetBitmaps(IDB_DELETE_BITMAP,RGB(255,255,255));
	m_grayScaleDeleteButton.SetIcon(IDI_TRASH_ICON,32,32);
	m_grayScaleDeleteButton.SetFont(GetFont());
	m_grayScaleDeleteButton.EnableBalloonTooltip();
	m_grayScaleDeleteButton.SetTooltipText(Msg);
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_grayScaleDeleteButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_grayScaleDeleteButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_grayScaleDeleteButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);
//	m_grayScaleDeleteButton.DrawTransparent(TRUE);

	Msg.LoadString ( IDS_DISPLAYANSICONTRAST );
	m_testAnsiPatternButton.SetBitmaps(IDB_ANSICONTRAST_BITMAP,RGB(1,1,1));
	m_testAnsiPatternButton.SetFont(GetFont());
	m_testAnsiPatternButton.EnableBalloonTooltip();
	m_testAnsiPatternButton.SetTooltipText(Msg);
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_FG_IN,FxGetSysColor(COLOR_MENUTEXT));
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_FG_OUT,FxGetSysColor(COLOR_MENUTEXT));
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_FG_FOCUS,FxGetSysColor(COLOR_MENUTEXT));
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_IN,FxGetMenuBgColor());
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_OUT,FxGetMenuBgColor());
	m_testAnsiPatternButton.SetColor(CButtonST::BTNST_COLOR_BK_FOCUS,FxGetMenuBgColor());
	m_testAnsiPatternButton.OffsetColor(CButtonST::BTNST_COLOR_BK_IN, 30);
	m_testAnsiPatternButton.OffsetColor(CButtonST::BTNST_COLOR_FG_IN, 30);

	if ( m_displayMode == 12 )
		m_testAnsiPatternButton.ShowWindow ( SW_SHOW );
	else
		m_testAnsiPatternButton.ShowWindow ( SW_HIDE );
}
void CMainView::InitGroups()
{

	CString groupFontName="Verdana";
	int groupFontSize=8;

	m_grayScaleGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_grayScaleGroup.SetFontName(groupFontName);
	m_grayScaleGroup.SetFontSize(groupFontSize - 1);
	m_grayScaleGroup.SetFontBold(TRUE);
	m_grayScaleGroup.SetAlignment(SS_LEFT);
//	m_grayScaleGroup.SetFontItalic(TRUE);
	m_grayScaleGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));
	
	m_sensorGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_sensorGroup.SetFontName(groupFontName);
	m_sensorGroup.SetFontSize(groupFontSize);
	m_sensorGroup.SetFontBold(TRUE);
//	m_sensorGroup.SetFontItalic(TRUE);
	m_sensorGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));
	
	m_generatorGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_generatorGroup.SetFontName(groupFontName);
	m_generatorGroup.SetFontSize(groupFontSize);
	m_generatorGroup.SetFontBold(TRUE);
//	m_generatorGroup.SetFontItalic(TRUE);
	m_generatorGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_datarefGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_datarefGroup.SetFontName(groupFontName);
	m_datarefGroup.SetFontSize(groupFontSize);
	m_datarefGroup.SetFontBold(TRUE);
//	m_datarefGroup.SetFontItalic(TRUE);
	m_datarefGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_displayGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_displayGroup.SetFontName(groupFontName);
	m_displayGroup.SetFontSize(groupFontSize);
	m_displayGroup.SetFontBold(TRUE);
//	m_displayGroup.SetFontItalic(TRUE);
	m_displayGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_paramGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_paramGroup.SetFontName(groupFontName);
	m_paramGroup.SetFontSize(groupFontSize);
	m_paramGroup.SetFontBold(TRUE);
//	m_paramGroup.SetFontItalic(TRUE);
	m_paramGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_selectGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_selectGroup.SetFontName(groupFontName);
	m_selectGroup.SetFontSize(groupFontSize);
	m_selectGroup.SetFontBold(TRUE);
//	m_selectGroup.SetFontItalic(TRUE);
	m_selectGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));

	m_viewGroup.SetXPGroupStyle(CXPGroupBox::XPGB_WINDOW);
	m_viewGroup.SetFontName(groupFontName);
	m_viewGroup.SetFontSize(groupFontSize);
	m_viewGroup.SetFontBold(TRUE);
//	m_viewGroup.SetFontItalic(TRUE);
	m_viewGroup.SetCaptionTextColor(LightenColor(70,FxGetSysColor(COLOR_MENUTEXT)));
}	

BOOL CMainView::OnEraseBkgnd(CDC* pDC) 
{
	CRect windowRect;
	GetClientRect(windowRect );

	COLORREF colorTop,colorBottom;
	FxGetMenuBgColors(colorTop,colorBottom);
	DrawGradient(pDC,windowRect,colorTop,colorBottom,false);

	return TRUE;
}

void CMainView::OnSysColorChange() 
{
	CFormView::OnSysColorChange();
	delete m_pBgBrush;
	m_pBgBrush= new CBrush(FxGetMenuBgColor());
	InitButtons();
	OnSelchangeComboMode();
	InitGroups();
	Invalidate(TRUE);	
}

HBRUSH CMainView::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr=CFormView::OnCtlColor(pDC, pWnd, nCtlColor);
	switch(nCtlColor)
	{
		case CTLCOLOR_STATIC:
			pDC->SetBkMode(TRANSPARENT);
			pDC->SetTextColor(FxGetSysColor(COLOR_MENUTEXT));
			return *m_pBgBrush;
			break;
		case CTLCOLOR_BTN:
		case CTLCOLOR_EDIT:
			break;
		default:
			pDC->SetBkMode(TRANSPARENT);
			pDC->SetTextColor(FxGetSysColor(COLOR_MENUTEXT));
			return *m_pBgBrush;
	}
	return hbr;
}

void CMainView::OnSize(UINT nType, int cx, int cy) 
{
	RECT			Rect;
	RECT			ClientRect;
	HDWP			hwdp;
	SCtrlInitPos *	pCtrlPos;
	POSITION		pos;

	//CFormView::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here
	if ( m_bPositionsInit )
	{
		GetClientRect ( & ClientRect );

		hwdp = BeginDeferWindowPos ( m_CtrlInitPos.GetCount () );
		
		pos = m_CtrlInitPos.GetHeadPosition ();
		while ( pos )
		{
			pCtrlPos = (SCtrlInitPos *) m_CtrlInitPos.GetNext ( pos );
			Rect = pCtrlPos -> m_Rect;

			switch ( pCtrlPos -> m_pLayout -> m_LeftMode )
			{
				case LAYOUT_LEFT:
					 // No move
					 break;

				case LAYOUT_RIGHT:
					 Rect.left += ClientRect.right - m_OriginalRect.right;
					 break;
			}

			switch ( pCtrlPos -> m_pLayout -> m_RightMode )
			{
				case LAYOUT_LEFT:
					 // No move
					 break;

				case LAYOUT_RIGHT:
					 Rect.right += ClientRect.right - m_OriginalRect.right;
					 break;
			}

			switch ( pCtrlPos -> m_pLayout -> m_TopMode )
			{
				case LAYOUT_TOP:
					 // No move
					 break;

				case LAYOUT_BOTTOM:
					 Rect.top += ClientRect.bottom - m_OriginalRect.bottom;
					 break;

				case LAYOUT_TOP_OFFSET:
					 Rect.top += m_nSizeOffset;
					 break;
			}

			switch ( pCtrlPos -> m_pLayout -> m_BottomMode )
			{
				case LAYOUT_TOP:
					 // No move
					 break;

				case LAYOUT_BOTTOM:
					 Rect.bottom += ClientRect.bottom - m_OriginalRect.bottom;
					 break;

				case LAYOUT_TOP_OFFSET:
					 Rect.bottom += m_nSizeOffset;
					 break;
			}
			
			hwdp = DeferWindowPos ( hwdp, pCtrlPos -> m_hWnd, NULL, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW );
		}

		EndDeferWindowPos ( hwdp );

		if ( m_displayMode != 2 && m_pGrayScaleGrid -> GetColumnCount () < 13 )
		{
			m_pGrayScaleGrid -> ExpandColumnsToFit(FALSE);
		}
		else
		{
		}

		m_pSelectedColorGrid->SetRedraw ( FALSE, FALSE );
		m_pSelectedColorGrid->ExpandColumnsToFit(TRUE);
		m_pSelectedColorGrid->AutoSizeColumn(0);
		m_pSelectedColorGrid->ExpandColumnsToFit(TRUE);
		m_pSelectedColorGrid->SetRedraw ( TRUE, FALSE );
	    m_pSelectedColorGrid->AutoSizeRows();

		pos = m_CtrlInitPos.GetHeadPosition ();
		while ( pos )
		{
			pCtrlPos = (SCtrlInitPos *) m_CtrlInitPos.GetNext ( pos );
			::InvalidateRect ( pCtrlPos -> m_hWnd, NULL, FALSE );
		}

		if ( m_pInfoWnd )
		{
			CWnd *	pWnd;

			pWnd = GetDlgItem ( IDC_STATIC_VIEW );
			pWnd -> GetWindowRect ( & Rect );
			ScreenToClient ( & Rect );

			m_pInfoWnd -> SetWindowPos ( pWnd, Rect.left, Rect.top, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOACTIVATE );
		}

		if ( ClientRect.bottom - ClientRect.top < m_InitialWindowSize.y + m_nSizeOffset )
		{
			m_Target.ShowWindow ( SW_HIDE );
		}
		else
		{
			if ( ! m_Target.IsWindowVisible () )
				m_Target.ShowWindow ( SW_SHOW );
		}
		
		InitGrid();
		UpdateGrid();
	}
}

void CMainView::OnSelchangeInfoDisplay() 
{
	CWnd *					pWnd;
	CRect					Rect;
	CEdit *					pEdit;
	CTargetWnd *			pTargetWnd;
	CSpectrumWnd *			pSpectrumWnd;
	CCreateContext			context;
	CCIEChartView *			pCIEChartView;
	CLuminanceHistoView *	pLuminanceHistoView;
	CGammaHistoView	*		pGammaHistoView;
	CRGBHistoView *			pRGBHistoView;
	CColorTempHistoView *	pColorTempHistoView;
	CMeasuresHistoView *	pMeasuresHistoView;
	CSubFrame *				pFrame;

	if ( m_pInfoWnd )
	{
		m_pInfoWnd -> DestroyWindow ();
		if ( m_infoDisplay < 3 )
			delete m_pInfoWnd;
		m_pInfoWnd = NULL;
	}

	pWnd = GetDlgItem ( IDC_STATIC_VIEW );
	pWnd -> GetWindowRect ( & Rect );
	ScreenToClient ( & Rect );
    int size=GetDocument()->GetMeasure()->GetGrayScaleSize();

	m_infoDisplay = m_comboDisplay.GetCurSel ();
	switch ( m_infoDisplay )
	{
		case 0:
			 pEdit = new CEdit;
			
			 pEdit -> Create (WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN, Rect, this, IDC_INFO_VIEW );
			 pEdit -> SetFont ( GetFont () );
			 pEdit -> SetWindowText ( GetDocument()->GetMeasure()->GetInfoString() );

			 m_pInfoWnd = pEdit;
			 break;

		case 1: // target
             pTargetWnd = new CTargetWnd;			
			 pTargetWnd -> Create (NULL, NULL, WS_VISIBLE | WS_CHILD, Rect, this, IDC_INFO_VIEW, NULL );
             if ( (m_displayMode <= 11 &&  m_displayMode != 2) ) 
             {
		        if (m_displayMode == 3)
				    size = 101;
				else if (m_displayMode == 4)
				size = -1 * GetDocument()->GetMeasure()->GetNearWhiteScaleSize();
				if (m_displayMode > 4 && m_displayMode < 12)
					size=GetDocument()->GetMeasure()->GetSaturationSize();
			    pTargetWnd -> m_pRefColor = & m_SelectedColor;
			    pTargetWnd -> Refresh (GetDocument()->GetGenerator()->m_b16_235,  (m_pGrayScaleGrid -> GetSelectedCellRange().IsValid()?m_pGrayScaleGrid -> GetSelectedCellRange().GetMinCol():-1), size, m_displayMode, GetDocument(), FALSE );
             }
			 m_pInfoWnd = pTargetWnd;
			 break;

		case 2: // spectrum
			 pSpectrumWnd = new CSpectrumWnd;
			
			 pSpectrumWnd -> Create (NULL, NULL, WS_VISIBLE | WS_CHILD, Rect, this, IDC_INFO_VIEW, NULL );
			 pSpectrumWnd -> m_pRefColor = & m_SelectedColor;
			 pSpectrumWnd -> Refresh ();

			 m_pInfoWnd = pSpectrumWnd;
			 break;

		case 3: // luminance
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CLuminanceHistoView );

			 pLuminanceHistoView = (CLuminanceHistoView *) context.m_pNewViewClass->CreateObject();
			 pLuminanceHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pLuminanceHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pLuminanceHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 4: // gamma
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CGammaHistoView );

			 pGammaHistoView = (CGammaHistoView *) context.m_pNewViewClass->CreateObject();
			 pGammaHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pGammaHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pGammaHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 5: // RGB
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CRGBHistoView );

			 pRGBHistoView = (CRGBHistoView *) context.m_pNewViewClass->CreateObject();
			 pRGBHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pRGBHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pRGBHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 6: // Color temperature
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CColorTempHistoView );

			 pColorTempHistoView = (CColorTempHistoView *) context.m_pNewViewClass->CreateObject();
			 pColorTempHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pColorTempHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pColorTempHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 7: // CIE
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CCIEChartView );

			 pCIEChartView = (CCIEChartView *) context.m_pNewViewClass->CreateObject();
			 pCIEChartView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pCIEChartView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pCIEChartView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;

		case 8: // Combined histogram for free measures
			 pFrame = new CSubFrame;

			 pFrame -> Create ( NULL, NULL, WS_CHILD | WS_VISIBLE, Rect, this );

			 context.m_pCurrentDoc = GetDocument ();
			 context.m_pCurrentFrame = pFrame;
			 context.m_pLastView = this;
			 context.m_pNewDocTemplate = GetDocument () -> GetDocTemplate ();
			 context.m_pNewViewClass = RUNTIME_CLASS ( CMeasuresHistoView );

			 pMeasuresHistoView = (CMeasuresHistoView *) context.m_pNewViewClass->CreateObject();
			 pMeasuresHistoView -> Create ( NULL, NULL, AFX_WS_DEFAULT_VIEW, CRect(0,0,0,0), pFrame, IDC_INFO_VIEW, & context );
			 pMeasuresHistoView -> OnInitialUpdate ();
			 pFrame -> SetActiveView ( pMeasuresHistoView, FALSE );
			 pFrame -> OnSize ( 0, 0, 0 );

			 m_pInfoWnd = pFrame;
			 break;
	}

	if ( m_pInfoWnd ) 
		m_pInfoWnd -> Invalidate ();

}

void CMainView::OnChangeInfosEdit() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CFormView::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	if ( m_comboDisplay.GetCurSel () == 0 && m_pInfoWnd )
	{
		CString str;
		m_pInfoWnd -> GetWindowText ( str );
		GetDocument()->GetMeasure()->SetInfoString(str);
		GetDocument()->SetModifiedFlag(TRUE);
	}
}

void CMainView::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_MEASURES, NULL );
}

void CMainView::OnDatarefCheck() 
{
	UpdateData(TRUE);
	UpdateDataRef((IsDlgButtonChecked(IDC_DATAREF_CHECK)==BST_CHECKED), GetDocument());

	AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	
	UpdateData(FALSE);
}

void CMainView::OnInitDefaults()
{
	int rv = GetColorApp()->InMeasureMessageBox("This will reset your preferences to their default values.\nAre you sure?", "Reset prefs", MB_YESNO);
	if (rv == 6)
	{
		CString strlang = GetConfig()->strLang;
		DeleteFile("ColorHCFR.ini");
		GetConfig()->InitDefaults();
		GetConfig()->LoadSettings();
		GetConfig()->ApplySettings(TRUE);
		GetConfig()->WriteProfileString ( "Options", "Language", strlang );
		GetConfig()->SaveSettings();
		GetConfig()->m_bSave = TRUE;
		GetDocument()->SetModifiedFlag(TRUE);
		GetDocument()->UpdateAllViews ( NULL, UPD_EVERYTHING );
		AfxGetMainWnd()->SendMessage(WM_SYSCOLORCHANGE);
	}
}

void CMainView::OnAdjustXYZCheck()
{
	BOOL	bAdjust = m_AdjustXYZCheckButton.GetCheck ();
	Matrix CurrentMatrix = GetDocument ()->m_pSensor->GetSensorMatrix();
	

	if (!bAdjust) //restore uncorrected sensor values
	{
		ASSERT(0);
		GetDocument ()->m_pSensor->SetSensorMatrixMod( CurrentMatrix );
		GetDocument ()->m_pSensor->SetSensorMatrix( Matrix::IdentityMatrix(3) );
		GetDocument ()->m_measure.ApplySensorAdjustmentMatrix( CurrentMatrix.GetInverse() );
		m_AdjustXYZCheckButton.SetCheck(FALSE);
	}
	else  //reapply saved correction matrix
	{
		ASSERT(0);
		GetDocument ()->m_pSensor->SetSensorMatrix( GetDocument ()->m_pSensor->GetSensorMatrixMod() );
		GetDocument ()->m_measure.ApplySensorAdjustmentMatrix(GetDocument ()->m_pSensor->GetSensorMatrixMod() );
		GetDocument ()->m_pSensor->SetSensorMatrixMod( Matrix::IdentityMatrix(3) );
		m_AdjustXYZCheckButton.SetCheck(TRUE);
	}

	if ( m_pGrayScaleGrid->GetSelectedCellRange().IsValid () )
	{
		m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
		m_pGrayScaleGrid->SetFocusCell(-1,-1);
		SetSelectedColor ( noDataColor );
		(CMDIFrameWnd *)AfxGetMainWnd()->SendMessage(WM_COMMAND,IDM_REFRESH_CONTROLS,NULL);	// refresh mainframe controls
	}

	GetDocument()->UpdateAllViews ( NULL, UPD_EVERYTHING );
	AfxGetMainWnd () -> SendMessageToDescendants ( WM_COMMAND, IDM_REFRESH_REFERENCE );
}


void CMainView::OnEditCopy() 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
	{
		NM_GRIDVIEW GridItem;
		CCellRange Selection = m_pGrayScaleGrid -> GetSelectedCellRange();
		int minCol = Selection.GetMinCol();
		int maxCol = Selection.GetMaxCol();
		int minRow = Selection.GetMinRow();
		int maxRow = Selection.GetMaxRow();

		if ( minRow < 1 )
			minRow = 1;
		if ( maxRow > 3 )
			maxRow = 3;
		
		m_pGrayScaleGrid -> SetRedraw(FALSE);

		for ( int nRow = minRow ; nRow <= maxRow ; nRow ++ )
		{
			for ( int nCol = minCol ; nCol <= maxCol ; nCol ++ )
			{
				GridItem.iColumn = nCol;
				GridItem.iRow = nRow;
				OnGrayScaleGridBeginEdit ( (NMHDR *) & GridItem, NULL );
			}
		}


		m_pGrayScaleGrid -> OnEditCopy();
		UpdateGrid();
		m_pGrayScaleGrid -> SetRedraw(TRUE);
	}

	if ( m_comboDisplay.GetCurSel () == 0 && m_pInfoWnd -> m_hWnd == ::GetFocus () )
		m_pInfoWnd -> SendMessage ( WM_COPY );

	if ( m_pGrayScaleGrid -> m_hWnd == ::GetParent(::GetFocus ()) )
		GetFocus () -> SendMessage ( WM_COPY );
}

void CMainView::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
		m_pGrayScaleGrid -> OnUpdateEditCopy(pCmdUI);
}

void CMainView::OnEditCut() 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
	{
		OnEditCopy();
		return;
	}

	if ( m_comboDisplay.GetCurSel () == 0 && m_pInfoWnd -> m_hWnd == ::GetFocus () )
		m_pInfoWnd -> SendMessage ( WM_CUT );

	if ( m_pGrayScaleGrid -> m_hWnd == ::GetParent(::GetFocus ()) )
		GetFocus () -> SendMessage ( WM_CUT );
}

void CMainView::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
		m_pGrayScaleGrid -> OnUpdateEditCut(pCmdUI);
}

void CMainView::OnEditPaste() 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
		m_pGrayScaleGrid -> OnEditPaste();

	if ( m_comboDisplay.GetCurSel () == 0 && m_pInfoWnd -> m_hWnd == ::GetFocus () )
		m_pInfoWnd -> SendMessage ( WM_PASTE );

	if ( m_pGrayScaleGrid -> m_hWnd == ::GetParent(::GetFocus ()) )
		GetFocus () -> SendMessage ( WM_PASTE );
}

void CMainView::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	if ( m_pGrayScaleGrid -> m_hWnd == ::GetFocus () )
		m_pGrayScaleGrid -> OnUpdateEditPaste(pCmdUI);
}

void CMainView::OnEditUndo() 
{
}

void CMainView::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
}

void CMainView::UpdateAllGrids() 
{
	InitGrid();
	UpdateGrid();
	if(m_pGrayScaleGrid->GetColumnCount() > 12)	// Needed after data is set to get correctly sized cells when scrollbar is present
	{
		m_pGrayScaleGrid->ExpandRowsToFit(TRUE);
//		m_pGrayScaleGrid->AutoSizeColumns();
	}
	
	if ( m_pGrayScaleGrid->GetSelectedCellRange().IsValid () )
	{
		m_pGrayScaleGrid->SetSelectedRange(-1,-1,-1,-1);
		m_pGrayScaleGrid->SetFocusCell(-1,-1);
	}
}


// Tool function used during serialization (retrieve current view state: selected grid, unit, information, grid size)
DWORD CMainView::GetUserInfo ()
{
	DWORD	dwSubViewUserInfo = 0;
	if ( m_infoDisplay >= 3 && m_pInfoWnd != NULL )
	{
		// Info display is a sub view
		dwSubViewUserInfo = ( (CSavingView *) ( ( (CSubFrame *) m_pInfoWnd ) -> GetActiveView () ) ) -> GetUserInfo ();
	}
		
	// m_displayMode: selected grid - between 0 and 12						-> bits 0-5		0-64
	// m_displayType: selected unit - between 0 and 4						-> bits 6-9		0-16
	// m_infoDisplay: selected information screen - between 0 and 8			-> bits 10-15	0-64
	// m_nSizeOffset: window offset (grid added height) - between 0 and 105	-> bits 16-23	0-256
	// dwSubViewUserInfo: sub view parameters use the rest:					-> bits 24-31	0-256

	return ( m_displayMode & 0x003F ) | ( ( m_displayType & 0x000F ) << 6 ) | ( ( m_infoDisplay & 0x003F ) << 10 ) | ( ( m_nSizeOffset & 0x00FF ) << 16 ) | ( ( dwSubViewUserInfo & 0x00FF ) << 24 );
}

void CMainView::SetUserInfo ( DWORD dwUserInfo )
{
	m_dwInitialUserInfo = dwUserInfo;
}

CSubFrame::CSubFrame() : CFrameWnd ()
{
}

IMPLEMENT_DYNAMIC(CSubFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CSubFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CSubFrame)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLuminanceWnd message handlers

void CSubFrame::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// No paint: fully transparent window
}

void CSubFrame::OnSize(UINT nType, int cx, int cy) 
{
	CView *	pView;
	CRect	Rect;

	pView = GetActiveView ();

	if ( pView )
	{
		GetClientRect ( & Rect );
		pView -> SetWindowPos ( NULL, 0, 0, Rect.Width (), Rect.Height (), SWP_NOACTIVATE | SWP_NOZORDER );
	}	
	CFrameWnd::OnSize(nType, cx, cy);
}

BOOL CSubFrame::OnEraseBkgnd(CDC* pDC) 
{
	// No erase at all: fully transparent window
	return TRUE;
}

void CSubFrame::OnDestroy() 
{
	CView *	pView;

	pView = GetActiveView ();
	pView -> DestroyWindow ();

	CFrameWnd::OnDestroy();
}


void CMainView::OnDeltaposSpinView(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
	// TODO: Add your control notification handler code here
	
	if ( pNMUpDown -> iDelta > 0 )
	{
		if ( m_nSizeOffset < 100 )
		{
			m_nSizeOffset += 21;
			( (CMultiFrame *) GetParentFrame () ) -> EnsureMinimumSize ();
			InvalidateRect ( NULL );
			OnSize (0,0,0);
		}
	}
	else
	{
		if ( m_nSizeOffset > 0 )
		{
			m_nSizeOffset -= 21;
			InvalidateRect ( NULL );
			OnSize (0,0,0);
		}
	}

	*pResult = 0;
}

void CMainView::OnAnsiContrastPatternTestButton() 
{
	CString	Msg;

	CGenerator *pGenerator=GetDocument()->GetGenerator();

	pGenerator->Init();
	pGenerator->DisplayAnsiBWRects(FALSE);

	Msg.LoadString ( IDS_CLICKOK );
	GetColorApp()->InMeasureMessageBox(Msg, "ANSI Pattern");
	pGenerator->Release();
}

