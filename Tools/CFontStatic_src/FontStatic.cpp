/////////////////////////////////////////////////////////////////////////////
//
//   Filename: FontStatic.cpp
//  Classname: CFontStatic
//
// Written by: Patrik Svensson (patrik.svensson@home.se)
//
//             This class can be used by anyone for any purpose, 
//             but if you like it, then send me a mail and tell me so ;)
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FontStatic.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

CFontStatic::CFontStatic()
{
	// Set the font style
	m_bBold = false;
	m_bItalic = false;
	m_bUnderlined = false;
	m_bStrikethrough = false;
	m_bAntialias = false;

	// Set the horizontal alignment
	m_bCenter = false;
	m_bLeft = true;
	m_bRight = false;

	// Set default font
	m_bBgColor = false;
	m_nSize = 8;
	m_dwColor = RGB(0,0,0);
	m_szFont.Format("MS Sans Serif");
}

CFontStatic::~CFontStatic()
{
}

/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CFontStatic, CStatic)
	//{{AFX_MSG_MAP(CFontStatic)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

void CFontStatic::PreSubclassWindow() 
{
	ModifyStyle(0, SS_NOTIFY, TRUE);
	CStatic::PreSubclassWindow();
}

/////////////////////////////////////////////////////////////////////////////

void CFontStatic::OnPaint() 
{
	CFont fFont;
	CFont *fOldFont;
	int nWeight;
	CPaintDC dc(this);
	DWORD dwQuality;
	CRect rect;
	UINT unFormat=0;

	// Get the window text
	CString szText;
	this->GetWindowText(szText);

	// Get the client rect
	this->GetClientRect(rect);

	// Set the background
	if(m_bBgColor)
		dc.FillSolidRect(rect,m_dwBgColor);
	else
		dc.FillSolidRect(rect,GetSysColor(COLOR_3DFACE));

	// Set the text-color and background
	dc.SetTextColor(m_dwColor);
	dc.SetBkMode(TRANSPARENT);

	// Determine the weight of the font
	if(m_bBold)
		nWeight = FW_BLACK;
	else
		nWeight = FW_NORMAL;

	// Set the quality of the font
	if(m_bAntialias)
		dwQuality = ANTIALIASED_QUALITY;
	else
		dwQuality = DEFAULT_QUALITY;

	// Set the horizontal alignment
	if(m_bCenter)
		unFormat|=DT_CENTER;
	else if(m_bRight)
		unFormat|=DT_RIGHT;
	else if(m_bLeft)
		unFormat|=DT_LEFT;

	// Set no prefix and wordwrapping
	unFormat|=DT_NOPREFIX;
	unFormat|=DT_WORDBREAK;

	// Create the font
	fFont.Detach();
	if(fFont.CreateFont(m_nSize,0,0,0, nWeight ,m_bItalic, m_bUnderlined, m_bStrikethrough, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_CHARACTER_PRECIS, dwQuality, DEFAULT_PITCH | FF_DONTCARE, m_szFont)!=NULL)
	{
		fOldFont = dc.SelectObject(&fFont);
		dc.DrawText(szText,rect,unFormat);
	}
	else
		AfxMessageBox("Font could not be created!");

}

/////////////////////////////////////////////////////////////////////////////

void CFontStatic::SetFontStyle(DWORD dwStyle)
{
	// Set the style of the static
	if(dwStyle&FS_BOLD)
		m_bBold = true;
	if(dwStyle&FS_ITALIC)
		m_bItalic = true;
	if(dwStyle&FS_UNDERLINED)
		m_bUnderlined = true;
	if(dwStyle&FS_STRIKETHROUGH)
		m_bStrikethrough = true;
	if(dwStyle&FS_ANTIALIAS)
		m_bAntialias = true;
	if(dwStyle&FS_NORMAL)
	{
		// This will (even if it's combined with other defines) 
		// set all styles to false.

		m_bBold = false;
		m_bItalic = false;
		m_bUnderlined = false;
		m_bStrikethrough = false;
		m_bAntialias = false;
		m_bCenter = false;
		m_bLeft = true;
		m_bRight = false;
	}

	// Set all alignments to false
	m_bCenter = false;
	m_bLeft = false;
	m_bRight = false;

	// Set the horizontal alignment
	if(dwStyle&FS_CENTER)
		m_bCenter = true;
	else if(dwStyle&FS_RIGHT)
		m_bRight = true;
	else if(dwStyle&FS_LEFT)
		m_bLeft = true;
	else
		m_bLeft = true;
}

/////////////////////////////////////////////////////////////////////////////

void CFontStatic::SetBackground(DWORD dwBgColor)
{
	// Set the background color
	m_dwBgColor = dwBgColor;
	m_bBgColor = true;
}

/////////////////////////////////////////////////////////////////////////////

void CFontStatic::SetFontStatic(CString szFont, int nSize, DWORD dwColor, DWORD dwStyle)
{
	// Set the font, size and color.
	m_szFont = szFont;
	m_nSize = nSize;
	m_dwColor = dwColor;

	// Set the style
	SetFontStyle(dwStyle);
}

/////////////////////////////////////////////////////////////////////////////


