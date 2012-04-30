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

// graphscalepropdialog.cpp : implementation file
//

#include "stdafx.h"
#include "..\ColorHCFR.h"
#include "graphscalepropdialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGraphScalePropDialog dialog


CGraphScalePropDialog::CGraphScalePropDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CGraphScalePropDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CGraphScalePropDialog)
	m_maxXGrow = 0;
	m_maxYGrow = 0;
	m_minXGrow = 0;
	m_minYGrow = 0;
	m_xAxisStep = 0;
	m_yAxisStep = 0;
	m_maxY = 0;
	m_minY = 0;
	m_maxX = 0;
	m_minX = 0;
	//}}AFX_DATA_INIT
}


void CGraphScalePropDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGraphScalePropDialog)
	DDX_Text(pDX, IDC_MAXXGROW_EDIT, m_maxXGrow);
	DDX_Text(pDX, IDC_MAXYGROW_EDIT, m_maxYGrow);
	DDX_Text(pDX, IDC_MINXGROW_EDIT, m_minXGrow);
	DDX_Text(pDX, IDC_MINYGROW_EDIT, m_minYGrow);
	DDX_Text(pDX, IDC_XAXISSTEP_EDIT, m_xAxisStep);
	DDX_Text(pDX, IDC_YAXISSTEP_EDIT, m_yAxisStep);
	DDX_Text(pDX, IDC_YMAX_EDIT, m_maxY);
	DDX_Text(pDX, IDC_YMIN_EDIT, m_minY);
	DDX_Text(pDX, IDC_XMAX_EDIT, m_maxX);
	DDX_Text(pDX, IDC_XMIN_EDIT, m_minX);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGraphScalePropDialog, CDialog)
	//{{AFX_MSG_MAP(CGraphScalePropDialog)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGraphScalePropDialog message handlers

void CGraphScalePropDialog::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_GRAPH_SCALE, NULL );
}

BOOL CGraphScalePropDialog::OnHelpInfo ( HELPINFO * pHelpInfo )
{
	OnHelp ();
	return TRUE;
}
