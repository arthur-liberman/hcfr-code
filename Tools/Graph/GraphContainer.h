/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#if !defined(AFX_GRAPHCONTAINER_H__6556B6BA_DE08_11D3_B4B4_00C04F89477F__INCLUDED_)
#define AFX_GRAPHCONTAINER_H__6556B6BA_DE08_11D3_B4B4_00C04F89477F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GraphContainer.h : header file
//

#include "gruler.h"
#include "GraphPanel.h"

struct SGraphChange;

/////////////////////////////////////////////////////////////////////////////
// CGraphContainer window

class CGraphContainer : public CWnd
{
// Construction
public:
	CGraphContainer();

// Attributes
public:

    FVRuler vruler;
    FHRuler hruler;
    CGraphPanel* graph_panel;

// Operations
public:

    void UpdateWindows(unsigned long what_to_update);
    void UpdateViews(DWORD message_id, void* param);
    DWORD GetGraphFlags();
    void GetGraphWorldCoords(double* x1, double* x2, double* y1, double* y2);

    void DrawGraphToDC(CDC* dest_dc, CRect& rect_to_draw);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraphContainer)
	protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CGraphContainer();

	// Generated message map functions
protected:
	//{{AFX_MSG(CGraphContainer)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GRAPHCONTAINER_H__6556B6BA_DE08_11D3_B4B4_00C04F89477F__INCLUDED_)
