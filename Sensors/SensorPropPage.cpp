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

// SensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "SensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSensorPropPage property page

IMPLEMENT_DYNCREATE(CSensorPropPage, CPropertyPageWithHelp)

CSensorPropPage::CSensorPropPage() : CPropertyPageWithHelp(CSensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(CSensorPropPage)
	m_calibrationDate = _T("");
	m_information = _T("");
	//}}AFX_DATA_INIT

	m_pGrid=NULL;
	m_matrix=IdentityMatrix(3);
}

CSensorPropPage::~CSensorPropPage()
{
}

void CSensorPropPage::SetMatrix(Matrix aMatrix)
{
	m_matrix=aMatrix;
}

Matrix CSensorPropPage::GetMatrix()
{
	return m_matrix;
}

void CSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSensorPropPage)
	DDX_Control(pDX, IDC_SENSORMATRIX_STATIC, m_matrixStatic);
	DDX_Text(pDX, IDC_CALIBRATIONDATE_STATIC, m_calibrationDate);
	DDX_Text(pDX, IDC_EDIT_INFOS, m_information);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CSensorPropPage)
	//}}AFX_MSG_MAP

	ON_NOTIFY(GVN_ENDLABELEDIT, IDC_SENSORMATRIX_GRID, OnGridEndEdit)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSensorPropPage message handlers

BOOL CSensorPropPage::OnInitDialog() 
{
	CPropertyPageWithHelp::OnInitDialog();
	
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

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CSensorPropPage::OnSetActive() 
{
	for(int i=0;i<3;i++)
		for(int j=0;j<3;j++)
		{
			CString strValue;
			strValue.Format("%f",m_matrix[i][j]);
			m_pGrid->SetItemText(i,j,strValue);
		}
	
	return CPropertyPageWithHelp::OnSetActive();
}

BOOL CSensorPropPage::OnKillActive() 
{
	if( m_matrix.Determinant() == 0.0)
	{
		MessageBox("La matrice est non-inversible...\n", "Erreur",MB_ICONERROR | MB_OK);
		return FALSE;
	}
	
	return CPropertyPageWithHelp::OnKillActive();
}

void CSensorPropPage::OnOK() 
{
	delete m_pGrid;
	m_pGrid=NULL;

	CPropertyPageWithHelp::OnOK();
}

BOOL CSensorPropPage::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
    if (m_pGrid && IsWindow(m_pGrid->m_hWnd))
        if (m_pGrid->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
            return TRUE;
	
	return CPropertyPageWithHelp::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CSensorPropPage::OnGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult)
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

UINT CSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_MATRIX;
}
