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

// ExportReplaceDialog.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "ExportReplaceDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CExportReplaceDialog dialog


CExportReplaceDialog::CExportReplaceDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CExportReplaceDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CExportReplaceDialog)
	m_appendRadioChecked = 0;
	//}}AFX_DATA_INIT
	m_replaceMeasureList.RemoveAll();
	m_selectedMeasure = -1;
}


void CExportReplaceDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CExportReplaceDialog)
	DDX_Control(pDX, IDOK, m_okButton);
	DDX_Control(pDX, IDC_REPLACEMEASURE_LIST, m_replaceMeasureListBox);
	DDX_Radio(pDX, IDC_APPEND_RADIO, m_appendRadioChecked);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CExportReplaceDialog, CDialog)
	//{{AFX_MSG_MAP(CExportReplaceDialog)
	ON_BN_CLICKED(IDC_APPEND_RADIO, OnAppendRadio)
	ON_BN_CLICKED(IDC_REPLACE_RADIO, OnReplaceRadio)
	ON_LBN_SELCHANGE(IDC_REPLACEMEASURE_LIST, OnSelchangeReplaceMeasureList)
	ON_LBN_DBLCLK(IDC_REPLACEMEASURE_LIST, OnDblclkReplaceMeasureList)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CExportReplaceDialog message handlers

void CExportReplaceDialog::OnAppendRadio() 
{
	m_replaceMeasureListBox.EnableWindow(FALSE);
}

void CExportReplaceDialog::OnReplaceRadio() 
{
	m_replaceMeasureListBox.EnableWindow(TRUE);
	m_okButton.EnableWindow(m_selectedMeasure!=LB_ERR );
}

void CExportReplaceDialog::AddMeasure(CString aMeasure)
{
	m_replaceMeasureList.Add(aMeasure);
}

BOOL CExportReplaceDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	for(int i=0;i<m_replaceMeasureList.GetSize();i++)
		m_replaceMeasureListBox.AddString(m_replaceMeasureList.GetAt(i));

	m_replaceMeasureListBox.EnableWindow(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CExportReplaceDialog::OnSelchangeReplaceMeasureList() 
{
	m_selectedMeasure = m_replaceMeasureListBox.GetCurSel();
	m_okButton.EnableWindow(m_selectedMeasure!=LB_ERR );
}

void CExportReplaceDialog::OnDblclkReplaceMeasureList() 
{
	CDialog::OnOK();
	
}

void CExportReplaceDialog::OnOK() 
{
	// TODO: Add extra validation here
	
	CDialog::OnOK();
}

void CExportReplaceDialog::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_EXPORT_XLS, NULL );
}

BOOL CExportReplaceDialog::OnHelpInfo ( HELPINFO * pHelpInfo )
{
	OnHelp ();
	return TRUE;
}
