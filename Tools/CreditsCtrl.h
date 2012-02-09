#if !defined(AFX_CREDITSCTRL_H__0D506182_3886_4D4C_A609_DA8914C17718__INCLUDED_)
#define AFX_CREDITSCTRL_H__0D506182_3886_4D4C_A609_DA8914C17718__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CreditsCtrl.h : header file
//
#include <afxtempl.h>

/////////////////////////////////////////////////////////////////////////////
// CCreditsCtrl window

class CCreditsCtrl : public CWnd
{
// Construction
public:
	CCreditsCtrl();
	virtual ~CCreditsCtrl();
	BOOL Create(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID = 0, UINT nDefaultCursorID = 0, UINT nLinkCursorID = 0);
	BOOL Create(DWORD dwExStyle, DWORD dwStyle, UINT nPlaceholderID, CWnd* pParentWnd, UINT nID = 0, UINT nDefaultCursorID = 0, UINT nLinkCursorID = 0);

// Attributes
public:
	static LPCTSTR m_lpszClassName;

	// local structures used during parsing
	typedef struct
	{
		TCHAR szName[MAX_PATH];
		int nSize;
		int bBold;
		int bItalic;
		int bUnderline;
		int bStrikeout;
		COLORREF crColor;
		COLORREF crBkColor;
	} font_attribs;
	typedef struct
	{
		int nAlign; // 0: left, 1: center, 2: right
		int nVAlign; // 0: top, 1: middle, 2: bottom
		int nMaxWidth;
		int nMaxHeight;
	} general_attribs;
	typedef struct
	{
		RECT rcRect;
		int nVAlign;
		CString sLink;
	} line_rect;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCreditsCtrl)
	//}}AFX_VIRTUAL

// Implementation
public:

	////// Stuff that can be customized to fit your neeeds /////////////////

	BOOL m_bCanScroll;		// TRUE (default) if the user is allowed to manually scroll through the control using drag & drop
	int m_nCurBitmapOffset;	// normally you shouldn't touch this one
	int m_nTimerSpeed;		// default: 40
	HCURSOR m_hDefaultCursor;	// normally you would want to set this one in Create()
	HCURSOR m_hLinkCursor;		// normally you would want to set this one in Create()
	CString m_sData;			// use SetDataString() or FormatDataString()
	void(*m_pBackgroundPaint)(CDC*,RECT,BOOL,DWORD);	// background drawing function
	DWORD m_dwBackgroundPaintLParam;	// lParam to m_pBackgroundPaint.. by default this is the background color
	COLORREF m_crInternalTransparentColor;	// normally you would use a color close or equal to the background color
	CArray<CRect,CRect&> m_HotRects;
	CArray<CString,CString&> m_HotRectActions;
	
	////// Functions you always should use /////////////////////////////////

	// sets data string...
	CString SetDataString(LPCTSTR lpszNewString);
	CString SetDataString(UINT nStringResourceID);
	CString FormatDataString(LPCTSTR lpszFormat, ...);
	CString FormatDataString(UINT nFormatID, ...);
	// returns data string
	CString GetDataString();

	////// Functions you might want to use /////////////////////////////////

	// returns the index if the m_HotRects array the point matches or -1 for no hit
	int HitTest(CPoint pt);
	// sets the background color used by the default background drawer function
	void SetDefaultBkColor(COLORREF crColor);
	// parses the m_sData string, creates m_bmpNormal & m_bmpHot and fill the m_HotRects & m_HotRectActions arrays
	void Initialize();
	// set link cursor to a hand icon
	void SetDefaultLinkCursor();

	////// Functions you probably won't need ///////////////////////////////

	// helpers for parsing
	void Parse_AppendText(CDC *pDC, CDC *pHoverDC, int *pnCurHPos, int *pnCurVPos, int *pnCurHeight, CArray<line_rect,line_rect&>* parcLineRects, general_attribs *pga, font_attribs *pfa, CString sText, CString sCurLink, font_attribs link);
	void Parse_AppendBitmap(CDC *pDC, CDC *pHoverDC, int *pnCurHPos, int *pnCurVPos, int *pnCurHeight, CArray<line_rect,line_rect&>* parcLineRects, general_attribs *pga, CBitmap *pBitmap, COLORREF crBorder, int nBorder, CString sCurLink, font_attribs link);
	void Parse_AppendElement(CDC *pDC, CDC *pHoverDC, int *pnCurHPos, int *pnCurVPos, int *pnCurHeight, CArray<line_rect,line_rect&>* parcLineRects, general_attribs *pga, int nElementWidth, int nElementHeight, CDC *pElementDC, CDC *pHoverElementDC, CString sCurLink);
	void Parse_VAlignLine(CDC *pDestDC, CDC *pHoverDestDC, CDC *pLineDC, CDC *pHoverLineDC, int nCurHPos, int nCurVPos, int nCurHeight, CArray<line_rect,line_rect&>* parcLineRects, general_attribs *pga);
	BOOL StringToColor(CString string, COLORREF &cr);
	// default background drawer
	static void DrawBackground(CDC *pDC, RECT rect, BOOL bAnimate, DWORD lParam);
	// drawing routines
	void TransparentBlt(CDC *pSrcDC, CDC* pDestDC,COLORREF crTrans,const CRect& rcDest,const CRect& rcSrc);
	void DrawTransparentBitmap(CBitmap *pBitmap, CDC* pDC,COLORREF crTrans,const CRect& rcDest,const CRect& rcSrc);

	////// Stuff you really shouldn't touch ////////////////////////////////

protected:
	// Internally used variables
	CRect m_rcClient;
	CRect m_rcHotRect;
	BOOL m_bIsScrolling;
	int m_nScrollStart;
	int m_nBitmapHeight;
	CBitmap m_bmpHot;
	CBitmap m_bmpNormal;

	// Generated message map functions
protected:
	//{{AFX_MSG(CCreditsCtrl)
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CREDITSCTRL_H__0D506182_3886_4D4C_A609_DA8914C17718__INCLUDED_)
