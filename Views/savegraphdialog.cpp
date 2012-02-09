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

// savegraphdialog.cpp : implementation file
//

#include "stdafx.h"
#include "..\ColorHCFR.h"
#include "savegraphdialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSaveGraphDialog dialog


CSaveGraphDialog::CSaveGraphDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CSaveGraphDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSaveGraphDialog)
	m_sizeType = -1;
	m_saveWidth = 0;
	m_saveHeight = 0;
	m_fileType = -1;
	m_jpegQuality = 0;
	//}}AFX_DATA_INIT

	m_sizeType=GetConfig()->GetProfileInt("Save graph","Size type",0);	
	m_saveWidth=GetConfig()->GetProfileInt("Save graph","Image Width",0);	
	m_saveHeight=GetConfig()->GetProfileInt("Save graph","Image Height",0);	
	m_fileType=GetConfig()->GetProfileInt("Save Graph","File type",0);	
	m_jpegQuality=GetConfig()->GetProfileInt("Save graph","Jpeg Quality",95);	
}

CSaveGraphDialog::~CSaveGraphDialog()
{
	GetConfig()->WriteProfileInt("Save graph","Size type",m_sizeType);	
	GetConfig()->WriteProfileInt("Save graph","Image Width",m_saveWidth);	
	GetConfig()->WriteProfileInt("Save graph","Image Height",m_saveHeight);	
	GetConfig()->WriteProfileInt("Save Graph","File type",m_fileType);	
	GetConfig()->WriteProfileInt("Save graph","Jpeg Quality",m_jpegQuality);	
}

void CSaveGraphDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSaveGraphDialog)
	DDX_Radio(pDX, IDC_FILE_TYPE_RADIO, m_fileType);
	DDX_Text(pDX, IDC_SAVE_HEIGHT_EDIT, m_saveHeight);
	DDX_Text(pDX, IDC_SAVE_WIDTH_EDIT, m_saveWidth);
	DDX_Radio(pDX, IDC_SIZE_RADIO, m_sizeType);
	DDX_Text(pDX, IDC_SAVE_JPEGQUALITY_EDIT, m_jpegQuality);
	DDV_MinMaxUInt(pDX, m_jpegQuality, 0, 100);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSaveGraphDialog, CDialog)
	//{{AFX_MSG_MAP(CSaveGraphDialog)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSaveGraphDialog message handlers

void CSaveGraphDialog::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_GRAPH_SAVE, NULL );
}

BOOL CSaveGraphDialog::OnHelpInfo ( HELPINFO * pHelpInfo )
{
	OnHelp ();
	return TRUE;
}
