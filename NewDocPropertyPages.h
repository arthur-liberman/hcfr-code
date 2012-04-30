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

// NewDocPropertyPages.h : header file
//

#ifndef __NEWDOCPROPERTYPAGES_H__
#define __NEWDOCPROPERTYPAGES_H__

/////////////////////////////////////////////////////////////////////////////
// CSensorSelectionPropPage dialog

class CSensorSelectionPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CSensorSelectionPropPage)

// Construction
public:
	CSensorSelectionPropPage();
	~CSensorSelectionPropPage();

	static LPCSTR GetThcFileSubDir ( int nSensorID );

	int m_currentID;

// Dialog Data
	//{{AFX_DATA(CSensorSelectionPropPage)
	enum { IDD = IDD_SELECT_SENSOR_PROPPAGE };
	CComboBox	m_trainingFileCombo;
	CComboBox	m_sensorChoiceCtrl;
	CString	m_sensorChoice;
	CString	m_trainingFileName;
	int		m_sensorTrainingMode;
	//}}AFX_DATA

	int GetCurrentID() { return m_currentID; }
	LPCSTR	GetFileSubDir () { return GetThcFileSubDir ( m_currentID ); }

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSensorSelectionPropPage)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnWizardFinish();
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSensorSelectionPropPage)
	afx_msg void OnSelchangeSensorchoiceCombo();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


/////////////////////////////////////////////////////////////////////////////
// CGeneratorSelectionPropPage dialog

class CGeneratorSelectionPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CGeneratorSelectionPropPage)

// Construction
public:
	CGeneratorSelectionPropPage();
	~CGeneratorSelectionPropPage();

	int m_currentID;

// Dialog Data
	//{{AFX_DATA(CGeneratorSelectionPropPage)
	enum { IDD = IDD_SELECT_GENERATOR_PROPPAGE };
	CComboBox	m_generatorChoiceCtrl;
	CString	m_generatorChoice;
	//}}AFX_DATA

	int GetCurrentID() { return m_currentID; }

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CGeneratorSelectionPropPage)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnWizardFinish();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CGeneratorSelectionPropPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};



#endif // __NEWDOCPROPERTYPAGES_H__
