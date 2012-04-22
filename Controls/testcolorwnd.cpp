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

// testcolorwnd.cpp : implementation file
//

#include "stdafx.h"
#include "..\ColorHCFR.h"
#include "..\DataSetDoc.h"
#include "testcolorwnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTestColorWnd

CTestColorWnd::CTestColorWnd(CWnd* pParent) : CDialog (CTestColorWnd::IDD, pParent)
{
//	m_colorPicker.SetDefaultText("");

}

CTestColorWnd::~CTestColorWnd()
{
}


void CTestColorWnd::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CTestColorWnd)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CTestColorWnd, CDialog)
	//{{AFX_MSG_MAP(CTestColorWnd)
	ON_WM_PAINT()
	ON_WM_CREATE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_WHATS_THIS, OnHelp)
	ON_WM_HELPINFO()
	//}}AFX_MSG_MAP
    ON_MESSAGE(CPN_CLOSEUP, OnCloseUp)
    ON_MESSAGE(CPN_DROPDOWN, OnDropDown)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTestColorWnd message handlers

void CTestColorWnd::OnPaint() 
{
	int				r, g, b;
	char			szBuf [ 256 ];
	CPaintDC		dc(this); // device context for painting
	CFont			font;
	LOGFONT			logFont;
	CString			Msg;
	CDataSetDoc *	pDoc = NULL;
	
	CRect rect;
	GetClientRect(&rect);

	COLORREF clr = m_colorPicker.GetColor();
	COLORREF textclr = 0x00000000;	// black

	r = GetRValue(clr);
	g = GetGValue(clr);
	b = GetBValue(clr);

	CMDIChildWnd * pActiveMDI = ( ( CMDIFrameWnd * ) AfxGetMainWnd () ) -> MDIGetActive ();
	
	if ( pActiveMDI )
		pDoc = ( CDataSetDoc * ) pActiveMDI -> GetActiveDocument ();

	if ( pDoc && pDoc -> GetGenerator () -> m_b16_235 )
	{
		// Use 16 -> 235 scale
		if ( r < 16 )
			r = 16;
		else if ( r > 235 )
			r = 235;

		if ( g < 16 )
			g = 16;
		else if ( g > 235 )
			g = 235;

		if ( b < 16 )
			b = 16;
		else if ( b > 235 )
			b = 235;
	}

	clr = RGB(r,g,b);

	if ( r < 128 && g < 128 && b < 160 )
		textclr = 0x00FFFFFF;	// white

	// fill the background with selected color
	dc.FillSolidRect(rect,clr);
	
	// draw text
	Msg.LoadString ( IDS_COLORRGBVALUES );
	sprintf ( szBuf, (LPCSTR)Msg, (int) r, (int) g, (int) b );
	
	memset ( &logFont, 0, sizeof (logFont) );
	logFont.lfHeight =			14;
	logFont.lfWeight =			FW_BOLD;
	logFont.lfCharSet =			ANSI_CHARSET;
	logFont.lfOutPrecision =	OUT_DEFAULT_PRECIS;
	logFont.lfClipPrecision =	CLIP_DEFAULT_PRECIS;
	logFont.lfQuality =			DEFAULT_QUALITY;
	strcpy (logFont.lfFaceName, "Courier New");

	VERIFY(font.CreateFontIndirect(&logFont));

	COLORREF oldtxtclr = dc.SetTextColor ( textclr );
	COLORREF oldbkclr = dc.SetBkColor ( clr );

	CFont* def_font = dc.SelectObject(&font);
	dc.TextOut(m_colorPickerRect.left + m_colorPickerRect.Width() + 3, m_colorPickerRect.top + 3, szBuf); 
	dc.SelectObject(def_font);

	dc.SetTextColor ( oldtxtclr );
	dc.SetBkColor ( oldbkclr );
}

int CTestColorWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	CString Msg;
	Msg.LoadString ( IDS_COLOR );
	m_colorPickerRect.SetRect(0,0,40,20);
	m_colorPicker.Create(Msg,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,m_colorPickerRect,this,IDC_TESTCOLOR_BUTTON);
	
	return 0;
}

LONG CTestColorWnd::OnCloseUp(UINT lParam, LONG wParam) 
{
	CColourPopupXP::m_nNumColours=40;
	CColourPopupXP::m_nFxNumColumns=8;

	// restore original values (for other use of CColourPickerXP)
	for(int i=0; i<CColourPopupXP::m_nNumColours; i++)
	{
		CColourPopupXP::m_crColours[i].crColour=m_colorTableBackup[i].crColour;
		strcpy(CColourPopupXP::m_crColours[i].szName,m_colorTableBackup[i].szName);
	} 

	Invalidate(TRUE);
	return 0;
}

LONG CTestColorWnd::OnDropDown(UINT lParam, LONG wParam) 
{
	Invalidate(TRUE); // Needed otherwise it's crapped
	return 0;
}

BOOL CTestColorWnd::PreTranslateMessage(MSG* pMsg) 
{
	CString			Msg;
	double			base, coef;
	CDataSetDoc *	pDoc = NULL;

	switch(pMsg->message)
	{
		case WM_LBUTTONDOWN:
			CColourPopupXP::m_nNumColours=44;
			CColourPopupXP::m_nFxNumColumns=11;

			CMDIChildWnd * pActiveMDI = ( ( CMDIFrameWnd * ) AfxGetMainWnd () ) -> MDIGetActive ();
			
			if ( pActiveMDI )
				pDoc = ( CDataSetDoc * ) pActiveMDI -> GetActiveDocument ();

			if ( pDoc && pDoc -> GetGenerator () -> m_b16_235 )
			{
				// Use 16 -> 235 scale
				base = 16.0;
				coef = (235.0 - 16.0) / 10.0;
			}
			else
			{
				// Use 0 -> 255 scale
				base = 0.0;
				coef = 25.5;
			}
			// backup original values (for other use of CColourPickerXP)
			for(int i=0; i<CColourPopupXP::m_nNumColours; i++)
			{
				m_colorTableBackup[i].crColour=CColourPopupXP::m_crColours[i].crColour;
				strcpy(m_colorTableBackup[i].szName,CColourPopupXP::m_crColours[i].szName);
			}
			
			// fill selection color array with interesting ones for us
			for(int i=0; i<11; i++)
			{
				Msg.LoadString ( IDS_GRAYIRE );
				CColourPopupXP::m_crColours[i].crColour=RGB(i*coef+base,i*coef+base,i*coef+base);
				sprintf(CColourPopupXP::m_crColours[i].szName,(LPCSTR)Msg,i*10);
			}
			for(int i=0; i<11; i++)
			{
				Msg.LoadString ( IDS_REDIRE );
				CColourPopupXP::m_crColours[i+11].crColour=RGB(i*coef+base,base,base);
				sprintf(CColourPopupXP::m_crColours[i+11].szName,(LPCSTR)Msg,i*10);
			}
			for(int i=0; i<11; i++)
			{
				Msg.LoadString ( IDS_GREENIRE );
				CColourPopupXP::m_crColours[i+22].crColour=RGB(base,i*coef+base,base);
				sprintf(CColourPopupXP::m_crColours[i+22].szName,(LPCSTR)Msg,i*10);
			}
			for(int i=0; i<11; i++)
			{
				Msg.LoadString ( IDS_BLUEIRE );
				CColourPopupXP::m_crColours[i+33].crColour=RGB(base,base,i*coef+base);
				sprintf(CColourPopupXP::m_crColours[i+33].szName,(LPCSTR)Msg,i*10);
			}
			break;
	} 
	return CDialog::PreTranslateMessage(pMsg);
}

void CTestColorWnd::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	// load and display popup menu
	CNewMenu menu;
	menu.LoadMenu(IDR_WHATS_THIS);
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup);
	
	pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_VERTICAL,
		point.x, point.y, this);
}

void CTestColorWnd::OnCancel() 
{
	ShowWindow ( SW_HIDE );
}

void CTestColorWnd::OnOK() 
{
}

void CTestColorWnd::OnHelp() 
{
	GetConfig () -> DisplayHelp ( HID_CTRL_TESTCOLOR, NULL );
}

BOOL CTestColorWnd::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	OnHelp ();
	return TRUE;
}
