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

#if !defined(AFX_GENERALPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_)
#define AFX_GENERALPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GeneralPropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGeneralPropPage dialog

class CGeneralPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CGeneralPropPage)

// Construction
public:
	CGeneralPropPage();
	~CGeneralPropPage();

// Dialog Data
	BOOL	m_isModified;

	//{{AFX_DATA(CGeneralPropPage)
	enum { IDD = IDD_GENERAL_PROP_PAGE };
	BOOL	m_doMultipleInstance;
	BOOL	m_doUpdateCheck;
	BOOL	m_bDisplayTestColors;
	int		m_latencyTime;
	int		m_ablFreq;
	int		m_ablDuration;
	int		m_ablLevel;
	BOOL	m_doSavePosition;
	BOOL	m_bContinuousMeasures;
	BOOL	m_bLatencyBeep;
	BOOL	bDisplayRT;
	BOOL	m_bABL;
	int		m_BWColorsToAdd;
	BOOL	m_bDetectPrimaries;
	BOOL	m_useHSV;
	BOOL	m_isSettling;
	BOOL	m_bUseRoundDown;
	CEdit	m_bUseRoundDownCtrl;
	BOOL	m_bUse10bit;
	CEdit	m_bUse10bitCtrl;
	BOOL	m_bDisableHighDPI;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CGeneralPropPage)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CGeneralPropPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	afx_msg void OnControlClicked(UINT nID);
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GENERALPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_)
