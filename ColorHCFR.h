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
//	Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// ColorHCFR.h : main header file for the COLORHCFR application
//

#if !defined(AFX_COLORHCFR_H__377B1C45_C5FB_483D_9410_96B1FCC42EC2__INCLUDED_)
#define AFX_COLORHCFR_H__377B1C45_C5FB_483D_9410_96B1FCC42EC2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#include "WinAppEx.h"
#include "HelpID.h"
#include "Color.h"
#include "CrashDump.h"

// Return codes for CColorHCFRApp::GetLuxMeasure

#define LUXMETER_OK					0	// Value is OK
#define LUXMETER_NOT_RUNNING		1	// Luxmeter not running
#define LUXMETER_SCALE_TOO_LOW		2	// Scale is too low, and can be changed
#define LUXMETER_SCALE_TOO_HIGH		3	// Scale is too high, and can be changed
#define LUXMETER_SCALE_TOO_LOW_MAX	4	// Scale is too low, and is the highest: cannot measure value
#define LUXMETER_SCALE_TOO_HIGH_MIN	5	// Scale is too high, and is the lowest: cannot measure value
#define LUXMETER_NEW_SCALE_OK		6	// User changed scale and now value can be measured


//////////////////////////////////////////////////////////////////////
// Help handling in property sheets

class CPropertyPageWithHelp : public CPropertyPage
{
 	DECLARE_DYNAMIC(CPropertyPageWithHelp)

public:

	CPropertyPageWithHelp () : CPropertyPage () { m_psp.dwFlags |= PSP_HASHELP; m_psp.hInstance = AfxGetResourceHandle (); }
	CPropertyPageWithHelp(UINT nIDTemplate, UINT nIDCaption = 0) : CPropertyPage ( nIDTemplate, nIDCaption ) { m_psp.dwFlags |= PSP_HASHELP; m_psp.hInstance = AfxGetResourceHandle (); }
    virtual ~CPropertyPageWithHelp() {}
	
	virtual UINT GetHelpId ( LPSTR lpszTopic ) { return 0; }

	afx_msg void OnContextMenu(CWnd* pWnd, CPoint pos);
	DECLARE_MESSAGE_MAP()
};

class CPropertySheetWithHelp : public CPropertySheet
{
 	DECLARE_DYNAMIC(CPropertySheetWithHelp)

 public:
	CPropertySheetWithHelp() : CPropertySheet() { m_psh.dwFlags |= PSH_HASHELP; }
	CPropertySheetWithHelp(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0) : CPropertySheet ( nIDCaption, pParentWnd, iSelectPage ) { m_psh.dwFlags |= PSH_HASHELP; }
	CPropertySheetWithHelp(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0) : CPropertySheet ( pszCaption, pParentWnd, iSelectPage ) { m_psh.dwFlags |= PSH_HASHELP; }
    virtual ~CPropertySheetWithHelp() {}
	afx_msg BOOL CPropertySheetWithHelp::OnHelpInfo ( HELPINFO * pHelpInfo );
	afx_msg void OnHelp();
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CColorHCFRApp:
// See ColorHCFR.cpp for the implementation of this class
//

#include "ColorHCFRConfig.h"

class	CDataSetDoc ; //Ki
CDataSetDoc * GetDataRef();	//Ki
void SetDataRef(CDataSetDoc *m_pRefData);	//Ki
void	UpdateDataRef(BOOL ActiveDataRef, CDataSetDoc * pDoc);	//Ki

CColorHCFRConfig * GetConfig();

class CColorHCFRApp : public CWinAppEx
{
public:

	CColorHCFRConfig *m_pConfig;
	CColorReference *m_pColorReference;
	CDataSetDoc *m_pReferenceData; //Ki

public:
	CColorHCFRApp();
	~CColorHCFRApp();


	afx_msg void OnFileNew()	{ CWinAppEx::OnFileNew (); }

	void CreateCIEBitmaps ( BOOL bBackGround = FALSE );
	
	// CIE Bitmaps handling
	HANDLE	m_hCIEThread;	// Background thread creating bitmaps
	HANDLE	m_hCIEEvent;	// Event set when bitmaps are created

	// CIE Bitmaps (xy)
	CBitmap m_chartBitmap;
	CBitmap m_chartBitmap_white;
	CBitmap m_lightenChartBitmap;

	// CIE Bitmaps (u'v')
	CBitmap m_chartBitmap_uv;
	CBitmap m_chartBitmap_uv_white;
	CBitmap m_lightenChartBitmap_uv;

	// Measuring cursor
	HCURSOR	m_hCursorMeasure;
	HCURSOR	m_hSavedCursor;
	CWnd *	m_pPatternWnd;

	void SetPatternWindow ( CWnd * pWnd );
	void BringPatternWindowToTop ();
	void BeginMeasureCursor ();
	void RestoreMeasureCursor ();
	void EndMeasureCursor ();
	int InMeasureMessageBox(LPCSTR lpText, LPCSTR lpCaption = NULL, UINT uType = MB_OK);

	// LX-1108 luxmeter handling
	void StartLuxMeasures ();
	BOOL IsLuxmeterRunning ();
	void SetLuxmeterValue ( LPCSTR lpszString, DWORD dwStartTick );

	void BeginLuxMeasure ();
	UINT GetLuxMeasure ( double * pLuxValue );

	// Luxmeter connection handling
	HANDLE	m_hLuxThread;		// Background thread handling luxmeter
	BOOL	m_bStopLuxThread;	// Flag used to stop background thread
	CString	m_LuxPort;			// COM port, empty when no luxmeter to handle

	// Luxmeter values
	CRITICAL_SECTION	m_LuxCritSec;
	char				m_RefLuxParams [ 2 ];
	double				m_CurrentLuxValue;
	BOOL				m_bLowLuxValue;
	BOOL				m_bHighLuxValue;
	BOOL				m_bLowestLuxScale;
	BOOL				m_bHighestLuxScale;
	DWORD				m_dwPreviousLuxValueTime;
	DWORD				m_dwLuxValueTime;
	BOOL				m_bInsideMeasure;
	DWORD				m_dwMeasureStartTime;
	double				m_LastLuxValues [ 8 ];
	int					m_NbMeasures;
	BOOL				m_bLastMeasureOutOfRange;
	BOOL				m_bFirstMeasureWithNewScaleOk;
	BOOL				m_bLuxMeasureValid;
	int					m_nFirstValidMeasure;
	double				m_MeasuredLuxValue;
	double				m_MeasuredLuxValue_Initial;


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CColorHCFRApp)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
	//{{AFX_MSG(CColorHCFRApp)
	afx_msg void OnAppAbout();
	afx_msg void OnUpdateViewDataSet(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewCiechart(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewColortemphisto(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewLuminancehisto(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewGammahisto(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewNearBlackhisto(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewNearWhitehisto(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewRgbhisto(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewSatLumhisto(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewSatLumshift(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewMeasuresCombo(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
    CrashDump m_enablesCrashDump;
};

inline CColorHCFRApp * GetColorApp () { return (CColorHCFRApp *) AfxGetApp (); }
inline CColorReference& GetColorReference() {return *(GetColorApp()->m_pColorReference);}

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_COLORHCFR_H__377B1C45_C5FB_483D_9410_96B1FCC42EC2__INCLUDED_)
