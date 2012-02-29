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
//	Laurent GARNIER
/////////////////////////////////////////////////////////////////////////////

// WebUpdate.cpp: implementation of the CWebUpdate class.
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "WebUpdate.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#define	FTP_SERVERNAME		"hcfr.sourceforge.net"
#define	FTP_PORT			21
#define	FTP_USERNAME		NULL
#define	FTP_PASSWORD		NULL
#define	FTP_DIR_SOFT		"/"
#define	FTP_DIR_ETALONS		"/fichiers_etalons/"
#define	FTP_DIR_IRPROFILES	"/fichiers_ir/"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CWebUpdate::CWebUpdate()
{
	m_pInetSession = NULL;
	m_pFtpConnection = NULL;
}

CWebUpdate::~CWebUpdate()
{
	Disconnect();
}

//////////////////////////////////////////////////////////////////////

BOOL CWebUpdate::Connect()
{
	CWaitCursor cursor; // this will automatically display a wait cursor

	Disconnect();

	m_pInetSession = new CInternetSession(AfxGetAppName(), 1, PRE_CONFIG_INTERNET_ACCESS);
	if (m_pInetSession == NULL)
	{
		return FALSE;
	}

	try
	{
		m_pFtpConnection = m_pInetSession->GetFtpConnection(FTP_SERVERNAME, FTP_USERNAME, FTP_PASSWORD, FTP_PORT);
	}
	catch (CInternetException* pEx)
	{
		// catch errors from WinINet
		pEx->Delete();
		m_pFtpConnection = NULL;
		m_pInetSession->Close();
		delete m_pInetSession;
		m_pInetSession = NULL;
		return FALSE;
	}

	return TRUE;
}

void CWebUpdate::Disconnect()
{
	if (m_pFtpConnection != NULL)
	{
		m_pFtpConnection->Close();
		delete m_pFtpConnection;
		m_pFtpConnection = NULL;
	}
	if (m_pInetSession != NULL)
	{
		m_pInetSession->Close();
		delete m_pInetSession;
		m_pInetSession = NULL;
	}
}

CString CWebUpdate::CheckNewSoft()
{
	CString strSearchDir;
	CString strFileName;
	CString strCurrentFile;
	CString strNewFile;
	CString strNewSoftFile;
	CString str;
	BOOL bContinue;
	char szVersion[32];

	strNewSoftFile.Empty();

	if (m_pFtpConnection == NULL)
	{
		return strNewSoftFile;
	}

	CWaitCursor cursor; // this will automatically display a wait cursor

	strSearchDir = FTP_DIR_SOFT;
	strSearchDir += "Setup_v*.exe";

	CFtpFileFind ftpFind(m_pFtpConnection);

	bContinue = ftpFind.FindFile(strSearchDir);
	if (!bContinue)
	{
		return strNewSoftFile;
	}

	LoadString(GetModuleHandle(NULL), IDS_STRING_VERSION_WEB, szVersion, sizeof(szVersion));
	strCurrentFile = "Setup_v";
	strCurrentFile += szVersion;
	strCurrentFile += ".exe";

	strNewFile = strCurrentFile;
	while (bContinue)
	{
		// FindNextFile muxt be called before info can be gleaned from ftpFind
		bContinue = ftpFind.FindNextFile();
		strFileName = ftpFind.GetFileName();
		str = strFileName.Mid(7, 1);
		if ((str != "_") && (strFileName > strNewFile))
		{
			strNewFile = strFileName;
		}
	}

	ftpFind.Close();

	if (strNewFile != strCurrentFile)
	{
		strNewSoftFile = FTP_DIR_SOFT;
		strNewSoftFile += strNewFile;
	}

	return strNewSoftFile;
}

BOOL CWebUpdate::TransferFile(CString strFtpFile, CString strLocalFile)
{
	if (m_pFtpConnection == NULL)
	{
		return FALSE;
	}

	CWaitCursor cursor; // this will automatically display a wait cursor

	return m_pFtpConnection->GetFile(strFtpFile,
									 strLocalFile,
									 FALSE,
									 FILE_ATTRIBUTE_NORMAL,
									 FTP_TRANSFER_TYPE_BINARY);
}

BOOL CWebUpdate::TransferCalibrationFiles(HWND hInfoWnd)
{
	CString strSearchDir;
	CString strFileName;
	CString strFile;
	CString strLocalDir;
	CString strLocalFile;
	BOOL bContinue;
	BOOL bResult = FALSE;
	char	szBuf [ 512 ];

	if (m_pFtpConnection == NULL)
	{
		return FALSE;
	}

	CWaitCursor cursor; // this will automatically display a wait cursor

	strSearchDir = FTP_DIR_ETALONS;
	strSearchDir += "*.thc";

	strLocalDir = GetConfig()->m_ApplicationPath;
	strLocalDir += "Etalon_HCFR\\";
	CreateDirectory(strLocalDir, NULL);

	CFtpFileFind ftpFind(m_pFtpConnection);

	bContinue = ftpFind.FindFile(strSearchDir);
	while (bContinue)
	{
		// FindNextFile muxt be called before info can be gleaned from ftpFind
		bContinue = ftpFind.FindNextFile();
		strFileName = ftpFind.GetFileName();
		strFile = FTP_DIR_ETALONS;
		strFile += strFileName;
		strLocalFile = strLocalDir;
		strLocalFile += strFileName;

		sprintf ( szBuf, "Loading %s...", (LPCSTR) strFileName );
		::SetWindowText ( hInfoWnd, szBuf );

		bResult = m_pFtpConnection->GetFile(strFile,
											strLocalFile,
											FALSE,
											FILE_ATTRIBUTE_NORMAL,
											FTP_TRANSFER_TYPE_BINARY);
		if (bResult == FALSE)
		{
			bContinue = FALSE;
		}
	}

	ftpFind.Close();

	return bResult;
}

BOOL CWebUpdate::TransferIRProfiles(HWND hInfoWnd)
{
	CString strSearchDir;
	CString strFileName;
	CString strFile;
	CString strLocalDir;
	CString strLocalFile;
	BOOL bContinue;
	BOOL bResult = FALSE;
	char	szBuf [ 512 ];

	if (m_pFtpConnection == NULL)
	{
		return FALSE;
	}

	CWaitCursor cursor; // this will automatically display a wait cursor

	strSearchDir = FTP_DIR_IRPROFILES;
	strSearchDir += "*.ihc";

	strLocalDir = GetConfig()->m_ApplicationPath;
	strLocalDir += "Profils_IR\\";
	CreateDirectory(strLocalDir, NULL);

	CFtpFileFind ftpFind(m_pFtpConnection);

	bContinue = ftpFind.FindFile(strSearchDir);
	while (bContinue)
	{
		// FindNextFile muxt be called before info can be gleaned from ftpFind
		bContinue = ftpFind.FindNextFile();
		strFileName = ftpFind.GetFileName();
		strFile = FTP_DIR_IRPROFILES;
		strFile += strFileName;
		strLocalFile = strLocalDir;
		strLocalFile += strFileName;

		sprintf ( szBuf, "Loading %s...", (LPCSTR) strFileName );
		::SetWindowText ( hInfoWnd, szBuf );

		bResult = m_pFtpConnection->GetFile(strFile,
											strLocalFile,
											FALSE,
											FILE_ATTRIBUTE_NORMAL,
											FTP_TRANSFER_TYPE_BINARY);
		if (bResult == FALSE)
		{
			bContinue = FALSE;
		}
	}

	ftpFind.Close();

	return bResult;
}
