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

// AdjustMatrixDlg.cpp : implementation file
//

#include "stdafx.h"
#include "colorhcfr.h"
#include "AdjustMatrixDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAdjustMatrixDlg dialog


CAdjustMatrixDlg::CAdjustMatrixDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAdjustMatrixDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CAdjustMatrixDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_pGrid=NULL;
	m_matrix=IdentityMatrix(3);
}


void CAdjustMatrixDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAdjustMatrixDlg)
	DDX_Control(pDX, IDC_SENSORMATRIX_STATIC, m_matrixStatic);
	DDX_Control(pDX, IDC_EDIT_COMMENT, m_Comment);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAdjustMatrixDlg, CDialog)
	//{{AFX_MSG_MAP(CAdjustMatrixDlg)
	ON_BN_CLICKED(IDC_LOAD, OnLoad)
	ON_BN_CLICKED(IDC_SAVE, OnSave)
	ON_BN_CLICKED(IDHELP, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
	ON_NOTIFY(GVN_ENDLABELEDIT, IDC_SENSORMATRIX_GRID, OnGridEndEdit)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAdjustMatrixDlg message handlers

BOOL CAdjustMatrixDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_pGrid = new CGridCtrl;     // Create the Gridctrl object
	if (!m_pGrid ) return FALSE;

	CRect rect;
	m_matrixStatic.GetWindowRect(&rect);	// size control to Static control size
	ScreenToClient(&rect);
	m_pGrid->Create(rect, this,IDC_SENSORMATRIX_GRID,WS_CHILD | WS_TABSTOP | WS_VISIBLE);		// Create the Gridctrl window

	m_pGrid->SetFixedRowCount(0);
	m_pGrid->SetFixedColumnCount(0);

	m_pGrid->SetRowCount(3);     // fill it up with stuff
	m_pGrid->SetColumnCount(3);
	
	m_pGrid->SetEditable(TRUE);
	m_pGrid->EnableDragAndDrop(TRUE);

	m_pGrid->ExpandToFit(TRUE);

	for(int i=0;i<3;i++)
	{
		for(int j=0;j<3;j++)
		{
			CString strValue;
			strValue.Format("%f",m_matrix[i][j]);
			m_pGrid->SetItemText(i,j,strValue);
		}
	}

	m_Comment.SetWindowText ( m_strComment );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CAdjustMatrixDlg::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
    if (m_pGrid && IsWindow(m_pGrid->m_hWnd))
        if (m_pGrid->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
            return TRUE;
	
	return CDialog::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CAdjustMatrixDlg::OnGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult)
{
    NM_GRIDVIEW* pItem = (NM_GRIDVIEW*) pNotifyStruct;

	CString aNewStr=m_pGrid->GetItemText(pItem->iRow,pItem->iColumn);
	aNewStr.Replace(",",".");	// replace decimal separator if necessary
 	float aVal;
	BOOL bAcceptChange = !aNewStr.IsEmpty() && sscanf(aNewStr,"%f",&aVal);
	if(bAcceptChange)
	{
		m_matrix[pItem->iRow][pItem->iColumn]=aVal;
		CString strValue;
		strValue.Format("%f",m_matrix[pItem->iRow][pItem->iColumn]);
		m_pGrid->SetItemText(pItem->iRow,pItem->iColumn,strValue);	// reformat value
	}
}

void CAdjustMatrixDlg::OnLoad() 
{
	CFileDialog fileOpenDialog( TRUE, "mhc", NULL, OFN_NOCHANGEDIR, "XYZ Conversion Matrix File (*.mhc)|*.mhc||" );
	fileOpenDialog.m_ofn.lpstrInitialDir = (LPCSTR) GetConfig () -> m_ApplicationPath;

	if(fileOpenDialog.DoModal()==IDOK)
	{
		CFile loadFile(fileOpenDialog.GetPathName(),CFile::modeRead);
		CArchive ar(&loadFile,CArchive::load);
		m_matrix.Serialize(ar);
		ar >> m_strComment;
		
		for(int i=0;i<3;i++)
		{
			for(int j=0;j<3;j++)
			{
				CString strValue;
				strValue.Format("%f",m_matrix[i][j]);
				m_pGrid->SetItemText(i,j,strValue);
			}
		}
		m_pGrid->RedrawWindow();
		m_Comment.SetWindowText ( m_strComment );
	}
}

void CAdjustMatrixDlg::OnSave() 
{
	m_Comment.GetWindowText ( m_strComment );

	CFileDialog fileSaveDialog( FALSE, "mhc", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "XYZ Conversion Matrix File (*.mhc)|*.mhc||" );
	fileSaveDialog.m_ofn.lpstrInitialDir = (LPCSTR) GetConfig () -> m_ApplicationPath;

	if(fileSaveDialog.DoModal()==IDOK)
	{
		CFile saveFile(fileSaveDialog.GetPathName(),CFile::modeCreate|CFile::modeWrite);
		CArchive ar(&saveFile,CArchive::store);
		m_matrix.Serialize(ar);
		ar << m_strComment;
	}
}

void CAdjustMatrixDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

void CAdjustMatrixDlg::OnHelp() 
{
	// TODO: Add your control notification handler code here
	GetConfig () -> DisplayHelp ( HID_CONVERSION_MATRIX, NULL );
}

BOOL CAdjustMatrixDlg::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	OnHelp ();
	return TRUE;
}

void CAdjustMatrixDlg::OnOK() 
{
	// TODO: Add extra validation here
	m_Comment.GetWindowText ( m_strComment );
	
	CDialog::OnOK();
}
