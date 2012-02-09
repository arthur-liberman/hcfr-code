// CreditsCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "CreditsCtrl.h"

#include <afxtempl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define CCREDITCTRL_TIMER1	100

// use transparent BitBlts if supported? ( UNTESTED!! )
//#define CCREDITCTRL_USE_TRANSPARENT_BITBLT

// stuff that _should_ have been defined in some header :-/
#ifndef C1_TRANSPARENT
    #define C1_TRANSPARENT 0x0001
#endif 
#ifndef CAPS1
    #define CAPS1 94
#endif 
#ifndef NEWTRANSPARENT
    #define NEWTRANSPARENT 3
#endif

/////////////////////////////////////////////////////////////////////////////
// CCreditsCtrl
LPCTSTR CCreditsCtrl::m_lpszClassName = NULL;

CCreditsCtrl::CCreditsCtrl()
{
	m_nTimerSpeed = 40;
	m_nCurBitmapOffset = 0;
	m_crInternalTransparentColor = RGB(255,0,255);
	m_pBackgroundPaint = CCreditsCtrl::DrawBackground;
	m_dwBackgroundPaintLParam = GetSysColor(COLOR_BTNFACE);//(DWORD)m_crBackgroundColor;
	m_hLinkCursor = NULL;
	m_hDefaultCursor = NULL;
	m_bCanScroll = TRUE;
	m_bIsScrolling = FALSE;
}

CCreditsCtrl::~CCreditsCtrl()
{
}


BEGIN_MESSAGE_MAP(CCreditsCtrl, CWnd)
	//{{AFX_MSG_MAP(CCreditsCtrl)
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CCreditsCtrl::Create(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, UINT nDefaultCursorID, UINT nLinkCursorID)
{
	m_hDefaultCursor = nDefaultCursorID == 0 ? AfxGetApp()->LoadStandardCursor(IDC_ARROW) : AfxGetApp()->LoadCursor(nDefaultCursorID);
	if(nLinkCursorID == 0)
		SetDefaultLinkCursor();
	else
		m_hLinkCursor = AfxGetApp()->LoadCursor(nLinkCursorID);

	// register window class & create CWnd object
	if (m_lpszClassName == NULL)
		m_lpszClassName = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW);

	BOOL bResult = CreateEx(dwExStyle, m_lpszClassName, _T(""), dwStyle,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		pParentWnd->GetSafeHwnd(), (HMENU)nID, NULL );

	// start scrolling
	if(bResult)
		SetTimer(CCREDITCTRL_TIMER1,m_nTimerSpeed*10,NULL);
	
	return bResult;
}

BOOL CCreditsCtrl::Create(DWORD dwExStyle, DWORD dwStyle, UINT nPlaceholderID, CWnd* pParentWnd, UINT nID, UINT nDefaultCursorID, UINT nLinkCursorID)
{
	// get rect from placeholder and call create with the found rect
	RECT rect;
	pParentWnd->GetDlgItem(nPlaceholderID)->GetWindowRect(&rect);
	pParentWnd->ScreenToClient(&rect);
	return Create(dwExStyle, dwStyle, rect, pParentWnd, nID, nDefaultCursorID, nLinkCursorID);
}

/////////////////////////////////////////////////////////////////////////////
// CCreditsCtrl message handlers

void CCreditsCtrl::OnPaint() 
{
	static BOOL bFirstDraw = TRUE;

	CPaintDC dc(this); // device context for painting

	// init memory DC
	CDC memDC;
	memDC.CreateCompatibleDC(&dc);
	int nMemDCSave = memDC.SaveDC();
	CBitmap memBmp;
	memBmp.CreateCompatibleBitmap(&dc,m_rcClient.Width(),m_rcClient.Height());
	memDC.SelectObject(&memBmp);

	// draw backgorund
	if(m_pBackgroundPaint!=NULL)
		(*m_pBackgroundPaint)(&memDC,m_rcClient,m_bIsScrolling||bFirstDraw,m_dwBackgroundPaintLParam);

	// calculate hot rectagle position and save background at that location
	CDC hotBgDC;
	CBitmap hotbgBmp,*pOldHBgBmp;
	CRect rcHotRect;
	if(m_rcHotRect != CRect(0,0,0,0))
	{
		hotBgDC.CreateCompatibleDC(&memDC);
		hotbgBmp.CreateCompatibleBitmap(&memDC,m_rcHotRect.Width(),m_rcHotRect.Height());
		pOldHBgBmp = hotBgDC.SelectObject(&hotbgBmp);

		if(m_nBitmapHeight <= m_rcClient.bottom)
			rcHotRect = m_rcHotRect;
		else if(m_nBitmapHeight-m_nCurBitmapOffset+m_rcHotRect.top < m_rcClient.bottom)
			rcHotRect.SetRect(m_rcHotRect.left,m_nBitmapHeight-m_nCurBitmapOffset+m_rcHotRect.top,m_rcHotRect.right,m_nBitmapHeight-m_nCurBitmapOffset+m_rcHotRect.bottom);
		else
			rcHotRect.SetRect(m_rcHotRect.left,m_rcHotRect.top-m_nCurBitmapOffset,m_rcHotRect.right,m_rcHotRect.bottom-m_nCurBitmapOffset);

		hotBgDC.BitBlt(0,0,m_rcHotRect.Width(),m_rcHotRect.Height(),&memDC,rcHotRect.left,rcHotRect.top,SRCCOPY);
	}

	// draw normal bitmap
	if(m_nBitmapHeight <= m_rcClient.bottom)
	{
		CRect rect = m_rcClient;
		rect.bottom = m_nBitmapHeight;
		DrawTransparentBitmap(&m_bmpNormal,&memDC,m_crInternalTransparentColor,rect,rect);
	} else
	{
		DrawTransparentBitmap(&m_bmpNormal,&memDC,m_crInternalTransparentColor,CRect(0,0,m_rcClient.right,min(m_nBitmapHeight-m_nCurBitmapOffset,m_rcClient.bottom)),CRect(0,m_nCurBitmapOffset,0,0/*the two last values are not taken into account by DrawTransparentBitmap anyway*/));
		if(m_nBitmapHeight-m_nCurBitmapOffset < m_rcClient.bottom)
			DrawTransparentBitmap(&m_bmpNormal,&memDC,m_crInternalTransparentColor,CRect(0,m_nBitmapHeight-m_nCurBitmapOffset,m_rcClient.right,m_rcClient.bottom),CRect(0,0,0,0/*the two last values are not taken into account by DrawTransparentBitmap anyway*/));
	}

	// draw hot rect onto generic background
	if(m_rcHotRect != CRect(0,0,0,0))
	{
		memDC.BitBlt(rcHotRect.left,rcHotRect.top,rcHotRect.Width(),rcHotRect.Height(),&hotBgDC,0,0,SRCCOPY);
		DrawTransparentBitmap(&m_bmpHot,&memDC,m_crInternalTransparentColor,rcHotRect,m_rcHotRect);
		hotBgDC.SelectObject(pOldHBgBmp);
	}
	
	// copy memory DC to screen
	dc.BitBlt(0,0,m_rcClient.Width(),m_rcClient.Height(),&memDC,0,0,SRCCOPY);

	memDC.RestoreDC(nMemDCSave);

	if(bFirstDraw)
		bFirstDraw = FALSE;
}

CString CCreditsCtrl::SetDataString(LPCTSTR lpszNewString)
{
	CString sOldString = m_sData;
	m_sData = lpszNewString;
	if(IsWindow(m_hWnd))
		Initialize();
	return sOldString;
}

CString CCreditsCtrl::SetDataString(UINT nStringResourceID)
{
	CString sOldString = m_sData;
	m_sData.LoadString(nStringResourceID);
	if(IsWindow(m_hWnd))
		Initialize();
	return sOldString;
}

CString CCreditsCtrl::FormatDataString(LPCTSTR lpszFormat, ...)
{
	ASSERT(AfxIsValidString(lpszFormat));
	
	CString sOldString = m_sData; // store old string

	// let CString do the formatting
	va_list argList;
	va_start(argList, lpszFormat);
	m_sData.FormatV(lpszFormat, argList);
	va_end(argList);

	if(IsWindow(m_hWnd)) // Initialize bitmaps if we have already been Create()d
		Initialize();

	return sOldString;
}

CString CCreditsCtrl::FormatDataString(UINT nFormatID, ...)
{
	CString strFormat;
	VERIFY(strFormat.LoadString(nFormatID) != 0); // load resource string
	
	CString sOldString = m_sData; // store old string

	// let CString do the formatting
	va_list argList;
	va_start(argList, nFormatID);
	m_sData.FormatV(strFormat, argList);
	va_end(argList);

	if(IsWindow(m_hWnd)) // Initialize bitmaps if we have already been Create()d
		Initialize();

	return sOldString;
}

CString CCreditsCtrl::GetDataString()
{
	return m_sData;
}

void CCreditsCtrl::SetDefaultLinkCursor()
{
	// following code is taken from Chris Maunders hyperlink control (http://www.codeproject.com) - tnx
	if (m_hLinkCursor == NULL)		// No cursor handle - load our own
	{		
        // Get the windows directory
		CString strWndDir;
		GetWindowsDirectory(strWndDir.GetBuffer(MAX_PATH), MAX_PATH);
		strWndDir.ReleaseBuffer();

		strWndDir += _T("\\winhlp32.exe");
		// This retrieves cursor #106 from winhlp32.exe, which is a hand pointer
		HMODULE hModule = LoadLibrary(strWndDir);
		if (hModule) {
			HCURSOR hHandCursor = ::LoadCursor(hModule, MAKEINTRESOURCE(106));
			if (hHandCursor)
				m_hLinkCursor = CopyCursor(hHandCursor);
		}
		FreeLibrary(hModule);
	}
}

void CCreditsCtrl::OnTimer(UINT nIDEvent) 
{
	if(nIDEvent == CCREDITCTRL_TIMER1)
	{
		if(IsWindowVisible())
		{
			// increment bitmap offset
			if(++m_nCurBitmapOffset > m_nBitmapHeight)
				m_nCurBitmapOffset = 1;

			// update cursor
			CPoint point,pt;
			GetCursorPos(&point);
			pt = point;
			ScreenToClient(&point);
			if(m_rcClient.PtInRect(point) && WindowFromPoint(pt)==this)
			{
				CRect rect;
				int n;
				if((n = HitTest(point)) != -1)
				{
					rect = m_HotRects[n];
					SetCursor(m_hLinkCursor);
				}
				else
				{
					rect = CRect(0,0,0,0);
					SetCursor(m_hDefaultCursor);
				}
				if(rect != m_rcHotRect)
					m_rcHotRect = rect;
			}

			// update window
			Invalidate(FALSE);
			UpdateWindow();

			// set timer
			SetTimer(CCREDITCTRL_TIMER1,m_nTimerSpeed,NULL);
		}
	} else
		CWnd::OnTimer(nIDEvent);
}

void CCreditsCtrl::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	
	if(IsWindow(m_hWnd))
		GetClientRect(m_rcClient);
	Initialize();
}

void CCreditsCtrl::TransparentBlt(CDC *pSrcDC, CDC* pDestDC,COLORREF crTrans,const CRect& rcDest,const CRect& rcSrc)
{
	int SaveDestDC = pDestDC->SaveDC();
	int SaveSrcDC = pSrcDC->SaveDC();

#ifdef CCREDITCTRL_USE_TRANSPARENT_BITBLT	// use transparent BitBlts if supported?
		// Only attempt this if device supports functionality. ( untested!! )
	if(pDestDC->GetDeviceCaps(CAPS1) & C1_TRANSPARENT)
	{
		// Special transparency background mode
		pDestDC->SetBkMode(NEWTRANSPARENT);
		pDestDC->SetBkColor(crTrans);
		// Actual blt is a simple source copy; transparency is automatic.
		pDestDC->BitBlt(rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(), pSrcDC, rcSrc.left, rcSrc.top, SRCCOPY);
	} else	// if driver doesn't support transparent BitBlts, do it the hard way
	{
#endif
		// initialize memory DC and monochrome mask DC
		CDC tmpDC, maskDC;
		CBitmap bmpTmp, bmpMask;
		int SaveTmpDC, SaveMaskDC;
		tmpDC.CreateCompatibleDC(pDestDC);
		maskDC.CreateCompatibleDC(pDestDC);
		SaveTmpDC = tmpDC.SaveDC();
		SaveMaskDC = maskDC.SaveDC();
		bmpTmp.CreateCompatibleBitmap(pDestDC,rcDest.Width(),rcDest.Height());
		bmpMask.CreateBitmap(rcDest.Width(),rcDest.Height(),1,1,NULL);
		tmpDC.SelectObject(&bmpTmp);
		maskDC.SelectObject(&bmpMask);

		// copy existing data from destination dc to memory dc
		tmpDC.BitBlt(0,0,rcDest.Width(),rcDest.Height(),pDestDC,rcDest.left,rcDest.top,SRCCOPY);

		// create mask
		pSrcDC->SetBkColor(crTrans);
		maskDC.BitBlt(0,0,rcDest.Width(),rcDest.Height(),pSrcDC,rcSrc.left,rcSrc.top,SRCCOPY);

		// do some BitBlt magic
		tmpDC.SetBkColor(RGB(255,255,255));
		tmpDC.SetTextColor(RGB(0,0,0));
		tmpDC.BitBlt(0,0,rcDest.Width(),rcDest.Height(),pSrcDC,rcSrc.left,rcSrc.top,SRCINVERT);
		tmpDC.BitBlt(0,0,rcDest.Width(),rcDest.Height(),&maskDC,0,0,SRCAND);
		tmpDC.BitBlt(0,0,rcDest.Width(),rcDest.Height(),pSrcDC,rcSrc.left,rcSrc.top,SRCINVERT);
		
		// copy what we have in our memory DC to the destination DC
		pDestDC->BitBlt(rcDest.left,rcDest.top,rcDest.Width(),rcDest.Height(),&tmpDC,0,0,SRCCOPY);

		// clean up
		tmpDC.RestoreDC(SaveTmpDC);
		maskDC.RestoreDC(SaveMaskDC);

#ifdef CCREDITCTRL_USE_TRANSPARENT_BITBLT
	}
#endif
	pDestDC->RestoreDC(SaveDestDC);
	pSrcDC->RestoreDC(SaveSrcDC);
}

void CCreditsCtrl::DrawTransparentBitmap(CBitmap *pBitmap, CDC* pDC,COLORREF crTrans,const CRect& rcDest,const CRect& rcSrc)
{
	int SaveImageDC;

	// initialize image DC
	CDC imageDC;
	imageDC.CreateCompatibleDC(pDC);
	SaveImageDC = imageDC.SaveDC();
	imageDC.SelectObject(pBitmap);

	TransparentBlt(&imageDC,pDC,crTrans,rcDest,rcSrc);

	// clean up
	imageDC.RestoreDC(SaveImageDC);
}

void CCreditsCtrl::Initialize()
{
	//// [Initialize] ///////////////////////////////////////
	//                                                     //
	//  Create bitmaps and calc hot regions from m_sData   //
	//                                                     //
	/////////////////////////////////////////////////////////
	int nMaxHeight = 5000;

	// initialize normal and hot DCs
	CDC *pDC = GetDC();
	CDC normalDC, hotDC;
	normalDC.CreateCompatibleDC(pDC);
	hotDC.CreateCompatibleDC(pDC);
	int nSaveDCNormal = normalDC.SaveDC();
	int nSaveDCHot = hotDC.SaveDC();

	// initialize bitmaps
	if(m_bmpNormal.m_hObject)
		m_bmpNormal.DeleteObject();
	m_bmpNormal.CreateCompatibleBitmap(pDC,m_rcClient.Width(),nMaxHeight);
	if(m_bmpHot.m_hObject)
		m_bmpHot.DeleteObject();
	m_bmpHot.CreateCompatibleBitmap(pDC,m_rcClient.Width(),nMaxHeight);

	// select bitmaps into DCs
	normalDC.SelectObject(&m_bmpNormal);
	hotDC.SelectObject(&m_bmpHot);

	// fill with transparent color
	normalDC.FillSolidRect(0,0,m_rcClient.right,nMaxHeight,m_crInternalTransparentColor);
	hotDC.FillSolidRect(0,0,m_rcClient.right,nMaxHeight,m_crInternalTransparentColor);

	CString sData = m_sData;
	
	// substitute line break tags with newline characters
//	sData.Remove('\n');
//	sData.Replace("<br>","\n");
//	sData.Replace("<p>","\n\n");

	// make sure we get the last line displayed
	sData += '\n';

	// variables used for parsing
	CList<font_attribs,font_attribs&> font_attribs_tree;
	font_attribs fa;
	fa.bBold = FALSE;
	fa.bItalic = FALSE;
	fa.bUnderline = FALSE;
	fa.bStrikeout = FALSE;
	fa.crBkColor = CLR_NONE;
	fa.crColor = RGB(0,0,0);
	fa.nSize = 12;
	strcpy(fa.szName,"Arial");
	font_attribs_tree.AddTail(fa); // default font
	CList<general_attribs,general_attribs&> general_attribs_tree;
	general_attribs ga;
	ga.nAlign = 1;
	ga.nVAlign = 1;
	ga.nMaxWidth = m_rcClient.Width();
	ga.nMaxHeight = nMaxHeight;
	general_attribs_tree.AddTail(ga); // default alignment
	font_attribs link;
	BOOL bInsideTag = FALSE;
	CString sCurTagName;
	CString sCurElement;
	CString sCurOption;
	int nCurHPos = 0;
	int nCurVPos = 0;
	int nCurLineHeight = 0;
	CArray<line_rect,line_rect&> arcLineRects; // list containg information about the elements in the current line. used for vertical alignment of these element at line break.
	BOOL bIsLineEmpty = TRUE;
	BOOL bIsOption = FALSE;
	TCHAR cTmp;
	COLORREF crHrColor;
	int nHrWidth;
	int nHrSize;
	int nHrAlign;
	CString sCurLink;
	COLORREF crBitmap;
	int nBitmapBorder;
	CString sBitmap;

	CDC lineDC;
	lineDC.CreateCompatibleDC(&normalDC);
	CBitmap lineBmp;
	lineBmp.CreateCompatibleBitmap(&normalDC,ga.nMaxWidth,ga.nMaxHeight);
	CBitmap *pOldBmp = lineDC.SelectObject(&lineBmp);

	CDC hover_lineDC;
	hover_lineDC.CreateCompatibleDC(&hotDC);
	CBitmap hover_lineBmp;
	hover_lineBmp.CreateCompatibleBitmap(&hotDC,ga.nMaxWidth,ga.nMaxHeight);
	CBitmap *pOldHoverBmp = hover_lineDC.SelectObject(&hover_lineBmp);

	// main parsing loop... processing character by character
	//  (don't even _try_ to understand what's going on here :)
	for(int i = 0; i < sData.GetLength() && i >= 0; i++)
	{
		if(!bInsideTag)
		{
			if(sData[i] == '<')
			{
				if(sCurElement != "")
				{
					Parse_AppendText(&lineDC,&hover_lineDC,&nCurHPos,&nCurVPos,&nCurLineHeight,&arcLineRects,&general_attribs_tree.GetTail(),&font_attribs_tree.GetTail(),sCurElement, sCurLink, link);
					bIsLineEmpty = FALSE;
				}
				sCurTagName = "";
				sCurElement = "";
				bInsideTag = TRUE;
				continue;
			}
			if(sData[i] == '\n') // line break
			{
				if(bIsLineEmpty) // if line is empty add the height of a space with the current font
				{
					fa = font_attribs_tree.GetTail();
					CFont font;
					font.CreateFont(-fa.nSize,0,0,0,fa.bBold?FW_BOLD:0,fa.bItalic,fa.bUnderline,fa.bStrikeout,0,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,DEFAULT_PITCH|FF_DONTCARE,&fa.szName[0]);
					CFont *pOldFont = lineDC.SelectObject(&font);
					CRect rect(0,0,ga.nMaxWidth,ga.nMaxHeight);
					lineDC.DrawText(" ",rect,DT_CALCRECT);
					lineDC.SelectObject(pOldFont);
					nCurVPos += rect.Height();
				} else
				{
					if(sCurElement != "")
						Parse_AppendText(&lineDC,&hover_lineDC,&nCurHPos,&nCurVPos,&nCurLineHeight,&arcLineRects,&general_attribs_tree.GetTail(),&font_attribs_tree.GetTail(),sCurElement,sCurLink,link);
					Parse_VAlignLine(&normalDC,&hotDC,&lineDC,&hover_lineDC,nCurHPos,nCurVPos,nCurLineHeight,&arcLineRects,&general_attribs_tree.GetTail());
					nCurVPos += nCurLineHeight;
					bIsLineEmpty = TRUE;
				}
				arcLineRects.RemoveAll();
				nCurLineHeight = 0;
				nCurHPos = 0;
				sCurElement = "";
				continue;
			}
			sCurElement += sData[i];
			bIsLineEmpty = FALSE;
		} else
		{
			if(sData[i] == '>')
			{
				if(sCurTagName == "font") // <font face="s" size="n" style="[-b|b][-i|i][-u|u][-s|s]" color="n,n,n" background="n,n,n">
				{
					font_attribs_tree.AddTail(fa);
					general_attribs_tree.AddTail(ga);
				} else if(sCurTagName == "" && sCurElement == "/font") // closing font tag.. revove the last attributes from the lists
				{
					if(font_attribs_tree.GetCount() > 1)
						font_attribs_tree.RemoveTail();
					if(general_attribs_tree.GetCount() > 1)
						general_attribs_tree.RemoveTail();
				} else if(sCurTagName == "" && sCurElement == "hr") // no parameters specified for the hr tag.. use the defaults
				{
					crHrColor = GetSysColor(COLOR_BTNSHADOW); // default color
					nHrWidth = ga.nMaxWidth-100; // default width
					nHrSize = 2;	// default height
					nHrAlign = 1;	// center by default
					sCurTagName = "hr";
				}
				if(sCurTagName == "hr") // wrap line is needed and draw rect
				{
					if(!bIsLineEmpty)
					{
						Parse_VAlignLine(&normalDC,&hotDC,&lineDC,&hover_lineDC,nCurHPos,nCurVPos,nCurLineHeight,&arcLineRects,&general_attribs_tree.GetTail());
						nCurVPos += nCurLineHeight;
						bIsLineEmpty = TRUE;
					}
					arcLineRects.RemoveAll();
					nCurLineHeight = 0;
					nCurHPos = 0;
					CRect rect;
					rect.left = nHrAlign == 0 ? 0 : (nHrAlign == 2 ? ga.nMaxWidth-nHrWidth : ga.nMaxWidth/2-nHrWidth/2);
					rect.right = rect.left + nHrWidth;
					rect.top = nCurVPos + 2;
					rect.bottom = rect.top + nHrSize;
					normalDC.FillSolidRect(rect,crHrColor);
					nCurVPos += 4+nHrSize;
				} else if(sCurTagName == "" && sCurElement== "/a" && sCurLink != "") // if we have an ending link tag AND valid link action and link region...
					sCurLink = "";
				else if(sCurTagName == "img" && sBitmap != "")
				{
					if(sBitmap[0]=='#') // only resource bitmaps allowed at this time
					{
						CBitmap bmp;
						bmp.LoadBitmap(atoi(sBitmap.Mid(1)));
						Parse_AppendBitmap(&lineDC,&hover_lineDC,&nCurHPos,&nCurVPos,&nCurLineHeight,&arcLineRects,&general_attribs_tree.GetTail(),&bmp, crBitmap, nBitmapBorder, sCurLink, link);
						bIsLineEmpty = FALSE;
					}
					crBitmap = CLR_NONE;
					nBitmapBorder = 0;
					sBitmap = "";
				} else if(sCurTagName == "br" || (sCurTagName == "" && sCurElement== "br")) // just substitute with newline character
				{
					sData.SetAt(i,'\n');
					i--;
				} else if(sCurTagName == "p" || (sCurTagName == "" && sCurElement== "p")) // just substitute with 2 newline characters
				{
					sData.SetAt(i,'\n');
					sData.SetAt(i-1,'\n');
					i-= 2;
				}
				sCurElement = "";
				bInsideTag = FALSE;
				continue;
			}
			if(sData[i] == ' ' && !bIsOption)
			{
				if(sCurElement != "")
				{
					if(sCurTagName == "")
					{
						sCurTagName = sCurElement;
						sCurTagName.MakeLower();
						if(sCurTagName == "font") // store latest font attributes. these are the ones that are modified by the font tags parameters
						{
							fa = font_attribs_tree.GetTail();
							ga = general_attribs_tree.GetTail();
						} else if(sCurTagName == "hr") // set default hr options...
						{
							crHrColor = GetSysColor(COLOR_BTNSHADOW);
							nHrWidth = ga.nMaxWidth-10;
							nHrSize = 2;
							nHrAlign = 1;
						} else if(sCurTagName == "a") // init link hot attributes
						{
							link = font_attribs_tree.GetTail();
							link.crColor = 0xeeffffff;
							link.crBkColor = 0xeeffffff;
							link.bBold = -10;
							link.bItalic = -10;
							link.bUnderline = -10;
							link.bStrikeout = -10;
							link.nSize = 0;
							link.szName[0] = '\0';
						} else if(sCurTagName == "img")
						{
							nBitmapBorder = 2;
							crBitmap = CLR_NONE;
							sBitmap = "";
						}
					} else
					{
						sCurOption = sCurTagName;
						sCurOption.MakeLower();
					}
				}
				sCurElement = "";
				continue;
			}
			if(sData[i] == '"' || sData[i] == '\'') // this happens when we have a new parameter value to parse
			{
				if(bIsOption && sData[i]==cTmp) // "sData[i]==cTmp" : closing (double)quote has to match opening quote
				{
					if(sCurTagName == "font") // parse font tag paramaters
					{
						if(sCurOption == "size") // font size
						{
							int nSize = atoi(sCurElement);
							if(nSize > 0 && nSize < 2000) // let's be reasonable
								fa.nSize = nSize;
						} else if(sCurOption == "face") // font face
						{
							strcpy(fa.szName,sCurElement.Left(MAX_PATH-1));
						} else if(sCurOption == "style") // font style (bold (b) ,italic (i) ,underline (u) ,strikeout (s) )
						{
							if(sCurElement.Find("-b")!=-1 || sCurElement.Find("-B")!=-1)
								fa.bBold = FALSE;
							else if(sCurElement.FindOneOf("bB")!=-1)
								fa.bBold = TRUE;
							if(sCurElement.Find("-i")!=-1 || sCurElement.Find("-I")!=-1)
								fa.bItalic = FALSE;
							else if(sCurElement.FindOneOf("iI")!=-1)
								fa.bItalic = TRUE;
							if(sCurElement.Find("-u")!=-1 || sCurElement.Find("-U")!=-1)
								fa.bUnderline = FALSE;
							else if(sCurElement.FindOneOf("uU")!=-1)
								fa.bUnderline = TRUE;
							if(sCurElement.Find("-s")!=-1 || sCurElement.Find("-S")!=-1)
								fa.bStrikeout = FALSE;
							else if(sCurElement.FindOneOf("sS")!=-1)
								fa.bStrikeout = TRUE;
						} else if(sCurOption == "color") // font color
							StringToColor(sCurElement,fa.crColor);
						else if(sCurOption == "background") // font background-color
							StringToColor(sCurElement,fa.crBkColor);
						else if(sCurOption == "align") // horisontal font alignment. here we change the "general_attribs"
						{					// only the latest open font tag with this parameter takes effect at a line break!!
							sCurElement.MakeLower();
							if(sCurElement == "left")
								ga.nAlign = 0;
							else if(sCurElement == "center")
								ga.nAlign = 1;
							else if(sCurElement == "right")
								ga.nAlign = 2;
						} else if(sCurOption == "valign") // vertical font alignment. here we change the "general_attribs"
						{
							sCurElement.MakeLower();
							if(sCurElement == "top")
								ga.nVAlign = 0;
							else if(sCurElement == "middle")
								ga.nVAlign = 1;
							else if(sCurElement == "bottom")
								ga.nVAlign = 2;
						}
					} else if(sCurTagName == "a")
					{
						if(sCurOption == "href") // what to do
							sCurLink = sCurElement;
						else if(sCurOption == "size") // font size
						{
							int nSize = atoi(sCurElement);
							if(nSize > 0 && nSize < 2000) // let's be reasonable
								link.nSize = nSize;
						} else if(sCurOption == "face") // font face
						{
							strcpy(link.szName,sCurElement.Left(MAX_PATH-1));
						} else if(sCurOption == "style") // font style (bold (b) ,italic (i) ,underline (u) ,strikeout (s) )
						{
							if(sCurElement.Find("-b")!=-1 || sCurElement.Find("-B")!=-1)
								link.bBold = FALSE;
							else if(sCurElement.FindOneOf("bB")!=-1)
								link.bBold = TRUE;
							if(sCurElement.Find("-i")!=-1 || sCurElement.Find("-I")!=-1)
								link.bItalic = FALSE;
							else if(sCurElement.FindOneOf("iI")!=-1)
								link.bItalic = TRUE;
							if(sCurElement.Find("-u")!=-1 || sCurElement.Find("-U")!=-1)
								link.bUnderline = FALSE;
							else if(sCurElement.FindOneOf("uU")!=-1)
								link.bUnderline = TRUE;
							if(sCurElement.Find("-s")!=-1 || sCurElement.Find("-S")!=-1)
								link.bStrikeout = FALSE;
							else if(sCurElement.FindOneOf("sS")!=-1)
								link.bStrikeout = TRUE;
						} else if(sCurOption == "color") // font color
							StringToColor(sCurElement,link.crColor);
						else if(sCurOption == "background") // font background-color
							StringToColor(sCurElement,link.crBkColor);
					} else if(sCurTagName == "img") // image tag: <img src="#resourceID">
					{
						// TODO: alow usage of filenames in <img> tag
						if(sCurOption == "src" && sCurElement != "")
							sBitmap = sCurElement;
						if(sCurOption == "color")
							StringToColor(sCurElement,crBitmap);
						if(sCurOption == "border" && sCurElement != "")
							nBitmapBorder = atoi(sCurElement);
					} else if(sCurTagName == "hr") // horisontal ruler
					{
						if(sCurElement != "")
						{
							if(sCurOption == "color") // color
								StringToColor(sCurElement,crHrColor);
							else if(sCurOption == "width") // width
								nHrWidth = atoi(sCurElement);
							else if(sCurOption == "size") // height
								nHrSize = atoi(sCurElement);
							else if(sCurOption == "align") // horz alignment
							{
								sCurElement.MakeLower();
								if(sCurElement=="left")
									nHrAlign = 0;
								else if(sCurElement=="right")
									nHrAlign = 2;
								else
									nHrAlign = 1;
							}
						}
					} else if((sCurTagName == "vspace") && (sCurOption == "size")) // vertical space
					{
						if(!bIsLineEmpty) // insert linebreak only is line isn't empty
						{
							Parse_VAlignLine(&normalDC,&hotDC,&lineDC,&hover_lineDC,nCurHPos,nCurVPos,nCurLineHeight,&arcLineRects,&general_attribs_tree.GetTail());
							nCurVPos += nCurLineHeight;
							bIsLineEmpty = TRUE;
						}
						arcLineRects.RemoveAll();
						nCurLineHeight = 0;
						nCurHPos = 0;
						nCurVPos += atoi(sCurElement); // add "size" parameters value to vertical offset
					} else if((sCurTagName == "hspace") && (sCurOption == "size")) // horisontal space
						nCurHPos += atoi(sCurElement); // add "size" parameters value to horisontal offset
					sCurElement = "";
					bIsOption = FALSE;
				} else if(sData[i-1] == '=') // parameter is beginning
				{
					sCurOption = sCurElement;
					sCurOption = sCurOption.Left(sCurOption.GetLength()-1); // remove trailing "=";
					sCurOption.MakeLower();
					sCurOption.TrimRight();
					sCurElement = "";
					cTmp = sData[i];
					bIsOption = TRUE;
				}
				continue;
			}
			sCurElement += sData[i]; // append non-formatting-significant character to curent element
		}

	}
	lineDC.SelectObject(pOldBmp);
	hover_lineDC.SelectObject(pOldHoverBmp);
	//... finished parsing

	m_nBitmapHeight = nCurVPos;

	// clean up
	normalDC.RestoreDC(nSaveDCNormal);
	hotDC.RestoreDC(nSaveDCHot);
}

void CCreditsCtrl::DrawBackground(CDC *pDC, RECT rect, BOOL bAnimate, DWORD lParam)
{
	pDC->FillSolidRect(&rect,(COLORREF)lParam);
}

void CCreditsCtrl::SetDefaultBkColor(COLORREF crColor)
{
	m_dwBackgroundPaintLParam = (DWORD)crColor; // default background color	
}

void CCreditsCtrl::Parse_AppendText(CDC *pDC, CDC *pHoverDC, int *pnCurHPos, int *pnCurVPos, int *pnCurHeight, CArray<line_rect,line_rect&>* parcLineRects, general_attribs *pga, font_attribs *pfa, CString sText, CString sCurLink, font_attribs link)
{
	CRect rect(0,0,pga->nMaxWidth,pga->nMaxHeight);
	CDC dc,hoverDC;
	CBitmap hoverBmp,bmp,*pOldHBmp;
	dc.CreateCompatibleDC(pDC);

	CFont font,hover_font,*pOldHFont;
	font.CreateFont(-pfa->nSize,0,0,0,pfa->bBold?FW_BOLD:0,pfa->bItalic,pfa->bUnderline,pfa->bStrikeout,0,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,DEFAULT_PITCH|FF_DONTCARE,pfa->szName);
	CFont *pOldFont = dc.SelectObject(&font);
	dc.SetTextColor(pfa->crColor == CLR_NONE ? m_crInternalTransparentColor : pfa->crColor);
	dc.SetBkColor(pfa->crBkColor == CLR_NONE ? m_crInternalTransparentColor : pfa->crBkColor);
	dc.SetBkMode(OPAQUE);
	
	dc.DrawText(sText,rect,DT_CALCRECT|DT_SINGLELINE);

	if(sCurLink != "")
	{
		if(link.bBold == -10) link.bBold = pfa->bBold;
		if(link.bItalic == -10) link.bItalic = pfa->bItalic;
		if(link.bUnderline == -10) link.bUnderline = pfa->bUnderline;
		if(link.bStrikeout == -10) link.bStrikeout = pfa->bStrikeout;
		if(link.crColor == 0xeeffffff) link.crColor = pfa->crColor;
		if(link.crBkColor == 0xeeffffff) link.crBkColor = pfa->crBkColor;
		if(link.nSize == 0) link.nSize = pfa->nSize;
		if(link.szName[0] == '\0') strcpy(link.szName,pfa->szName);

		CRect rect2(0,0,pga->nMaxWidth,pga->nMaxHeight);

		hoverDC.CreateCompatibleDC(pDC);

		hover_font.CreateFont(-link.nSize,0,0,0,link.bBold?FW_BOLD:0,link.bItalic,link.bUnderline,link.bStrikeout,0,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,DEFAULT_PITCH|FF_DONTCARE,link.szName);
		pOldHFont = hoverDC.SelectObject(&hover_font);

		hoverDC.DrawText(sText,rect2,DT_CALCRECT|DT_SINGLELINE);

		if(rect.Width() < rect2.Width())
			rect = rect2;

		hoverBmp.CreateCompatibleBitmap(pDC,rect.right,rect.bottom);
		pOldHBmp = hoverDC.SelectObject(&hoverBmp);

		hoverDC.FillSolidRect(rect,m_crInternalTransparentColor);
		hoverDC.SetTextColor(link.crColor == CLR_NONE ? m_crInternalTransparentColor : link.crColor);
		hoverDC.SetBkColor(link.crBkColor == CLR_NONE ? m_crInternalTransparentColor : link.crBkColor);
		hoverDC.SetBkMode(OPAQUE);

		hoverDC.DrawText(sText,rect,DT_SINGLELINE);
	}
	
	bmp.CreateCompatibleBitmap(pDC,rect.right,rect.bottom);
	CBitmap *pOldBmp = dc.SelectObject(&bmp);

	dc.FillSolidRect(rect,m_crInternalTransparentColor);
	dc.SetBkColor(pfa->crBkColor == CLR_NONE ? m_crInternalTransparentColor : pfa->crBkColor);
	dc.DrawText(sText,rect,DT_SINGLELINE);

	if(sCurLink != "")
	{
		Parse_AppendElement(pDC,pHoverDC,pnCurHPos,pnCurVPos,pnCurHeight,parcLineRects,pga,rect.Width(),rect.Height(),&dc,&hoverDC,sCurLink);
		hoverDC.SelectObject(pOldHBmp);
		hoverDC.SelectObject(pOldHFont);
	}
	else
		Parse_AppendElement(pDC,pHoverDC,pnCurHPos,pnCurVPos,pnCurHeight,parcLineRects,pga,rect.Width(),rect.Height(),&dc,&dc,sCurLink);

	// clean up
	dc.SelectObject(pOldBmp);
	dc.SelectObject(pOldFont);
}

void CCreditsCtrl::Parse_AppendBitmap(CDC *pDC, CDC *pHoverDC, int *pnCurHPos, int *pnCurVPos, int *pnCurHeight, CArray<line_rect,line_rect&>* parcLineRects, general_attribs *pga, CBitmap *pBitmap, COLORREF crBorder, int nBorder, CString sCurLink, font_attribs link)
{
	BITMAP bm;
	pBitmap->GetBitmap(&bm);
	
	CDC bmpDC;
	bmpDC.CreateCompatibleDC(pDC);
	CBitmap *pOldBmp1 = bmpDC.SelectObject(pBitmap);

	CDC dc;
	dc.CreateCompatibleDC(pDC);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(pDC,bm.bmWidth+nBorder*2,bm.bmHeight+nBorder*2);
	CBitmap *pOldBmp2 = dc.SelectObject(&bmp);
	dc.FillSolidRect(0,0,bm.bmWidth+nBorder*2,bm.bmHeight+nBorder*2,crBorder==CLR_NONE?m_crInternalTransparentColor:crBorder);
	dc.BitBlt(nBorder,nBorder,bm.bmWidth,bm.bmHeight,&bmpDC,0,0,SRCCOPY);

	if(sCurLink == "")
		Parse_AppendElement(pDC,pHoverDC,pnCurHPos,pnCurVPos,pnCurHeight,parcLineRects,pga,bm.bmWidth+nBorder*2,bm.bmHeight+nBorder*2,&dc,&dc,sCurLink);
	else
	{
		CDC hoverDC;
		hoverDC.CreateCompatibleDC(pDC);
		CBitmap bmp;
		bmp.CreateCompatibleBitmap(pDC,bm.bmWidth+nBorder*2,bm.bmHeight+nBorder*2);
		CBitmap *pOldBmp = hoverDC.SelectObject(&bmp);	
		hoverDC.FillSolidRect(0,0,bm.bmWidth+nBorder*2,bm.bmHeight+nBorder*2,link.crColor==CLR_NONE?m_crInternalTransparentColor:link.crColor==0xeeffffff?crBorder:link.crColor);
		hoverDC.BitBlt(nBorder,nBorder,bm.bmWidth,bm.bmHeight,&bmpDC,0,0,SRCCOPY);
		Parse_AppendElement(pDC,pHoverDC,pnCurHPos,pnCurVPos,pnCurHeight,parcLineRects,pga,bm.bmWidth+nBorder*2,bm.bmHeight+nBorder*2,&dc,&hoverDC,sCurLink);
		hoverDC.SelectObject(pOldBmp);
	}

	// clean up
	dc.SelectObject(pOldBmp2);
	bmpDC.SelectObject(pOldBmp1);
}

void CCreditsCtrl::Parse_AppendElement(CDC *pDC, CDC *pHoverDC, int *pnCurHPos, int *pnCurVPos, int *pnCurHeight, CArray<line_rect,line_rect&>* parcLineRects, general_attribs *pga, int nElementWidth, int nElementHeight, CDC *pElementDC, CDC *pHoverElementDC, CString sCurLink)
{
	if(*pnCurHeight < nElementHeight)
		*pnCurHeight = nElementHeight;
	
	CRect rect;
	rect.left = *pnCurHPos;
	rect.top = 0;
	rect.right = rect.left+nElementWidth;
	rect.bottom = nElementHeight;

	line_rect lr;
	lr.rcRect = rect;
	lr.nVAlign = pga->nVAlign;
	lr.sLink = sCurLink;
	parcLineRects->Add(lr);

	pDC->BitBlt(rect.left,rect.top,rect.Width(),rect.Height(),pElementDC,0,0,SRCCOPY);
	pHoverDC->BitBlt(rect.left,rect.top,rect.Width(),rect.Height(),pHoverElementDC,0,0,SRCCOPY);
	*pnCurHPos += nElementWidth;	
}

void CCreditsCtrl::Parse_VAlignLine(CDC *pDestDC, CDC *pHoverDestDC, CDC *pLineDC, CDC *pHoverLineDC, int nCurHPos, int nCurVPos, int nCurHeight, CArray<line_rect,line_rect&>* parcLineRects, general_attribs *pga)
{
	{
		CArray<line_rect,line_rect&> LinkElements;

		CRect rect;
		CDC memDC;
		memDC.CreateCompatibleDC(pDestDC);
		CBitmap memBmp;
		memBmp.CreateCompatibleBitmap(pDestDC,nCurHPos,nCurHeight);
		CBitmap *pOldBmp = memDC.SelectObject(&memBmp);
		memDC.FillSolidRect(0,0,nCurHPos,nCurHeight,m_crInternalTransparentColor);
		for(int i = 0; i < parcLineRects->GetSize(); i++)
		{
			rect.left = (*parcLineRects)[i].rcRect.left;
			// calculate elements vertical position
			if((*parcLineRects)[i].nVAlign == 0) // top align
				rect.top = 0;
			else if((*parcLineRects)[i].nVAlign == 1) // middle align
				rect.top = nCurHeight/2-(*parcLineRects)[i].rcRect.bottom/2;
			else // bottom align
				rect.top = nCurHeight - (*parcLineRects)[i].rcRect.bottom;
			rect.bottom = rect.top + (*parcLineRects)[i].rcRect.bottom;
			// don't touch horz alignment
			rect.left = (*parcLineRects)[i].rcRect.left;
			rect.right = (*parcLineRects)[i].rcRect.right;

			// draw element
			memDC.BitBlt(rect.left,rect.top,rect.Width(),rect.Height(),pLineDC,(*parcLineRects)[i].rcRect.left,(*parcLineRects)[i].rcRect.top,SRCCOPY);

			// add link to list(if necessary)
			if((*parcLineRects)[i].sLink != "")
			{
				line_rect lr;
				lr.sLink = (*parcLineRects)[i].sLink;
				lr.rcRect = rect;
				LinkElements.Add(lr);
			}
		}

		rect.top = nCurVPos;
		rect.bottom = rect.top + nCurHeight;
		if(pga->nAlign == 0) // left align
			rect.left = 0;
		else if(pga->nAlign == 1) // center align
			rect.left = pga->nMaxWidth/2 - nCurHPos/2;
		else // right align
			rect.left = pga->nMaxWidth - nCurHPos;
		rect.right = rect.left + nCurHPos;

		TransparentBlt(&memDC,pDestDC,m_crInternalTransparentColor,rect,CRect(0,0,rect.Width(),rect.Height()));
		memDC.SelectObject(pOldBmp);

		// calculate horisontal offset of links in list and add them to global list
		for(i = 0; i < LinkElements.GetSize(); i++)
		{
			CRect rc = LinkElements[i].rcRect;
			rc.OffsetRect(rect.left,nCurVPos);
			m_HotRects.Add(rc);
			m_HotRectActions.Add(LinkElements[i].sLink);
		}
	}

	// do the same, but this time for the hover CD
	{
		CRect rect;
		CDC memDC;
		memDC.CreateCompatibleDC(pHoverDestDC);
		CBitmap memBmp;
		memBmp.CreateCompatibleBitmap(pHoverDestDC,nCurHPos,nCurHeight);
		CBitmap *pOldBmp = memDC.SelectObject(&memBmp);
		memDC.FillSolidRect(0,0,nCurHPos,nCurHeight,m_crInternalTransparentColor);
		for(int i = 0; i < parcLineRects->GetSize(); i++)
		{
			rect.left = (*parcLineRects)[i].rcRect.left;
			// calculate elements vertical position
			if((*parcLineRects)[i].nVAlign == 0) // top align
				rect.top = 0;
			else if((*parcLineRects)[i].nVAlign == 1) // middle align
				rect.top = nCurHeight/2-(*parcLineRects)[i].rcRect.bottom/2;
			else // bottom align
				rect.top = nCurHeight - (*parcLineRects)[i].rcRect.bottom;
			rect.bottom = rect.top + (*parcLineRects)[i].rcRect.bottom;
			// don't touch horz alignment
			rect.left = (*parcLineRects)[i].rcRect.left;
			rect.right = (*parcLineRects)[i].rcRect.right;

			// draw element
			memDC.BitBlt(rect.left,rect.top,rect.Width(),rect.Height(),pHoverLineDC,(*parcLineRects)[i].rcRect.left,(*parcLineRects)[i].rcRect.top,SRCCOPY);
		}

		rect.top = nCurVPos;
		rect.bottom = rect.top + nCurHeight;
		if(pga->nAlign == 0) // left align
			rect.left = 0;
		else if(pga->nAlign == 1) // center align
			rect.left = pga->nMaxWidth/2 - nCurHPos/2;
		else // right align
			rect.left = pga->nMaxWidth - nCurHPos;
		rect.right = rect.left + nCurHPos;

		TransparentBlt(&memDC,pHoverDestDC,m_crInternalTransparentColor,rect,CRect(0,0,rect.Width(),rect.Height()));
		memDC.SelectObject(pOldBmp);
	}
}

BOOL CCreditsCtrl::StringToColor(CString string, COLORREF &cr)
{
	int i,r,g,b;
	if(string=="")
		return FALSE;
	else if((string=="none")||(string=="transparant"))
		cr = CLR_NONE;
	else if((i = string.Find(','))==-1)
		return FALSE;
	else
	{
		r = atoi(string.Left(i));
		string.Delete(0,i+1);
		if((i = string.Find(','))==-1)
			return FALSE;
		else
		{
			g = atoi(string.Left(i));
			string.Delete(0,i+1);
			b = atoi(string);
			cr = RGB(r,g,b);
		}
	}
	return TRUE;
}

void CCreditsCtrl::OnMouseMove(UINT nFlags, CPoint point) 
{
	if(m_bIsScrolling)
	{
		m_nCurBitmapOffset = m_nScrollStart-point.y;
		m_nCurBitmapOffset %= m_nBitmapHeight;
		if(m_nCurBitmapOffset < 0)
			m_nCurBitmapOffset = m_nCurBitmapOffset+m_nBitmapHeight;
		Invalidate(FALSE);
		UpdateWindow();
		return;
	}
	if(GetCapture()==this)
		ReleaseCapture();
	else
		SetCapture();

	if(m_rcClient.PtInRect(point))
	{
		int n;
		CRect rect;
		if((n = HitTest(point)) != -1)
		{
			rect = m_HotRects[n];
			SetCursor(m_hLinkCursor);
		}
		else
		{
			rect = CRect(0,0,0,0);
			SetCursor(m_hDefaultCursor);
		}
		if(rect != m_rcHotRect)
		{
			m_rcHotRect = rect;
//			Invalidate(FALSE);
//			UpdateWindow();
		}
	}
}

void CCreditsCtrl::OnLButtonDown(UINT nFlags, CPoint point) 
{
	int n = HitTest(point);
	if(n != -1)
	{
		m_rcHotRect = CRect(0,0,0,0); // will be update next timer tick
		CString s = m_HotRectActions[n];
		if(s[0] == '#')
		{
			int i = s.Find('#',1);
			CString arg = i==-1?"":s.Mid(1,i-1);
			s = s.Mid(i==-1?1:i+1);
			void(*func)(LPCTSTR) = (void(*)(LPCTSTR))atol(s);
			if(func)
				(*func)(i==-1?NULL:(LPCTSTR)arg);
		} else
			ShellExecute(NULL,NULL,s,NULL,NULL,SW_SHOW);
	} else if(m_bCanScroll && (m_nBitmapHeight > m_rcClient.bottom))
	{
		KillTimer(CCREDITCTRL_TIMER1);
		m_bIsScrolling = TRUE;
		m_nScrollStart = point.y + m_nCurBitmapOffset;
		Invalidate();
		UpdateWindow();
		SetCapture();
	}
}

void CCreditsCtrl::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if(m_bIsScrolling)
	{
		ReleaseCapture();
		m_bIsScrolling = FALSE;
		OnTimer(CCREDITCTRL_TIMER1);
	}
}

int CCreditsCtrl::HitTest(CPoint pt)
{
	if(m_nBitmapHeight <= m_rcClient.bottom)
	{
		for(int i = 0; i < m_HotRects.GetSize(); i++)
		{
			if(m_HotRects[i].PtInRect(pt))
				return i;
		}
		return -1;
	}

	pt.y += m_nCurBitmapOffset;
	CPoint pt0 = pt;
	if(pt0.y > m_nBitmapHeight-m_nCurBitmapOffset)
		pt0.y -= m_nBitmapHeight;

	for(int i = 0; i < m_HotRects.GetSize(); i++)
	{
		if(m_HotRects[i].PtInRect(pt) || m_HotRects[i].PtInRect(pt0))
			return i;
	}
	return -1;
}