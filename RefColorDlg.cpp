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

// RefColorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "RefColorDlg.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRefColorDlg dialog


CRefColorDlg::CRefColorDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRefColorDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CRefColorDlg)
	m_red1 = 0.412;
	m_red2 = 0.213;
	m_red3 = 0.019;
	m_green1 = 0.358;
	m_green2 = 0.715;
	m_green3 = 0.119;
	m_blue1 = 0.180;
	m_blue2 = 0.072;
	m_blue3 = 0.951;
	m_white1 = 0.950;
	m_white2 = 1.0;
	m_white3 = 1.089;
	//}}AFX_DATA_INIT

	m_RedColor = noDataColor;
	m_GreenColor = noDataColor;
	m_BlueColor = noDataColor;
	m_WhiteColor = noDataColor;
	m_bxyY = FALSE;
}


void CRefColorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CRefColorDlg)
	DDX_Control(pDX, IDC_STATIC_1, m_label1);
	DDX_Control(pDX, IDC_STATIC_2, m_label2);
	DDX_Control(pDX, IDC_STATIC_3, m_label3);
	DDX_Text(pDX, IDC_RED_1, m_red1);
	DDX_Text(pDX, IDC_RED_2, m_red2);
	DDX_Text(pDX, IDC_RED_3, m_red3);
	DDX_Text(pDX, IDC_GREEN_1, m_green1);
	DDX_Text(pDX, IDC_GREEN_2, m_green2);
	DDX_Text(pDX, IDC_GREEN_3, m_green3);
	DDX_Text(pDX, IDC_BLUE_1, m_blue1);
	DDX_Text(pDX, IDC_BLUE_2, m_blue2);
	DDX_Text(pDX, IDC_BLUE_3, m_blue3);
	DDX_Text(pDX, IDC_WHITE_1, m_white1);
	DDX_Text(pDX, IDC_WHITE_2, m_white2);
	DDX_Text(pDX, IDC_WHITE_3, m_white3);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CRefColorDlg, CDialog)
	//{{AFX_MSG_MAP(CRefColorDlg)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	ON_BN_CLICKED(IDC_RADIO1, OnRadio1)
	ON_BN_CLICKED(IDC_RADIO2, OnRadio2)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRefColorDlg message handlers

BOOL CRefColorDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	CheckRadioButton ( IDC_RADIO1, IDC_RADIO2, IDC_RADIO1 );
	m_bxyY = FALSE;
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CRefColorDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

void CRefColorDlg::OnOK() 
{
	UpdateData (TRUE);

	if ( IsDlgButtonChecked ( IDC_RADIO1 ) )
	{
		m_RedColor[0]=m_red1;
		m_RedColor[1]=m_red2;
		m_RedColor[2]=m_red3;
		m_GreenColor[0]=m_green1;
		m_GreenColor[1]=m_green2;
		m_GreenColor[2]=m_green3;
		m_BlueColor[0]=m_blue1;
		m_BlueColor[1]=m_blue2;
		m_BlueColor[2]=m_blue3;
		m_WhiteColor[0]=m_white1;
		m_WhiteColor[1]=m_white2;
		m_WhiteColor[2]=m_white3;
	}
	else
	{
		CColor	aColor;
		
		aColor[0]=m_red1;
		aColor[1]=m_red2;
		aColor[2]=m_red3;
		m_RedColor.SetxyYValue(aColor);
		
		aColor[0]=m_green1;
		aColor[1]=m_green2;
		aColor[2]=m_green3;
		m_GreenColor.SetxyYValue(aColor);
		
		aColor[0]=m_blue1;
		aColor[1]=m_blue2;
		aColor[2]=m_blue3;
		m_BlueColor.SetxyYValue(aColor);
		
		aColor[0]=m_white1;
		aColor[1]=m_white2;
		aColor[2]=m_white3;
		m_WhiteColor.SetxyYValue(aColor);
	}

	CDialog::OnOK();
}

void CRefColorDlg::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_COLOR_REFERENCES, NULL );
}

BOOL CRefColorDlg::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	OnHelp ();
	return TRUE;
}

void CRefColorDlg::OnRadio1() 
{
	// TODO: Add your control notification handler code here
	m_label1.SetWindowText ( "X:" );
	m_label2.SetWindowText ( "Y:" );
	m_label3.SetWindowText ( "Z:" );

	if ( m_bxyY )
	{
		// Convert xyY values to XYZ values
		UpdateData(TRUE);
		
		CColor	aColor, aColor2;
		
		aColor[0]=m_red1;
		aColor[1]=m_red2;
		aColor[2]=m_red3;
		aColor2.SetxyYValue(aColor);
		m_red1=floor( aColor2.GetX() * 1000.0 + 0.5 ) / 1000.0;
		m_red2=floor( aColor2.GetY() * 1000.0 + 0.5 ) / 1000.0;
		m_red3=floor( aColor2.GetZ() * 1000.0 + 0.5 ) / 1000.0;
		
		aColor[0]=m_green1;
		aColor[1]=m_green2;
		aColor[2]=m_green3;
		aColor2.SetxyYValue(aColor);
		m_green1=floor( aColor2.GetX() * 1000.0 + 0.5 ) / 1000.0;
		m_green2=floor( aColor2.GetY() * 1000.0 + 0.5 ) / 1000.0;
		m_green3=floor( aColor2.GetZ() * 1000.0 + 0.5 ) / 1000.0;
		
		aColor[0]=m_blue1;
		aColor[1]=m_blue2;
		aColor[2]=m_blue3;
		aColor2.SetxyYValue(aColor);
		m_blue1=floor( aColor2.GetX() * 1000.0 + 0.5 ) / 1000.0;
		m_blue2=floor( aColor2.GetY() * 1000.0 + 0.5 ) / 1000.0;
		m_blue3=floor( aColor2.GetZ() * 1000.0 + 0.5 ) / 1000.0;
		
		aColor[0]=m_white1;
		aColor[1]=m_white2;
		aColor[2]=m_white3;
		aColor2.SetxyYValue(aColor);
		m_white1=floor( aColor2.GetX() * 1000.0 + 0.5 ) / 1000.0;
		m_white2=floor( aColor2.GetY() * 1000.0 + 0.5 ) / 1000.0;
		m_white3=floor( aColor2.GetZ() * 1000.0 + 0.5 ) / 1000.0;
		
		UpdateData(FALSE);
	}

	m_bxyY = FALSE;
}

void CRefColorDlg::OnRadio2() 
{
	// TODO: Add your control notification handler code here
	m_label1.SetWindowText ( "x:" );
	m_label2.SetWindowText ( "y:" );
	m_label3.SetWindowText ( "Y:" );

	if ( ! m_bxyY )
	{
		// Convert XYZ values to xyY values
		UpdateData(TRUE);
		
		CColor	aColor, aColor2;
		
		aColor[0]=m_red1;
		aColor[1]=m_red2;
		aColor[2]=m_red3;
		aColor2=aColor.GetxyYValue();
		m_red1=floor( aColor2.GetX() * 1000.0 + 0.5 ) / 1000.0;
		m_red2=floor( aColor2.GetY() * 1000.0 + 0.5 ) / 1000.0;
		m_red3=floor( aColor2.GetZ() * 1000.0 + 0.5 ) / 1000.0;
		
		aColor[0]=m_green1;
		aColor[1]=m_green2;
		aColor[2]=m_green3;
		aColor2=aColor.GetxyYValue();
		m_green1=floor( aColor2.GetX() * 1000.0 + 0.5 ) / 1000.0;
		m_green2=floor( aColor2.GetY() * 1000.0 + 0.5 ) / 1000.0;
		m_green3=floor( aColor2.GetZ() * 1000.0 + 0.5 ) / 1000.0;
		
		aColor[0]=m_blue1;
		aColor[1]=m_blue2;
		aColor[2]=m_blue3;
		aColor2=aColor.GetxyYValue();
		m_blue1=floor( aColor2.GetX() * 1000.0 + 0.5 ) / 1000.0;
		m_blue2=floor( aColor2.GetY() * 1000.0 + 0.5 ) / 1000.0;
		m_blue3=floor( aColor2.GetZ() * 1000.0 + 0.5 ) / 1000.0;
		
		aColor[0]=m_white1;
		aColor[1]=m_white2;
		aColor[2]=m_white3;
		aColor2=aColor.GetxyYValue();
		m_white1=floor( aColor2.GetX() * 1000.0 + 0.5 ) / 1000.0;
		m_white2=floor( aColor2.GetY() * 1000.0 + 0.5 ) / 1000.0;
		m_white3=floor( aColor2.GetZ() * 1000.0 + 0.5 ) / 1000.0;
		
		UpdateData(FALSE);
	}

	m_bxyY = TRUE;
}

