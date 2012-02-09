/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#if !defined(AFX_GRAPHFRAME_H__6556B6B9_DE08_11D3_B4B4_00C04F89477F__INCLUDED_)
#define AFX_GRAPHFRAME_H__6556B6B9_DE08_11D3_B4B4_00C04F89477F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GraphFrame.h : header file
//

#include "GraphContainer.h"
#include "pviewbar.h"
#include "graph_general.h"

/////////////////////////////////////////////////////////////////////////////
// CGraphFrame frame

class CGraphFrame : public CFrameWnd, public virtual CGraphBaseClass
{
	DECLARE_DYNCREATE(CGraphFrame)
protected:
	CGraphFrame();           // protected constructor used by dynamic creation

// Attributes
public:

    CGraphContainer graph_container;
    CPointViewBar pview_bar;
    BOOL m_bOwnTimer;

// Operations
public:

    BOOL Create(LPCTSTR lpszWindowName, const RECT& rect, CWnd* pParentWnd, 
	BOOL bOwnTimer = FALSE, DWORD dwStyle = WS_CHILD | WS_VISIBLE);

    void OperateWithPointView(unsigned long pview_operations);
    void UpdateWindows(unsigned long what_to_update);
    void UpdateViews(DWORD message_id, void* param);
    DWORD GetGraphFlags(){return graph_container.GetGraphFlags();};
    void GetGraphWorldCoords(double* x1, double* x2, double* y1, double* y2)
    {
	graph_container.GetGraphWorldCoords(x1, x2, y1, y2);
    };

    void DrawGraphToDC(CDC* dest_dc, CRect& rect_to_draw);

    virtual void AppendMenuItems(CMenu* menu);
    virtual void OnRBMenuCommand(UINT command_id);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraphFrame)
	public:
	virtual void RecalcLayout(BOOL bNotify = TRUE);
	//}}AFX_VIRTUAL

// Implementation
protected:

    void OnPointViewBarCommand(UINT nID);
    void OnPointViewUpdate(CCmdUI* pCmdUI);

	virtual ~CGraphFrame();

	// Generated message map functions
	//{{AFX_MSG(CGraphFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GRAPHFRAME_H__6556B6B9_DE08_11D3_B4B4_00C04F89477F__INCLUDED_)
