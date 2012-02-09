// PPTooltip.cpp : implementation file
//
#include "../../stdafx.h"
#include "PPTooltip.h"

// allow multimonitor-aware code on Win95 systems
// comment out the first line if you already define it in another file
// comment out both lines if you don't care about Win95
#define COMPILE_MULTIMON_STUBS
#include "multimon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define TIMER_HIDE		0x101 //the identifier of the timer for hide the tooltip
#define TIMER_SHOWING	0x102 //the identifier of the timer for tooltip's fade in
#define TIMER_SHOW		0x100 //the identifier of the timer for show the tooltip
#define TIMER_HIDING	0x103 //the identifier of the timer for tooltip's fade out

#define PERCENT_STEP_FADEIN		20 //How mush percent will adding during fade in
#define PERCENT_STEP_FADEOUT	20 //How mush percent will adding during fade out
#define PERCENT_MAX_TRANSPARENCY 100 //How mush percent by maximum transparency
#define PERCENT_MIN_TRANSPARENCY 0 //How mush percent by minimum transparency

#define MAX_LENGTH_DEBUG_STRING 25 //

struct PPTOOLTIP_ENUM_CHILD 
{
	HWND hwndExclude;
	HWND hWnd;
	POINT ptScreen;
	DWORD dwFlags;
};

BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam) 
{ 
	PPTOOLTIP_ENUM_CHILD * structEnum = (PPTOOLTIP_ENUM_CHILD*)lParam;
	if (hwndChild != structEnum->hwndExclude)
	{
		DWORD dwStyle = ::GetWindowLong(hwndChild, GWL_STYLE);
		
		if (structEnum->dwFlags & CWP_SKIPDISABLED)
		{
			if (dwStyle & WS_DISABLED)
				return TRUE;
		}

		if ((dwStyle & 0xF) != BS_GROUPBOX)
		{
			CRect rcWindow;// = wi.rcWindow;
			::GetWindowRect(hwndChild, &rcWindow);
			
			if (rcWindow.PtInRect(structEnum->ptScreen))
			{
				structEnum->hWnd = hwndChild;
				return FALSE;
			} //if 
		} //if
	} //if

	return TRUE;
} //End EnumChildProc

//////////////////
// Note that windows are enumerated in top-down Z-order, so the menu
// window should always be the first one found.
//
static BOOL CALLBACK MyEnumProc(HWND hwnd, LPARAM lParam)
{
	char buf[16];
	GetClassName(hwnd,buf,sizeof(buf));
	if (strcmp(buf,"#32768")==0)  // special classname for menus
	{
		*((HWND*)lParam) = hwnd;	 // found it
		return FALSE;
	}
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CPPToolTip

CPPToolTip::CPPToolTip()
{
	// Default values
	m_dwTimeAutoPop = 5000;
	m_dwTimeInitial = 500;
	m_dwTimeFadeIn = 500;
	m_dwTimeFadeOut = 500;
	m_dwBehaviour = PPTOOLTIP_CLOSE_LEAVEWND | PPTOOLTIP_NOCLOSE_OVER;	 //The tooltip's behaviour
	m_dwEffectBk = 0;
	m_dwDirection = 0;
	m_dwStyles = 0;
	m_nGranularity = 0;
	m_nTransparency = 0;
	m_bDelayNextTool = FALSE;
	m_dwShowEffect = SHOWEFFECT_FADEINOUT;
	m_dwHideEffect = SHOWEFFECT_FADEINOUT;

	m_nTooltipState = PPTOOLTIP_STATE_HIDEN;
	m_nTooltipType = PPTOOLTIP_NORMAL;
	m_nNextTooltipType = PPTOOLTIP_NORMAL;

	m_ptOriginal.x = m_ptOriginal.y = 0;

	m_rcCurTool.SetRectEmpty();

	m_hwndDisplayedTool = NULL;

	m_hBitmapBk = NULL;
	m_hUnderTooltipBk = NULL;

	m_hbrBorder = NULL;
	m_hrgnTooltip = NULL;

	SetColorBk(::GetSysColor(COLOR_INFOBK));
	SetBorder(::GetSysColor(COLOR_INFOTEXT));
	EnableHyperlink();
	SetNotify(FALSE);
	SetDefaultSizes();
	SetDirection();
	SetBehaviour();
	SetDebugMode(FALSE);
	SetDelayTime(PPTOOLTIP_TIME_INITIAL, 500);
	SetDelayTime(PPTOOLTIP_TIME_AUTOPOP, 5000);
	SetDelayTime(PPTOOLTIP_TIME_FADEIN, 0);
	SetDelayTime(PPTOOLTIP_TIME_FADEOUT, 0);
	SetTooltipShadow(6, 6);

#ifdef PPTOOLTIP_USE_MENU
	MenuToolPosition();
#endif //PPTOOLTIP_USE_MENU
	
	// Register the window class if it has not already been registered.
	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if(!(::GetClassInfo(hInst, PPTOOLTIP_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		wndcls.style			= CS_SAVEBITS;
		wndcls.lpfnWndProc		= ::DefWindowProc;
		wndcls.cbClsExtra		= wndcls.cbWndExtra = 0;
		wndcls.hInstance		= hInst;
		wndcls.hIcon			= NULL;
		wndcls.hCursor			= LoadCursor(hInst, IDC_ARROW );
		wndcls.hbrBackground	= NULL;
		wndcls.lpszMenuName		= NULL;
		wndcls.lpszClassName	= PPTOOLTIP_CLASSNAME;
		
		if (!AfxRegisterClass(&wndcls))
			AfxThrowResourceException();
	}
}

CPPToolTip::~CPPToolTip()
{
	FreeResources();
	RemoveAllTools();
	HideBorder();
}


BEGIN_MESSAGE_MAP(CPPToolTip, CWnd)
	//{{AFX_MSG_MAP(CPPToolTip)
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_SETCURSOR()
	ON_WM_ACTIVATEAPP()
	//}}AFX_MSG_MAP
	ON_MESSAGE(UDM_TOOLTIP_REPAINT, OnRepaintWindow)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CPPToolTip message handlers
BOOL CPPToolTip::Create(CWnd* pParentWnd, BOOL bBalloon /* = TRUE */) 
{
	TRACE(_T("CPPToolTip::Create\n"));
	
	ASSERT_VALID(pParentWnd);
	
	DWORD dwStyle = WS_POPUP; 
	DWORD dwExStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
	m_hParentWnd = pParentWnd->GetSafeHwnd();
	
	if (!CreateEx(dwExStyle, PPTOOLTIP_CLASSNAME, NULL, dwStyle, 0, 0, 0, 0, pParentWnd->GetSafeHwnd(), NULL, NULL))
		return FALSE;

	//
	SetDefaultSizes(bBalloon);
	m_drawer.SetCallbackRepaint(this->GetSafeHwnd(), UDM_TOOLTIP_REPAINT);

	return TRUE;
} //End of Create

BOOL CPPToolTip::DestroyWindow() 
{
//	SetWindowRgn(NULL, FALSE);
	Pop();
	return CWnd::DestroyWindow();
} //End of DestroyWindow

/////////////////////////////////////////////////////////////////////
//		A tooltip with PPTOOLTIP_DISABLE_AUTOPOP behaviour don't hide on 
//	change active application
//-------------------------------------------------------------------
// Fixed by vanhoopy (July 10, 2003)
/////////////////////////////////////////////////////////////////////
#if _MSC_VER < 1300
void CPPToolTip::OnActivateApp(BOOL bActive, HTASK hTask)
#else
void CPPToolTip::OnActivateApp(BOOL bActive, DWORD hTask)
#endif
{
	CWnd::OnActivateApp(bActive, hTask);
	
	if (!bActive) 
		Pop();
} //End of the WM_ACTIVATEAPP handler

LRESULT CPPToolTip::SendNotify(LPPOINT pt, PPTOOLTIP_INFO & ti) 
{ 
	TRACE(_T("CPPToolTip::SendNotify()\n")); 
	// Make sure this is a valid window  
	if (!IsWindow(GetSafeHwnd())) 
		return 0L; 
	// See if the user wants to be notified  
	if (!IsNotify()) 
		return 0L; 
	
	NM_PPTOOLTIP_DISPLAY lpnm; 
	lpnm.hwndTool = m_hwndNextTool;
	lpnm.pt = pt;  
	lpnm.ti = &ti; 
	lpnm.hdr.hwndFrom = m_hWnd; 
	lpnm.hdr.idFrom   = GetDlgCtrlID(); 
	lpnm.hdr.code     = UDM_TOOLTIP_DISPLAY; 
	
	::SendMessage(m_hNotifyWnd, WM_NOTIFY, lpnm.hdr.idFrom, (LPARAM)&lpnm);  

	return 0L;
} //End of SendNotify

/////////////////////////////////////////////////////////////////////
// CPPToolTip::IsNotify()
//		This function determines will be send the notification messages from 
//	the control or not before display.
//-------------------------------------------------------------------
// Return value:
//	TRUE if the control will be notified the specified window
///////////////////////////////////////////////////////////////////////
BOOL CPPToolTip::IsNotify()
{
	TRACE(_T("CPPToolTip::IsNotify\n"));
	
	return (BOOL)(m_hNotifyWnd != NULL);
}  //End of IsNotify

BOOL CPPToolTip::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	CPoint ptClient;
	::GetCursorPos(&ptClient);
	ScreenToClient(&ptClient);
	TRACE (_T("CPPToolTip::OnSetCursor(x=%d, y=%d)\n"), ptClient.x, ptClient.y);
	if (m_drawer.OnSetCursor(ptClient))
		return TRUE; //The cursor over the hyperlink
	
	::SetCursor(::LoadCursor(NULL, IDC_ARROW));

//	return CWnd::OnSetCursor(pWnd, nHitTest, message);
	return TRUE;
} //End of the WM_SETCURSOR handler

LRESULT CPPToolTip::OnRepaintWindow(WPARAM wParam, LPARAM lParam)
{
	TRACE (_T("CPPToolTip::OnRepaintWindow()\n"));
	if (m_bHyperlinkEnabled)
	{
		//Window's repaint enabled
		CDC * pDC = GetDC();
		OnRedrawTooltip(pDC->GetSafeHdc());
		ReleaseDC(pDC);
	}
    return TRUE;
} //End of the UDM_TOOLTIP_REPAINT handler

void CPPToolTip::OnDrawBorder(HDC hDC, HRGN hRgn)
{
	ASSERT (hDC);
	ASSERT (hRgn);

	::FrameRgn(hDC, hRgn, m_hbrBorder, m_szBorder.cx, m_szBorder.cy);
} //End OnDrawBorder

////////////////////////////////////////////////////////////////////////
//
//      +-----------------+    +-------------------+   +-----------------+  
//   +->|     Screen      +--->| m_hUnderTooltipBk |   |   m_hBitmapBk   |
//   |  +--------+--------+    +-------------------+   +--------+--------+
//   |           |                                            |
//   |  +--------V--------+                          +--------V--------+
//   |  |                 |     +--------------+     |                 |
//   |  |                 |     |   DrawHTML   |---->|                 |
//   |  |                 |     +--------------+     |                 |
//   |  |                 |                          |     MemDC       |
//   |  |                 |     +--------------+     |                 |
//   |  |                 |     | OnDrawBorder |---->|                 |
//   |  |     TempDC      |     +--------------+     +--------+--------+
//   |  |                 |                                   |         
//   |  |                 |     +--------------+              |         
//   |  |                 |<----+  DrawShadow  |              |         
//   |  |                 |     +--------------+              |         
//   |  |                 |                                   |         
//   |  |                 <---------ALPHA---------------------+         
//   |  |                 |
//   |  +--------+--------+
//   |           |          
//   +-----------+
//
////////////////////////////////////////////////////////////////////////
void CPPToolTip::OnRedrawTooltip(HDC hDC, BYTE nTransparency /* = 0 */)
{
	TRACE (_T("CPPToolTip::OnRedrawTooltip(Transparency = %d)\n"), nTransparency);

	//ENG: If a transparency more then max value
	//RUS: Если значение прозрачности больше максимально допустимого
	if (nTransparency > PERCENT_MAX_TRANSPARENCY)
		nTransparency = PERCENT_MAX_TRANSPARENCY;

	//ENG: If device context not passed
	//RUS: Если контекст устройства не передавался, то получаем его и устанавливаем признак автоматического удаления
	BOOL bAutoReleaseDC = FALSE;
	if (NULL == hDC)
	{
		hDC = ::GetDC(this->GetSafeHwnd());
		bAutoReleaseDC = TRUE;
	} //if

	//ENG: Creates memory context
	//RUS: Создаем контекст устройства в памяти
	HDC hMemDC = ::CreateCompatibleDC(hDC);
	HDC hBkDC = ::CreateCompatibleDC(hDC);
	HDC hTempDC = ::CreateCompatibleDC(hDC);
	HBITMAP hOldBkBitmap = (HBITMAP)::SelectObject(hBkDC, m_hBitmapBk);
	HBITMAP hBitmap = ::CreateCompatibleBitmap(hDC, m_rcBoundsTooltip.Width(), m_rcBoundsTooltip.Height());
	HBITMAP hOldBitmap = (HBITMAP)::SelectObject(hMemDC, hBitmap);
	HBITMAP hTempBitmap = ::CreateCompatibleBitmap(hDC, m_rcBoundsTooltip.Width(), m_rcBoundsTooltip.Height());
	HBITMAP hOldTempBitmap = (HBITMAP)::SelectObject(hTempDC, hTempBitmap);

	//ENG: Gets the rectangle of the tooltip without a shadow
	//RUS: Получаем размер тултипа без тени
	CRect rect = m_rcBoundsTooltip;
	rect.DeflateRect(0, 0, m_szOffsetShadow.cx, m_szOffsetShadow.cy);
	
	//ENG: Copy background to the temporary bitmap
	//RUS: Копируем фон под тултипом в память
	::BitBlt(hMemDC, 0, 0, rect.Width(), rect.Height(),
		hBkDC, 0, 0, SRCCOPY);
	
	//ENG: Draw HTML string
	//RUS: Отображаем HTML строку
	m_drawer.DrawPreparedOutput(CDC::FromHandle(hMemDC), m_tiDisplayed.sTooltip, m_rcTipArea);

	//ENG: Gets a region of a window
	//RUS: Получаем регион окна
//	HRGN hrgn = ::CreateRectRgn(0, 0, 0, 0);
//	GetWindowRgn(hrgn);

	//ENG: Draw border of the tooltip
	//RUS: Отображаем контур тултипа
	if ((NULL != m_hbrBorder) && (m_szBorder.cx) && (m_szBorder.cy))
		OnDrawBorder(hMemDC, m_hrgnTooltip);

	if (NULL == m_hUnderTooltipBk)
	{
		//ENG: Stores a background under the tooltip to a bitmap
		//RUS: Сохраняем фон под тултипом в битмап
		m_hUnderTooltipBk = ::CreateCompatibleBitmap(hDC, m_rcBoundsTooltip.Width(), m_rcBoundsTooltip.Height());
		::SelectObject(hBkDC, m_hUnderTooltipBk);
		::BitBlt(hBkDC, 0, 0, m_rcBoundsTooltip.Width(), m_rcBoundsTooltip.Height(),
				 hDC, m_rcBoundsTooltip.left, m_rcBoundsTooltip.top, SRCCOPY);
	}
	else
	{
		//ENG: Restores a background from a bitmap
		//RUS: Восстанавливаем фон из битмапа
		::SelectObject(hBkDC, m_hUnderTooltipBk);
	} //if

	//ENG: Copy a original background bitmap to the temporary DC
	//RUS: Копируем оригинальный битмап во временный контекст памяти
	::BitBlt(hTempDC, m_rcBoundsTooltip.left, m_rcBoundsTooltip.top, m_rcBoundsTooltip.Width(), m_rcBoundsTooltip.Height(),
		hBkDC, 0, 0, SRCCOPY);

	//ENG: Draws a shadow
	//RUS: Выводим тень
	if (m_szOffsetShadow.cx || m_szOffsetShadow.cy)
	{
		//ENG: Creates a mask of the tooltip for shadow
		//RUS: Создаем маску тултипа для вывода тени
		HBITMAP hMask = ::CreateCompatibleBitmap(hDC, rect.Width(), rect.Height());
		HDC hMaskDC = ::CreateCompatibleDC(hDC);
		//ENG: Creates a mask of the tooltip
		//RUS: Создание маски тултипа
		BYTE nColor = LOBYTE(::MulDiv(255, 100 - m_nDarkenShadow, 100));
		nColor += ((255 - nColor) * nTransparency) / 100;
		HBRUSH hBrush = ::CreateSolidBrush(RGB(nColor, nColor, nColor));
		HBITMAP hOldMask = (HBITMAP)::SelectObject(hMaskDC, hMask);
		::BitBlt(hMaskDC, 0, 0, rect.Width(), rect.Height(), NULL, 0, 0, WHITENESS);
		::FillRgn(hMaskDC, m_hrgnTooltip, hBrush);
		::DeleteObject(hBrush);
		::SelectObject(hMaskDC, hOldMask);
		::DeleteDC(hMaskDC);
		
		//HBITMAP hTempBmp = m_drawer.GetDrawManager()->CreateImageEffect(m_hTooltipMask, rect.Width(), rect.Height(), IMAGE_EFFECT_LIGHTEN, )
		m_drawer.GetDrawManager()->DrawShadow(hTempDC, 
											  m_szOffsetShadow.cx, 
											  m_szOffsetShadow.cy,
										      rect.Width(), rect.Height(), hMask,
			   							      m_bGradientShadow, 
											  m_szDepthShadow.cx, m_szDepthShadow.cy);
		::DeleteObject(hMask);
	} //if

	//ENG: Merges a tooltip on with the client area 
	//RUS: Накладываем тултип на клиентскую часть с альфа-наложением
	::SelectClipRgn(hTempDC, m_hrgnTooltip);
	m_drawer.GetDrawManager()->AlphaBitBlt(hTempDC, m_rcBoundsTooltip.left, m_rcBoundsTooltip.top,
											rect.Width(), rect.Height(),
											hMemDC, 0, 0, 100 - nTransparency);
	::SelectClipRgn(hTempDC, NULL);

	//ENG: Output a tooltip to the screen
	//RUS: Выводим тултип на экран
	::BitBlt(hDC, m_rcBoundsTooltip.left, m_rcBoundsTooltip.top,
		m_rcBoundsTooltip.Width(), m_rcBoundsTooltip.Height(),
		hTempDC, 0, 0, SRCCOPY);
	
	//ENG: Free resources
	//RUS: Освобождаем задействованные ресурсы
	::SelectObject(hBkDC, hOldBkBitmap);
	::SelectObject(hMemDC, hOldBitmap);
	::SelectObject(hTempDC, hOldTempBitmap);
	::DeleteObject(hBitmap);
	::DeleteObject(hTempBitmap);
	::DeleteDC(hBkDC);
	::DeleteDC(hMemDC);
	::DeleteDC(hTempDC);

	//ENG: Releases device context if needed
	//RUS: Освобождаем контекст устройства если это необходимо
	if (bAutoReleaseDC)
		::ReleaseDC(this->GetSafeHwnd(), hDC);
} //End of OnRedrawWindow

void CPPToolTip::OnPaint() 
{
	TRACE("CPPToolTip::OnPaint()\n");
	CPaintDC dc(this); // device context for painting
	
	//Copying info about current tool to displayed
	m_hwndDisplayedTool = m_hwndNextTool;
	m_tiDisplayed = m_tiNextTool;
	m_nTooltipType = m_nNextTooltipType;

	OnRedrawTooltip(dc.GetSafeHdc(), m_dwCurTransparency);
} //End of the WM_PAINT handler

BOOL CPPToolTip::PreTranslateMessage(MSG* pMsg) 
{
	RelayEvent(pMsg);
	
	return CWnd::PreTranslateMessage(pMsg);
}

BOOL CPPToolTip::RelayEvent(MSG* pMsg)
{
	ASSERT(m_hParentWnd);

	HWND hWnd = NULL;
	POINT pt;
	CRect rect;
	PPTOOLTIP_INFO ti;
	CString strTemp;

	switch(pMsg->message)
	{
	case WM_SETFOCUS:
		rect.left = 0;
		break;
	case WM_LBUTTONDOWN:
		TRACE("CPPToolTip::WM_LBUTTONDOWN\n");
		if (IsCursorOverTooltip())
		{
			//Left Button was pressed over the tooltip
			pt = pMsg->pt;
			ScreenToClient(&pt);
			m_drawer.OnLButtonDown(CPoint(pt.x, pt.y)); //
		} //if
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONDBLCLK:
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_MOUSEWHEEL:
		// The user has interupted the current tool - dismiss it
		Pop();
		break;
	case WM_MOUSEMOVE:
		if ((PPTOOLTIP_HELP == m_nTooltipType) || (PPTOOLTIP_HELP == m_nNextTooltipType))
			return FALSE;
		if ((m_ptOriginal.x != pMsg->pt.x) || (m_ptOriginal.y != pMsg->pt.y))
		{
			// The mouse pointer's position was changed

			//Initialize values
			rect.SetRectEmpty();
			m_ptOriginal = pt = pMsg->pt;
			::ScreenToClient(m_hParentWnd, &pt);
			if (m_bDebugMode)
			{
				//Debug mode
				ti.sTooltip = GetDebugInfoTool(&pt);
				ti.nMask = 0;
				m_hwndDisplayedTool = NULL;
				SetNewTooltip(this->GetSafeHwnd(), ti);
			}
			else if (IsCursorOverTooltip() && !(m_tiDisplayed.nBehaviour & PPTOOLTIP_TRACKING_MOUSE)) 
			{
				//ENG: Mouse over a tooltip and tracking mode was disabled
				//RUS: Курсор над тултипом при выключенном режиме "тракинга"
				if (!(m_tiDisplayed.nBehaviour & PPTOOLTIP_NOCLOSE_OVER))
				{
					//ENG: A tooltip don't hides when mouse over him
					//RUS: Если не установлен стиль не закрывания тултипа если курсор над ним
					HideTooltip();
				}
				else
				{
					//ENG: Resetup autopop timer
					//RUS: Переустанавливаем таймер автозакрытия тултипа
					SetAutoPopTimer();
				} //if
			}
			else
			{
				//ENG: Searching a toolbar's item
				//RUS: Ищем элемент на панели инструментов
				hWnd = FindToolBarItem(pMsg->pt, ti);
				if (NULL == hWnd)
				{
					//ENG: Searching a hot area of the tooltip
					//RUS: Ищем предопределенную горячую зону тултипа
					hWnd = FindTool(&pt, ti);
				} //if
					
				if (NULL == hWnd)
				{
					//ENG: An item with a tooltip wasn't found
					//RUS: Ни один элемент, отображающий тултип, не найден
					m_hwndDisplayedTool = NULL;
					m_tiDisplayed.rectBounds.SetRectEmpty();
					KillTimer(TIMER_SHOW);
					HideTooltip();
				}
				else 
				{
					if ((hWnd != m_hwndDisplayedTool) || (ti.rectBounds != m_tiDisplayed.rectBounds/* m_rcDisplayedTool*/))
					{
						//ENG: Sets new tooltip for the new window or for the new window's item
						//RUS: Если новое окно или новый элемент окна, то установить новый тултип
						SetNewTooltip(hWnd, ti);
					}
					else
					{
						//ENG: Nothing was changed
						//RUS: Если ни окно, ни элемент окна не изменялись
						if (m_tiDisplayed.nBehaviour & PPTOOLTIP_TRACKING_MOUSE)
						{
							//ENG: If sets tracking mode
							//RUS: Если установлен режим "тракинга"
							SetAutoPopTimer();
							OutputTooltipOnScreen(&pMsg->pt);
						}
						else if (!(m_tiDisplayed.nBehaviour & PPTOOLTIP_CLOSE_LEAVEWND))
						{
							//ENG: A tooltip must hide at anything mouse move
							//RUS: Если должен прятаться прилюбом движении мыши
							if ((hWnd == m_hwndDisplayedTool) && 
								!(m_tiDisplayed.nBehaviour & PPTOOLTIP_MULTIPLE_SHOW))
							{
								//ENG: "Multiple show" mode was disabled
								//RUS: Если не установлен режим множественного показа тултипа
								HideTooltip();
							}
							else
							{
								//ENG: "Multiple show" mode was enabled
								//RUS: Если установлен режим множественного показа тултипа
								SetNewTooltip(hWnd, ti);
							} //if
						}
						else
						{
							//ENG: A tooltip don't must when a mouse is over window
							//RUS: Тултип не должен прятаться пока находится над окном
							SetAutoPopTimer();
						} //if
					} //if
				} //if
			} //if
		} //if
		break;
	} //switch

	return FALSE;
} //End RelayEvent

void CPPToolTip::SetNewTooltip(HWND hWnd, const PPTOOLTIP_INFO & ti, BOOL bDisplayWithDelay /* = TRUE */, TooltipType type /* = PPTOOLTIP_NORMAL */)
{
	TRACE (_T("CPPToolTip::SetNewTooltip(hWnd=0x%08X, CRect(left=%d, top=%d, right=%d, bottom=%d), nID=%d)\n"), 
		hWnd, ti.rectBounds.left, ti.rectBounds.top, ti.rectBounds.right, ti.rectBounds.bottom, ti.nIDTool);
	
	m_bNextToolExist = FALSE;

	//ENG: Hides a tooltip
	//RUS: Прячем тултип если он показан или показывается
	if ((PPTOOLTIP_STATE_SHOWING == m_nTooltipState) || 
		(PPTOOLTIP_STATE_SHOWN == m_nTooltipState))
		HideTooltip();

	//ENG: If a tooltip wasn't hidden
	//RUS: Если тултип еще не спрятан, ждем ...
	m_nNextTooltipType = type;
	m_hwndNextTool = hWnd;
	m_tiNextTool = ti;
	if (PPTOOLTIP_STATE_HIDEN != m_nTooltipState)
	{
		TRACE(_T("SetNewTooltip2(%d)\n"), m_nNextTooltipType);
		m_bNextToolExist = TRUE;
		if (bDisplayWithDelay && m_dwTimeInitial)
			m_bDelayNextTool = TRUE;
		else 
			m_bDelayNextTool = FALSE;
		return;
	} //if

	//ENG: Start the show timer
	//RUS: Начинаем показ нового тултипа
	if (bDisplayWithDelay && m_dwTimeInitial)
		SetTimer(TIMER_SHOW, m_dwTimeInitial, NULL);
	else
		OnTimer(TIMER_SHOW);
} //End of SetNewTooltip

void CPPToolTip::OnTimer(UINT nIDEvent) 
{
	POINT pt;
	switch (nIDEvent)
	{
	case TIMER_SHOW:
		TRACE(_T("OnTimerShow(%d)\n"), m_nNextTooltipType);
		//ENG: Kill SHOW timer 
		//RUS: Убить таймер ожидания показа тултипа
		KillTimer(TIMER_SHOW);
		//ENG: Get current mouse coordinates
		//RUS: Получить текущее положение тултипа
		if ((PPTOOLTIP_HELP == m_nNextTooltipType) || 
			(PPTOOLTIP_MENU == m_nNextTooltipType))
			pt = m_ptOriginal;
		else GetCursorPos(&pt);

		if ((pt.x != m_ptOriginal.x) || (pt.y != m_ptOriginal.y))
		{
			//ENG: If mouse coordinates was changed
			//RUS: Если курсор сдвинулся, то уничтожить тултип
			TRACE(_T("OnTimerShow(HideTooltip)\n"));
			HideTooltip();
		}
		else if (PPTOOLTIP_STATE_HIDEN == m_nTooltipState)
		{
			TRACE(_T("OnTimerShow(Showing)\n"));
			m_nTooltipState = PPTOOLTIP_STATE_SHOWING;
			//Display first step
			PrepareDisplayTooltip(&m_ptOriginal);
			
			//Fade In effect
			if ((SHOWEFFECT_FADEINOUT == m_dwShowEffect) && m_dwTimeFadeIn)
			{
				TRACE(_T("OnTimerShow(FadeIn)\n"));
				SetTimer(TIMER_SHOWING, m_dwTimeFadeIn, NULL); //Fade in showing was enabled
			}
			else
			{
				TRACE(_T("OnTimerShow(Shown)\n"));
				m_nTooltipState = PPTOOLTIP_STATE_SHOWN; //Tooltip is already show
				if (m_dwTimeAutoPop && !(m_tiNextTool.nBehaviour & PPTOOLTIP_DISABLE_AUTOPOP))
					SetTimer(TIMER_HIDE, m_dwTimeAutoPop, NULL); //Hiding by timer was enabled
			} //if
		} //if
		break;
	case TIMER_SHOWING:
		TRACE("OnTimerFadeIn(Current Transparency=%d )\n", m_dwCurTransparency);
		//ENG: If fade-in effect was finished then sets minimum transparency
		//RUS: Если шаги эффекта плавного показа исчерпаны, то установить минимальную прозрачность
		if (m_dwCurTransparency > (PERCENT_MIN_TRANSPARENCY + PERCENT_STEP_FADEIN))
			m_dwCurTransparency -= PERCENT_STEP_FADEIN;
		else m_dwCurTransparency = PERCENT_MIN_TRANSPARENCY;

		if (m_dwCurTransparency <= m_tiDisplayed.nTransparency)
		{
			//ENG: Kills showing timer and sets a tooltip's state as SHOWN
			//RUS: Убиваем таймер плавного показа и устанавливаем состояние тултипа как SHOWN
			m_dwCurTransparency = m_tiDisplayed.nTransparency;
			KillTimer(TIMER_SHOWING);
			m_nTooltipState = PPTOOLTIP_STATE_SHOWN;
			//ENG: Starts timer to auto pop of the tooltip
			//RUS: Запуск таймера на автоматическое сокрытие тултипа
			SetAutoPopTimer();
		} //if
		//ENG: Redraw tooltip with new transparency factor
		//RUS: Перерисовать текущий тултип с новым коэффициентом прозрачности
		OnRedrawTooltip(NULL, m_dwCurTransparency);
		break;	
	case TIMER_HIDE:
		TRACE(_T("OnTimerHide()\n"));
		//ENG: Kill all timers except HIDING timer
		//RUS: Убиваем все таймеры за исключением таймера HIDING
		KillTimer(TIMER_SHOW);
		KillTimer(TIMER_SHOWING);
		KillTimer(TIMER_HIDE);
		//ENG: If hiding timer don't start
		//RUS: Проверяем запущен ли таймер сокрытия тултипа
		if (PPTOOLTIP_STATE_HIDING != m_nTooltipState)
		{
			m_nTooltipState = PPTOOLTIP_STATE_HIDING;
			if ((SHOWEFFECT_FADEINOUT == m_dwHideEffect) && m_dwTimeFadeOut)
			{
				//ENG: If fade-out effect enabled and setted fade-out timestep
				//RUS: Если эффект плавного сокрытия разрешен и указано время шага сокрытия
				SetTimer(TIMER_HIDING, m_dwTimeFadeOut, NULL);
			}
			else
			{
				//ENG: Sets a maximum transparency and to stops a hiding of a tooltip
				//RUS: Устанавливаем максимальную прозрачность и останавливаем сокрытие
				m_dwCurTransparency = PERCENT_MAX_TRANSPARENCY;
				OnTimer(TIMER_HIDING);
			} //if
		} //if
		break;
	case TIMER_HIDING:
		TRACE("OnTimerFadeOut(Current Transparency=%d)\n", m_dwCurTransparency);
		//ENG: If fade-out effect was finished then sets maximum transparency
		//RUS: Если шаги эффекта плавного сокрытия исчерпаны, то установить максимальную прозрачность
		if (m_dwCurTransparency < (PERCENT_MAX_TRANSPARENCY - PERCENT_STEP_FADEOUT))
			m_dwCurTransparency += PERCENT_STEP_FADEOUT;
		else m_dwCurTransparency = PERCENT_MAX_TRANSPARENCY;
		
		if (PERCENT_MAX_TRANSPARENCY == m_dwCurTransparency)
		{
			//ENG: Kills hiding timer and hides a tooltip
			//RUS: Убиваем таймер плавного сокрытия и прячем окно тултипа
			KillTimer(TIMER_HIDING);
			ShowWindow(SW_HIDE);
			m_nTooltipState = PPTOOLTIP_STATE_HIDEN;
			if (m_bNextToolExist)
			{
				//ENG: If next tooltip is exist then starts show
				//RUS: Если существует подготовленный тултип, то начать его отображение
				m_bNextToolExist = FALSE;
//				m_nTooltipType = m_nNextTooltipType;
//				m_nNextTooltipType = PPTOOLTIP_NORMAL;
				if (m_bDelayNextTool) SetTimer(TIMER_SHOW, m_dwTimeInitial, NULL);
				else OnTimer(TIMER_SHOW);
			}
			else
			{
				//ENG: If next tooltip wasn't exist
				//RUS: Если подготовленный тултип не существует
				m_nTooltipType = PPTOOLTIP_NORMAL;
				m_nNextTooltipType = PPTOOLTIP_NORMAL;
			} //if
		}
		else 
		{
			//ENG: Redraw tooltip with new transparency factor
			//RUS: Перерисовать текущий тултип с новым коэффициентом прозрачности
			OnRedrawTooltip(NULL, m_dwCurTransparency);
		} //if
		break;	
	default:
		CWnd::OnTimer(nIDEvent);
		break;
	} //switch
} //End of the WM_TIMER handler

void CPPToolTip::HideTooltip()
{
	//TRACE (_T("CPPToolTip::HideTooltip(CurState=%d)\n"), m_nTooltipState);
	switch(m_nTooltipState)
	{
	case PPTOOLTIP_STATE_SHOWING:
		//ENG: Kill showing tooltip
		//RUS: Если тултип только отбражается, то прекращаем его отображать
		KillTimer(TIMER_SHOWING);
	case PPTOOLTIP_STATE_SHOWN:
		//ENG: Hiding a tooltip
		//RUS: Прячем тултип
		OnTimer(TIMER_HIDE);
		break;
	} //switch
} //End of HideTooltip

void CPPToolTip::SetAutoPopTimer()
{
	TRACE (_T("CPPToolTip::SetAutoPopTimer()\n"));
	if (m_dwTimeAutoPop && !(m_tiDisplayed.nBehaviour & PPTOOLTIP_DISABLE_AUTOPOP))
		SetTimer(TIMER_HIDE, m_dwTimeAutoPop, NULL);
} //End of SetAutoPopTimer

void CPPToolTip::KillTimers(DWORD dwIdTimer /* = NULL */)
{
	TRACE (_T("CPPToolTip::KillTimers()\n"));
	if (dwIdTimer == NULL)
	{
		KillTimer(TIMER_SHOW);
		KillTimer(TIMER_HIDE);
		KillTimer(TIMER_SHOWING);
		KillTimer(TIMER_HIDING);
	}
	else 
	{
		KillTimer(dwIdTimer);
	} //if
} //End KillTimers

void CPPToolTip::Pop()
{
	TRACE (_T("CPPToolTip::Pop()\n"));
	m_nTooltipState = PPTOOLTIP_STATE_HIDEN;
	m_nTooltipType = PPTOOLTIP_NORMAL;
	m_nNextTooltipType = PPTOOLTIP_NORMAL;
	KillTimers();
	if (IsVisible())
	{
		if (m_tiDisplayed.nBehaviour & PPTOOLTIP_MULTIPLE_SHOW)
		{
			//If for tool to set a multiple show then reset last window
			m_hwndDisplayedTool = NULL;
			m_tiDisplayed.rectBounds.SetRectEmpty();
		} //if
		ShowWindow(SW_HIDE);
	} //if
} //End of Pop

void CPPToolTip::PrepareDisplayTooltip(LPPOINT lpPoint)
{
	TRACE (_T("CPPToolTip::PrepareDisplayTooltip()\n"));
	
	//Fills default members
	if (!(m_tiNextTool.nMask & PPTOOLTIP_MASK_STYLES))
		m_tiNextTool.nStyles = m_dwStyles;

	if (!(m_tiNextTool.nMask & PPTOOLTIP_MASK_EFFECT))
	{
		m_tiNextTool.nEffect = m_dwEffectBk;
		m_tiNextTool.nGranularity = m_nGranularity;
	} //if

	if (!(m_tiNextTool.nMask & PPTOOLTIP_MASK_COLORS))
	{
		m_tiNextTool.crBegin = m_clrBeginBk;
		m_tiNextTool.crMid = m_clrMidBk;
		m_tiNextTool.crEnd = m_clrEndBk;
	} //if

	if (!(m_tiNextTool.nMask & PPTOOLTIP_MASK_DIRECTION))
		m_tiNextTool.nDirection = m_dwDirection;

	if (!(m_tiNextTool.nMask & PPTOOLTIP_MASK_BEHAVIOUR))
		m_tiNextTool.nBehaviour = m_dwBehaviour;

	if (!(m_tiNextTool.nMask & PPTOOLTIP_MASK_TRANSPARENCY))
		m_tiNextTool.nTransparency = m_nTransparency;
	
	//Send notify
	POINT pt = *lpPoint; //Pointer in screen coordinates
	SendNotify(&pt, m_tiNextTool);
	
	//If string and icon are not exist then exit
	if (m_tiNextTool.sTooltip.IsEmpty())
		return;
	
	//calculate the width and height of the box dynamically
	CDC * pDC = GetDC();
	ASSERT(pDC->GetSafeHdc());
	
	CSize sz (0, 0);
	sz = m_drawer.PrepareOutput(pDC, m_tiNextTool.sTooltip);
	
	m_rcTipArea.SetRect(0, 0, sz.cx, sz.cy);
	m_rcTooltip = m_rcTipArea;

	//Inflates on MARGIN_CX and MARGIN_CY sizes
	m_rcTipArea.OffsetRect(m_dwSizes[PPTTSZ_MARGIN_CX], m_dwSizes[PPTTSZ_MARGIN_CY]);
	m_rcTooltip.InflateRect(0, 0, 2 * m_dwSizes[PPTTSZ_MARGIN_CX], 2 * m_dwSizes[PPTTSZ_MARGIN_CY]);

	//Inflates on 
	//Gets tooltip's rect with anchor
	POINT ptAnchor;
	m_dwCurDirection = GetTooltipDirection(m_tiNextTool.nDirection, &pt, &ptAnchor, m_rcTooltip, m_rcBoundsTooltip, m_rcTipArea);

	//ENG: Clears resources
	//RUS: Очищаем ресурсы
	FreeResources();

	//ENG: Creates a new region of the window
	//RUS: Создаем новый регион окна
	m_hrgnTooltip = GetTooltipRgn(m_dwCurDirection, ptAnchor.x, ptAnchor.y, m_rcBoundsTooltip.Width(), m_rcBoundsTooltip.Height());

	CRect rect = m_rcBoundsTooltip;
	
	//ENG: Creates a background bitmap
	//RUS: Создаем битмап фона тултипа
	m_hBitmapBk = ::CreateCompatibleBitmap(pDC->GetSafeHdc(), rect.Width(), rect.Height());
	HDC hMemDC = ::CreateCompatibleDC(pDC->GetSafeHdc());
	
	//ENG: Creates a background of the tooltip's body
	//RUS: Создание фона тела тултипа
	HBITMAP hOldBitmap = (HBITMAP)::SelectObject(hMemDC, m_hBitmapBk);
	m_drawer.GetDrawManager()->FillEffect(hMemDC, 
								m_tiNextTool.nEffect, 
								m_rcTooltip,
								m_tiNextTool.crBegin,
								m_tiNextTool.crMid,
								m_tiNextTool.crEnd,
								m_tiNextTool.nGranularity,
								5);
	//ENG: Fills an anchor
	//RUS: Заполняем якорь тултипа
	switch (m_dwCurDirection)
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
		m_drawer.GetDrawManager()->MultipleCopy(hMemDC, m_rcBoundsTooltip.left, m_rcBoundsTooltip.top, m_rcTooltip.left - m_rcBoundsTooltip.left, m_rcBoundsTooltip.Height(),
					 hMemDC, m_rcTooltip.left + 1, m_rcBoundsTooltip.top, 1, m_rcBoundsTooltip.Height());
		break;
	case PPTOOLTIP_RIGHTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_VCENTER:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		m_drawer.GetDrawManager()->MultipleCopy(hMemDC, m_rcTooltip.right, m_rcBoundsTooltip.top, m_rcBoundsTooltip.right - m_rcTooltip.right, m_rcBoundsTooltip.Height(),
					 hMemDC, m_rcTooltip.right - 1, m_rcBoundsTooltip.top, 1, m_rcBoundsTooltip.Height());
		break;
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
	case PPTOOLTIP_BOTTOMEDGE_CENTER:
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
		m_drawer.GetDrawManager()->MultipleCopy(hMemDC, m_rcBoundsTooltip.left, m_rcTooltip.bottom, m_rcBoundsTooltip.Width(), m_rcBoundsTooltip.bottom - m_rcTooltip.bottom,
					 hMemDC, m_rcBoundsTooltip.left, m_rcTooltip.bottom - 1, m_rcBoundsTooltip.Width(), 1);
		break;
	case PPTOOLTIP_TOPEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_CENTER:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		m_drawer.GetDrawManager()->MultipleCopy(hMemDC, m_rcBoundsTooltip.left, m_rcBoundsTooltip.top, m_rcBoundsTooltip.Width(), m_rcTooltip.top - m_rcBoundsTooltip.top,
					 hMemDC, m_rcBoundsTooltip.left, m_rcTooltip.top + 1, m_rcBoundsTooltip.Width(), 1);
		break;
	} //switch

	::SelectObject(hMemDC, hOldBitmap);
	::DeleteObject(hOldBitmap);
	::DeleteDC(hMemDC);
	
	ReleaseDC(pDC);
	
	//ENG: Calculate the tooltip's placement on the screen
	//RUS: Вычисление положения тултипа на экране
	rect.left = pt.x - ptAnchor.x;
	rect.top = pt.y - ptAnchor.y;
	rect.right = rect.left + m_rcBoundsTooltip.Width();
	rect.bottom = rect.top + m_rcBoundsTooltip.Height();

	//ENG: If fade-in effect ia available
	//RUS: Если плавное отображение доступно
	if ((SHOWEFFECT_FADEINOUT == m_dwShowEffect) && m_dwTimeFadeIn)
		m_dwCurTransparency = 100;
	else
		m_dwCurTransparency = m_tiNextTool.nTransparency;

	HRGN hrgnWindow = ::CreateRectRgn(0, 0, 0, 0);
	if (m_szOffsetShadow.cx || m_szOffsetShadow.cy)
	{
		//ENG: If shadow will drawn
		//RUS: Если тень будет отображаться
		HRGN hrgnShadow = ::CreateRectRgn(0, 0, 0, 0);
		::CombineRgn(hrgnShadow, m_hrgnTooltip, hrgnShadow, RGN_COPY);
		::OffsetRgn(hrgnShadow, m_szOffsetShadow.cx, m_szOffsetShadow.cy);
		::CombineRgn(hrgnWindow, m_hrgnTooltip, hrgnShadow, RGN_OR);
		::DeleteObject(hrgnShadow);

		//ENG: Increments the sizes of tooltip to drawing a shadow
		//RUS: Увеличиваем размеры тултипа для рисования тени
		m_rcBoundsTooltip.right += m_szOffsetShadow.cx;
		m_rcBoundsTooltip.bottom += m_szOffsetShadow.cy;
	}
	else
	{
		//ENG: The current window has not a shadow
		//RUS: Текущее окно не имеет тени
		::CombineRgn(hrgnWindow, m_hrgnTooltip, NULL, RGN_COPY);
	}//if

	//ENG: Applies a region
	//RUS: Применяем регион
	SetWindowRgn(hrgnWindow, FALSE);
	
	//ENG: Sets a tooltip on the screen
	//RUS: Устанавливаем тултип на экране
	SetWindowPos(NULL, 
		rect.left, rect.top,
		m_rcBoundsTooltip.Width(), 
		m_rcBoundsTooltip.Height(),
		SWP_SHOWWINDOW|SWP_NOCOPYBITS|SWP_NOACTIVATE/*|SWP_NOZORDER*/);
} //End of PrepareDisplayTooltip

void CPPToolTip::FreeResources()
{
	if (NULL != m_hrgnTooltip)
	{
		::DeleteObject(m_hrgnTooltip);
		m_hrgnTooltip = NULL;
	} //if

	if (NULL != m_hBitmapBk)
	{
		::DeleteObject(m_hBitmapBk);
		m_hBitmapBk = NULL;
	} //if

	if (NULL != m_hUnderTooltipBk)
	{
		::DeleteObject(m_hUnderTooltipBk);
		m_hUnderTooltipBk = NULL;
	} //if
} //End of FreeResources

void CPPToolTip::OutputTooltipOnScreen(LPPOINT lpPoint, HDC hDC /* = NULL */)
{
	TRACE("OutputTooltipOnScreen()\n");
	CRect rect = m_rcBoundsTooltip;
	rect.OffsetRect(*lpPoint);
	//m_dwCurDirection = GetTooltipDirection(m_tiNextTool.nDirection, &pt, &ptAnchor, m_rcTooltip, m_rcBoundsTooltip, m_rcTipArea);
	MoveWindow(rect);
} //End OutputTooltipOnScreen

DWORD CPPToolTip::GetTooltipDirection(DWORD dwDirection, const LPPOINT lpPoint, LPPOINT lpAnchor, CRect & rcBody, CRect & rcFull, CRect & rcTipArea)
{
	//Get Window's rectangle
	CWindowDC wdc(NULL);
	CRect rWindow;
	
	// the the whole virtual desktop .... not only the primary screen.JFN
    rWindow.left    = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    rWindow.top     = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
	rWindow.right   = rWindow.left + ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rWindow.bottom  = rWindow.top + ::GetSystemMetrics(SM_CYVIRTUALSCREEN);

	//-------------------------------------------
	// Initializing size of the bounds rect
	rcFull = rcBody;
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
		rcFull.right += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		break;
	case PPTOOLTIP_RIGHTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_VCENTER:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		rcFull.right += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		break;
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
	case PPTOOLTIP_BOTTOMEDGE_CENTER:
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
		rcFull.bottom += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		break;
	case PPTOOLTIP_TOPEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_CENTER:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		rcFull.bottom += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		break;
	} //switch
	
	//---------------------------------------------------
	//If needed change a horizontal direction
	CPoint pt(*lpPoint);
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
		pt.x += rcFull.Width();
		if (pt.x > rWindow.right)
			dwDirection ^= 0x10;
		break;
	case PPTOOLTIP_RIGHTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_VCENTER:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		pt.x -= rcFull.Width();
		if (pt.x < rWindow.left)
			dwDirection ^= 0x10;
		break;
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_LEFT:
		pt.x += rcFull.Width();
		pt.x -= m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		if (pt.x > rWindow.right)
			dwDirection ^= 0x01;
		break;
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		pt.x -= rcFull.Width();
		pt.x += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		if (pt.x < rWindow.left)
			dwDirection ^= 0x01;
		break;
	} //switch

	//---------------------------------------------------
	//If needed change a vertical direction
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_TOP:
		pt.y -= rcFull.Height();
		pt.y += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		if (pt.y > rWindow.bottom)
			dwDirection ^= 0x01;
		break;
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		pt.y += rcFull.Height();
		pt.y += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		if (pt.y < rWindow.top)
			dwDirection ^= 0x01;
		break;
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
	case PPTOOLTIP_BOTTOMEDGE_CENTER:
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
		pt.y -= rcFull.Height();
		if (pt.y < rWindow.top)
			dwDirection ^= 0x10;
		break;
	case PPTOOLTIP_TOPEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_CENTER:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		pt.y += rcFull.Height();
		if (pt.y > rWindow.bottom)
			dwDirection ^= 0x10;
		break;
	} //switch

	//---------------------------------------------------
	// Set the anchor's point
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
		lpAnchor->x = rcFull.left;
		break;
	case PPTOOLTIP_RIGHTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_VCENTER:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		lpAnchor->x = rcFull.right;
		break;
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
	case PPTOOLTIP_BOTTOMEDGE_CENTER:
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
		lpAnchor->y = rcFull.bottom;
		break;
	case PPTOOLTIP_TOPEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_CENTER:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		lpAnchor->y = rcFull.top;
		break;
	} //switch
	
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_TOP:
		lpAnchor->y = rcFull.top + m_dwSizes [PPTTSZ_MARGIN_ANCHOR];
		break;
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		lpAnchor->y = rcFull.bottom - m_dwSizes [PPTTSZ_MARGIN_ANCHOR];
		break;
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_RIGHTEDGE_VCENTER:
		lpAnchor->y = rcFull.Height() / 2;
		break;
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_LEFT:
		lpAnchor->x = rcFull.left + m_dwSizes [PPTTSZ_MARGIN_ANCHOR];
		break;
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		lpAnchor->x = rcFull.right - m_dwSizes [PPTTSZ_MARGIN_ANCHOR];
		break;
	case PPTOOLTIP_BOTTOMEDGE_CENTER:
	case PPTOOLTIP_TOPEDGE_CENTER:
		lpAnchor->x = rcFull.left + rcFull.Width() / 2;
		break;
	} //switch

	//---------------------------------------------------
	//If needed offset anchor
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_TOP:
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_RIGHTEDGE_VCENTER:
		if ((lpPoint->y - lpAnchor->y) < rWindow.top)
			lpAnchor->y = lpPoint->y - rWindow.top;
		break;
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		if ((lpPoint->y + rcFull.Height() - lpAnchor->y) > rWindow.bottom)
			lpAnchor->y = rcFull.Height() - rWindow.bottom + lpPoint->y;
		break;
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_LEFT:
	case PPTOOLTIP_BOTTOMEDGE_CENTER:
	case PPTOOLTIP_TOPEDGE_CENTER:
		if ((lpPoint->x - lpAnchor->x) < rWindow.left)
			lpAnchor->x = lpPoint->x - rWindow.left;
		break;
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		if ((lpPoint->x + rcFull.Width() - lpAnchor->x) > rWindow.right)
			lpAnchor->x = rcFull.Width() - rWindow.right + lpPoint->x;
		break;
	} //switch
	
	//---------------------------------------------
	// Offset the body rectangle
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
		rcBody.OffsetRect(m_dwSizes [PPTTSZ_HEIGHT_ANCHOR], 0);
		rcTipArea.OffsetRect(m_dwSizes [PPTTSZ_HEIGHT_ANCHOR], 0);
		break;
	case PPTOOLTIP_TOPEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_CENTER:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		rcBody.OffsetRect(0, m_dwSizes [PPTTSZ_HEIGHT_ANCHOR]);
		rcTipArea.OffsetRect(0, m_dwSizes [PPTTSZ_HEIGHT_ANCHOR]);
		break;
	} //switch

	return dwDirection;
}

HRGN CPPToolTip::GetTooltipRgn(DWORD dwDirection, int x, int y, int nWidth, int nHeight)
{
	HRGN hRgn = NULL;
	
	HRGN hrgnBody = NULL;
	CRect rcBody(0, 0, nWidth, nHeight);

	HRGN hrgnAnchor = NULL;
	POINT ptAnchor [3];
	ptAnchor [0].x = x;
	ptAnchor [0].y = y;
	
	HRGN hrgnRect = NULL;
	
	//------------------------------
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
		rcBody.left += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		ptAnchor [1].x = ptAnchor [2].x = rcBody.left;
		break;
	case PPTOOLTIP_RIGHTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_VCENTER:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		rcBody.right -= m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		ptAnchor [1].x = ptAnchor [2].x = rcBody.right;
		break;
	case PPTOOLTIP_TOPEDGE_LEFT:
	case PPTOOLTIP_TOPEDGE_CENTER:
	case PPTOOLTIP_TOPEDGE_RIGHT:
		rcBody.top += m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		ptAnchor [1].y = ptAnchor [2].y = rcBody.top;
		break;
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
	case PPTOOLTIP_BOTTOMEDGE_CENTER:
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
		rcBody.bottom -= m_dwSizes [PPTTSZ_HEIGHT_ANCHOR];
		ptAnchor [1].y = ptAnchor [2].y = rcBody.bottom;
		break;
	} //switch

	//------------------------------
	switch(dwDirection) 
	{
	case PPTOOLTIP_LEFTEDGE_TOP:
	case PPTOOLTIP_RIGHTEDGE_TOP:
		ptAnchor [1].y = rcBody.top + m_dwSizes [PPTTSZ_MARGIN_ANCHOR];
		ptAnchor [2].y = ptAnchor [1].y + m_dwSizes [PPTTSZ_WIDTH_ANCHOR];
		break;
	case PPTOOLTIP_LEFTEDGE_BOTTOM:
	case PPTOOLTIP_RIGHTEDGE_BOTTOM:
		ptAnchor [1].y = rcBody.bottom - m_dwSizes [PPTTSZ_MARGIN_ANCHOR];
		ptAnchor [2].y = ptAnchor [1].y - m_dwSizes [PPTTSZ_WIDTH_ANCHOR];
		break;
	case PPTOOLTIP_LEFTEDGE_VCENTER:
	case PPTOOLTIP_RIGHTEDGE_VCENTER:
		ptAnchor [1].y = rcBody.top + (rcBody.Height() - m_dwSizes [PPTTSZ_WIDTH_ANCHOR]) / 2;
		ptAnchor [2].y = ptAnchor [1].y + m_dwSizes [PPTTSZ_WIDTH_ANCHOR];
		break;
	case PPTOOLTIP_TOPEDGE_LEFT:
	case PPTOOLTIP_BOTTOMEDGE_LEFT:
		ptAnchor [1].x = rcBody.left + m_dwSizes [PPTTSZ_MARGIN_ANCHOR];
		ptAnchor [2].x = ptAnchor [1].x + m_dwSizes [PPTTSZ_WIDTH_ANCHOR];
		break;
	case PPTOOLTIP_TOPEDGE_RIGHT:
	case PPTOOLTIP_BOTTOMEDGE_RIGHT:
		ptAnchor [1].x = rcBody.right - m_dwSizes [PPTTSZ_MARGIN_ANCHOR];
		ptAnchor [2].x = ptAnchor [1].x - m_dwSizes [PPTTSZ_WIDTH_ANCHOR];
		break;
	case PPTOOLTIP_TOPEDGE_CENTER:
	case PPTOOLTIP_BOTTOMEDGE_CENTER:
		ptAnchor [1].x = rcBody.left + (rcBody.Width() - m_dwSizes [PPTTSZ_WIDTH_ANCHOR]) / 2;
		ptAnchor [2].x = ptAnchor [1].x + m_dwSizes [PPTTSZ_WIDTH_ANCHOR];
		break;
	} //switch

	//------------------------------
	//Gets the tooltip body's region
	hrgnBody = ::CreateRoundRectRgn(rcBody.left, rcBody.top, rcBody.right + 1, rcBody.bottom + 1, 
			m_dwSizes[PPTTSZ_ROUNDED_CX], m_dwSizes[PPTTSZ_ROUNDED_CY]);

	//Gets the tooltip anchor's region
	if (m_dwSizes [PPTTSZ_HEIGHT_ANCHOR] && m_dwSizes [PPTTSZ_WIDTH_ANCHOR])
	{
		hrgnAnchor = ::CreatePolygonRgn(ptAnchor, 3, ALTERNATE);
	}
	else
	{
		hrgnAnchor = ::CreateRectRgn(0, 0, 0, 0);
	} //if

	hRgn = ::CreateRectRgn(0, 0, 0, 0);
	::CombineRgn(hRgn, hrgnBody, hrgnAnchor, RGN_OR);

	if (NULL != hrgnBody)
		::DeleteObject(hrgnBody);
	if (NULL != hrgnAnchor)
		::DeleteObject(hrgnAnchor);

	return hRgn;
} //End GetTooltipRgn

BOOL CPPToolTip::IsCursorOverTooltip() const
{
    ASSERT(m_hParentWnd);
	
    // Is tooltip visible?
    if (!IsVisible() || !IsWindow(m_hWnd))
		return FALSE;
	
    POINT pt;
    GetCursorPos(&pt);
	
	CPPToolTip * pWnd = (CPPToolTip*)WindowFromPoint(pt);
	
	return (pWnd == this);
}

HWND CPPToolTip::GetWndFromPoint(const LPPOINT lpPoint, BOOL bUseDisabled /* = TRUE */)
{
	ASSERT(m_hParentWnd);
	
    POINT pt = *lpPoint;
	// Find the window under the cursor
    ::ClientToScreen(m_hParentWnd, &pt);

	PPTOOLTIP_ENUM_CHILD structEnum;
	structEnum.hwndExclude = this->GetSafeHwnd();
	structEnum.hWnd = NULL;
	structEnum.ptScreen = pt;
	structEnum.dwFlags = (bUseDisabled) ? 0 : CWP_SKIPDISABLED;
	::EnumChildWindows(m_hParentWnd, EnumChildProc, (LPARAM)&structEnum);

	HWND hWnd = m_hParentWnd;
	if (NULL != structEnum.hWnd)
		hWnd = structEnum.hWnd;

    return hWnd;
} //End GetWndFromPoint

CString CPPToolTip::GetDebugInfoTool(LPPOINT lpPoint)
{
	HWND hWnd = GetWndFromPoint(lpPoint);
	HWND hParent = ::GetParent (hWnd);

	_TCHAR ch[128];
	PPTOOLTIP_INFO ti;
	CString str, strTemp;
	CString strOutput = _T("<table>");
	
	///////////////////////////////////////////////////////////////////
	//Table of a window
	strOutput += _T("<tr><td><font color=darkblue>Window</font><table border=1>");

	//1. Window's class name and Window Owner's class name
	strOutput += _T("<tr><td bgcolor=buttonface>Class Name</td><td bgcolor=window>");
	::GetClassName (hWnd, ch, 128);
	strOutput += GetMaxDebugString((CString)ch);
	strOutput += _T("</td></tr>");
	
	//2. Window's title and Window Owner's title
	strOutput += _T("<tr><td bgcolor=buttonface>Title</td><td bgcolor=white>");
	::GetWindowText (hWnd, ch, 128);
	strOutput += GetMaxDebugString((CString)ch);
	strOutput += _T("</td></tr>");
	
	//3. Window's handle and Window Owner's handle
	str.Format(_T("<tr><td bgcolor=buttonface>Handle</td><td bgcolor=white>0x%08X</td></tr>"), hWnd);
	strOutput += str;
	
	//4. Window's ID
	str.Format(_T("<tr><td bgcolor=buttonface>Control ID</td><td bgcolor=white>%d</td></tr>"), GetWindowLong(hWnd, GWL_ID));
	strOutput += str;

	//5. Window's styles
	str.Format(_T("<tr><td bgcolor=buttonface>Styles</td><td bgcolor=white>0x%08X</td></tr>"), (DWORD)::GetWindowLong (hWnd, GWL_STYLE));
	strOutput += str;
	
	//6. Window's rect
	RECT rc; 
	::GetWindowRect(hWnd, &rc);
	str.Format(_T("<tr><td bgcolor=buttonface>RECT</td><td bgcolor=white>(%d, %d)-(%d, %d)</td></tr>"), rc.left, rc.top, rc.right, rc.bottom);
	strOutput += str;

	//7. Window's width
	str.Format(_T("<tr><td bgcolor=buttonface>Width</td><td bgcolor=white>%d</td></tr>"), rc.right - rc.left);
	strOutput += str;
	
	//8. Window's height
	str.Format(_T("<tr><td bgcolor=buttonface>Height</td><td bgcolor=white>%d</td></tr>"), rc.bottom - rc.top);
	strOutput += str;

	//9. Window's has tooltip
	strOutput += _T("<tr><td bgcolor=buttonface>Has Tooltip</td><td bgcolor=white>");
	HWND hwndTool = FindTool(lpPoint, ti);
	strOutput += (NULL != hwndTool) ? _T("Yes") : _T("No");
	strOutput += _T("</td></tr>");

	strOutput += _T("</table></td>");

	///////////////////////////////////////////////////////////////////
	//Table of a window owner
	strOutput += _T("<td><font color=darkblue>Window Owner</font><table border=1>");
	
	//1. Window's class name and Window Owner's class name
	strOutput += _T("<tr><td bgcolor=buttonface>Class Name</td><td bgcolor=white>");
	if (NULL != hParent)
	{
		::GetClassName (hParent, ch, 128);
		strOutput += GetMaxDebugString((CString)ch);
	}
	else strOutput += _T("N/A");
	strOutput += _T("</td></tr>");
	
	//2. Window's title and Window Owner's title
	strOutput += _T("<tr><td bgcolor=buttonface>Title</td><td bgcolor=white>");
	if (NULL != hParent)
	{
		::GetWindowText (hParent, ch, 128);
		strOutput += GetMaxDebugString((CString)ch);
	}
	else strOutput += _T("N/A");
	strOutput += _T("</td></tr>");
	
	//3. Window's handle and Window Owner's handle
	str.Format(_T("<tr><td bgcolor=buttonface>Handle</td><td bgcolor=white>0x%08X</td></tr>"), hParent);
	strOutput += str;

	strOutput += _T("</table>");
	
	///////////////////////////////////////////////////////////////////
	//Table of a window owner
	strOutput += _T("<font color=darkblue>Mouse Cursor</font><table border=1>");
	
	//1.
	str.Format(_T("<tr><td bgcolor=buttonface>X</td><td bgcolor=white>%d</td></tr>"), lpPoint->x);
	strOutput += str;
	
	//2.
	str.Format(_T("<tr><td bgcolor=buttonface>Y</td><td bgcolor=white>%d</td></tr>"), lpPoint->y);
	strOutput += str;

	strOutput += _T("</table></td></tr></table>");

	///////////////////////////////////////////////////////////////////////////
	return strOutput;
}

CString CPPToolTip::GetMaxDebugString(CString str)
{
	str.Replace(_T("<"), _T("?")); //Replaces the begins of the tags
	if (str.GetLength() > MAX_LENGTH_DEBUG_STRING)
	{
		str = str.Left(MAX_LENGTH_DEBUG_STRING - 4);
		str += _T(" ...");
	} //if

	return str;
}

HWND CPPToolTip::FindTool(const LPPOINT lpPoint, PPTOOLTIP_INFO & ti)
{
	//ENG: Gets a window's handle under the cursor
	//RUS: Получаем дескриптор окна под курсором
	HWND hWnd = GetWndFromPoint(lpPoint, FALSE /* m_nStyles & PPTOOLTIP_SHOW_DISABLED */);
	
	if (NULL != hWnd)
	{
		//ENG: A window under the cursor was existed
		//RUS: Окно под курсором существует
		if (FindTool(hWnd, lpPoint, ti))
		{
			//ENG: ... and tool was found
			//RUS: ... и найден инструмент
			return hWnd;
		} //if

		//ENG: Checks a child window
		//RUS: Проверяем дочернее окно
		hWnd = ::GetParent(hWnd);
		if (NULL != hWnd)
		{
			if (FindTool(hWnd, lpPoint, ti))
				return hWnd;
		} //if
	} //if

	return NULL;
} //End of FindTool

BOOL CPPToolTip::FindTool(HWND hWnd, const LPPOINT lpPoint, PPTOOLTIP_INFO & ti)
{
	//ENG: Searching a specified HWND
	//RUS: Ищем указанный HWND
	mapIter item = m_ToolMap.find(hWnd);
	if (item == m_ToolMap.end())
	{
		//ENG: Specified HWND wasn't found
		//RUS: Указанный HWND не найден
		return FALSE; 
	} //if
	
	//ENG: Gets the array with the hotarea's parameters
	//RUS: Получаем массив с параметрами горячих зон указаного окна
	arHotArea & hotarea = item->second;
	if ((hotarea.size() == 1) && hotarea[0].rectBounds.IsRectEmpty())
	{
		//ENG: If a bounding rectangle of a hotarea wasn't define
		//RUS: Если ограничивающий прямоугольник горячей зоны не определен
		ti = hotarea[0];
		return TRUE;
	} //if

	POINT ptClient = *lpPoint;
	if (hWnd != m_hParentWnd)
	{
		//ENG: If HWND specified window isn't a parent
		//RUS: Если HWND не относится к родительскому окну, то преобразуем координаты
		::ScreenToClient(hWnd, &ptClient);
	} //if
	
	CScrollView * pScroll = (CScrollView*)CScrollView::FromHandle(hWnd);
	if (pScroll->IsKindOf(RUNTIME_CLASS(CScrollView))) 
	{
		//ENG: If HWND of CScrollView or derived class then corrects the coordinates
		//RUS: Если HWND принадлежит CScrollView или производному от него классу, то корректируем координаты
		CPoint ptScroll = pScroll->GetScrollPosition();
		ptClient.x += ptScroll.x;
		ptClient.y += ptScroll.y;
	} //if
	
	//ENG: Search a hotarea under the mouse
	//RUS: Ищем горячую зону под курсором
	arHotArea::iterator iter;
	for (iter = hotarea.begin(); iter != hotarea.end(); ++iter)
	{
		ti = *iter;
		if (ti.rectBounds.PtInRect(ptClient))
		{
			//ENG: A hotarea was found
			//RUS: Зона найдена
			return TRUE;
		} //if
	} //for
	
	return FALSE;
} //End of FindTool

HWND CPPToolTip::FindToolBarItem(POINT point, PPTOOLTIP_INFO & ti)
{
	//ENG: Toolbar control was disabled
	//RUS: Контроль за подсказками к панелям инструментов отключен
	if (!m_wndToolBars.size())
		return NULL;

	//ENG: Gets a window under mouse
	//RUS: Определяем окно под курсором
	HWND hWnd = ::WindowFromPoint(point);
	if (NULL != hWnd)
	{
		//ENG: A window was found. Searching a coincidence with toolbar windows
		//RUS: Окно обнаружено. Поиск совпадения с окнами панелей инструментов
		for (int i = 0; i < (int)m_wndToolBars.size(); i++)
		{
			if (m_wndToolBars[i] == hWnd)
			{
				//ENG: A toolbar under mouse
				//RUS: Панель инструментов под курсором
				CToolBar * pBar = (CToolBar*)CToolBar::FromHandle(hWnd);
				pBar->ScreenToClient(&point);
				//ENG: Gets a item's count of the toolbar
				//RUS: Получаем количество элементов панели инструментов
				int count = pBar->GetCount();
				CRect rect;
				//ENG: Searching an toolbar's item under mouse
				//RUS: Поиск элемента панели инструментов находящегося под курсором
				for (int i = 0; i < count; i++)
				{
					pBar->GetItemRect(i, rect);
					if (rect.PtInRect(point))
					{
						//ENG: Toolbar's item was found
						//RUS: Элемент панели инструментов обнаружен
						ti.nIDTool = pBar->GetItemID(i);
						ti.rectBounds = rect;
						ti.nMask = 0;
						ti.sTooltip = m_drawer.GetResCommandPrompt(ti.nIDTool, 1);
						return hWnd;
					} //if
				} //for
				return NULL;
			} //if
		} //for
	} //if
	return NULL;
} //End of FindToolBarItem


////////////////////////////////////////////////////////////////////////////////////////////
// *** public methods ***
////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
// CPPToolTip::AddTool()
// Registers a tool with the tooltip control.
//------------------------------------------------------------------
// Parameters:
//		pWnd			- Pointer to the window that contains the tool.  
//		lpszString		- Pointer to the text for the tool. 
//      dwIdString		- ID of string resource
//		hIcon			- Handle of the icon
//		dwIdIcon		- ID of icon resource
//		szIcon			- Specifies the width and the height, in pixels, of the icon to load.
//		lpRectBounds	- Pointer to a RECT structure containing coordinates of the tool's bounding rectangle. 
//						  The coordinates are relative to the upper-left corner of the client area of the window identified by pWnd. 
//					      NULL if bounding rectangle don't uses for specified window
//		dwIdTool		- ID of the tool
//		ti				- Reference to PPTOOLTIP_INFO structure containing the parameters of the tooltip 
//
// Remarks:
//		  A tooltip control can be associated with more than one tool. Call this function to register a tool 
//		with the tooltip control, so that the information stored in the tooltip is displayed when the cursor is on the tool.
////////////////////////////////////////////////////////////////////
void CPPToolTip::AddTool(CWnd * pWnd, DWORD dwIdString, HICON hIcon, LPCRECT lpRectBounds /*= NULL*/, DWORD dwIDTool /*= 0*/)
{
	CString str;
	str.LoadString(dwIdString);
	AddTool(pWnd, (LPCTSTR)str, hIcon, lpRectBounds, dwIDTool);
}

void CPPToolTip::AddTool(CWnd * pWnd, LPCTSTR lpszString, HICON hIcon, LPCRECT lpRectBounds /*= NULL*/, DWORD dwIDTool /*= 0*/)
{
	CString str;
	str.Format(_T("<table><tr><td><icon handle=0x%X></td><td>%s</td></tr></table>"), 
		hIcon, lpszString);
	AddTool(pWnd, str, lpRectBounds, dwIDTool);
}

void CPPToolTip::AddTool(CWnd * pWnd, DWORD dwIdString, DWORD dwIdIcon, CSize & szIcon /* = CSize(0, 0) */, LPCRECT lpRectBounds /*= NULL*/, DWORD dwIDTool /*= 0*/)
{
	CString str;
	str.LoadString(dwIdString);
	AddTool(pWnd, (LPCTSTR)str, dwIdIcon, szIcon, lpRectBounds, dwIDTool);
}

void CPPToolTip::AddTool(CWnd * pWnd, LPCTSTR lpszString, DWORD dwIdIcon, CSize & szIcon /* = CSize(0, 0) */, LPCRECT lpRectBounds /*= NULL*/, DWORD dwIDTool /*= 0*/)
{
	CString str;
	str.Format(_T("<table><tr><td><icon idres=%d width=%d height=%d></td><td>%s</td></tr></table>"), 
		dwIdIcon, szIcon.cx, szIcon.cy, lpszString);
	AddTool(pWnd, str, lpRectBounds, dwIDTool);
}

void CPPToolTip::AddTool(CWnd * pWnd, DWORD dwIdString, LPCRECT lpRectBounds /* = NULL */, DWORD dwIDTool /* = 0 */)
{
	CString str;
	str.LoadString(dwIdString);
	AddTool(pWnd, (LPCTSTR)str, lpRectBounds, dwIDTool);
}

void CPPToolTip::AddTool(CWnd * pWnd, LPCTSTR lpszString /* = NULL */, LPCRECT lpRectBounds /* = NULL */, DWORD dwIDTool /* = 0 */)
{
	PPTOOLTIP_INFO ti;

	ti.nIDTool = dwIDTool;
	if (NULL != lpRectBounds)
		ti.rectBounds = *lpRectBounds;
	else 
		ti.rectBounds.SetRectEmpty();
	ti.sTooltip = (CString)lpszString;
	ti.nMask = 0;
	ti.nStyles = 0;
	ti.nDirection = 0;
	ti.nEffect = 0;
	ti.nBehaviour = 0;
	ti.nGranularity = 0;
	ti.crBegin = RGB(0, 0, 0);
	ti.crMid = RGB(0, 0, 0);
	ti.crEnd = RGB(0, 0, 0);

	AddTool(pWnd, ti);
}

void CPPToolTip::AddTool(CWnd * pWnd, PPTOOLTIP_INFO & ti)
{
	//TRACE("CPPToolTip::AddTool(hWnd=0x%08X)\n", pWnd->GetSafeHwnd());
	ASSERT (pWnd);

	//ENG: Gets HWND of a window
	//RUS: Получаем HWND окна
	HWND hWnd = pWnd->GetSafeHwnd();
	
	//ENG: Searching a specified HWND
	//RUS: Ищем указанный HWND
	mapIter item = m_ToolMap.find(hWnd);
	
	if (item == m_ToolMap.end())
	{
		//ENG: A tooltip for a specified HWND wasn't found therefore create it
		//RUS: Тултип для указанного HWND не обнаружен, поэтому создаем его
		arHotArea hotarea;
		hotarea.push_back(ti);
		m_ToolMap.insert(std::make_pair(hWnd, hotarea));
		return;
	} //if

	//ENG: Gets parameters of the tooltip
	//RUS: Получаем параметры тултипа
	arHotArea & hotarea = item->second;

	//ENG: A tooltip has more one rectangle areas. Check all theirs
	//RUS: Тултип содержит более одной прямоугольной области, проверим всех их
	arHotArea::iterator iter;
	for (iter = hotarea.begin(); iter != hotarea.end(); ++iter)
	{
		if (ti.rectBounds == (*iter).rectBounds)
		{
			//ENG: Specified window's rect already exist and so updates him
			//RUS: Указанный прямоугольник окна уже существует, поэтому просто обновляем его параметры
			*iter = ti;
			return;
		} //if
	} //for
	
	//ENG: Adds a new tool 
	//RUS: Добавляем новый инструмент
//	hotarea.push_back(ti);				// fx mod: change insertion order to allow last inserted 
	hotarea.insert(hotarea.begin(),ti);	// area to be on top (to be displayed first)
} //End of AddTool

////////////////////////////////////////////////////////////////////
// CPPToolTip::RemoveTool()
//   Removes the tool specified by pWnd and lpRectBounds from the collection of 
// tools supported by a tooltip control.
//------------------------------------------------------------------
// Parameters:
//		pWnd			- Pointer to the window that contains the tool.  
//		lpRectBounds	- Pointer to a RECT structure containing coordinates of the tool's bounding rectangle. 
//						  The coordinates are relative to the upper-left corner of the client area of the window identified by pWnd. 
//					      NULL if bounding rectangle don't uses for specified window
////////////////////////////////////////////////////////////////////
void CPPToolTip::RemoveTool(CWnd * pWnd, LPCRECT lpRectBounds /* = NULL */)
{
	TRACE ("CPPToolTip::RemoveTool(hWnd=0x%08X)\n", pWnd->GetSafeHwnd());
	ASSERT(pWnd);
	
	//ENG: Gets HWND of a window
	//RUS: Получаем HWND окна
	HWND hWnd = pWnd->GetSafeHwnd();

	//ENG: Searching a specified HWND
	//RUS: Ищем указанный HWND
	mapIter item = m_ToolMap.find(hWnd);

	if (item == m_ToolMap.end())
	{
		//ENG: Specified HWND wasn't found
		//RUS: Указанный HWND не найден
		return; 
	} //if

	if (NULL == lpRectBounds)
	{
		//ENG: Removes all tools for the specified window
		//RUS: Удаляются все инструменты для указанного окна
		m_ToolMap.erase(item);
	}
	else
	{
		//ENG: Search the tool to remove
		//RUS: Поиск инструмента для удаления
		arHotArea & hotarea = item->second;
		arHotArea::iterator iter;
		for (iter = hotarea.begin(); iter != hotarea.end(); ++iter)
		{
			if ((*iter).rectBounds == *lpRectBounds)
			{
				//ENG: The tool was found
				//RUS: Инструмент найден
				if (hotarea.size() > 1)
				{
					//ENG: If the specified window has more one rectangle areas then removes the specified area only
					//RUS: Если указанное окно содержит более одной области, то удаляем только указанную область
					hotarea.erase(iter);
				}
				else
				{
					//ENG: If the specified window has one rectangle area only then removes the tool for specified window
					//RUS: Если указанное окно имеет только одну прямоугольную область, то удалить весь инструмент
					m_ToolMap.erase(item);
				} //if
				return;
			} //if
		} //for
	} //if
} //End of RemoveTool

////////////////////////////////////////////////////////////////////
// CPPToolTip::RemoveAllTools()
//   Removes all tools from the collection of tools supported by a tooltip control.
////////////////////////////////////////////////////////////////////
void CPPToolTip::RemoveAllTools()
{
	TRACE ("CPPToolTip::RemoveAllTools()\n");

	//ENG: Removes all tools
	//RUS: Удаляем все инструменты
	if (m_ToolMap.size())
		m_ToolMap.clear();

	//ENG: Removes all toolbars
	//RUS: Удаляем все панели инструментов
	if (m_wndToolBars.size())
		m_wndToolBars.clear();
} //End of RemoveAllTools

////////////////////////////////////////////////////////////////////
// CPPToolTip::AddToolBar()
// Registers a toolbar to the tooltip control.
//------------------------------------------------------------------
// Parameters:
//		pBar			- Pointer to the toolbar window.  
////////////////////////////////////////////////////////////////////
void CPPToolTip::AddToolBar(CToolBar * pBar)
{
	TRACE ("CPPToolTip::AddToolBar(hWnd=0x%08X)\n", pBar->GetSafeHwnd());
	ASSERT(pBar);

	//ENG: Gets HWND toolbar's window
	//RUS: Получаем HWND окна панели инструментов
	HWND hWnd = pBar->GetSafeHwnd();

	//ENG: Searching a clone of a toolbar
	//RUS: Поиск дубликата указанной панели инструментов
	arToolBarWnd::iterator iter;
	for (iter = m_wndToolBars.begin(); iter != m_wndToolBars.end(); ++iter)
	{
		if (*iter == hWnd)
		{
			//ENG: A clone was found
			//RUS: Дубликат найден
			return;
		} //if
	} //for

	//ENG: Stores HWND toolbar's window
	//RUS: Запоминаем HWND окна панели инструментов
	m_wndToolBars.push_back(hWnd);

	//ENG: Disables a standard tooltip for the specified toolbar
	//RUS: Запрещаем стандартные подсказки для указаной панели инструментов
	DWORD dwStyles = pBar->GetBarStyle();
	dwStyles &= ~CBRS_TOOLTIPS;
	pBar->SetBarStyle(dwStyles);
} //End of AddToolBar

////////////////////////////////////////////////////////////////////
// CPPToolTip::EnableHyperlink()
// Enables redrawing hyperlinks and hot areas.
////////////////////////////////////////////////////////////////////
void CPPToolTip::EnableHyperlink(BOOL bEnable /* = TRUE */)
{
	m_bHyperlinkEnabled = bEnable;
} //End of EnableHyperlink

////////////////////////////////////////////////////////////////////
// CPPToolTip::SetCallbackHyperlink()
//   Sets the callback message that will be sent to the specified window 
// if user clicks a hyperlink or hotareas with a msg parameter.
//------------------------------------------------------------------
// Parameters:
//		hWnd			- Handle of the window that will receive the callback message.
//						  Pass NULL to remove any previously specified callback message.
//		nMessage		- Callback message to send to window.
//		lParam			- A 32 bits user specified value that will be passed to the callback function.
//
// Remarks:
//    The callback function must be in the form:
//  LRESULT On_MenuCallback(WPARAM wParam, LPARAM lParam)
//		wParam			- Pointer to the string specified as parameter in <a msg=> tag.
//		lParam			- The 32 bits user specified value.
////////////////////////////////////////////////////////////////////
void CPPToolTip::SetCallbackHyperlink(HWND hWnd, UINT nMessage, LPARAM lParam /* = 0 */)
{
	TRACE(_T("CPPToolTip::SetCallbackHyperlink()\n"));
	
	m_drawer.SetCallbackHyperlink(hWnd, nMessage, lParam);
} //End of SetCallbackHyperlink

/////////////////////////////////////////////////////////////////////
// CPPToolTip::SetNotify()
// This function sets or removes the notification messages from the control before display.
//-------------------------------------------------------------------
// Parameters:
//		bParentNotify	- If TRUE the tooltip will be send the notification to parent window
//						  Else the notification will not send
//		hWnd			- If non-NULL the tooltip will be send the notification to specified window
//						  Else the notification will not send
///////////////////////////////////////////////////////////////////////
void CPPToolTip::SetNotify(BOOL bParentNotify /* = TRUE */)
{
	HWND hWnd = NULL;
	
	if (bParentNotify)
		hWnd = m_hParentWnd;
	
	SetNotify(hWnd);
} //End of SetNotify

void CPPToolTip::SetNotify(HWND hWnd)
{
	TRACE(_T("CPPToolTip::SetNotify\n"));
	
	m_hNotifyWnd = hWnd;
} //End of SetNotify

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::SetSize()
//    Sets the specified size
//---------------------------------------------------------------------------
//  Parameters :
//		nSizeIndex		- index of the size. This parameter can be one 
//						  of the following values:
//							PPTTSZ_ROUNDED_CX - The width of the ellipse used 
//												to draw the rounded corners, in logical units.
//							PPTTSZ_ROUNDED_CY - The height of the ellipse used 
//												to draw the rounded corners, in logical units.
//							PPTTSZ_MARGIN_CX  - The left and right margins of the tooltip's 
//												text from the tooltip's edges. 
//							PPTTSZ_MARGIN_CY  - The top and bottom margins of the tooltip's 
//												text from the tooltip's edges.
//							PPTTSZ_WIDTH_ANCHOR - The width of the tooltip's anchor
//							PPTTSZ_HEIGHT_ANCHOR - The height of the tooltip's anchor 
//							PPTTSZ_MARGIN_ANCHOR - The margin of the tooltip's anchor from 
//												   his edge.
//							PPTTSZ_OFFSET_ANCHOR_CX - The horizontal offset of the tooltip's anchor
//													  from the hot spot of a cursor
//							PPTTSZ_OFFSET_ANCHOR_CY - The vertical offset of the tooltip's anchor
//													  from the hot spot of a cursor
//		nValue			- size's value
/////////////////////////////////////////////////////////////////////////////
void CPPToolTip::SetSize(DWORD nSizeIndex, DWORD nValue)
{
	TRACE(_T("CPPToolTip::SetSize(nSizeIndex = %d, nValue = %d)\n"), nSizeIndex, nValue);
	if (nSizeIndex >= PPTTSZ_MAX_SIZES)
		return;

	m_dwSizes [nSizeIndex] = nValue;
} //End of SetSize

/////////////////////////////////////////////////////////////////////////////
//  CPPTootTip::GetSize()
//    Gets the specified size
//---------------------------------------------------------------------------
//  Parameters :
//		nSizeIndex		- An index of the sizes. See CPPToolTip::SetSize for a 
//						  description of the valid values.
//  Returns :
//		size's value
//
/////////////////////////////////////////////////////////////////////////////
DWORD CPPToolTip::GetSize(DWORD nSizeIndex)
{
	TRACE(_T("CPPToolTip::GetSize(nSizeIndex = %d)\n"), nSizeIndex);
	if (nSizeIndex >= PPTTSZ_MAX_SIZES)
		return 0;

	return m_dwSizes [nSizeIndex];
} //End of GetSize

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::SetDefaultSizes()
//    Sets all sizes to default values
//---------------------------------------------------------------------------
//  Parameters:
//		bBalloonSize	- If TRUE all sizes will be sets for balloon tooltip
//						  otherwise tooltip will look as standard 
/////////////////////////////////////////////////////////////////////////////
void CPPToolTip::SetDefaultSizes(BOOL bBalloonSize /* = TRUE */)
{
	TRACE(_T("CPPToolTip::SetDefaultSizes()\n"));

	if (bBalloonSize)
	{
		SetSize(PPTTSZ_ROUNDED_CX, 16);
		SetSize(PPTTSZ_ROUNDED_CY, 16);
		SetSize(PPTTSZ_MARGIN_CX, 12);
		SetSize(PPTTSZ_MARGIN_CY, 12);
		SetSize(PPTTSZ_WIDTH_ANCHOR, 12);
		SetSize(PPTTSZ_HEIGHT_ANCHOR, 16);
		SetSize(PPTTSZ_MARGIN_ANCHOR, 16);
		SetSize(PPTTSZ_OFFSET_ANCHOR_CX, 0);
		SetSize(PPTTSZ_OFFSET_ANCHOR_CY, 0);
	}
	else
	{
		SetSize(PPTTSZ_ROUNDED_CX, 0);
		SetSize(PPTTSZ_ROUNDED_CY, 0);
		SetSize(PPTTSZ_MARGIN_CX, 3);
		SetSize(PPTTSZ_MARGIN_CY, 1);
		SetSize(PPTTSZ_WIDTH_ANCHOR, 0);
		SetSize(PPTTSZ_HEIGHT_ANCHOR, 0);
		SetSize(PPTTSZ_MARGIN_ANCHOR, 0);
		SetSize(PPTTSZ_OFFSET_ANCHOR_CX, 0);
		SetSize(PPTTSZ_OFFSET_ANCHOR_CY, 0);
	} //if
} //End of SetDefaultSizes

/////////////////////////////////////////////////////////////////////////////
// CPPToolTip::SetColorBk()
//   Sets background's colors 
//---------------------------------------------------------------------------
//  Parameters:
//		color			- A solid color for background's effect 
//		clrBegin		- A begin color for background's effect
//		clrMid			- A middle color for background's effect
//		clrEnd			- A end color for background's effect
/////////////////////////////////////////////////////////////////////////////
void CPPToolTip::SetColorBk(COLORREF color)
{
	SetColorBk(color, color, color);
} //End of SetColorBk

void CPPToolTip::SetColorBk(COLORREF clrBegin, COLORREF clrEnd)
{
	SetColorBk(clrBegin, clrBegin, clrEnd);
} //End of SetColorBk

void CPPToolTip::SetColorBk(COLORREF clrBegin, COLORREF clrMid, COLORREF clrEnd)
{
	m_clrBeginBk = clrBegin;
	m_clrMidBk = clrMid;
	m_clrEndBk = clrEnd;
} //End of SetColorBk

/////////////////////////////////////////////////////////////////////////////
// CPPToolTip::SetEffectBk()
//   Sets a background's effect 
//---------------------------------------------------------------------------
//  Parameters:
//		dwEffect		- A background's effect 
//		nGranularity	- Adds an uniform noise to the effect. 
//						  A good value is from 5 to 20; 0 to disable the effect. 
//						  The noise has a positive effect because it hides the palette steps.
/////////////////////////////////////////////////////////////////////////////
void CPPToolTip::SetEffectBk(DWORD dwEffect, BYTE nGranularity /* = 5 */)
{
	m_dwEffectBk = dwEffect;
	m_nGranularity = nGranularity;
} //End of SetEffectBk

/////////////////////////////////////////////////////////////////////////////
// CPPToolTip::SetBehaviour()
//   Sets a tooltip's behaviour 
//---------------------------------------------------------------------------
//  Parameters:
//		dwBehaviour		- A tooltip's behaviour. 0 for normal tooltip without 
//						  specific behaviours. This parameter can be any combination 
//						  of CPPToolTip behaviours:
//							PPTOOLTIP_MULTIPLE_SHOW		- Multiple show for single control
//							PPTOOLTIP_TRACKING_MOUSE	- Tracking for mouse
//							PPTOOLTIP_CLOSE_LEAVEWND	- Close tooltip if mouse leave the control
//							PPTOOLTIP_NOCLOSE_OVER		- No close tooltip if mouse over him
//							PPTOOLTIP_DISABLE_AUTOPOP	- Disables autopop tooltip from timer
/////////////////////////////////////////////////////////////////////////////
void CPPToolTip::SetBehaviour(DWORD dwBehaviour /* = 0 */)
{
	m_dwBehaviour = dwBehaviour;
} //End of SetBehaviour

/////////////////////////////////////////////////////////////////////////////
// CPPToolTip::GetBehaviour()
//   Gets a tooltip's behaviour 
//---------------------------------------------------------------------------
// Return value:
//		A tooltip's behaviour. See CPPToolTip::SetBehaviour for a description of the 
//	valid values.
/////////////////////////////////////////////////////////////////////////////
DWORD CPPToolTip::GetBehaviour()
{
	return m_dwBehaviour;
} //End of GetBehaviour

/////////////////////////////////////////////////////////////////////
//  CPPToolTip::SetDelayTime()
//   Call this function to set the delay time for a tooltip control. 
// The delay time is the length of time the cursor must remain on a tool 
// before the tooltip window appears. The default delay time is 500 milliseconds.
//-------------------------------------------------------------------
// Parameters:
//		dwDuration		- Flag that specifies which duration value will be retrieved. 
//						  This parameter can be one of the following values:
//							PPTOOLTIP_TIME_AUTOPOP  - Retrieve the length of time the tooltip 
//													  window remains visible if the pointer is 
//													  stationary within a tool's bounding rectangle. 
//							PPTOOLTIP_TIME_INITIAL  - Retrieve the length of time the pointer 
//			 										  must remain stationary within a tool's bounding 
//													  rectangle before the tool tip window appears. 
//							PPTOOLTIP_TIME_FADEIN	- Retrieve the length of time for each step of
//													  fade-in effect
//							PPTOOLTIP_TIME_FADEOUT	- Retrieve the length of time for each step of
//													  fade-out effect
//						  For compatibility with 1.x versions of CPPToolTip a following values
//						  are available also:
//							TTDT_AUTOPOP			- Same PPTOOLTIP_TIME_AUTOPOP 
//							TTDT_INITIAL			- Same PPTOOLTIP_TIME_INITIAL 
//		nTime			- The specified delay time, in milliseconds.
/////////////////////////////////////////////////////////////////////
void CPPToolTip::SetDelayTime(DWORD dwDuration, DWORD dwTime)
{
	switch (dwDuration)
	{
	case PPTOOLTIP_TIME_AUTOPOP:
		m_dwTimeAutoPop = dwTime;
		break;
	case PPTOOLTIP_TIME_INITIAL:
		m_dwTimeInitial = dwTime;
		break;
	case PPTOOLTIP_TIME_FADEIN:
		m_dwTimeFadeIn = dwTime;
		break;
	case PPTOOLTIP_TIME_FADEOUT:
		m_dwTimeFadeOut = dwTime;
		break;
	}
} //End of SetDelayTime

/////////////////////////////////////////////////////////////////////
// CPPToolTip::GetDelayTime()
// Retrieves the initial, pop-up, and reshow durations currently set 
// for a CPPToolTip control
//-------------------------------------------------------------------
// Parameters:
//		dwDuration		- Flag that specifies which duration value will be retrieved. 
//						  See CPPToolTip::SetDelayTime for a description of the valid values. 
// Return value:
//	The specified delay time, in milliseconds
///////////////////////////////////////////////////////////////////////
DWORD CPPToolTip::GetDelayTime(DWORD dwDuration) const
{
	DWORD dwTime = 0;
	switch (dwDuration)
	{
	case PPTOOLTIP_TIME_AUTOPOP:
		dwTime = m_dwTimeAutoPop;
		break;
	case PPTOOLTIP_TIME_INITIAL:
		dwTime = m_dwTimeInitial;
		break;
	case PPTOOLTIP_TIME_FADEIN:
		dwTime = m_dwTimeFadeIn;
		break;
	case PPTOOLTIP_TIME_FADEOUT:
		dwTime = m_dwTimeFadeOut;
		break;
	}

	return dwTime;
} //End of GetDelayTime

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::SetDirection()
//    Sets a placement of the tooltip's anchor
//---------------------------------------------------------------------------
//  Parameters :
//		dwDirection		- A placement of the tooltip's anchor. This parameter 
//					      can be one of the following values:
//							PPTOOLTIP_TOPEDGE_LEFT			- A left corner of the top edge
//							PPTOOLTIP_TOPEDGE_RIGHT			- A right corner of the top edge
//							PPTOOLTIP_TOPEDGE_CENTER		- By center of the top edge
//							PPTOOLTIP_BOTTOMEDGE_LEFT		- A left corner of the bottom edge
//							PPTOOLTIP_BOTTOMEDGE_RIGHT		- A right corner of the bottom edge
//							PPTOOLTIP_BOTTOMEDGE_CENTER		- By center of the bottom edge
//							PPTOOLTIP_LEFTEDGE_TOP			- A top corner of the left edge
//							PPTOOLTIP_LEFTEDGE_BOTTOM		- A bottom corner of the left edge
//							PPTOOLTIP_LEFTEDGE_VCENTER		- By center of the left edge
//							PPTOOLTIP_RIGHTEDGE_TOP			- A top corner of the right edge
//							PPTOOLTIP_RIGHTEDGE_BOTTOM		- A bottom corner of the right edge
//							PPTOOLTIP_RIGHTEDGE_VCENTER		- By center of the right edge
//						  For compatibility with 1.x versions of CPPToolTip a following values
//						  are available also:
//							PPTOOLTIP_LEFT_TOP				- Same PPTOOLTIP_TOPEDGE_LEFT
//							PPTOOLTIP_RIGHT_TOP				- Same PPTOOLTIP_TOPEDGE_RIGHT
//							PPTOOLTIP_LEFT_BOTTOM			- Same PPTOOLTIP_BOTTOMEDGE_LEFT
//							PPTOOLTIP_RIGHT_BOTTOM			- Same PPTOOLTIP_BOTTOMEDGE_RIGHT
/////////////////////////////////////////////////////////////////////////////
void CPPToolTip::SetDirection(DWORD dwDirection /* = PPTOOLTIP_BOTTOMEDGE_LEFT */)
{
	TRACE(_T("CPPToolTip::SetDirection(nDirection = %d)\n"), dwDirection);

	m_dwDirection = dwDirection;
} //End of SetDirection	

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::GetDirection()
//    Gets a placement of the tooltip's anchor
//---------------------------------------------------------------------------
//  Returns :
//	  A placement of the tooltip's anchor. See CPPToolTip::SetDirection for a description of 
//	the valid values.
/////////////////////////////////////////////////////////////////////////////
DWORD CPPToolTip::GetDirection()
{
	TRACE(_T("CPPToolTip::GetDirection()\n"));

	return m_dwDirection;
} //End of GetDirection

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::SetCssStyles()
//    Applies a CSS-like styles for the tooltip's HTML
//---------------------------------------------------------------------------
//  Parameters:
//		lpszCssStyles	- Pointer to a null-terminated string that specifies 
//						  CSS-lite styles for drawing a tooltip text.
/////////////////////////////////////////////////////////////////////////////
void CPPToolTip::SetCssStyles(LPCTSTR lpszCssStyles /* = NULL */)
{
	m_drawer.SetCssStyles(lpszCssStyles);
} //End of SetCssStyles

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::GetCssStyles()
//    Applies a CSS-like styles for the tooltip's HTML
//---------------------------------------------------------------------------
//  Return value:
//		Pointer to a null-terminated string that specifies CSS-lite styles 
//  for drawing a tooltip text.
/////////////////////////////////////////////////////////////////////////////
LPCTSTR CPPToolTip::GetCssStyles()
{
	return m_drawer.GetCssStyles();
} //End of GetCssStyles

/////////////////////////////////////////////////////////////////////////////
//  CPPToolTip::SetDebugMode()
//    Sets a debug mode. In this mode tooltip will display for any control
// and will contain debug info about control.
//---------------------------------------------------------------------------
//  Parameters:
//		bDebug			- TRUE set a debug mode.
/////////////////////////////////////////////////////////////////////////////
void CPPToolTip::SetDebugMode(BOOL bDebug /* = TRUE */)
{
	m_bDebugMode = bDebug;
} //End of SetDebugMode

////////////////////////////////////////////////////////////////////
// CPPToolTip::ShowHelpTooltip()
// Shows the help tooltip in any place of screen.
//------------------------------------------------------------------
// Parameters:
//		pt				- Pointer to a POINT structure that receives the screen coordinates of the tooltip's anchor  
//		lpszString		- Pointer to the text for the help tooltip. 
//      dwIdString		- ID of string resource
//		hIcon			- Handle of the icon
//		dwIdIcon		- ID of icon resource
//		szIcon			- Specifies the width and the height, in pixels, of the icon to load.
//		ti				- Reference to PPTOOLTIP_INFO structure containing the parameters of the tooltip 
////////////////////////////////////////////////////////////////////
void CPPToolTip::ShowHelpTooltip(LPPOINT pt, DWORD dwIdText, HICON hIcon /* = NULL */)
{
	CString str;
	str.LoadString(dwIdText);
	ShowHelpTooltip(pt, (LPCTSTR)str, hIcon);
} //End ShowHelpTooltip

void CPPToolTip::ShowHelpTooltip(LPPOINT pt, DWORD dwIdText, DWORD dwIdIcon, CSize & szIcon /* = CSize(0, 0) */)
{
	CString str;
	str.LoadString(dwIdText);
	ShowHelpTooltip(pt, (LPCTSTR)str, dwIdIcon, szIcon);
} //End ShowHelpTooltip

void CPPToolTip::ShowHelpTooltip(LPPOINT pt, LPCTSTR lpszString, HICON hIcon /* = NULL */)
{
	PPTOOLTIP_INFO ti;
	if (NULL == hIcon)
	{
		ti.sTooltip = (CString)lpszString;
	}
	else
	{
		ti.sTooltip.Format(_T("<table><tr><td><icon handle=0x%X></td><td>%s</td></tr></table>"), 
							hIcon, lpszString);
	} //if
	
	ti.nMask = 0;
	ShowHelpTooltip(pt, ti);
} //End ShowHelpTooltip

void CPPToolTip::ShowHelpTooltip(LPPOINT pt, LPCTSTR lpszString, DWORD dwIdIcon, CSize & szIcon /* = CSize(0, 0) */)
{
	CString str;
	str.Format(_T("<table><tr><td><icon idres=%d width=%d height=%d></td><td>%s</td></tr></table>"), 
		dwIdIcon, szIcon.cx, szIcon.cy, lpszString);
	ShowHelpTooltip(pt, (LPCTSTR)str);
} //End ShowHelpTooltip

void CPPToolTip::ShowHelpTooltip(LPPOINT pt, PPTOOLTIP_INFO & ti)
{
	TRACE("CPPToolTip::ShowHelpTooltip\n");

	m_ptOriginal = CPoint(pt->x, pt->y);
	ti.nBehaviour = m_dwBehaviour | PPTOOLTIP_DISABLE_AUTOPOP;
	ti.nMask = PPTOOLTIP_MASK_BEHAVIOUR;
	SetNewTooltip(NULL, ti, FALSE, PPTOOLTIP_HELP);
} //End ShowHelpTooltip

////////////////////////////////////////////////////////////////////
// CPPToolTip::SetBorder()
// Sets a border of the tooltip.
//------------------------------------------------------------------
// Parameters:
//		color			- Color of the tooltip's border
//		hbr				- Brush for drawing tooltip's border
//      nWidth			- A width of the brush
//		nHeight			- A height of the brush
////////////////////////////////////////////////////////////////////
void CPPToolTip::SetBorder(COLORREF color, int nWidth /* = 1 */, int nHeight /* = 1 */)
{
	HBRUSH hbr = ::CreateSolidBrush(color);
	SetBorder(hbr, nWidth, nHeight);
} //End of SetBorder

void CPPToolTip::SetBorder(HBRUSH hbr, int nWidth /* = 1 */, int nHeight /* = 1 */)
{
	HideBorder();
	if (nWidth && nHeight && (NULL != hbr))
	{
		m_hbrBorder = hbr;
		m_szBorder.cx = nWidth;
		m_szBorder.cy = nHeight;
	} //if
} //End of SetBorder

////////////////////////////////////////////////////////////////////
// CPPToolTip::HideBorder()
// Hides border of the tooltip.
////////////////////////////////////////////////////////////////////
void CPPToolTip::HideBorder()
{
	if (NULL != m_hbrBorder)
	{
		::DeleteObject(m_hbrBorder);
		m_hbrBorder = NULL;
	} //if
	m_szBorder.cx = 0;
	m_szBorder.cy = 0;
} //End of HideBorder

////////////////////////////////////////////////////////////////////////////////
// Begin of the menu methods block. Build-in support for menu
#ifdef PPTOOLTIP_USE_MENU
//////////////////
// Need to handle WM_ENTERIDLE to cancel the tip if the user 
// moved the mouse off the popup menu. For main menus, Windows 
// will send a WM_MENUSELECT message for the parent menu when 
// this happens, but for context menus there's no other way to 
// know the user moved the mouse off the menu.
//
void CPPToolTip::OnEnterIdle(UINT nWhy, CWnd* pWho)
{
	if ((MSGF_MENU == nWhy))
	{
		if (m_nTooltipType == PPTOOLTIP_MENU)
		{
			if (PPTOOLTIP_STATE_SHOWN == m_nTooltipState)
			{
				CPoint pt;
				GetCursorPos(&pt);
				if (pWho->GetSafeHwnd() != ::WindowFromPoint(pt)) 
				{
					HideTooltip();
				} //if
			} //if
		} //if
	} //if
} //End of OnEnterIdle

void CPPToolTip::OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSubMenu)
{
	if (((nFlags & 0xFFFF) == 0xFFFF) || (nFlags & MF_POPUP) || (nFlags & MF_SEPARATOR))
	{
		//HideTooltip();
		Pop();
	} 
	else if (nItemID && hSubMenu) 
	{
		HWND hwndMenu = GetRunningMenuWnd(); //CWnd::WindowFromPoint(pt);
		if (NULL != hwndMenu)
		{
			CRect rcMenu;
			::GetWindowRect(hwndMenu, rcMenu); // whole menu rect
			
			// find Item Rectangle and position
			int count = ::GetMenuItemCount(hSubMenu);
			int cy = rcMenu.top + GetSystemMetrics(SM_CYEDGE) + 1;
			for(int nItem = 0; nItem < count; nItem++) 
			{
				CRect rect;
				::GetMenuItemRect(m_hParentWnd, hSubMenu, nItem, &rect);
				if(nItemID == ::GetMenuItemID(hSubMenu, nItem)) 
				{
					UINT nState = GetMenuState(hSubMenu, nItemID, MF_BYCOMMAND);
					CString str;
					if (MF_DISABLED & nState)
						str = m_drawer.GetResCommandPrompt(nItemID, 2); //String for disabled item
					else
						str = m_drawer.GetResCommandPrompt(nItemID, 0);

					CPoint pt;
					// found menu item: adjust rectangle to right and down
					pt.x = rcMenu.left;
					pt.y = cy;
					if (m_dwMenuToolPos & PPTOOLTIP_MENU_CENTER)
						pt.x += rect.Width() / 2;
					else if (m_dwMenuToolPos & PPTOOLTIP_MENU_RIGHT)
						pt.x += rect.Width();
					
					if (m_dwMenuToolPos & PPTOOLTIP_MENU_VCENTER)
						pt.y += rect.Height() / 2;
					else if (m_dwMenuToolPos & PPTOOLTIP_MENU_BOTTOM)
						pt.y += rect.Height();

					PPTOOLTIP_INFO ti;
					ti.rectBounds = rect;
					ti.nMask = 0;
					ti.sTooltip = str;
					m_nNextTooltipType = PPTOOLTIP_MENU;
					m_ptOriginal = pt;
					SetNewTooltip(hwndMenu, ti, TRUE, PPTOOLTIP_MENU);

					return;
				} //if
				cy += rect.Height(); // add height
			} //for
		} //if
		//ENG: Menu item was not found
		//RUS: Элемент меню не найден.
		Pop();
	} //if
} //End of OnMenuSelect

////////////////////////////////////////////////////////////////////
// CPPToolTip::GetRunningMenuWnd()
// Get running menu window.
////////////////////////////////////////////////////////////////////
HWND CPPToolTip::GetRunningMenuWnd()
{
	HWND hwnd = NULL;
	EnumWindows(MyEnumProc,(LPARAM)&hwnd);
	return hwnd;
} //End of GetRunningMenuWnd

////////////////////////////////////////////////////////////////////
// CPPToolTip::MenuToolPosition()
// Sets a position of the tooltip's anchor about menu item.
//------------------------------------------------------------------
// Parameters:
//		nPos			- A tooltip's position. This parameter can be any combination 
//						  of single horizontal value and single vertical value of CPPToolTip:
//							--- Horizontal position ---
//							PPTOOLTIP_MENU_LEFT		0x00
//							PPTOOLTIP_MENU_RIGHT	0x01
//							PPTOOLTIP_MENU_CENTER	0x02
//							--- Vertical position ---
//							PPTOOLTIP_MENU_TOP		0x00
//							PPTOOLTIP_MENU_BOTTOM	0x10
//							PPTOOLTIP_MENU_VCENTER  0x20
////////////////////////////////////////////////////////////////////
void CPPToolTip::MenuToolPosition(DWORD nPos /* = PPTOOLTIP_MENU_LEFT | PPTOOLTIP_MENU_TOP */)
{
	m_dwMenuToolPos = nPos;
} //End of MenuToolPosition

// End of menu methods block
///////////////////////////////////////////////////////////
#endif //PPTOOLTIP_USE_MENU

////////////////////////////////////////////////////////////////////
// CPPToolTip::EnableEscapeSequences()
//		Enables the escape sequences. If the escape sequences was disabled
//	HTML-lite compiler will ignore the codes less then 0x20 (such \n, \r, \t).
////////////////////////////////////////////////////////////////////
void CPPToolTip::EnableEscapeSequences(BOOL bEnable)
{
	m_drawer.EnableEscapeSequences(bEnable);
} //End of EnableEscapeSequences

////////////////////////////////////////////////////////////////////
// CPPToolTip::SetImageList()
//		Sets an image list for using it into the HTML string with <ilst> tag.
//------------------------------------------------------------------
// Parameters:
//		nIdBitmap		- Resource IDs of the bitmap to be associated with the image list.
//		hBitmap			- Handle of the bitmap to be associated with the image list.
//      cx				- Dimensions of each image, in pixels.
//		cy				- Dimensions of each image, in pixels.
//		nCount			- The number of images in the image list
//		crMask			- Color used to generate a mask. Each pixel of this color in 
//						  the specified bitmap is changed to transparent, and the 
//						  corresponding bit in the mask is set to one.
////////////////////////////////////////////////////////////////////
void CPPToolTip::SetImageList(UINT nIdBitmap, int cx, int cy, int nCount, COLORREF crMask /* = RGB(255, 0, 255) */)
{
	m_drawer.SetImageList(nIdBitmap, cx, cy, nCount, crMask);
} //End of SetImageList

void CPPToolTip::SetImageList(HBITMAP hBitmap, int cx, int cy, int nCount, COLORREF crMask /* = RGB(255, 0, 255) */)
{
	m_drawer.SetImageList(hBitmap, cx, cy, nCount, crMask);
} //End of SetImageList

////////////////////////////////////////////////////////////////////
// CPPToolTip::SetTransparency()
//		Sets a transparency of the tooltip.
//------------------------------------------------------------------
// Parameters:
//		nTransparency	- A transparency value to be used on the tooltip. 
//						  The default 0 assumes that your tooltip is opaque and 0xFF (255) 
//						  for full transparency of the tooltip.
////////////////////////////////////////////////////////////////////
void CPPToolTip::SetTransparency(BYTE nTransparency /* = 0 */) 
{
	if (nTransparency <= PERCENT_MAX_TRANSPARENCY)
		m_nTransparency = nTransparency;
} //End of SetTransparency

////////////////////////////////////////////////////////////////////
// CPPToolTip::SetTooltipShadow()
//		Sets a tooltip's shadow.
//------------------------------------------------------------------
// Parameters:
//		nOffsetX, 
//		nOffsetY		- The offsets of the tooltip's shadow from the tooltip's window.
//		nDarkenPercent	- So far as colors under the shadow will be darken (0 - 100)
//      bGradient		- TRUE to use a gradient shadow.
//		nDepthX,
//		nDepthY			- The gradient depths of the tooltip's shadow.
////////////////////////////////////////////////////////////////////
void CPPToolTip::SetTooltipShadow(int nOffsetX, int nOffsetY, BYTE nDarkenPercent /* = 50 */, 
								  BOOL bGradient /* = TRUE */, int nDepthX /* = 7 */, int nDepthY /* = 7 */)
{
	m_szOffsetShadow.cx = nOffsetX;
	m_szOffsetShadow.cy = nOffsetY;
	m_szDepthShadow.cx = nDepthX;
	m_szDepthShadow.cy = nDepthY;
	m_nDarkenShadow = min(100, nDarkenPercent);
	m_bGradientShadow = bGradient;
} //End of SetTooltipShadow

void CPPToolTip::SetImageShadow(int nOffsetX, int nOffsetY, BYTE nDarkenPercent /* = 50 */, 
					BOOL bGradient /* = TRUE */, int nDepthX /* = 7 */, int nDepthY /* = 7 */)
{
	m_drawer.SetImageShadow(nOffsetX, nOffsetY, nDarkenPercent, bGradient, nDepthX, nDepthY);
} //End of SetImageShadow