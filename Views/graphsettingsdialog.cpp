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
/////////////////////////////////////////////////////////////////////////////

// graphsettingsdialog.cpp : implementation file
//

#include "stdafx.h"
#include "..\ColorHCFR.h"
#include "graphsettingsdialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGraphSettingsDialog dialog


CGraphSettingsDialog::CGraphSettingsDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CGraphSettingsDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CGraphSettingsDialog)
	m_lineType = -1;
	m_lineWidthStr = _T("");
	m_doShowAxis = FALSE;
	m_doGradientBg = FALSE;
	m_doShowXLabel = FALSE;
	m_doShowYLabel = FALSE;
	m_doShowPoints = FALSE;
	m_doShowToolTips = FALSE;
	m_doShowAllPoints = FALSE;
	m_doShowAllToolTips = FALSE;
	//}}AFX_DATA_INIT
	m_selectedGraph=-1;
}


void CGraphSettingsDialog::DoDataExchange(CDataExchange* pDX)
{
	if(!pDX->m_bSaveAndValidate && m_selectedGraph != CB_ERR)
	{
		m_graphColorButton.SetColor(m_graphArray[m_selectedGraph].m_color);	
		m_lineType=m_graphArray[m_selectedGraph].m_penStyle;	
		m_lineWidthStr.Format("%d",m_graphArray[m_selectedGraph].m_penWidth);	
		m_doShowPoints=m_graphArray[m_selectedGraph].m_doShowPoints;	
		m_doShowToolTips=m_graphArray[m_selectedGraph].m_doShowToolTips;	
	}

	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGraphSettingsDialog)
	DDX_Control(pDX, IDC_GRAPH_LINEWIDTH_COMBO, m_lineWidthCombo);
	DDX_Control(pDX, IDC_GRAPH_LINETYPE_COMBO, m_graphLineCombo);
	DDX_Control(pDX, IDC_GRAPHCOLOR_BUTTON, m_graphColorButton);
	DDX_Control(pDX, IDC_GRAPH_NAME_COMBO, m_graphNameCombo);
	DDX_CBIndex(pDX, IDC_GRAPH_LINETYPE_COMBO, m_lineType);
	DDX_CBString(pDX, IDC_GRAPH_LINEWIDTH_COMBO, m_lineWidthStr);
	DDX_Check(pDX, IDC_GRAPH_SHOWAXIS_CHECK, m_doShowAxis);
	DDX_Check(pDX, IDC_GRAPH_SHOWGRADIENT_CHECK, m_doGradientBg);
	DDX_Check(pDX, IDC_GRAPH_SHOWXLABELS_CHECK, m_doShowXLabel);
	DDX_Check(pDX, IDC_GRAPH_SHOWYLABELS_CHECK, m_doShowYLabel);
	DDX_Check(pDX, IDC_GRAPH_SHOWPOINTS_CHECK, m_doShowPoints);
	DDX_Check(pDX, IDC_GRAPH_SHOWTOOLTIPS_CHECK, m_doShowToolTips);
	DDX_Check(pDX, IDC_GRAPH_SHOWALLPOINTS_CHECK, m_doShowAllPoints);
	DDX_Check(pDX, IDC_GRAPH_SHOWALLTOOLTIPS_CHECK, m_doShowAllToolTips);
	//}}AFX_DATA_MAP

	m_graphLineCombo.EnableWindow(atoi(m_lineWidthStr) == 1);

	if(pDX->m_bSaveAndValidate && m_selectedGraph != CB_ERR)
	{
		m_graphArray[m_selectedGraph].m_color=m_graphColorButton.GetColor();	
		m_graphArray[m_selectedGraph].m_penStyle=m_lineType;	
		m_graphArray[m_selectedGraph].m_penWidth=atoi(m_lineWidthStr);	
		m_graphArray[m_selectedGraph].m_doShowPoints=m_doShowPoints;	
		m_graphArray[m_selectedGraph].m_doShowToolTips=m_doShowToolTips;
	}
}


BEGIN_MESSAGE_MAP(CGraphSettingsDialog, CDialog)
	//{{AFX_MSG_MAP(CGraphSettingsDialog)
	ON_CBN_SELCHANGE(IDC_GRAPH_NAME_COMBO, OnSelchangeGraphNameCombo)
	ON_CBN_SELCHANGE(IDC_GRAPH_LINEWIDTH_COMBO, OnSelchangeGraphLinewidthCombo)
	ON_CBN_EDITCHANGE(IDC_GRAPH_LINEWIDTH_COMBO, OnEditchangeGraphLinewidthCombo)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraphSettingsDialog message handlers

BOOL CGraphSettingsDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
//	m_graphColorButton.SetStyle(TRUE);
	m_graphNameCombo.ResetContent();
	for(int i=0;i<m_graphArray.GetSize();i++)
		m_graphNameCombo.AddString(m_graphArray[i].m_Title);

	m_selectedGraph=m_graphNameCombo.SelectString(0,GetConfig()->GetProfileString("Graph Settings","Last Selected Graph",""));
	if(m_selectedGraph == CB_ERR)
		m_selectedGraph=m_graphNameCombo.SetCurSel(0);

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CGraphSettingsDialog::OnSelchangeGraphNameCombo() 
{
	UpdateData(TRUE);
	m_selectedGraph=m_graphNameCombo.GetCurSel();
	CString str;
	m_graphNameCombo.GetLBText(m_selectedGraph,str);
	GetConfig()->WriteProfileString("Graph Settings","Last Selected Graph",str); // to restore section next time
	UpdateData(FALSE);
}

void CGraphSettingsDialog::OnSelchangeGraphLinewidthCombo() 
{
	m_graphLineCombo.EnableWindow(m_lineWidthCombo.GetCurSel() == 3);	// width of 1 is selected => selection of line type is allowed
}

void CGraphSettingsDialog::OnEditchangeGraphLinewidthCombo() 
{
	UpdateData(TRUE);
	m_graphLineCombo.EnableWindow(m_graphArray[m_selectedGraph].m_penWidth == 1); // width of 1 is selected => selection of line type is allowed
}

void CGraphSettingsDialog::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_GRAPH_PROP, NULL );
}

BOOL CGraphSettingsDialog::OnHelpInfo ( HELPINFO * pHelpInfo )
{
	OnHelp ();
	return TRUE;
}
