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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_SpyderIISensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_)
#define AFX_SpyderIISensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SpyderIISensorPropPage.h : header file
//

class CSpyderIISensor;

/////////////////////////////////////////////////////////////////////////////
// CSpyderIISensorPropPage dialog

class CSpyderIISensorPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CSpyderIISensorPropPage)
 
// Construction
public:
	CSpyderIISensorPropPage();
	~CSpyderIISensorPropPage();

// Dialog Data
	//{{AFX_DATA(CSpyderIISensorPropPage)
	enum { IDD = IDD_S2SENSOR_PROP_PAGE };
	int		m_CalibrationMode;
	UINT	m_ReadTime;
	BOOL	m_bAdjustTime;
	BOOL	m_bAverageReads;
	BOOL	m_debugMode;
	//}}AFX_DATA

	CSpyderIISensor *	m_pSensor;

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSpyderIISensorPropPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSpyderIISensorPropPage)
	afx_msg void OnSpyderIIsensorGetversion();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SpyderIISensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_)
