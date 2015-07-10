////////////////////////////////////////////////////////////////
// PixieLib(TM) Copyright 1997-1998 Paul DiLascia
// If this code works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.
//
#ifndef _SUBCLASSW_H
#define _SUBCLASSW_H

//////////////////
// Generic class to hook messages on behalf of a CWnd.
// Once hooked, all messages go to CSubclassWnd::WindowProc before going
// to the window. Specific subclasses can trap messages and do something.
//
// To use:
//
// * Derive a class from CSubclassWnd.
//
// * Override CSubclassWnd::WindowProc to handle messages. Make sure you call
//   CSubclassWnd::WindowProc if you don't handle the message, or your
//   window will never get messages. If you write seperate message handlers,
//   you can call Default() to pass the message to the window.
//
// * Instantiate your derived class somewhere and call HookWindow(pWnd)
//   to hook your window, AFTER it has been created.
//	  To unhook, call Unhook or HookWindow(NULL).
//
// This is a very important class, crucial to many of the widgets Window
// widgets implemented in PixieLib. To see how it works, look at the HOOK
// sample program.
//
class CSubclassWnd : public CObject {
public:
	CSubclassWnd();
	~CSubclassWnd();

	// Subclass a window. Hook(NULL) to unhook (automatic on WM_NCDESTROY)
	virtual BOOL	RawHookWindow(HWND  hwnd);
	BOOL	HookWindow(CWnd* pWnd)	{ return RawHookWindow(pWnd->GetSafeHwnd()); }
	void	Unhook()                
    { 
        RawHookWindow((HWND)NULL); 
    }
	BOOL	IsHooked()              { return m_hWnd!=NULL; }

	friend LRESULT CALLBACK HookWndProc(HWND, UINT, WPARAM, LPARAM);
	friend class CSubclassWndMap;

	virtual LRESULT WindowProc(UINT msg, WPARAM wp, LPARAM lp);
	LRESULT Default();				// call this at the end of handler fns

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	HWND            m_hWnd;         // the window hooked
	WNDPROC			m_pOldWndProc;  // ..and original window proc
	CSubclassWnd*	m_pNext;        // next in chain of hooks for this window

	DECLARE_DYNAMIC(CSubclassWnd);
};

#endif // _SUBCLASSW_H

