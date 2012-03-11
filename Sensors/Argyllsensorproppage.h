/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
// Copyright (c) 2012 Hcfr project.  All rights reserved.
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
//   Georges GALLERAND
//   John Adcock
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_ArgyllSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_)
#define AFX_ArgyllSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ArgyllSensorPropPage.h : header file
//

class CArgyllSensor;

/////////////////////////////////////////////////////////////////////////////
// CArgyllSensorPropPage dialog

class CArgyllSensorPropPage : public CPropertyPageWithHelp
{
    DECLARE_DYNCREATE(CArgyllSensorPropPage)
 
// Construction
public:
    CArgyllSensorPropPage();
    virtual ~CArgyllSensorPropPage();

// Dialog Data
    //{{AFX_DATA(CArgyllSensorPropPage)
    enum { IDD = IDD_ARGYLL_SENSOR_PROP_PAGE };
    int        m_DisplayType;
    int        m_ReadingType;
    BOOL       m_DebugMode;
    BOOL       m_HiRes;
    CString    m_MeterName;
    CComboBox  m_DisplayTypeCombo;
    CButton    m_HiResCheckBox;
    BOOL       m_HiResCheckBoxEnabled;
    //}}AFX_DATA
    
    CArgyllSensor* m_pSensor;

    virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
    // ClassWizard generate virtual function overrides
    //{{AFX_VIRTUAL(CArgyllSensorPropPage)
    public:
    virtual BOOL OnSetActive();
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
protected:
    // Generated message map functions
    //{{AFX_MSG(CArgyllSensorPropPage)
    afx_msg void OnCalibrate();
    afx_msg void OnSelchangeModeCombo();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()

};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ArgyllSensorPropPage_H__4A28F0DD_80F3_4CE4_BD25_AD38CBF304C9__INCLUDED_)
