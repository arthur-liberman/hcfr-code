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

#if !defined(AFX_SIMULPROPPAGE_H__C94C3504_4BEC_4495_A79A_58E67718C3DD__INCLUDED_)
#define AFX_SIMULPROPPAGE_H__C94C3504_4BEC_4495_A79A_58E67718C3DD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SimulatedSensorPropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSimulatedSensorPropPage dialog

class CSimulatedSensorPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CSimulatedSensorPropPage)

// Construction
public:
	CSimulatedSensorPropPage();
	~CSimulatedSensorPropPage();

// Dialog Data
	//{{AFX_DATA(CSimulatedSensorPropPage)
	enum { IDD = IDD_SIMULATED_SENSOR_PROP_PAGE };
	UINT	m_offsetBlue;
	UINT	m_offsetGreen;
	UINT	m_offsetRed;
	BOOL	m_doGainError;
	BOOL	m_doGammaError;
	BOOL	m_doOffsetError;
	double	m_gammaErrorMax;
	double	m_offsetErrorMax;
	double	m_gainErrorMax;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSimulatedSensorPropPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSimulatedSensorPropPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SIMULPROPPAGE_H__C94C3504_4BEC_4495_A79A_58E67718C3DD__INCLUDED_)
