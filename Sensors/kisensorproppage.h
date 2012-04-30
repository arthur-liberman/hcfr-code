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

#if !defined(AFX_KiSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_)
#define AFX_KiSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// KiSensorPropPage.h : header file
//

class CKiSensor;

/////////////////////////////////////////////////////////////////////////////
// CKiSensorPropPage dialog

class CKiSensorPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CKiSensorPropPage)
 
// Construction
public:
	CKiSensorPropPage();
	virtual ~CKiSensorPropPage();

// Dialog Data
	//{{AFX_DATA(CKiSensorPropPage)
	enum { IDD = IDD_KISENSOR_PROP_PAGE };
	CString	m_comPort;
	BOOL	m_debugMode;
	int		m_timeoutMesure;
	BOOL	m_bMeasureWhite;
	int		m_nSensorsUsed;
	int		m_nInterlaceMode;
	BOOL	m_bFastMeasure;
	//}}AFX_DATA

	CKiSensor *	m_pSensor;

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CKiSensorPropPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CKiSensorPropPage)
	afx_msg void OnKisensorGetversion();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KiSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_)
