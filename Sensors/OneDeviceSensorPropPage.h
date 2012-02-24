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

#if !defined(AFX_OneDeviceSensorPropPage_H__384588AA_1E7A_496B_BEF4_7ACF16423757__INCLUDED_)
#define AFX_OneDeviceSensorPropPage_H__384588AA_1E7A_496B_BEF4_7ACF16423757__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// OneDeviceSensorPropPage.h : header file
//

#include "Matrix.h"
#include "Tools/GridCtrl/GridCtrl.h"

/////////////////////////////////////////////////////////////////////////////
// COneDeviceSensorPropPage dialog

class COneDeviceSensor;

class COneDeviceSensorPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(COneDeviceSensorPropPage)

// Construction
public:
	COneDeviceSensorPropPage();
	~COneDeviceSensorPropPage();

	double m_calibrationIRELevel;
	CString m_calibrationReferenceName;

// Dialog Data
	//{{AFX_DATA(COneDeviceSensorPropPage)
	enum { IDD = IDD_ONEDEVICESENSOR_PROP_PAGE };
	CComboBox	m_displayRefCombo;
	CStatic	m_whiteMatrixStatic;
	CStatic	m_primariesMatrixStatic;
	BOOL	m_doBlackCompensation;
	UINT	m_maxAdditivityErrorPercent;
	BOOL	m_doVerifyAdditivity;
	BOOL	m_showAdditivityResults;
	CString	m_IRELevelStr;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COneDeviceSensorPropPage)
	public:
	virtual BOOL OnSetActive();
	virtual void OnOK();
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

public:
	void SetPrimariesMatrix(Matrix aMatrix) { m_primariesMatrix=aMatrix; }
	Matrix GetPrimariesMatrix() { return m_primariesMatrix; }
	void SetWhiteMatrix(Matrix aMatrix) { m_whiteMatrix=aMatrix; }
	Matrix GetWhiteMatrix() { return m_whiteMatrix; }

protected:
	CGridCtrl* m_pPrimariesGrid;
	CGridCtrl* m_pWhiteGrid;
	Matrix m_primariesMatrix;
	Matrix m_whiteMatrix;

	void UpdateGrids();
	void OnPrimariesGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult);
	void OnWhiteGridEndEdit(NMHDR *pNotifyStruct,LRESULT* pResult);

	void GetReferencesList();
	void LoadReferenceValues();

protected:
	// Generated message map functions
	//{{AFX_MSG(COneDeviceSensorPropPage)
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeDisplayRefCombo();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OneDeviceSensorPropPage_H__384588AA_1E7A_496B_BEF4_7ACF16423757__INCLUDED_)
