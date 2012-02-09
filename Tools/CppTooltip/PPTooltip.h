#ifndef _PPTOOLTIP_H
#define _PPTOOLTIP_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PPTooltip.h : header file

#pragma warning(disable : 4786)
#include "PPHtmlDrawer.h"
#include <vector>
#include <map>

//ENG: Comments a next line if you never use a tooltip for a menu
//RUS: Закоментируйте следующую строку, если вы не планируете использовать тултип для элементов меню
#define PPTOOLTIP_USE_MENU

#define PPTOOLTIP_CLASSNAME    _T("CPPToolTip")  // Window class name

//The 
#define UDM_TOOLTIP_FIRST		   (WM_USER + 100)
#define UDM_TOOLTIP_DISPLAY		   (UDM_TOOLTIP_FIRST) //User was changed the data
#define UDM_TOOLTIP_REPAINT		   (UDM_TOOLTIP_FIRST + 1)
#define UDM_TOOLTIP_HIDING		   (UDM_TOOLTIP_FIRST + 2)

//The behaviours
#define PPTOOLTIP_MULTIPLE_SHOW			0x00000001 //Multiple show for single control
#define PPTOOLTIP_TRACKING_MOUSE		0x00000002 //Tracking for mouse
#define PPTOOLTIP_CLOSE_LEAVEWND		0x00000004 //Close tooltip if mouse leave the control
#define PPTOOLTIP_NOCLOSE_OVER			0x00000008 //No close tooltip if mouse over him
#define PPTOOLTIP_DISABLE_AUTOPOP		0x00000010 //Disables autopop tooltip from timer

//The masks
#define PPTOOLTIP_MASK_STYLES			0x0001	// The styles for the tooltip gets from the structures
#define PPTOOLTIP_MASK_EFFECT			0x0002	// The background's type for the tooltip gets from the structures
#define PPTOOLTIP_MASK_COLORS			0x0004	// The background's colors for the tooltip gets from the structures
#define PPTOOLTIP_MASK_DIRECTION		0x0008  // The align for the tooltip gets from the structures
#define PPTOOLTIP_MASK_BEHAVIOUR		0x0010  // The behaviour for the tooltip gets from the structures
#define PPTOOLTIP_MASK_TRANSPARENCY		0x0020  // 

//The constants of the timers
#define PPTOOLTIP_TIME_INITIAL			TTDT_INITIAL
#define PPTOOLTIP_TIME_AUTOPOP			TTDT_AUTOPOP
#define PPTOOLTIP_TIME_FADEIN			4
#define PPTOOLTIP_TIME_FADEOUT			5

//Tooltip's directions
#define PPTOOLTIP_TOPEDGE_LEFT			0x00
#define PPTOOLTIP_TOPEDGE_RIGHT			0x01
#define PPTOOLTIP_TOPEDGE_CENTER		0x02
#define PPTOOLTIP_BOTTOMEDGE_LEFT		0x10
#define PPTOOLTIP_BOTTOMEDGE_RIGHT		0x11
#define PPTOOLTIP_BOTTOMEDGE_CENTER		0x12
#define PPTOOLTIP_LEFTEDGE_TOP			0x20
#define PPTOOLTIP_LEFTEDGE_BOTTOM		0x21
#define PPTOOLTIP_LEFTEDGE_VCENTER		0x22
#define PPTOOLTIP_RIGHTEDGE_TOP			0x30
#define PPTOOLTIP_RIGHTEDGE_BOTTOM		0x31
#define PPTOOLTIP_RIGHTEDGE_VCENTER		0x32
//----- Old version (1.x) ---------
#define PPTOOLTIP_LEFT_TOP				PPTOOLTIP_TOPEDGE_LEFT
#define PPTOOLTIP_RIGHT_TOP				PPTOOLTIP_TOPEDGE_RIGHT
#define PPTOOLTIP_LEFT_BOTTOM			PPTOOLTIP_BOTTOMEDGE_LEFT
#define PPTOOLTIP_RIGHT_BOTTOM			PPTOOLTIP_BOTTOMEDGE_RIGHT

#ifdef PPTOOLTIP_USE_MENU
//ENG: Anchor's position about a menu item
//RUS: Выравнивание кончика тултипа относительно элемента меню
#define PPTOOLTIP_MENU_LEFT		0x00
#define PPTOOLTIP_MENU_RIGHT	0x01
#define PPTOOLTIP_MENU_CENTER	0x02
#define PPTOOLTIP_MENU_TOP		0x00
#define PPTOOLTIP_MENU_BOTTOM	0x10
#define PPTOOLTIP_MENU_VCENTER  0x20

#define PPTOOLTIP_MENU_HMASK	0x0F
#define PPTOOLTIP_MENU_VMASK	0xF0
#endif //PPTOOLTIP_USE_MENU

// This info structure
typedef struct tagPPTOOLTIP_INFO
{
    UINT		nIDTool;		// ID of tool   
    CRect		rectBounds;		// Bounding rect for toolinfo to be displayed
	CString		sTooltip;		// The string of the tooltip
	UINT        nMask;			// The mask 
	UINT		nStyles;		// The tooltip's styles
	UINT        nDirection;		// Direction display the tooltip relate cursor point
	UINT		nEffect;		// The color's type or effects
	UINT        nBehaviour;		// The tooltip's behaviour
	BYTE        nGranularity;	// The effect's granularity
	BYTE        nTransparency;	// The factor of the window's transparency (0-100)
	COLORREF	crBegin;		// Begin Color
	COLORREF    crMid;			// Mid Color
	COLORREF	crEnd;			// End Color
} PPTOOLTIP_INFO;

// This structure sent to PPTooltip parent in a WM_NOTIFY message
typedef struct tagNM_PPTOOLTIP_DISPLAY {
    NMHDR hdr;
	HWND hwndTool;
	LPPOINT pt;
	PPTOOLTIP_INFO * ti;
} NM_PPTOOLTIP_DISPLAY;

/////////////////////////////////////////////////////////////////////////////
// CPPToolTip window

class CPPToolTip : public CWnd
{
//	friend BOOL CALLBACK EnumChildWinF(HWND hwnd, LPARAM lParam);
// Construction
public:
	BOOL Create(CWnd* pParentWnd, BOOL bBalloon = TRUE);

	CPPToolTip();
	virtual ~CPPToolTip();

// Attributes
public:
	enum {	PPTTSZ_ROUNDED_CX = 0,
			PPTTSZ_ROUNDED_CY,
			PPTTSZ_MARGIN_CX,
			PPTTSZ_MARGIN_CY,
			PPTTSZ_WIDTH_ANCHOR,
			PPTTSZ_HEIGHT_ANCHOR,
			PPTTSZ_MARGIN_ANCHOR,
			PPTTSZ_OFFSET_ANCHOR_CX,
			PPTTSZ_OFFSET_ANCHOR_CY,

			PPTTSZ_MAX_SIZES
		};

	enum {	SHOWEFFECT_NONE = 0,
			SHOWEFFECT_FADEINOUT,
			
			SHOWEFFECT_MAX
		};

	// Operations
protected:
	enum TooltipState {  
			PPTOOLTIP_STATE_HIDEN = 0,
			PPTOOLTIP_STATE_SHOWING,
			PPTOOLTIP_STATE_SHOWN,
			PPTOOLTIP_STATE_HIDING
		};

	enum TooltipType {
			PPTOOLTIP_NORMAL = 0,
			PPTOOLTIP_HELP,
			PPTOOLTIP_MENU
		};

	CPPHtmlDrawer m_drawer; //HTML drawer object
	
	HWND m_hParentWnd; // The handle of the parent window
	HWND m_hNotifyWnd; // The handle of the notified window

	BOOL m_bHyperlinkEnabled;
	BOOL m_bDebugMode;

	POINT m_ptOriginal;

	// Info about last displayed tool
	HWND  m_hwndDisplayedTool;
	TooltipType m_nTooltipType;
	PPTOOLTIP_INFO m_tiDisplayed; //Info about displayed tooltip

	// Info about last displayed tool
	BOOL  m_bDelayNextTool;
	BOOL  m_bNextToolExist;
	HWND  m_hwndNextTool;
	TooltipType m_nNextTooltipType;
	PPTOOLTIP_INFO m_tiNextTool; //Info about next tooltip
	
	// Info about current tool
	CRect m_rcCurTool;
	DWORD m_dwCurDirection;
	BYTE  m_dwCurTransparency;
	TooltipState  m_nTooltipState;

	//Colors
	COLORREF m_clrBeginBk;
	COLORREF m_clrMidBk;
	COLORREF m_clrEndBk;

	//Background
	HBITMAP m_hBitmapBk; //A bitmap with tooltip's background only
	HBITMAP m_hUnderTooltipBk;

	//Border of the tooltip
	HBRUSH m_hbrBorder;
	SIZE m_szBorder;

	//Shadow of the tooltip
	BOOL m_bGradientShadow;
	SIZE m_szOffsetShadow;
	SIZE m_szDepthShadow;
	BYTE m_nDarkenShadow;

	HRGN m_hrgnTooltip;

	//Default values for the window
	DWORD m_dwTimeAutoPop; //Retrieve the length of time the tool tip window remains visible if the pointer is stationary within a tool's bounding rectangle
	DWORD m_dwTimeInitial; //Retrieve the length of time the pointer must remain stationary within a tool's bounding rectangle before the tool tip window appears
	DWORD m_dwTimeFadeIn;
	DWORD m_dwTimeFadeOut;
	DWORD m_dwBehaviour;   //The tooltip's behaviour
	DWORD m_dwEffectBk;
	DWORD m_dwDirection;   //The default tooltip's direction
	DWORD m_dwStyles;
	BYTE  m_nGranularity;
	BYTE  m_nTransparency; //The current value of transparency
	DWORD m_dwShowEffect; //
	DWORD m_dwHideEffect;
	DWORD m_dwSizes [PPTTSZ_MAX_SIZES]; //All sizes 

#ifdef PPTOOLTIP_USE_MENU
	DWORD m_dwMenuToolPos;
#endif //PPTOOLTIP_USE_MENU

	//
	CRect m_rcTipArea; //The bound rect around the tip's area in the client coordinates.
	CRect m_rcTooltip; //The bound rect around the body of the tooltip in the client coordinates.
	CRect m_rcBoundsTooltip; //The bound rect around a tooltip include an anchor
	CRect m_rcUnderTooltip;  //The bound rect of the window under the tooltip in the screen coordinates

	//Initialize tools
	typedef std::vector<PPTOOLTIP_INFO>	arHotArea; // array of Tips rectangular spots
	typedef std::map<HWND, arHotArea>::iterator	mapIter;	// simplify reading
	std::map<HWND, arHotArea>	m_ToolMap;

	//Initialize list of toolbars
	typedef std::vector<HWND>	arToolBarWnd;
	arToolBarWnd m_wndToolBars;  // array of HWND of the toolbars

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPPToolTip)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL DestroyWindow();
	//}}AFX_VIRTUAL

// Implementation
public:
	BOOL RelayEvent(MSG* pMsg);

	//Tools
	void AddToolBar(CToolBar * pBar);
	void AddTool(CWnd * pWnd, DWORD dwIdString, LPCRECT lpRectBounds = NULL, DWORD dwIDTool = 0);
	void AddTool(CWnd * pWnd, LPCTSTR lpszString = NULL, LPCRECT lpRectBounds = NULL, DWORD dwIDTool = 0);
	void AddTool(CWnd * pWnd, PPTOOLTIP_INFO & ti);
	//To compatible with old version of CPPToolTip
	void AddTool(CWnd * pWnd, DWORD dwIdString, HICON hIcon, LPCRECT lpRectBounds = NULL, DWORD dwIDTool = 0);
	void AddTool(CWnd * pWnd, DWORD dwIdString, DWORD dwIdIcon, CSize & szIcon = CSize(0, 0), LPCRECT lpRectBounds = NULL, DWORD dwIDTool = 0);
	void AddTool(CWnd * pWnd, LPCTSTR lpszString, HICON hIcon, LPCRECT lpRectBounds = NULL, DWORD dwIDTool = 0);
	void AddTool(CWnd * pWnd, LPCTSTR lpszString, DWORD dwIdIcon, CSize & szIcon = CSize(0, 0), LPCRECT lpRectBounds = NULL, DWORD dwIDTool = 0);
	void RemoveTool(CWnd * pWnd, LPCRECT lpRectBounds = NULL);
	void RemoveAllTools();

	//Help tooltip
	void ShowHelpTooltip (LPPOINT pt, DWORD dwIdText, HICON hIcon = NULL);
	void ShowHelpTooltip (LPPOINT pt, DWORD dwIdText, DWORD dwIdIcon, CSize & szIcon = CSize(0, 0));
	void ShowHelpTooltip (LPPOINT pt, LPCTSTR lpszString, HICON hIcon = NULL);
	void ShowHelpTooltip (LPPOINT pt, LPCTSTR lpszString, DWORD dwIdIcon, CSize & szIcon = CSize(0, 0));
	void ShowHelpTooltip (LPPOINT pt, PPTOOLTIP_INFO & ti);
	void HideTooltip();

	// color's functions
	void SetColorBk(COLORREF color);
	void SetColorBk(COLORREF clrBegin, COLORREF clrEnd);
	void SetColorBk(COLORREF clrBegin, COLORREF clrMid, COLORREF clrEnd);
	void SetEffectBk(DWORD dwEffect, BYTE nGranularity = 5);

	//behaviour's methods
	void SetBehaviour(DWORD dwBehaviour = 0);
	DWORD GetBehaviour();

	void SetDelayTime(DWORD dwDuration, DWORD dwTime);
	DWORD GetDelayTime(DWORD dwDuration) const;

	void SetImageList(UINT nIdBitmap, int cx, int cy, int nCount, COLORREF crMask = RGB(255, 0, 255));
	void SetImageList(HBITMAP hBitmap, int cx, int cy, int nCount, COLORREF crMask = RGB(255, 0, 255));

	//functions for sizes
	void SetSize(DWORD nSizeIndex, DWORD nValue);
	DWORD GetSize(DWORD nSizeIndex);
	void SetDefaultSizes(BOOL bBalloonSize = TRUE);

	//functions for direction
	void SetDirection (DWORD dwDirection = PPTOOLTIP_BOTTOMEDGE_LEFT);
	DWORD GetDirection();

	void SetCallbackHyperlink(HWND hWnd, UINT nMessage, LPARAM lParam = 0);
	
	void EnableHyperlink(BOOL bEnable = TRUE);
	void SetDebugMode(BOOL bDebug = TRUE);

	//functions for
	void  SetNotify(HWND hWnd);
	void  SetNotify(BOOL bParentNotify = TRUE);

	void SetCssStyles(LPCTSTR lpszCssStyles = NULL);
	LPCTSTR GetCssStyles();

	void EnableEscapeSequences(BOOL bEnable);

	void HideBorder();
	void SetBorder(COLORREF color, int nWidth = 1, int nHeight = 1);
	void SetBorder(HBRUSH hbr, int nWidth = 1, int nHeight = 1);

	//Transparency of tooltip
	void SetTransparency(BYTE nTransparency = 0);
	inline BYTE GetTransparency() {return m_nTransparency;};

	//Shadow of the tooltip
	void SetTooltipShadow(int nOffsetX, int nOffsetY, BYTE nDarkenPercent = 50, BOOL bGradient = TRUE, int nDepthX = 7, int nDepthY = 7);
	void SetImageShadow(int nOffsetX, int nOffsetY, BYTE nDarkenPercent = 50, BOOL bGradient = TRUE, int nDepthX = 7, int nDepthY = 7);

#ifdef PPTOOLTIP_USE_MENU
	//Methods for the menu
	void MenuToolPosition(DWORD nPos = PPTOOLTIP_MENU_LEFT | PPTOOLTIP_MENU_TOP);
	inline DWORD GetMenuToolPosition() {return m_dwMenuToolPos;};
	void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSubMenu);
    void OnEnterIdle(UINT nWhy, CWnd* pWho);
	HWND GetRunningMenuWnd();
#endif //PPTOOLTIP_USE_MENU

private:
	virtual void OnDrawBorder(HDC hDC, HRGN hRgn);

	// Generated message map functions
protected:
	void Pop();
	void KillTimers(DWORD dwIdTimer = NULL);
	void SetNewTooltip(HWND hWnd, const PPTOOLTIP_INFO & ti, BOOL bDisplayWithDelay = TRUE, TooltipType type = PPTOOLTIP_NORMAL);
	HWND GetWndFromPoint(LPPOINT lpPoint, BOOL bUseDisabled = TRUE);
	LRESULT SendNotify(LPPOINT pt, PPTOOLTIP_INFO & ti);

	BOOL IsCursorOverTooltip() const;
	inline BOOL IsVisible() const {return ((GetStyle() & WS_VISIBLE) == WS_VISIBLE);}
	BOOL  IsNotify(); //Is enabled notification

	void PrepareDisplayTooltip(LPPOINT lpPoint);
	void OnRedrawTooltip(HDC hDC, BYTE nTransparency = 0);

	void OutputTooltipOnScreen(LPPOINT lpPoint, HDC hDC = NULL);
	void SetAutoPopTimer(); //Sets autopop timer

	void FreeResources();
	CString GetDebugInfoTool(LPPOINT lpPoint);

	DWORD GetTooltipDirection(DWORD dwDirection, const LPPOINT lpPoint, LPPOINT lpAnchor, CRect & rcBody, CRect & rcFull, CRect & rcTipArea);
	HRGN GetTooltipRgn(DWORD dwDirection, int x, int y, int nWidth, int nHeight);
	
	CString GetMaxDebugString(CString str);

	BOOL FindTool(HWND hWnd, const LPPOINT lpPoint, PPTOOLTIP_INFO & ti);
	HWND FindTool(const LPPOINT lpPoint, PPTOOLTIP_INFO & ti);
	HWND FindToolBarItem(POINT point, PPTOOLTIP_INFO & ti);

	//{{AFX_MSG(CPPToolTip)
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	//}}AFX_MSG
#if _MSC_VER < 1300
    afx_msg void OnActivateApp(BOOL bActive, HTASK hTask);
#else
    afx_msg void OnActivateApp(BOOL bActive, DWORD hTask);
#endif
    afx_msg LRESULT OnRepaintWindow(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif
