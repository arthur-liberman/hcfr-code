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
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// DuplicateDocDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DuplicateDocDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDuplicateDocDlg dialog


CDuplicateDocDlg::CDuplicateDocDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDuplicateDocDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDuplicateDocDlg)
	m_DuplContrastCheck = FALSE;
	m_DuplGrayLevelCheck = FALSE;
	m_DuplNearBlackCheck = FALSE;
	m_DuplNearWhiteCheck = FALSE;
	m_DuplPrimariesSatCheck = FALSE;
	m_DuplSecondariesSatCheck = FALSE;
	m_DuplPrimariesColCheck = FALSE;
	m_DuplSecondariesColCheck = FALSE;
	m_DuplInfoCheck = FALSE;
	m_DuplXYZCheck = FALSE;
	//}}AFX_DATA_INIT
}


void CDuplicateDocDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDuplicateDocDlg)
	DDX_Check(pDX, IDC_DUPLCONTRAST_CHECK, m_DuplContrastCheck);
	DDX_Check(pDX, IDC_DUPLGRAYLEVEL_CHECK, m_DuplGrayLevelCheck);
	DDX_Check(pDX, IDC_DUPLNEARBLACK_CHECK, m_DuplNearBlackCheck);
	DDX_Check(pDX, IDC_DUPLNEARWHITE_CHECK, m_DuplNearWhiteCheck);
	DDX_Check(pDX, IDC_DUPLPRIMARIESSAT_CHECK, m_DuplPrimariesSatCheck);
	DDX_Check(pDX, IDC_DUPLSECONDARIESSAT_CHECK, m_DuplSecondariesSatCheck);
	DDX_Check(pDX, IDC_DUPLPRIMARIESCOL_CHECK, m_DuplPrimariesColCheck);
	DDX_Check(pDX, IDC_DUPLSECONDARIESCOL_CHECK, m_DuplSecondariesColCheck);
	DDX_Check(pDX, IDC_DUPLINFO_CHECK, m_DuplInfoCheck);
	DDX_Check(pDX, IDC_DUPLXYZ_CHECK, m_DuplXYZCheck);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDuplicateDocDlg, CDialog)
	//{{AFX_MSG_MAP(CDuplicateDocDlg)
	ON_BN_CLICKED(IDC_INVERT_BUTTON, OnInvertButton)
	ON_BN_CLICKED(IDC_CLEAR_BUTTON, OnClearButton)
	ON_BN_CLICKED(ID_HELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDuplicateDocDlg message handlers

void CDuplicateDocDlg::OnInvertButton() 
{
	UpdateData(TRUE);
	m_DuplGrayLevelCheck =!m_DuplGrayLevelCheck;
	m_DuplNearBlackCheck =!m_DuplNearBlackCheck;
	m_DuplNearWhiteCheck =!m_DuplNearWhiteCheck;
	m_DuplPrimariesSatCheck =!m_DuplPrimariesSatCheck;
	m_DuplSecondariesSatCheck =!m_DuplSecondariesSatCheck;
	m_DuplPrimariesColCheck =!m_DuplPrimariesColCheck;
	m_DuplSecondariesColCheck =!m_DuplSecondariesColCheck;
	m_DuplContrastCheck =!m_DuplContrastCheck;
	m_DuplInfoCheck =!m_DuplInfoCheck;
	m_DuplXYZCheck =!m_DuplXYZCheck;
	UpdateData(FALSE);
}

void CDuplicateDocDlg::OnClearButton() 
{
	UpdateData(TRUE);
	m_DuplGrayLevelCheck = FALSE;
	m_DuplNearBlackCheck = FALSE;
	m_DuplNearWhiteCheck = FALSE;
	m_DuplPrimariesSatCheck = FALSE;
	m_DuplSecondariesSatCheck = FALSE;
	m_DuplPrimariesColCheck = FALSE;
	m_DuplSecondariesColCheck = FALSE;
	m_DuplContrastCheck = FALSE;
	m_DuplInfoCheck = FALSE;
	m_DuplXYZCheck = FALSE;
	UpdateData(FALSE);
}

void CDuplicateDocDlg::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_DUPLICATEDOC, NULL );
}

BOOL CDuplicateDocDlg::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	OnHelp ();
	return TRUE;
}

