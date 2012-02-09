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
//	François-Xavier CHABOUD
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_APPEARANCEPROPPAGE_H__D90BDEB8_5648_4F87_BB24_56E69EE71807__INCLUDED_)
#define AFX_APPEARANCEPROPPAGE_H__D90BDEB8_5648_4F87_BB24_56E69EE71807__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AppearancePropPage.h : header file
//

#include "ColourPickerXP.h"

/////////////////////////////////////////////////////////////////////////////
// CAppearancePropPage dialog

class CAppearancePropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CAppearancePropPage)

// Construction
public:
	CAppearancePropPage();
	~CAppearancePropPage();

// Dialog Data
	BOOL m_isModified;
	BOOL m_isWhiteModified;

	//{{AFX_DATA(CAppearancePropPage)
	enum { IDD = IDD_APPEARANCE_PROP_PAGE };
	CColourPickerXP	m_colorWindowButton;
	CColourPickerXP	m_colorMenuButton;
	CColourPickerXP	m_colorSelectionButton;
	CColourPickerXP	m_colorTextButton;
	int		m_themeComboIndex;
	BOOL	m_drawMenuborder;
	int		m_useCustomColor;
	BOOL	m_doSelectDisabledItem;
	BOOL	m_doGlooming;
	BOOL	m_doXpBlending;
	BOOL	m_bWhiteBkgndOnScreen;
	BOOL	m_bWhiteBkgndOnFile;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CAppearancePropPage)
	public:
	virtual BOOL OnApply();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	afx_msg LRESULT CAppearancePropPage::OnColorChanged(WPARAM wParam, LPARAM lParam);

	// Generated message map functions
	//{{AFX_MSG(CAppearancePropPage)
	afx_msg void OnControlClicked(UINT nID);
	afx_msg void OnWhiteCheckClicked(UINT nID);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_APPEARANCEPROPPAGE_H__D90BDEB8_5648_4F87_BB24_56E69EE71807__INCLUDED_)
