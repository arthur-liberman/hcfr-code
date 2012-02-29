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

// NewDocWizard.h : header file
//
// This class defines custom modal property sheet 
// CNewDocWizard.
 // CNewDocWizard has been customized to include
// a preview window.
 
#ifndef __NewDocWizard_H__
#define __NewDocWizard_H__

#include "NewDocPropertyPages.h"

/////////////////////////////////////////////////////////////////////////////
// CNewDocWizard

class CNewDocWizard : public CPropertySheetWithHelp
{
	DECLARE_DYNAMIC(CNewDocWizard)

// Construction
public:
	CNewDocWizard(CWnd* pWndParent = NULL);
    virtual ~CNewDocWizard();

// Attributes
public:
	CGeneratorSelectionPropPage m_Page1;
	CSensorSelectionPropPage m_Page2;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewDocWizard)
	//}}AFX_VIRTUAL

// Implementation
public:
	 virtual BOOL OnInitDialog();

// Generated message map functions
protected:
	//{{AFX_MSG(CNewDocWizard)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif	// __NewDocWizard_H__
