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

// simulproppage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "SimulatedSensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSimulatedSensorPropPage property page

IMPLEMENT_DYNCREATE(CSimulatedSensorPropPage, CPropertyPageWithHelp)

CSimulatedSensorPropPage::CSimulatedSensorPropPage() : CPropertyPageWithHelp(CSimulatedSensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CSimulatedSensorPropPage)
	m_offsetBlue = 0;
	m_offsetGreen = 0;
	m_offsetRed = 0;
	m_doGainError = FALSE;
	m_doGammaError = FALSE;
	m_doOffsetError = FALSE;
	m_gammaErrorMax = 0.0;
	m_offsetErrorMax = 0.0;
	m_gainErrorMax = 0.0;
	//}}AFX_DATA_INIT
}

CSimulatedSensorPropPage::~CSimulatedSensorPropPage()
{
}

void CSimulatedSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSimulatedSensorPropPage)
	DDX_Text(pDX, IDC_BLUEOFFSET_EDIT, m_offsetBlue);
	DDX_Text(pDX, IDC_GREENOFFSET_EDIT, m_offsetGreen);
	DDX_Text(pDX, IDC_REDOFFSET_EDIT, m_offsetRed);
	DDX_Check(pDX, IDC_GAIN_CHECK, m_doGainError);
	DDX_Check(pDX, IDC_GAMMA_CHECK, m_doGammaError);
	DDX_Check(pDX, IDC_OFFSET_CHECK, m_doOffsetError);
	DDX_Text(pDX, IDC_GAMMAERROR_EDIT, m_gammaErrorMax);
	DDX_Text(pDX, IDC_OFFSETERROR_EDIT, m_offsetErrorMax);
	DDX_Text(pDX, IDC_GAINERROR_EDIT, m_gainErrorMax);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSimulatedSensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CSimulatedSensorPropPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSimulatedSensorPropPage message handlers

UINT CSimulatedSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_SIMULATED;
}
