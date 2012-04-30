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

// KiSensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "KiSensor.h"
#include "KiSensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// First firmware with HW identifier
#define FIRMWARE_HWI_MAJOR_VERSION	5
#define FIRMWARE_HWI_MINOR_VERSION	30


/////////////////////////////////////////////////////////////////////////////
// CKiSensorPropPage property page

IMPLEMENT_DYNCREATE(CKiSensorPropPage, CPropertyPageWithHelp)

CKiSensorPropPage::CKiSensorPropPage() : CPropertyPageWithHelp(CKiSensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CKiSensorPropPage)
	m_comPort = _T("USB");
	m_debugMode = FALSE;
	m_timeoutMesure = 0;
	m_bMeasureWhite = FALSE;
	m_nSensorsUsed = 0;
	m_nInterlaceMode = 0;
	m_bFastMeasure = FALSE;
	//}}AFX_DATA_INIT
}

CKiSensorPropPage::~CKiSensorPropPage()
{
}

void CKiSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKiSensorPropPage)
	DDX_CBString(pDX, IDC_KISENSOR_COM_COMBO, m_comPort);
	DDV_MaxChars(pDX, m_comPort, 5);
	DDX_Check(pDX, IDC_KISENSOR_DEBUG_CB, m_debugMode);
	DDX_Text(pDX, IDC_TIMEOUT_MESURE_EDIT, m_timeoutMesure);
	DDV_MinMaxInt(pDX, m_timeoutMesure, 500, 120000);
	DDX_Radio(pDX, IDC_KISENSOR_TAOS_0, m_nSensorsUsed);
	DDX_Radio(pDX, IDC_KISENSOR_INTERLACE_0, m_nInterlaceMode);
	DDX_Check(pDX, IDC_KISENSOR_WHITE, m_bMeasureWhite);
	DDX_Check(pDX, IDC_KISENSOR_FASTMEASURE, m_bFastMeasure);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKiSensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CKiSensorPropPage)
	ON_BN_CLICKED(IDC_KISENSOR_GETVERSION, OnKisensorGetversion)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKiSensorPropPage message handlers
/////////////////////////////////////////////////////////////////////////////

void CKiSensorPropPage::OnKisensorGetversion() 
{
	char		szVersion [ 256 ] = "";
	CString		strMsg,strMsg2;
	CString		SaveSensorPort = m_pSensor -> m_comPort;
	CWaitCursor	wait;
	int		nMajor, nMinor;
	LPSTR	lpStr;

	UpdateData ( TRUE );

	m_pSensor -> m_comPort = m_comPort;
	if ( m_pSensor -> Init (FALSE) )
	{
		if ( m_pSensor -> acquire ( (char *) (LPCTSTR) m_pSensor -> m_RealComPort, 2000, (char) 0xFF, szVersion ) )
		{
			if ( szVersion [ 0 ] != '\0' )
			{
				strMsg.LoadString ( IDS_SENSORVERSION );
				strMsg += " ";
				strMsg += szVersion;
			}
		}

		m_pSensor -> Release ();

		lpStr = strchr ( szVersion, '.' );
		if (lpStr )
		{
			nMajor = atoi ( szVersion + 1 );
			nMinor = atoi ( lpStr + 1 );
			if ( nMajor > FIRMWARE_HWI_MAJOR_VERSION || ( nMajor == FIRMWARE_HWI_MAJOR_VERSION && nMinor >= FIRMWARE_HWI_MINOR_VERSION ) )
			{
				if ( m_pSensor -> acquire ( (char *) (LPCTSTR) m_pSensor -> m_RealComPort, 2000, (char) 0xFE, szVersion ) )
				{
					if ( szVersion [ 0 ] != '\0' )
					{
						strMsg2.LoadString ( IDS_SENSORHWVERSION );
						strMsg += "\n";
						strMsg += strMsg2;
						strMsg += " ";
						strMsg += szVersion;
					}
				}
				m_pSensor -> Release ();
			}
		}
	}

	m_pSensor -> m_comPort = SaveSensorPort;

	if ( ! strMsg.IsEmpty () )
		AfxMessageBox ( strMsg, MB_ICONINFORMATION | MB_OK );
	else
	{
		strMsg.LoadString ( IDS_SENSORNOTFOUND );
		AfxMessageBox ( strMsg, MB_ICONHAND | MB_OK );
	}
}

UINT CKiSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_KI;
}
