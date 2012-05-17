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

// NewDocPropertyPages.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "ColorHCFR.h"
#include "NewDocPropertyPages.h"
#include "ArgyllMeterWrapper.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNCREATE(CSensorSelectionPropPage, CPropertyPageWithHelp)
IMPLEMENT_DYNCREATE(CGeneratorSelectionPropPage, CPropertyPageWithHelp)

/////////////////////////////////////////////////////////////////////////////
// CGeneratorSelectionPropPage property page

CGeneratorSelectionPropPage::CGeneratorSelectionPropPage() : CPropertyPageWithHelp(CGeneratorSelectionPropPage::IDD)
{
	//{{AFX_DATA_INIT(CGeneratorSelectionPropPage)
	m_generatorChoice = _T("");
	//}}AFX_DATA_INIT

	// Init selection with previoulsy stored value
	CString	Msg;
	Msg.LoadString ( IDS_GDIGENERATOR_NAME );
	m_generatorChoice = GetConfig()->GetProfileString("Defaults","Generator",(LPCSTR)Msg);
}

CGeneratorSelectionPropPage::~CGeneratorSelectionPropPage()
{
	// save current selection in the registry to be the next default value
	 GetConfig()->WriteProfileString("Defaults","Generator",m_generatorChoice);
}

void CGeneratorSelectionPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGeneratorSelectionPropPage)
	DDX_Control(pDX, IDC_GENERATORCHOICE_COMBO, m_generatorChoiceCtrl);
	DDX_CBString(pDX, IDC_GENERATORCHOICE_COMBO, m_generatorChoice);
	//}}AFX_DATA_MAP

	if(pDX->m_bSaveAndValidate != 0)	// 0=init 1=save
		m_currentID=m_generatorChoiceCtrl.GetCurSel();
	else
	{
		// In case of language changing, the default name can be incorrect
		if ( m_generatorChoiceCtrl.GetCurSel() < 0 )
			m_generatorChoiceCtrl.SetCurSel(0);
	}
}


BEGIN_MESSAGE_MAP(CGeneratorSelectionPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CGeneratorSelectionPropPage)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



BOOL CGeneratorSelectionPropPage::OnSetActive() 
{
	CPropertySheetWithHelp * psheet = (CPropertySheetWithHelp*) GetParent();   
	psheet->SetWizardButtons(PSWIZB_NEXT);

	return CPropertyPageWithHelp::OnSetActive();
}

BOOL CGeneratorSelectionPropPage::OnWizardFinish() 
{
	UpdateData(TRUE);	// To update string
	return CPropertyPageWithHelp::OnWizardFinish();
}

UINT CGeneratorSelectionPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SELECT_GENERATOR;
}

/////////////////////////////////////////////////////////////////////////////
// CSensorSelectionPropPage property page

CSensorSelectionPropPage::CSensorSelectionPropPage() : CPropertyPageWithHelp(CSensorSelectionPropPage::IDD)
{
	//{{AFX_DATA_INIT(CSensorSelectionPropPage)
	m_sensorChoice = _T("");
	m_trainingFileName = _T("");
	m_sensorTrainingMode = -1;
	//}}AFX_DATA_INIT

	// Init selection with previoulsy stored value
	m_sensorChoice = GetConfig()->GetProfileString("Defaults","Sensor","");
	m_trainingFileName = GetConfig()->GetProfileString("Defaults","Training FileName","");
	m_sensorTrainingMode = GetConfig()->GetProfileInt("Defaults","Training Mode",1);
}

CSensorSelectionPropPage::~CSensorSelectionPropPage()
{
	// save current selection in the registry to be the next default value
	GetConfig()->WriteProfileString("Defaults","Sensor",m_sensorChoice);
	GetConfig()->WriteProfileString("Defaults","Training FileName",m_trainingFileName);
	GetConfig()->WriteProfileInt("Defaults","Training Mode",m_sensorTrainingMode);
}

void CSensorSelectionPropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SENSORTRAININGFILE_COMBO, m_trainingFileCombo);
	DDX_Control(pDX, IDC_SENSORCHOICE_COMBO, m_sensorChoiceCtrl);
    if(m_sensorChoiceCtrl.GetCount() == 0)
    {
        std::string errorMessage;
        ArgyllMeterWrapper::ArgyllMeterWrappers argyllMeters = ArgyllMeterWrapper::getDetectedMeters(errorMessage);
        if(!errorMessage.empty())
        {
            MessageBox(errorMessage.c_str(), "Argyll Error", MB_OK | MB_ICONEXCLAMATION);
        }
        for(size_t i(0); i < argyllMeters.size(); ++i)
        {
            AddSensor(argyllMeters[i]->getMeterName().c_str(), (int)argyllMeters[i]);
        }
        AddSensor(_T("Simulated sensor"), 1);
        AddSensor(_T("DTP-94"), 3);
        AddSensor(_T("HCFR Sensor"), 0);
#ifdef USE_NON_FREE_CODE
        AddSensor(_T("Spyder II"), 2);
        AddSensor(_T("Eye One"), 4);
        AddSensor(_T("Mazet MTCS-C2"), 5);
        AddSensor(_T("Spyder 3"), 6);
#endif
    }

	DDX_CBString(pDX, IDC_SENSORCHOICE_COMBO, m_sensorChoice);
	DDX_Radio(pDX, IDC_SENSORTRAININGMODE_RADIO1, m_sensorTrainingMode);

	if(pDX->m_bSaveAndValidate != 0)	// 0=init 1=save
		m_currentID=m_sensorChoiceCtrl.GetItemData(m_sensorChoiceCtrl.GetCurSel());
	else
	{
		// In case of language changing, the default name can be incorrect
		if ( m_sensorChoiceCtrl.GetCurSel() < 0 )
			m_sensorChoiceCtrl.SetCurSel(0);
	}
}


BEGIN_MESSAGE_MAP(CSensorSelectionPropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CSensorSelectionPropPage)
	ON_CBN_SELCHANGE(IDC_SENSORCHOICE_COMBO, OnSelchangeSensorchoiceCombo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CSensorSelectionPropPage::OnSetActive() 
{
	CPropertySheetWithHelp* psheet = (CPropertySheetWithHelp*) GetParent();   
	psheet->SetWizardButtons(PSWIZB_BACK |PSWIZB_FINISH);

	BOOL bRet = CPropertyPageWithHelp::OnSetActive();
	
	OnSelchangeSensorchoiceCombo();
	
	return bRet;
}

void CSensorSelectionPropPage::OnOK() 
{
	OnWizardFinish();
}

void CSensorSelectionPropPage::AddSensor(LPCTSTR name, int id)
{
    int index = m_sensorChoiceCtrl.AddString(name);
    m_sensorChoiceCtrl.SetItemData(index, id);
}

BOOL CSensorSelectionPropPage::OnInitDialog()
{
    BOOL result = CPropertyPageWithHelp::OnInitDialog();
    return result;
}


BOOL CSensorSelectionPropPage::OnWizardFinish() 
{
	int				nSel;
	CString			strPath;
	CString			strSubDir;
	CString			strFileName;

	UpdateData(TRUE);	// To update values

	strPath = GetConfig () -> m_ApplicationPath;
	strSubDir = GetFileSubDir ();
	if ( ! strSubDir.IsEmpty () )
	{
		strPath += strSubDir;
		strPath += '\\';
	}
	
	nSel = m_trainingFileCombo.GetCurSel ();
	if ( nSel >= 0 )
	{
		m_trainingFileCombo.GetLBText ( nSel, strFileName );
		m_trainingFileName = strPath + strFileName + ".thc";
	}
	else
		m_trainingFileName.Empty ();

	return CPropertyPageWithHelp::OnWizardFinish();
}

UINT CSensorSelectionPropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_SELECT_SENSOR;
}

LPCSTR CSensorSelectionPropPage::GetThcFileSubDir ( int nSensorID ) 
{
	switch ( nSensorID ) 
	{ 
		case 0: 
			 return "Etalon_HCFR"; 

		case 1:
			 return "Etalon_Simulation";

		case 2:
			 return "Etalon_S2";

		case 3: 
			 return "Etalon_DTP94";

		case 4: 
			 return "Etalon_I1";

		case 5: 
			 return "Etalon_MTCS";

		case 6:
			 return "Etalon_S3";

		default:
            return "Etalon_Argyll";
	} 
}

void CSensorSelectionPropPage::OnSelchangeSensorchoiceCombo() 
{
	int				i, nSel = -1;
	BOOL			bFileFound;
	char			szBuf [ 256 ];
	LPSTR			lpStr;
	CString			str;
	CString			strPath;
	CString			strSubDir;
	CString			strSearch;
	CString			strFileName;
	HANDLE			hFind;
	WIN32_FIND_DATA	wfd;

	UpdateData(TRUE);	// To update values
	
	m_trainingFileCombo.ResetContent ();

	strPath = GetConfig () -> m_ApplicationPath;
	strSubDir = GetFileSubDir ();
	
	if ( ! strSubDir.IsEmpty () )
	{
		if ( m_currentID < 2)
		{
			// Sensor needing calibration file
			str.LoadString ( IDS_CREATE_CALIBRATION_FILE );
		}
		else
		{
			// Sensor accepting calibration file
			CheckRadioButton ( IDC_SENSORTRAININGMODE_RADIO2, IDC_SENSORTRAININGMODE_RADIO1, IDC_SENSORTRAININGMODE_RADIO2 );
			str.LoadString ( IDS_DONOTUSE_CALIBRATION );
		}

		GetDlgItem (IDC_SENSORTRAININGMODE_RADIO2) -> SetWindowText ( str );
		
		GetDlgItem (IDC_SENSORTRAININGMODE_RADIO1) -> EnableWindow ( TRUE );
		GetDlgItem (IDC_SENSORTRAININGMODE_RADIO2) -> EnableWindow ( TRUE );
		m_trainingFileCombo.EnableWindow ( TRUE );

		strPath += strSubDir;
		strPath += '\\';
	
		GetConfig () -> EnsurePathExists ( strPath );

		bFileFound = FALSE;
		strSearch = strPath + "*.thc";
			
		hFind = FindFirstFile ( (LPCSTR) strSearch, & wfd );
		if ( hFind != INVALID_HANDLE_VALUE )
		{
			do
			{
				strcpy ( szBuf, wfd.cFileName );
				lpStr = strrchr ( szBuf, '.' );
				if ( lpStr )
					lpStr [ 0 ] = '\0';
				i = m_trainingFileCombo.AddString ( szBuf );
				strFileName = strPath + wfd.cFileName;
				if ( strFileName == m_trainingFileName )
					nSel = i;

				bFileFound = TRUE;
			} while ( FindNextFile ( hFind, & wfd ) );

			FindClose ( hFind );
		}
		m_trainingFileCombo.SetCurSel ( nSel >= 0 ? nSel : 0 );

		if ( ! bFileFound && m_currentID >= 2 && m_currentID < 7)
		{
			// No calibration file
			GetDlgItem (IDC_SENSORTRAININGMODE_RADIO1) -> EnableWindow ( FALSE );
			GetDlgItem (IDC_SENSORTRAININGMODE_RADIO2) -> EnableWindow ( FALSE );
			m_trainingFileCombo.EnableWindow ( FALSE );
		}
	}
	else
	{
		str.LoadString ( IDS_DONOTUSE_CALIBRATION );
		GetDlgItem (IDC_SENSORTRAININGMODE_RADIO2) -> SetWindowText ( str );
		CheckRadioButton ( IDC_SENSORTRAININGMODE_RADIO2, IDC_SENSORTRAININGMODE_RADIO1, IDC_SENSORTRAININGMODE_RADIO2 );
		GetDlgItem (IDC_SENSORTRAININGMODE_RADIO1) -> EnableWindow ( FALSE );
		GetDlgItem (IDC_SENSORTRAININGMODE_RADIO2) -> EnableWindow ( FALSE );
		m_trainingFileCombo.SetCurSel ( -1 );
		m_trainingFileCombo.EnableWindow ( FALSE );
	}
}

