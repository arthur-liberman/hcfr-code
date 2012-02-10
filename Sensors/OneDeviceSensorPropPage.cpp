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

// OneDeviceSensorPropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "OneDeviceSensor.h"
#include "OneDeviceSensorPropPage.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COneDeviceSensorPropPage property page

IMPLEMENT_DYNCREATE(COneDeviceSensorPropPage, CPropertyPageWithHelp)

COneDeviceSensorPropPage::COneDeviceSensorPropPage() : CPropertyPageWithHelp(COneDeviceSensorPropPage::IDD)
{
	//{{AFX_DATA_INIT(COneDeviceSensorPropPage)
	m_doBlackCompensation = FALSE;
	m_maxAdditivityErrorPercent = 0;
	m_doVerifyAdditivity = FALSE;
	m_showAdditivityResults = FALSE;
	m_IRELevelStr = _T("");
	//}}AFX_DATA_INIT

	m_pPrimariesGrid=NULL;
	m_pWhiteGrid=NULL;
	m_primariesMatrix=IdentityMatrix(3);
	m_whiteMatrix=Matrix(1.0,3,1);
	m_calibrationIRELevel=100.0;
	m_calibrationReferenceName="Moniteur sRGB";
}

COneDeviceSensorPropPage::~COneDeviceSensorPropPage()
{
	delete m_pPrimariesGrid;
	delete m_pWhiteGrid;
}

void COneDeviceSensorPropPage::DoDataExchange(CDataExchange* pDX)
{
	if(pDX->m_bSaveAndValidate == 0)	// 0=init 1=save
		m_IRELevelStr.Format("%g",m_calibrationIRELevel);

	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COneDeviceSensorPropPage)
	DDX_Control(pDX, IDC_DISPLAYREF_COMBO, m_displayRefCombo);
	DDX_Control(pDX, IDC_WHITEMATRIX_STATIC, m_whiteMatrixStatic);
	DDX_Control(pDX, IDC_PRIMARIESMATRIX_STATIC, m_primariesMatrixStatic);
	DDX_Check(pDX, IDC_BLACK_COMPENSATION_CHECK, m_doBlackCompensation);
	DDX_Text(pDX, IDC_MAX_ADDITIVITY_ERROR_EDIT, m_maxAdditivityErrorPercent);
	DDV_MinMaxUInt(pDX, m_maxAdditivityErrorPercent, 0, 100);
	DDX_Check(pDX, IDC_VERIFY_ADDITIVITY_CHECK, m_doVerifyAdditivity);
	DDX_Check(pDX, IDC_SHOW_ADDITIVITY_CHECK, m_showAdditivityResults);
	DDX_CBString(pDX, IDC_RGBLEVEL_COMBO, m_IRELevelStr);
	//}}AFX_DATA_MAP

	if(pDX->m_bSaveAndValidate == 1)	// 0=init 1=save
		sscanf(m_IRELevelStr,"%lg",&m_calibrationIRELevel);
}


BEGIN_MESSAGE_MAP(COneDeviceSensorPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(COneDeviceSensorPropPage)
	ON_CBN_SELCHANGE(IDC_DISPLAYREF_COMBO, OnSelchangeDisplayRefCombo)
	//}}AFX_MSG_MAP

	ON_NOTIFY(GVN_ENDLABELEDIT, IDC_PRIMARIESMATRIX_GRID, OnPrimariesGridEndEdit)
	ON_NOTIFY(GVN_ENDLABELEDIT, IDC_WHITEMATRIX_GRID, OnWhiteGridEndEdit)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COneDeviceSensorPropPage message handlers

BOOL COneDeviceSensorPropPage::OnInitDialog() 
{
	CPropertyPageWithHelp::OnInitDialog();
	
	// Primaries grid initialisation
	m_pPrimariesGrid = new CGridCtrl;     // Create the primaries Gridctrl object
	if (!m_pPrimariesGrid ) return FALSE;

	CRect rect;
	m_primariesMatrixStatic.GetWindowRect(&rect);	// size control to Static control size
	ScreenToClient(&rect);
	m_pPrimariesGrid->Create(rect, this,IDC_PRIMARIESMATRIX_GRID,WS_CHILD | WS_TABSTOP | WS_VISIBLE);		// Create the Gridctrl window

	m_pPrimariesGrid->SetFixedRowCount(1);
	m_pPrimariesGrid->SetFixedColumnCount(1);

	m_pPrimariesGrid->SetRowCount(4);     // fill it up with stuff
	m_pPrimariesGrid->SetColumnCount(4);

	// Set the font to bold
	CFont* pFont = m_pPrimariesGrid->GetFont();
	LOGFONT lf;
	pFont->GetLogFont(&lf);
	lf.lfWeight=FW_BOLD;

	// set column label
	CString columnLabels[3];
	columnLabels[0].LoadString ( IDS_RED );
	columnLabels[1].LoadString ( IDS_GREEN );
	columnLabels[2].LoadString ( IDS_BLUE );
	char *rowLabels[3]={"x","y","z"};

	GV_ITEM Item;
	Item.mask = GVIF_TEXT|GVIF_FORMAT;
	Item.nFormat = DT_CENTER|DT_WORDBREAK;
	for(int i=0;i<3;i++)
	{
		Item.row = 0;
		Item.col = i+1;
		m_pPrimariesGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold
		Item.strText=columnLabels[i];
		m_pPrimariesGrid->SetItem(&Item);
	}

	for(int i=0;i<3;i++)
	{
		Item.row = i+1;
		Item.col = 0;
		Item.lfFont=lf;
		m_pPrimariesGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold
		Item.strText=rowLabels[i];
		m_pPrimariesGrid->SetItem(&Item);
	}

	m_pPrimariesGrid->SetEditable(FALSE);
	m_pPrimariesGrid->EnableDragAndDrop(FALSE);
	// z row is not editable and is gray
	m_pPrimariesGrid->SetItemState(3,1, m_pPrimariesGrid->GetItemState(2,0) | GVIS_READONLY);
	m_pPrimariesGrid->SetItemFgColour(3,1,RGB(128,128,128));
	m_pPrimariesGrid->SetItemState(3,2, m_pPrimariesGrid->GetItemState(2,1) | GVIS_READONLY);
	m_pPrimariesGrid->SetItemFgColour(3,2,RGB(128,128,128));
	m_pPrimariesGrid->SetItemState(3,3, m_pPrimariesGrid->GetItemState(2,2) | GVIS_READONLY);
	m_pPrimariesGrid->SetItemFgColour(3,3,RGB(128,128,128));


	m_pPrimariesGrid->AutoSizeColumn(0);
	m_pPrimariesGrid->ExpandToFit(FALSE);

	// White grid initialisation
	m_pWhiteGrid = new CGridCtrl;     // Create the white Gridctrl object
	if (!m_pWhiteGrid ) return FALSE;

	m_whiteMatrixStatic.GetWindowRect(&rect);	// size control to Static control size
	ScreenToClient(&rect);
	m_pWhiteGrid->Create(rect, this,IDC_WHITEMATRIX_GRID,WS_CHILD | WS_TABSTOP | WS_VISIBLE);		// Create the Gridctrl window

	m_pWhiteGrid->SetFixedRowCount(0);
	m_pWhiteGrid->SetFixedColumnCount(1);

	m_pWhiteGrid->SetRowCount(3);     // fill it up with stuff
	m_pWhiteGrid->SetColumnCount(2);
	
	// Set labels
	for(int i=0;i<3;i++)
	{
		Item.row = i;
		Item.col = 0;
		m_pWhiteGrid->SetItemFont(Item.row,Item.col, &lf); // Set the font to bold
		Item.strText=rowLabels[i];
		m_pWhiteGrid->SetItem(&Item);
	}

	m_pWhiteGrid->SetEditable(FALSE);
	m_pWhiteGrid->EnableDragAndDrop(FALSE);
	// z row is not editable and is gray
	m_pWhiteGrid->SetItemState(2,1, m_pWhiteGrid->GetItemState(2,0) | GVIS_READONLY);
	m_pWhiteGrid->SetItemFgColour(2,1,RGB(128,128,128));

	m_pWhiteGrid->AutoSizeColumn(0);
	m_pWhiteGrid->ExpandToFit(FALSE);

	GetReferencesList();
	int m_selectedRef=m_displayRefCombo.SelectString(0,m_calibrationReferenceName);
	if(m_selectedRef == CB_ERR)
		m_selectedRef=m_displayRefCombo.SetCurSel(0);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void COneDeviceSensorPropPage::UpdateGrids()
{
	for(int i=0;i<3;i++)
		for(int j=0;j<3;j++)
		{
			CString strValue;
			strValue.Format("%.4f",m_primariesMatrix[i][j]);
			m_pPrimariesGrid->SetItemText(i+1,j+1,strValue);
		}
	m_pPrimariesGrid->Refresh();
	
	for(int i=0;i<3;i++)
	{
		CString strValue;
		strValue.Format("%.4f",m_whiteMatrix[i][0]);
		m_pWhiteGrid->SetItemText(i,1,strValue);
	}
	m_pWhiteGrid->Refresh();
}

BOOL COneDeviceSensorPropPage::OnSetActive() 
{
	UpdateGrids();
	return CPropertyPageWithHelp::OnSetActive();
}

void COneDeviceSensorPropPage::OnOK() 
{
	delete m_pPrimariesGrid;
	m_pPrimariesGrid=NULL;
	delete m_pWhiteGrid;
	m_pWhiteGrid=NULL;

	CPropertyPageWithHelp::OnOK();
}

BOOL COneDeviceSensorPropPage::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
    if (m_pPrimariesGrid && IsWindow(m_pPrimariesGrid->m_hWnd))
        if (m_pPrimariesGrid->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
            return TRUE;

	if (m_pWhiteGrid && IsWindow(m_pWhiteGrid->m_hWnd))
        if (m_pWhiteGrid->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
            return TRUE;
	
	return CPropertyPageWithHelp::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void COneDeviceSensorPropPage::OnPrimariesGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult)
{
    NM_GRIDVIEW* pItem = (NM_GRIDVIEW*) pNotifyStruct;
	int row=pItem->iRow-1;
	int col=pItem->iColumn-1;

	CString aNewStr=m_pPrimariesGrid->GetItemText(pItem->iRow,pItem->iColumn);
	aNewStr.Replace(",",".");	// replace decimal separator if necessary
 	float aVal;
	BOOL bAcceptChange = !aNewStr.IsEmpty() && sscanf(aNewStr,"%f",&aVal);
	if(bAcceptChange)
	{
		m_primariesMatrix[row][col]=aVal;
		CString strValue;
		strValue.Format("%.4f",m_primariesMatrix[row][col]);
		m_pPrimariesGrid->SetItemText(pItem->iRow,pItem->iColumn,strValue);	// reformat value
		// compute new z
		strValue.Format("%.4f",1.0-m_primariesMatrix[0][col]-m_primariesMatrix[1][col]);
		m_pPrimariesGrid->SetItemText(3,pItem->iColumn,strValue);
		m_pPrimariesGrid->Refresh();
	}
}

void COneDeviceSensorPropPage::OnWhiteGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult)
{
    NM_GRIDVIEW* pItem = (NM_GRIDVIEW*) pNotifyStruct;
	int row=pItem->iRow;
	int col=pItem->iColumn-1;

	CString aNewStr=m_pWhiteGrid->GetItemText(pItem->iRow,pItem->iColumn);
	aNewStr.Replace(",",".");	// replace decimal separator if necessary
 	float aVal;
	BOOL bAcceptChange = !aNewStr.IsEmpty() && sscanf(aNewStr,"%f",&aVal);
	if(bAcceptChange)
	{
		m_whiteMatrix[row][col]=aVal;
		CString strValue;
		strValue.Format("%.4f",m_whiteMatrix[row][col]);
		m_pWhiteGrid->SetItemText(pItem->iRow,pItem->iColumn,strValue);	// reformat value
		// compute new z
		strValue.Format("%.4f",1.0-m_whiteMatrix[0][col]-m_whiteMatrix[1][col]);
		m_pWhiteGrid->SetItemText(2,pItem->iColumn,strValue);
		m_pWhiteGrid->Refresh();
	}
}

void COneDeviceSensorPropPage::OnSelchangeDisplayRefCombo() 
{
	int m_selectedRef=m_displayRefCombo.GetCurSel();
	m_displayRefCombo.GetLBText(m_selectedRef,m_calibrationReferenceName);
	LoadReferenceValues();
}

void COneDeviceSensorPropPage::LoadReferenceValues() 
{
	if(m_calibrationReferenceName == "Défaut")
	{
		// Chromacities are the Rec709 ones
		Matrix chromacities(0.0,3,3);
		chromacities(0,0)=0.640;	// Red chromacities
		chromacities(1,0)=0.330;
		chromacities(2,0)=1.0-chromacities(1,0)-chromacities(0,0);

		chromacities(0,1)=0.300;	// Green chromacities
		chromacities(1,1)=0.600;
		chromacities(2,1)=1.0-chromacities(0,1)-chromacities(1,1);

		chromacities(0,2)=0.150;	// Blue chromacities
		chromacities(1,2)=0.060;
		chromacities(2,2)=1.0-chromacities(0,2)-chromacities(1,2);

		// Default white is D65
		Matrix white(0.0,3,1);
		white(0,0)=0.3127;
		white(1,0)=0.3290;
		white(2,0)=1.0-white(0,0)-white(1,0);

		m_primariesMatrix=chromacities;
		m_whiteMatrix=white;
	}
	else
	{
	   FILE *stream;
	   char line[255];

	   if( (stream = fopen( "display.ref", "r" )) != NULL )
	   {
			while( fgets( line, 255, stream ) != NULL)
			{
				BOOL titleFound=FALSE;
				CString refStr;
                int i;
				for(i=0;i<strlen(line);i++)
				{
					if(line[i] == ']')
						break;
					if(titleFound)
						refStr+=line[i];
					if(line[i] == '[')
						titleFound=TRUE;
				}
					
				if(refStr == m_calibrationReferenceName)
				{
					double xRed,yRed,xGreen,yGreen,xBlue,yBlue;
					double xWhite, yWhite;
					if(sscanf(line+i+1," Red: %lf %lf Green: %lf %lf Blue: %lf %lf White: %lf %lf\n",
								  &xRed,&yRed,&xGreen,&yGreen,&xBlue,&yBlue,&xWhite,&yWhite) == 8)
					{
						Matrix chromacities(0.0,3,3);
						chromacities(0,0)=xRed;	// Red chromacities
						chromacities(1,0)=yRed;
						chromacities(2,0)=1.0-chromacities(1,0)-chromacities(0,0);

						chromacities(0,1)=xGreen;	// Green chromacities
						chromacities(1,1)=yGreen;
						chromacities(2,1)=1.0-chromacities(0,1)-chromacities(1,1);

						chromacities(0,2)=xBlue;	// Blue chromacities
						chromacities(1,2)=yBlue;
						chromacities(2,2)=1.0-chromacities(0,2)-chromacities(1,2);

						Matrix white(0.0,3,1);
						white(0,0)=xWhite;
						white(1,0)=yWhite;
						white(2,0)=1.0-white(0,0)-white(1,0);

						m_primariesMatrix=chromacities;
						m_whiteMatrix=white;
					}
				}
			}
			fclose( stream );
	   }
	}

/*	if(m_calibrationReferenceName == "HS10")
	{
		// Chromacities are the HS10 ones
		Matrix chromacities(0.0,3,3);
		chromacities(0,0)=0.653571817;	// Red chromacities
		chromacities(1,0)=0.337065899;
		chromacities(2,0)=1.0-chromacities(1,0)-chromacities(0,0);

		chromacities(0,1)=0.31687167;	// Green chromacities
		chromacities(1,1)=0.658229427;
		chromacities(2,1)=1.0-chromacities(0,1)-chromacities(1,1);

		chromacities(0,2)=0.184219711;	// Blue chromacities
		chromacities(1,2)=0.133853381;
		chromacities(2,2)=1.0-chromacities(0,2)-chromacities(1,2);

		Matrix white(0.0,3,1);
		white(0,0)=0.3127;
		white(1,0)=0.329;
		white(2,0)=1.0-white(0,0)-white(1,0);

		m_primariesMatrix=chromacities;
		m_whiteMatrix=white;
	}
*/
	if(m_calibrationReferenceName == "Personnalisé")
	{
		m_pPrimariesGrid->SetEditable(TRUE);
		m_pPrimariesGrid->EnableDragAndDrop(TRUE);
		m_pWhiteGrid->SetEditable(TRUE);
		m_pWhiteGrid->EnableDragAndDrop(TRUE);
	}
	else
	{
		m_pPrimariesGrid->SetEditable(FALSE);
		m_pPrimariesGrid->EnableDragAndDrop(FALSE);
		m_pWhiteGrid->SetEditable(FALSE);
		m_pWhiteGrid->EnableDragAndDrop(FALSE);
	}

	UpdateGrids();
}

void COneDeviceSensorPropPage::GetReferencesList() 
{
	m_displayRefCombo.ResetContent();
	m_displayRefCombo.AddString("Défaut");
	m_displayRefCombo.AddString("Personnalisé");

   FILE *stream;
   char line[255];

   if( (stream = fopen( "display.ref", "r" )) != NULL )
   {
		while( fgets( line, 255, stream ) != NULL)
		{
			BOOL titleFound=FALSE;
			CString refStr;
			for(int i=0;i<strlen(line);i++)
			{
				if(line[i] == ']')
					break;
				if(titleFound)
					refStr+=line[i];
				if(line[i] == '[')
					titleFound=TRUE;
			}
				
			if(!refStr.IsEmpty())
				m_displayRefCombo.AddString(refStr);
		}
		fclose( stream );
   }
}

UINT COneDeviceSensorPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SENSOR_CALIBRATE;
}
