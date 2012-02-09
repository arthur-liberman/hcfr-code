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

#if !defined(AFX_GRAPHSCALEPROPDIALOG_H__512942C4_D6CB_459C_9D79_DB9FAA3B2E5D__INCLUDED_)
#define AFX_GRAPHSCALEPROPDIALOG_H__512942C4_D6CB_459C_9D79_DB9FAA3B2E5D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// graphscalepropdialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGraphScalePropDialog dialog

class CGraphScalePropDialog : public CDialog
{
// Construction
public:
	CGraphScalePropDialog(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CGraphScalePropDialog)
	enum { IDD = IDD_GRAPH_SCALE_PROP_PAGE };
	double	m_maxXGrow;
	double	m_maxYGrow;
	double	m_minXGrow;
	double	m_minYGrow;
	double	m_xAxisStep;
	double	m_yAxisStep;
	double	m_maxY;
	double	m_minY;
	double	m_maxX;
	double	m_minX;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraphScalePropDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CGraphScalePropDialog)
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo ( HELPINFO * pHelpInfo );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GRAPHSCALEPROPDIALOG_H__512942C4_D6CB_459C_9D79_DB9FAA3B2E5D__INCLUDED_)
