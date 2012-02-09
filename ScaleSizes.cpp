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

// ScaleSizes.cpp : implementation file
//

#include "stdafx.h"
#include "colorhcfr.h"
#include "DataSetDoc.h"
#include "ScaleSizes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CScaleSizes dialog


CScaleSizes::CScaleSizes(CDataSetDoc * pDoc, CWnd* pParent /*=NULL*/)
	: CDialog(CScaleSizes::IDD, pParent)
{
	//{{AFX_DATA_INIT(CScaleSizes)
	m_NbNearBlack = 0;
	m_NbNearWhite = 0;
	m_NbSat = 0;
	m_bIRE = FALSE;
	//}}AFX_DATA_INIT

	m_pDoc = pDoc;
	m_NbGrays = m_pDoc -> GetMeasure () -> GetGrayScaleSize () - 1;
	m_NbNearBlack = m_pDoc -> GetMeasure () -> GetNearBlackScaleSize () - 1;
	m_NbNearWhite = m_pDoc -> GetMeasure () -> GetNearWhiteScaleSize () - 1;
	m_NbSat = m_pDoc -> GetMeasure () -> GetSaturationSize () - 1;
	m_bIRE = m_pDoc -> GetMeasure () -> m_bIREScaleMode;

	if ( m_bIRE && m_NbGrays > 10 )
		m_NbGrays ++;
}


void CScaleSizes::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CScaleSizes)
	DDX_Control(pDX, IDC_COMBO_EDIT_GRAYS, m_ComboEditGrays);
	DDX_Control(pDX, IDC_COMBO_GRAYS, m_ComboGrays);
	DDX_Text(pDX, IDC_EDIT_NEARBLACK, m_NbNearBlack);
	DDV_MinMaxInt(pDX, m_NbNearBlack, 2, 50);
	DDX_Text(pDX, IDC_EDIT_NEARWHITE, m_NbNearWhite);
	DDV_MinMaxInt(pDX, m_NbNearWhite, 2, 50);
	DDX_Text(pDX, IDC_EDIT_SAT, m_NbSat);
	DDV_MinMaxInt(pDX, m_NbSat, 2, 50);
	DDX_Check(pDX, IDC_CHECK_IRE, m_bIRE);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CScaleSizes, CDialog)
	//{{AFX_MSG_MAP(CScaleSizes)
	ON_BN_CLICKED(IDC_CHECK_IRE, OnCheckIre)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScaleSizes message handlers

BOOL CScaleSizes::OnInitDialog() 
{
	CString	str;

	CDialog::OnInitDialog();
	
	str.Format ( "%d", m_NbGrays );

	if ( m_bIRE )
	{
		m_ComboGrays.ShowWindow ( SW_SHOW );
		m_ComboEditGrays.ShowWindow ( SW_HIDE );

		m_ComboGrays.SetCurSel ( m_ComboGrays.FindStringExact(-1,(LPCSTR) str) );
		m_ComboEditGrays.SetCurSel ( 1 );
	}
	else
	{
		m_ComboGrays.ShowWindow ( SW_HIDE );
		m_ComboEditGrays.ShowWindow ( SW_SHOW );

		m_ComboEditGrays.SetWindowText ( str );
		m_ComboGrays.SetCurSel ( 1 );
	}

	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CScaleSizes::OnOK() 
{
	CString	str;

	CDialog::OnOK();

	if ( m_bIRE )																		 
		m_ComboGrays.GetLBText ( m_ComboGrays.GetCurSel (), str );
	else
		m_ComboEditGrays.GetWindowText ( str );
	
	m_NbGrays = atoi ( (LPCSTR) str );

	if ( m_NbGrays >= 4 && m_NbGrays <= 100	&& m_NbNearBlack >= 2 && m_NbNearBlack <= 50 && m_NbNearWhite >= 2 && m_NbNearWhite <= 50 && m_NbSat >= 2 && m_NbSat <= 50 )
	{
		m_pDoc -> GetMeasure () -> SetIREScaleMode ( m_bIRE );
		m_pDoc -> GetMeasure () -> SetGrayScaleSize ( ( m_bIRE && m_NbGrays > 10 ) ? m_NbGrays : m_NbGrays + 1 );
		m_pDoc -> GetMeasure () -> SetNearBlackScaleSize ( m_NbNearBlack + 1 );
		m_pDoc -> GetMeasure () -> SetNearWhiteScaleSize ( m_NbNearWhite + 1 );
		m_pDoc -> GetMeasure () -> SetSaturationSize ( m_NbSat + 1 );
		
		GetConfig()->WriteProfileInt("References","IRELevels",m_bIRE);

		m_pDoc -> UpdateAllViews ( NULL );
	}
}

void CScaleSizes::OnCancel() 
{
	CDialog::OnCancel();
}

void CScaleSizes::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_SCALESIZES, NULL );
}

BOOL CScaleSizes::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	OnHelp ();
	return TRUE;
}

void CScaleSizes::OnCheckIre() 
{
	// TODO: Add your control notification handler code here
	UpdateData ();
	
	if ( m_bIRE )
	{
		m_ComboGrays.ShowWindow ( SW_SHOW );
		m_ComboEditGrays.ShowWindow ( SW_HIDE );
	}
	else
	{
		m_ComboGrays.ShowWindow ( SW_HIDE );
		m_ComboEditGrays.ShowWindow ( SW_SHOW );
	}
}
