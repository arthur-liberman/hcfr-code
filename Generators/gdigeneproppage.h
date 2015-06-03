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

#if !defined(AFX_GDIGENEPROPPAGE_H__761EFDB2_CC92_492A_8393_6C6049498029__INCLUDED_)
#define AFX_GDIGENEPROPPAGE_H__761EFDB2_CC92_492A_8393_6C6049498029__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GDIGenePropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGDIGenePropPage dialog

class CGDIGenePropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CGDIGenePropPage)

// Construction
public:
	CGDIGenePropPage();
	~CGDIGenePropPage();

// Dialog Data
	//{{AFX_DATA(CGDIGenePropPage)
	enum { IDD = IDD_GENERATOR_GDI_PROP_PAGE };
	CComboBox	m_monitorComboCtrl;
	UINT	m_rectSizePercent;
	UINT	m_bgStimPercent;
	UINT	m_Intensity;
	//}}AFX_DATA

	CArray <CString,CString> m_monitorNameArray;
    CEdit m_madVREdit;
    CEdit m_madVREdit2;
    CEdit m_madVREdit3;
    CEdit m_usePicEdit;
	int m_activeMonitorNum;
	int	m_nDisplayMode;
	BOOL m_b16_235;
	BOOL m_busePic;
	BOOL m_bdispTrip;
    BOOL m_madVR_3d;
    BOOL m_madVR_vLUT;
	BOOL m_madVR_OSD;

	HMONITOR	m_monitorHandle [ 16 ];

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CGDIGenePropPage)
	public:
	virtual void OnOK();
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CGDIGenePropPage)
	afx_msg void OnTestOverlay();
	afx_msg void OnClickmadVR();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GDIGENEPROPPAGE_H__761EFDB2_CC92_492A_8393_6C6049498029__INCLUDED_)
