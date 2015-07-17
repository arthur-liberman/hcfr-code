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

#if !defined(AFX_REFERENCESPROPPAGE_H__5F422D62_7816_45EA_B6A5_F05D414089E5__INCLUDED_)
#define AFX_REFERENCESPROPPAGE_H__5F422D62_7816_45EA_B6A5_F05D414089E5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ReferencesPropPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CReferencesPropPage dialog

class CReferencesPropPage : public CPropertyPageWithHelp
{
	DECLARE_DYNCREATE(CReferencesPropPage)

// Construction
public:
	CReferencesPropPage();
	~CReferencesPropPage();

// Dialog Data
	BOOL m_isModified;

	//{{AFX_DATA(CReferencesPropPage)
	enum { IDD = IDD_REFERENCE_PROP_PAGE };
	CEdit m_GammaRefEdit;
	CEdit m_eMeasuredGamma;
	CEdit m_GammaAvgEdit;
	CEdit m_GammaRelEdit;
	CEdit m_ManualBlackEdit;
	CEdit m_SplitEdit;
	CEdit m_manualWhitexedit;
	CEdit m_manualWhiteyedit;
	CEdit m_manualRedxedit;
	CEdit m_manualRedyedit;
	CEdit m_manualGreenxedit;
	CEdit m_manualGreenyedit;
	CEdit m_manualBluexedit;
	CEdit m_manualBlueyedit;
	CComboBox	m_whiteTargetCombo;
	int		m_whiteTarget;
	int		m_colorStandard;
	int		m_CCMode;
	double	m_GammaRef;
	double	m_GammaAvg;
    double  m_GammaRel;
	double	m_ManualBlack;
	double  m_Split;
	BOOL	m_changeWhiteCheck;
	BOOL	m_useMeasuredGamma;
	BOOL	m_bSave;
	BOOL	m_userBlack;
	int		m_GammaOffsetType;
	double	m_manualGOffset;
	double	m_manualWhitex;
	double	m_manualWhitey;
	double	m_manualRedx;
	double	m_manualRedy;
	double	m_manualGreenx;
	double	m_manualGreeny;
	double	m_manualBluex;
	double	m_manualBluey;
	double	m_manualWhitexold;
	double	m_manualWhiteyold;
	double	m_manualRedxold;
	double	m_manualRedyold;
	double	m_manualGreenxold;
	double	m_manualGreenyold;
	double	m_manualBluexold;
	double	m_manualBlueyold;
	//}}AFX_DATA

	virtual UINT GetHelpId ( LPSTR lpszTopic );

// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CReferencesPropPage)
	public:
	virtual BOOL OnApply();
	virtual void OnOK();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CReferencesPropPage)
	afx_msg void OnCheckColors();
	afx_msg void OnChangeEditIrisTime();
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEditGammaRef();
	afx_msg void OnChangeEditGammaRel();
	afx_msg void OnChangeEditManualBlack();
	afx_msg void OnChangeEditGammaAvg();
	afx_msg void OnChangeWhiteCheck();
	afx_msg void OnUseMeasuredGammaCheck();
	afx_msg void OnUserBlackCheck();
	afx_msg void OnSelchangeColorrefCombo();
	afx_msg void OnSelchangeCCmodeCombo();
	afx_msg void OnChangeEditGammaOffset();
	afx_msg void OnChangeEditManualGOffset();
	//}}AFX_MSG
	afx_msg void OnControlClicked(UINT nID);
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REFERENCESPROPPAGE_H__5F422D62_7816_45EA_B6A5_F05D414089E5__INCLUDED_)
