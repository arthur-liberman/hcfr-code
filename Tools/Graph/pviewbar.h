/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#ifndef _PVIEW_BAR_H_
#define _PVIEW_BAR_H_

#include "../sizecbar.h"
#include "graphcombobox.h"
#include "graph_general.h"
#include "points.h"


/////////////////////////////////////////////////////////////////////////////
// C3DListCtrl window

class C3DListCtrl : public CListCtrl, public virtual CGraphBaseClass
{
// Construction
public:
	C3DListCtrl();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(C3DListCtrl)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~C3DListCtrl();

	// Generated message map functions
protected:
	//{{AFX_MSG(C3DListCtrl)
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CPointViewBar window

class CPointViewBar : public CSizingControlBar, public virtual CGraphBaseClass
{
    #define GRAPH_COMBO_CHILD_ID	11005
    #define GRAPH_POINT_LIST_CHILD_ID   11006

// Construction
public:
	CPointViewBar();

// Attributes
public:

    CGraphComboBox graph_combo_box;
    C3DListCtrl point_list_ctrl;
    BOOL b_enabled;

// Operations
public:

    void AddGraph(SGraphChange* sgc);
    void RemoveGraph(SGraphChange* sgc);
    void ClearGraph(SGraphChange* sgc);
    void InitPointList(CGraphWnd* main_wnd);
    void AddPoint(SGraphChange* sgc);
    void EditPoint(SGraphChange* sgc);
    void RemovePoint(SGraphChange* sgc);
//    void AddPoint(CGraphWnd* main_wnd, long index, SSinglePoint* ssp);
    long GetCurrentGraphId();

    void OnSelectionChanged();
    void OnGetDispInfo(NMLVDISPINFO* disp_info);

    virtual void AppendMenuItems(CMenu* menu);
    virtual void OnRBMenuCommand(UINT command_id);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPointViewBar)
	protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPointViewBar();

	// Generated message map functions
protected:
	//{{AFX_MSG(CPointViewBar)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

#endif
