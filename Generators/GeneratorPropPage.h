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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_GENERATORPROPPAGE_H__CDF680C2_AD6C_4EC8_96E0_1069023C05C8__INCLUDED_)
#define AFX_GENERATORPROPPAGE_H__CDF680C2_AD6C_4EC8_96E0_1069023C05C8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GeneratorPropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGeneratorPropPage dialog

class CGeneratorPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CGeneratorPropPage)

// Construction
public:
	CGeneratorPropPage();
	~CGeneratorPropPage();

// Dialog Data
	//{{AFX_DATA(CGeneratorPropPage)
	enum { IDD = IDD_GENERATOR_PROP_PAGE };
	BOOL	m_doScreenBlanking;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CGeneratorPropPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CGeneratorPropPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GENERATORPROPPAGE_H__CDF680C2_AD6C_4EC8_96E0_1069023C05C8__INCLUDED_)
