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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// DTPSensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DTPSensor.h"
#include "DTPSensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDTPSensorPropPage property page

IMPLEMENT_DYNCREATE(CDTPSensorPropPage, CPropertyPageWithHelp)

CDTPSensorPropPage::CDTPSensorPropPage() : CPropertyPageWithHelp(CDTPSensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CDTPSensorPropPage)
	m_CalibrationMode = 2;
	m_timeout = 10000;
	m_bUseInternalOffsets = TRUE;
	m_debugMode = FALSE;
	m_bAverageReads = FALSE;
	//}}AFX_DATA_INIT
}

CDTPSensorPropPage::~CDTPSensorPropPage()
{
}

void CDTPSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDTPSensorPropPage)
	DDX_CBIndex(pDX, IDC_S2SENSOR_MODE_COMBO, m_CalibrationMode);
	DDX_Text(pDX, IDC_READTIME_EDIT, m_timeout);
	DDV_MinMaxInt(pDX, m_timeout, 500, 30000);
	DDX_Check(pDX, IDC_CHECK_USE_BLACKLEVEL, m_bUseInternalOffsets);
	DDX_Check(pDX, IDC_KISENSOR_DEBUG_CB, m_debugMode);
	DDX_Check(pDX, IDC_CHECK_AVG_MEASURES, m_bAverageReads);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDTPSensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CDTPSensorPropPage)
	ON_BN_CLICKED(IDC_DTP_BLACKLEVEL, OnDtpBlacklevel)
	ON_BN_CLICKED(IDC_DTP_TEMP, OnDtpTemp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDTPSensorPropPage message handlers
/////////////////////////////////////////////////////////////////////////////

UINT CDTPSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_DTP94;
}

void CDTPSensorPropPage::OnDtpBlacklevel() 
{
	char	szBuf [ 256 ];
	CString	Msg;

	Msg.LoadString ( IDS_DTP_CALIBRATE );
	if ( IDYES == AfxMessageBox ( Msg, MB_YESNO | MB_ICONQUESTION ) )
	{
		if ( m_pSensor -> Init (FALSE) )
		{
			if ( m_pSensor -> SendAndReceive ( "CO", NULL, 0 ) )
			{
				Msg.LoadString ( IDS_DTP_CALIBRATION_SUCCESS );

				if ( m_pSensor -> SendAndReceive ( "1CO", szBuf, sizeof(szBuf) ) )
				{
					Msg += szBuf;
				}
			}
			else
				Msg.LoadString ( IDS_ERROROCCURRED );
			
			AfxMessageBox ( Msg );

			m_pSensor -> Release ();
		}
	}
}

void CDTPSensorPropPage::OnDtpTemp() 
{
	char	szBuf [ 256 ];
	CString	Msg;
	
	if ( m_pSensor -> Init (FALSE) )
	{
		if ( m_pSensor -> SendAndReceive ( "MT", szBuf, sizeof(szBuf) ) )
			Msg = szBuf;
		else
			Msg.LoadString ( IDS_ERROROCCURRED );

		AfxMessageBox ( Msg );
		m_pSensor -> Release ();
	}
}
