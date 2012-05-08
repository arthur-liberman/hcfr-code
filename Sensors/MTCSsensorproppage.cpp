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

// MTCSSensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "MTCSSensor.h"
#include "MTCSSensorPropPage.h"
#include "math.h"

// Include for device interface (this device interface is outside GNU GPL license)
#ifdef USE_NON_FREE_CODE
#include "devlib\CHCFRDI3.h"
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CMTCSSensorPropPage property page

IMPLEMENT_DYNCREATE(CMTCSSensorPropPage, CPropertyPageWithHelp)

CMTCSSensorPropPage::CMTCSSensorPropPage() : CPropertyPageWithHelp(CMTCSSensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CMTCSSensorPropPage)
	m_ReadTime = 1000;
	m_bAdjustTime = FALSE;
	m_debugMode = FALSE;
	//}}AFX_DATA_INIT
}

CMTCSSensorPropPage::~CMTCSSensorPropPage()
{
}

void CMTCSSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMTCSSensorPropPage)
	DDX_Control(pDX, IDC_EDIT_BLACK_3, m_edit_black_3);
	DDX_Control(pDX, IDC_EDIT_BLACK_2, m_edit_black_2);
	DDX_Control(pDX, IDC_EDIT_BLACK_1, m_edit_black_1);
	DDX_Text(pDX, IDC_READTIME_EDIT, m_ReadTime);
	DDV_MinMaxInt(pDX, m_ReadTime, 100, 2000);
	DDX_Check(pDX, IDC_CHECK_VARIABLE_READTIME, m_bAdjustTime);
	DDX_Check(pDX, IDC_KISENSOR_DEBUG_CB, m_debugMode);
	DDX_Control(pDX, IDC_SENSORMATRIX_STATIC, m_matrixStatic);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMTCSSensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CMTCSSensorPropPage)
	ON_BN_CLICKED(IDC_DISPLAY_MATRIX, OnDisplayMatrix)
	ON_BN_CLICKED(IDC_FLASH_EEPROM, OnWriteDeviceMatrix)
	ON_BN_CLICKED(IDC_FLASH_EEPROM_DEF, OnWriteDefaultMatrix)
	ON_BN_CLICKED(IDC_DISPLAY_BLACK, OnDisplayBlack)
	ON_BN_CLICKED(IDC_FLASH_EEPROM_BLACK, OnWriteDeviceBlackLevel)
	ON_BN_CLICKED(IDC_MEASURE_BLACK, OnMeasureBlackLevel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMTCSSensorPropPage message handlers
/////////////////////////////////////////////////////////////////////////////

BOOL CMTCSSensorPropPage::OnInitDialog() 
{
	CPropertyPageWithHelp::OnInitDialog();
	
	m_pGrid = new CGridCtrl;     // Create the Gridctrl object
	if (!m_pGrid ) return FALSE;

	CRect rect;
	m_matrixStatic.GetWindowRect(&rect);	// size control to Static control size
	ScreenToClient(&rect);
	m_pGrid->Create(rect, this,IDC_SENSORMATRIX_GRID,WS_CHILD | WS_TABSTOP | WS_VISIBLE);		// Create the Gridctrl window

	m_pGrid->SetFixedRowCount(0);
	m_pGrid->SetFixedColumnCount(0);

	m_pGrid->SetRowCount(3);     // fill it up with stuff
	m_pGrid->SetColumnCount(3);
	
	m_pGrid->ExpandToFit(TRUE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CMTCSSensorPropPage::OnDisplayMatrix()
{
	CWaitCursor		wait;

	if ( m_pSensor -> Init ( FALSE ) )
	{
		for(int i=0;i<3;i++)
		{
			for(int j=0;j<3;j++)
			{
				CString strValue;
				strValue.Format("%.6f",m_pSensor -> m_DeviceMatrix[i][j]);
				m_pGrid->SetItemText(i,j,strValue);
			}
		}

		m_pGrid->RedrawWindow();
		m_pSensor -> Release ();
	}
}

void CMTCSSensorPropPage::OnWriteDeviceMatrix()
{
#ifdef USE_NON_FREE_CODE
	CString	Msg;
	int		nValues [ 9 ];
	short	sValues [ 9 ];
	
	if ( m_pSensor -> Init ( FALSE ) )
	{
		for(int i=0;i<3;i++)
		{
			for(int j=0;j<3;j++)
			{
				CString strValue;
				double	d;

				strValue = m_pGrid->GetItemText(i,j);

				nValues [(i*3)+j] = (int) ( atof((LPCSTR)strValue) * 1000000.0 );
				sValues [(i*3)+j] = (short) ( nValues [(i*3)+j] / 1000 );
				
				d = ( (double) nValues [(i*3)+j] ) / 1000000.0;

				strValue.Format("%.6f",d);
				m_pGrid->SetItemText(i,j,strValue);
			}
		}

		Msg.LoadString ( IDS_FLASH_MTCS_EEPROM );
		if ( IDYES == AfxMessageBox ( Msg, MB_YESNO | MB_ICONHAND ) )
		{
			if ( SetDevice3Matrix ( sValues ) )
			{
				WriteDevice3UserMemory ( (short*)nValues, DI3_USERMEMOFFSET_HIDEFMATRIX, 18 );

				Msg.LoadString ( IDS_OPERATION_SUCCESSFUL );
				AfxMessageBox ( Msg, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				Msg.LoadString ( IDS_OPERATION_FAILED );
				AfxMessageBox ( Msg, MB_OK | MB_ICONHAND );
			}
		}
		
		m_pSensor -> Release ();
	}
#endif
}

void CMTCSSensorPropPage::OnWriteDefaultMatrix()
{
#ifdef USE_NON_FREE_CODE
	if ( m_pSensor -> Init ( FALSE ) )
	{
		CString Msg;
		int		nValues [ 9 ] = { 103805, -22521, -11563, 7977, 72218, -7207, -2440, 1510, 171367 };
		short	sValues [ 9 ];

		for(int i=0;i<3;i++)
		{
			for(int j=0;j<3;j++)
			{
				CString strValue;
				double	d;

				d = ( (double) nValues [ (i*3)+j ] ) / 1000000.0;

				strValue.Format("%.6f",d);
				m_pGrid->SetItemText(i,j,strValue);

				sValues [ (i*3)+j ] = (short) ( nValues [ (i*3)+j ] / 1000 );
			}
		}
		m_pGrid->RedrawWindow();

		Msg.LoadString ( IDS_FLASH_MTCS_EEPROM );
		if ( IDYES == AfxMessageBox ( Msg, MB_YESNO | MB_ICONHAND ) )
		{
			if ( SetDevice3Matrix ( sValues ) )
			{
				WriteDevice3UserMemory ( (short*)nValues, DI3_USERMEMOFFSET_HIDEFMATRIX, 18 );

				Msg.LoadString ( IDS_OPERATION_SUCCESSFUL );
				AfxMessageBox ( Msg, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				Msg.LoadString ( IDS_OPERATION_FAILED );
				AfxMessageBox ( Msg, MB_OK | MB_ICONHAND );
			}
		}
		m_pSensor -> Release ();
	}
#endif
}

void CMTCSSensorPropPage::OnDisplayBlack() 
{
	CString			strValue;
	CWaitCursor		wait;

	if ( m_pSensor -> Init ( FALSE ) )
	{
		strValue.Format("%.3f",m_pSensor -> m_DeviceBlackLevel(0,0));
		m_edit_black_1.SetWindowText ( (LPCSTR) strValue );

		strValue.Format("%.3f",m_pSensor -> m_DeviceBlackLevel(1,0));
		m_edit_black_2.SetWindowText ( (LPCSTR) strValue );

		strValue.Format("%.3f",m_pSensor -> m_DeviceBlackLevel(2,0));
		m_edit_black_3.SetWindowText ( (LPCSTR) strValue );

		m_pSensor -> Release ();
	}
}

void CMTCSSensorPropPage::OnWriteDeviceBlackLevel() 
{
#ifdef USE_NON_FREE_CODE
	if ( m_pSensor -> Init ( FALSE ) )
	{
		CString Msg;
		char	szBuf [ 64 ];
		short	sValues [ 3 ];

		Msg.LoadString ( IDS_FLASH_MTCS_EEPROM );
		if ( IDYES == AfxMessageBox ( Msg, MB_YESNO | MB_ICONHAND ) )
		{
			m_edit_black_1.GetWindowText ( szBuf, sizeof ( szBuf ) );
			sValues [ 0 ] = (short) ( atof(szBuf) * 1000.0 );

			m_edit_black_2.GetWindowText ( szBuf, sizeof ( szBuf ) );
			sValues [ 1 ] = (short) ( atof(szBuf) * 1000.0 );

			m_edit_black_3.GetWindowText ( szBuf, sizeof ( szBuf ) );
			sValues [ 2 ] = (short) ( atof(szBuf) * 1000.0 );

			if ( SetDevice3BlackLevel ( sValues ) )
			{
				Msg.LoadString ( IDS_OPERATION_SUCCESSFUL );
				AfxMessageBox ( Msg, MB_OK | MB_ICONINFORMATION );
			}
			else
			{
				Msg.LoadString ( IDS_OPERATION_FAILED );
				AfxMessageBox ( Msg, MB_OK | MB_ICONHAND );
			}
		}
		m_pSensor -> Release ();
	}
#endif
}

void CMTCSSensorPropPage::OnMeasureBlackLevel() 
{
#ifdef USE_NON_FREE_CODE
	BOOL		bOk;
	BOOL		bForceOffset;
	CString		Msg;
	CString		strValue;
	CColor		BlackLevel;
	short		sShift;
	short		sOffsets[3];
	short		sValues [ 3 ];

	Msg.LoadString ( IDS_MTCS_CALIBRATE );
	if ( IDYES == AfxMessageBox ( Msg, MB_YESNO | MB_ICONQUESTION ) )
	{
		bForceOffset = ( GetKeyState ( VK_CONTROL ) < 0 );
		if ( m_pSensor -> Init ( FALSE ) )
		{
			if ( ! bForceOffset )
				BlackLevel = m_pSensor -> MeasureBlackLevel ( TRUE );
			
			// Check if returned values are compatible with authorized range
			if ( ! bForceOffset && BlackLevel[0] >= -32.0 && BlackLevel[0] <= +32.0 && BlackLevel[1] >= -32.0 && BlackLevel[1] <= +32.0 && BlackLevel[2] >= -32.0 && BlackLevel[2] <= +32.0 )
			{
				// Values OK.
				strValue.Format("%.3f",BlackLevel[0]);
				m_edit_black_1.SetWindowText ( (LPCSTR) strValue );

				strValue.Format("%.3f",BlackLevel[1]);
				m_edit_black_2.SetWindowText ( (LPCSTR) strValue );

				strValue.Format("%.3f",BlackLevel[2]);
				m_edit_black_3.SetWindowText ( (LPCSTR) strValue );
			}
			else
			{
				// Cannot set this black level. Must perform full offsets checks
				Msg.LoadString ( IDS_MTCS_CALIBRATE_EX );
				if ( IDYES == AfxMessageBox ( Msg, MB_YESNO | MB_ICONQUESTION ) )
				{
					// Force shift value
					m_pSensor -> m_nShiftValue = GetConfig () -> GetProfileInt ( "MTCS", "ShiftValue", 6 );
					if ( m_pSensor -> m_nShiftValue < 0 || m_pSensor -> m_nShiftValue > 6 )
						m_pSensor -> m_nShiftValue = 6;

					// Measure black level
					BlackLevel = m_pSensor -> MeasureBlackLevel ( FALSE );

					// Compute offsets
					sOffsets[0] = (short) floor(BlackLevel[0]);
					sOffsets[1] = (short) floor(BlackLevel[1]);
					sOffsets[2] = (short) floor(BlackLevel[2]);

					BlackLevel[0] -= (double)sOffsets[0];
					BlackLevel[1] -= (double)sOffsets[1];
					BlackLevel[2] -= (double)sOffsets[2];
					
					sValues [ 0 ] = (short) ( BlackLevel[0] * 1000.0 );
					sValues [ 1 ] = (short) ( BlackLevel[1] * 1000.0 );
					sValues [ 2 ] = (short) ( BlackLevel[2] * 1000.0 );

					// Display new black level
					strValue.Format("%.3f",BlackLevel[0]);
					m_edit_black_1.SetWindowText ( (LPCSTR) strValue );

					strValue.Format("%.3f",BlackLevel[1]);
					m_edit_black_2.SetWindowText ( (LPCSTR) strValue );

					strValue.Format("%.3f",BlackLevel[2]);
					m_edit_black_3.SetWindowText ( (LPCSTR) strValue );

					// Write results (shift and offsets)
					sShift = m_pSensor -> m_nShiftValue;
					bOk = WriteDevice3UserMemory ( & sShift, DI3_USERMEMOFFSET_SHIFT, 1 );
					if ( bOk )
						bOk = WriteDevice3UserMemory ( sOffsets, DI3_USERMEMOFFSET_BLACK_OFFSETS, 3 );

					// Write new black level
					if ( bOk && SetDevice3BlackLevel ( sValues ) )
					{
						Msg.LoadString ( IDS_OPERATION_SUCCESSFUL );
						AfxMessageBox ( Msg, MB_OK | MB_ICONINFORMATION );
					}
					else
					{
						Msg.LoadString ( IDS_OPERATION_FAILED );
						AfxMessageBox ( Msg, MB_OK | MB_ICONHAND );
					}

				}
			}

			if ( 0 )
			{
				// Display full user memory content (internal use)
				int		i;
				short	sValues [ 32 ];
				char	szBuf [ 512 ] = "";
				 
				memset ( sValues, 0, sizeof ( sValues ) );
				ReadDevice3UserMemory ( sValues, 0, 26 );

				strcpy ( szBuf, "User memory:" );
				for ( i = 0; i < 7; i ++ )
					sprintf ( strchr ( szBuf, 0 ), "\r\n%02d : %5d %5d %5d %5d", i * 4, (int) sValues [ ( i * 4 ) + 0 ], (int) sValues [ ( i * 4 ) + 1 ], (int) sValues [ ( i * 4 ) + 2 ], (int) sValues [ ( i * 4 ) + 3 ] );

				AfxMessageBox ( szBuf, MB_OK | MB_ICONINFORMATION );
			}
			
			m_pSensor -> Release ();
		}
	}
#endif
}

UINT CMTCSSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_MTCS;
}

