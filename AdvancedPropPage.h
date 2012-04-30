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

#if !defined(AFX_ADVANCEDPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_)
#define AFX_ADVANCEDPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AdvancedPropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAdvancedPropPage dialog

class CAdvancedPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CAdvancedPropPage)

// Construction
public:
	CAdvancedPropPage();
	~CAdvancedPropPage();

// Dialog Data
	BOOL	m_isModified;

	//{{AFX_DATA(CAdvancedPropPage)
	enum { IDD = IDD_ADVANCED_PROP_PAGE };
	CButton	m_OldDeltaE;
	CButton	m_DeltaEGray;
	BOOL	m_bConfirmMeasures;
	BOOL	m_bUseCalibrationFilesOnAllProbes;
	BOOL	m_bControlledMode;
	CString	m_comPort;
	BOOL	m_bUseOnlyPrimaries;
	BOOL	m_bUseImperialUnits;
	int		m_nLuminanceCurveMode;
	BOOL	m_bPreferLuxmeter;
	BOOL	m_bUseOldDeltaEFormula;
	BOOL	m_bUseDeltaELumaOnGrays;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAdvancedPropPage)
	public:
	virtual BOOL OnApply();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CAdvancedPropPage)
	afx_msg void OnSelchangeLuxmeterComCombo();
	//}}AFX_MSG
	afx_msg void OnControlClicked(UINT nID);
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADVANCEDPROPPAGE_H__822A7A51_CB51_4F23_8B29_D7BF5A335392__INCLUDED_)
