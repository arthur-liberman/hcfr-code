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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// KiGenePropPage.cpp : implementation file
//

#include "stdafx.h"
#include "ColorHCFR.h"
#include "KiGenePropPage.h"
#include "KiGenerator.h"
#include "IRProfile.h"
#include <io.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKiGenePropPage property page

IMPLEMENT_DYNCREATE(CKiGenePropPage, CPropertyPageWithHelp)

CKiGenePropPage::CKiGenePropPage() : CPropertyPageWithHelp(CKiGenePropPage::IDD)
{
	//{{AFX_DATA_INIT(CKiGenePropPage)
	m_showDialog = FALSE;
	m_comPort = _T("USB");
	m_patternCheckOnGrayscale = FALSE;
	m_patternCheckOnPrimaries = FALSE;
	m_patternCheckOnSaturations = FALSE;
	m_patternCheckOnSecondaries = FALSE;
	m_patternCheckMaxRetryBySeries = 0;
	m_patternCheckMaxRetryByPattern = 0;
	m_patternCheckMaxThreshold = 0;
	//}}AFX_DATA_INIT
	m_IRProfileFileIsModified = FALSE;
	m_IRProfileFileName = "";
	m_pGenerator = NULL;
}

CKiGenePropPage::~CKiGenePropPage()
{
}

void CKiGenePropPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPageWithHelp::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKiGenePropPage)
	DDX_Control(pDX, IDC_IRPROFILE_LIST, m_IRProfileList);
	DDX_Check(pDX, IDC_SHOWDIALOG_CHECK, m_showDialog);
	DDX_CBString(pDX, IDC_KISENSOR_COM_COMBO, m_comPort);
	DDV_MaxChars(pDX, m_comPort, 5);
	DDX_Check(pDX, IDC_PATTERN_CHECK_ON_GRAYSCALE_CHECK, m_patternCheckOnGrayscale);
	DDX_Check(pDX, IDC_PATTERN_CHECK_ON_PRIMARIES_CHECK, m_patternCheckOnPrimaries);
	DDX_Check(pDX, IDC_PATTERN_CHECK_ON_SATURATION_CHECK, m_patternCheckOnSaturations);
	DDX_Check(pDX, IDC_PATTERN_CHECK_ON_SECONDARIES_CHECK, m_patternCheckOnSecondaries);
	DDX_Text(pDX, IDC_EDIT_PATTERN_CHECK_MAX_RETRY_BY_SERIES, m_patternCheckMaxRetryBySeries);
	DDV_MinMaxUInt(pDX, m_patternCheckMaxRetryBySeries, 0, 10);
	DDX_Text(pDX, IDC_EDIT_PATTERN_CHECK_MAX_RETRY_BY_PATTERN, m_patternCheckMaxRetryByPattern);
	DDV_MinMaxUInt(pDX, m_patternCheckMaxRetryByPattern, 0, 3);
	DDX_Text(pDX, IDC_EDIT_PATTERN_CHECK_THRESHOLD, m_patternCheckMaxThreshold);
	DDV_MinMaxUInt(pDX, m_patternCheckMaxThreshold, 0, 100);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKiGenePropPage, CPropertyPageWithHelp)
	//{{AFX_MSG_MAP(CKiGenePropPage)
	ON_BN_CLICKED(IDC_NEWIRPROFILE, OnNewIRProfile)
	ON_BN_CLICKED(IDC_MODIFYIRPROFILE, OnModifyIRProfile)
	ON_LBN_SELCHANGE(IDC_IRPROFILE_LIST, OnSelchangeIRProfileList)
	ON_BN_CLICKED(IDC_DELETEIRPROFILE, OnDeleteIRProfile)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

UINT CKiGenePropPage::GetHelpId ( LPSTR lpszTopic )
{
	return HID_GENERATOR_KI;
}

BOOL CKiGenePropPage::OnInitDialog() 
{
	CPropertyPageWithHelp::OnInitDialog();
	
	// TODO: Add extra initialization here

	m_IRProfileFileIsModified = FALSE;

	int			i, nSel = -1;
	char			szBuf [ 256 ];
	LPSTR			lpStr;
	CString			strPath;
	CString			strSubDir;
	CString			strSearch;
	CString			strFileName;
	HANDLE			hFind;
	WIN32_FIND_DATA		wfd;

	UpdateData(TRUE);	// To update values
 	m_IRProfileList.ResetContent ();

	strPath = GetConfig () -> m_ApplicationPath;
	strSubDir = CIRProfile::GetIhcFileSubDir ();
	if ( ! strSubDir.IsEmpty () )
	{
		strPath += strSubDir;
		strPath += '\\';
	}
	GetConfig () -> EnsurePathExists ( strPath );
	strSearch = strPath + "*.ihc";
	hFind = FindFirstFile ( (LPCSTR) strSearch, & wfd );

	if ( hFind != INVALID_HANDLE_VALUE )
	{
		do
		{
			strcpy ( szBuf, wfd.cFileName );
			lpStr = strrchr ( szBuf, '.' );
			if ( lpStr )
				lpStr [ 0 ] = '\0';
			i = m_IRProfileList.AddString ( szBuf );
			strFileName = strPath + wfd.cFileName;
			if ( strFileName == m_IRProfileFileName )
				nSel = i;
		} while ( FindNextFile ( hFind, & wfd ) );

		FindClose ( hFind );
	}

	if (nSel >= 0)
		m_IRProfileList.SetCurSel ( nSel );

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CKiGenePropPage::OnNewIRProfile() 
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	m_IRProfilePropPage.m_comPort = m_comPort;
	m_IRProfilePropPage.m_pGenerator = m_pGenerator;
	
	m_IRProfilePropPage.m_Name = "";
	int result=m_IRProfilePropPage.DoModal();
	if ((result == IDOK) && (m_IRProfilePropPage.m_Name != ""))
	{
		m_IRProfileFileName = m_IRProfilePropPage.m_fileName;
		OnInitDialog();
		m_IRProfileFileIsModified = TRUE;
	}
}

void CKiGenePropPage::OnModifyIRProfile() 
{
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);
	m_IRProfilePropPage.m_comPort = m_comPort;
	m_IRProfilePropPage.m_pGenerator = m_pGenerator;
	
	int nindex = m_IRProfileList.GetCurSel();
	if (nindex != LB_ERR )
	{
		m_IRProfileList.GetText(nindex,m_IRProfilePropPage.m_Name);
		m_IRProfilePropPage.m_bUpdateExisting = TRUE;
	}
	else
		m_IRProfilePropPage.m_Name = "";
	
	int result=m_IRProfilePropPage.DoModal();
	if ((result == IDOK) && (m_IRProfilePropPage.m_Name != ""))
	{
		m_IRProfileFileName = m_IRProfilePropPage.m_fileName;
		OnInitDialog();
		m_IRProfileFileIsModified = TRUE;
	}
	m_IRProfilePropPage.m_bUpdateExisting = FALSE;
}

void CKiGenePropPage::OnSelchangeIRProfileList() 
{
	// TODO: Add your control notification handler code here
	CString fileName;
	int nindex = m_IRProfileList.GetCurSel();

	if (nindex != LB_ERR )
	{
		m_IRProfileList.GetText(nindex,fileName);
		CString str = GetConfig () -> m_ApplicationPath;
		str += CIRProfile::GetIhcFileSubDir();
		GetConfig () -> EnsurePathExists ( str );
		str += '\\';
		str += fileName;
		str +=".ihc";
		m_IRProfileFileName = str;
	}
}

void CKiGenePropPage::OnDeleteIRProfile() 
{
	CString str,str2,ProfileName;
	int nindex = m_IRProfileList.GetCurSel();
	if (nindex != LB_ERR )
	{
		str2.LoadString (IDS_IRDELETEPROFILE);
		m_IRProfileList.GetText(nindex,ProfileName);

		str.Format (str2,ProfileName);
		if (AfxMessageBox(str, MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			CString FileName;
			str = GetConfig () -> m_ApplicationPath;
			str += CIRProfile::GetIhcFileSubDir();
			GetConfig () -> EnsurePathExists ( str );
			str += '\\';
			str += ProfileName;
			str +=".ihc";
			FileName = str;
			if((_access( FileName, 0 )) != -1 )
				CFile::Remove(FileName);
		}
		OnInitDialog();
	}
	else
	{
		AfxMessageBox(IDS_NOIRPROFILESELECTED,MB_ICONERROR | MB_OK);
	}
}
