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
//	Ian C
/////////////////////////////////////////////////////////////////////////////

#if !defined(SPECTRAL_SAMPLE_DLG_INCLUDED_)
#define SPECTRAL_SAMPLE_DLG_INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


// CSpectralSampleDlg dialog

class CSpectralSampleDlg : public CDialog
{
	DECLARE_DYNAMIC(CSpectralSampleDlg)

public:
	CSpectralSampleDlg(CString displayName, CWnd* pParent = NULL);   // standard constructor
	virtual ~CSpectralSampleDlg();

// Dialog Data
	enum { IDD = IDD_GENERATE_SPECTRAL_SAMPLE };

	CString m_displayName;
	CString m_displayTech;
	CEdit m_displayNameCtrl;
	CEdit m_displayTechCtrl;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()
};

#endif