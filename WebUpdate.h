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

// WebUpdate.h: interface for the CWebUpdate class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WEBUPDATE_H__2D2AEE79_02C1_4015_A6F4_952DBA68B929__INCLUDED_)
#define AFX_WEBUPDATE_H__2D2AEE79_02C1_4015_A6F4_952DBA68B929__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "afxinet.h"

class CWebUpdate  
{
public:
	CWebUpdate();
	virtual ~CWebUpdate();

	BOOL Connect();
	void Disconnect();

	CString CheckNewSoft();
	BOOL TransferFile(CString strFtpFile, CString strLocalFile);
	BOOL TransferCalibrationFiles(HWND hInfoWnd);
	BOOL TransferIRProfiles(HWND hInfoWnd);

protected:
	CFtpConnection* m_pFtpConnection;
	CInternetSession* m_pInetSession;
};

#endif // !defined(AFX_WEBUPDATE_H__2D2AEE79_02C1_4015_A6F4_952DBA68B929__INCLUDED_)
