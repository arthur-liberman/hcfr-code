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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// SpectrumDlg.cpp : implementation file
//

#include "stdafx.h"
#include "..\ColorHCFR.h"
#include "..\MainFrm.h"
#include "SpectrumDlg.h"
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSpectrumWnd window


CSpectrumWnd::CSpectrumWnd()
	: CWnd()
{
	//{{AFX_DATA_INIT(CSpectrumWnd)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_pRefColor = NULL;
}


BEGIN_MESSAGE_MAP(CSpectrumWnd, CWnd)
	//{{AFX_MSG_MAP(CSpectrumWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSpectrumWnd message handlers

int CSpectrumWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	CString	Msg;

	int nRet = CWnd::OnCreate(lpCreateStruct);

	CRect rect;
	GetClientRect(&rect);	// fill entire window

	m_graphCtrl.Create(_T("Graph Window"), rect, this, IDC_STATIC_1);

	Msg.LoadString ( IDS_SPECTRUM );
	m_SpectrumGraphID = m_graphCtrl.AddGraph(RGB(255,255,0),(LPSTR)(LPCSTR)Msg);

	m_graphCtrl.SetXAxisProps("nm", 20, 370, 730);
	m_graphCtrl.SetYAxisProps("", 0.1, 0, 10);
	m_graphCtrl.SetScale(370,730,0,10);
	m_graphCtrl.m_doGradientBg = FALSE;
	m_graphCtrl.m_doSpectrumBg = TRUE;
	m_graphCtrl.m_doShowAllPoints = TRUE;
	m_graphCtrl.m_doShowAllToolTips = TRUE;

	ShowWindow ( SW_SHOW );

	return nRet;
}

void CSpectrumWnd::Refresh ()
{
	if ( m_pRefColor && (*m_pRefColor).isValid() && m_pRefColor -> HasSpectrum () )
	{
		int			i;
		CSpectrum	Spectrum = m_pRefColor -> GetSpectrum ();
		double		dMax = 0.0, dInterval, WaveLength;
        bool        isHiRes = (Spectrum.GetRows () > 40);

		m_graphCtrl.ClearGraph(m_SpectrumGraphID);

		m_graphCtrl.SetXAxisProps("nm", 50, Spectrum.m_WaveLengthMin, Spectrum.m_WaveLengthMax);

		for ( i = 0, WaveLength = (double) Spectrum.m_WaveLengthMin ; i < Spectrum.GetRows () ; WaveLength += (Spectrum.m_BandWidth + (isHiRes?0.3333:0.0)), i ++ )
		{
			m_graphCtrl.AddPoint(m_SpectrumGraphID, WaveLength , Spectrum[i]);
			if ( Spectrum[i] > dMax )
				dMax = Spectrum[i];
		}

		dMax = ceil ( dMax );
		dInterval = pow ( 10.0, floor ( log ( dMax ) / log ( 10.0 ) ) ) / 10.0;

		m_graphCtrl.SetYAxisProps("", dInterval, 0.0, dMax);
		m_graphCtrl.SetScale(Spectrum.m_WaveLengthMin, Spectrum.m_WaveLengthMax,0,dMax);
	}
	else
	{
		m_graphCtrl.ClearGraph(m_SpectrumGraphID);
		m_graphCtrl.SetXAxisProps("nm", 100, 370, 730);
		m_graphCtrl.SetYAxisProps("", 0.1, 0.0, 2.0);
		m_graphCtrl.SetScale(370, 730,0,2);
	}
	Invalidate ();
}

void CSpectrumWnd::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	
	if(IsWindow(m_graphCtrl.m_hWnd))
		m_graphCtrl.MoveWindow(0,0,cx,cy);
}

void CSpectrumWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// No paint: fully transparent window
}

BOOL CSpectrumWnd::OnEraseBkgnd(CDC* pDC) 
{
	// No erase at all: fully transparent window
	return TRUE;
}

void CSpectrumWnd::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CSpectrumWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_SPECTRUM, NULL );
}


