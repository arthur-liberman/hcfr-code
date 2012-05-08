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

#if !defined(AFX_SENSORPROPPAGE_H__7275FE58_53ED_42FF_BC4F_81AA64B1ACA6__INCLUDED_)
#define AFX_SENSORPROPPAGE_H__7275FE58_53ED_42FF_BC4F_81AA64B1ACA6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SensorPropPage.h : header file
//

#include "Matrix.h"
#include "Tools/GridCtrl/GridCtrl.h"

/////////////////////////////////////////////////////////////////////////////
// CSensorPropPage dialog

class CSensorPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CSensorPropPage)

// Construction
public:
	CSensorPropPage();
	~CSensorPropPage();

// Dialog Data
	//{{AFX_DATA(CSensorPropPage)
	enum { IDD = IDD_SENSOR_PROP_PAGE };
	CStatic	m_matrixStatic;
	CString	m_calibrationDate;
	CString	m_information;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSensorPropPage)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	virtual void OnOK();
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation

public:
	void SetMatrix(Matrix aMatrix);
	Matrix GetMatrix();

protected:
	CGridCtrl* m_pGrid;
	Matrix m_matrix;

	void OnGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult);

	// Generated message map functions
	//{{AFX_MSG(CSensorPropPage)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SENSORPROPPAGE_H__7275FE58_53ED_42FF_BC4F_81AA64B1ACA6__INCLUDED_)
