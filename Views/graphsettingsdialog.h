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

#if !defined(AFX_GRAPHSETTINGSDIALOG_H__7177C70A_2039_4C34_A4AB_3793B773E27A__INCLUDED_)
#define AFX_GRAPHSETTINGSDIALOG_H__7177C70A_2039_4C34_A4AB_3793B773E27A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// graphsettingsdialog.h : header file
//

#include "ColourPickerXP.h"
#include "GraphControl.h"

/////////////////////////////////////////////////////////////////////////////
// CGraphSettingsDialog dialog

class CGraphSettingsDialog : public CDialog
{
// Construction
public:
	CGraphSettingsDialog(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	CArray<CGraph,CGraph> m_graphArray;
	int m_selectedGraph;
	//{{AFX_DATA(CGraphSettingsDialog)
	enum { IDD = IDD_GRAPH_PROP_PAGE };
	CComboBox	m_lineWidthCombo;
	CComboBox	m_graphLineCombo;
	CColourPickerXP	m_graphColorButton;
	CComboBox	m_graphNameCombo;
	int		m_lineType;
	CString	m_lineWidthStr;
	BOOL	m_doShowAxis;
	BOOL	m_doGradientBg;
	BOOL	m_doShowXLabel;
	BOOL	m_doShowYLabel;
	BOOL	m_doShowPoints;
	BOOL	m_doShowToolTips;
	BOOL	m_doShowAllPoints;
	BOOL	m_doShowAllToolTips;
	BOOL	m_doShowDataLabel;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraphSettingsDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CGraphSettingsDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeGraphNameCombo();
	afx_msg void OnSelchangeGraphLinewidthCombo();
	afx_msg void OnEditchangeGraphLinewidthCombo();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo ( HELPINFO * pHelpInfo );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GRAPHSETTINGSDIALOG_H__7177C70A_2039_4C34_A4AB_3793B773E27A__INCLUDED_)
