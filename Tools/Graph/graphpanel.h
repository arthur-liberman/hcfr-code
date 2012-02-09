/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#if !defined(AFX_GRAPHPANEL_H__6556B6BB_DE08_11D3_B4B4_00C04F89477F__INCLUDED_)
#define AFX_GRAPHPANEL_H__6556B6BB_DE08_11D3_B4B4_00C04F89477F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GraphPanel.h : header file
//

#include "graph_general.h"
#include "..\..\resource.h"

class FOffScreen;
class FVRuler;
class FHRuler;
class CCoordinates;
class CGraphWnd;
struct SGraphChange;
class CGraphPropertyPage;

/////////////////////////////////////////////////////////////////////////////
// CGraphPanel window

class CGraphPanel : public CWnd, public virtual CGraphBaseClass
{
public:

    #define MAX_TIP_COUNT 4

// Construction
public:
    CGraphPanel(FVRuler* ver_ruler, FHRuler* hor_ruler);

// Attributes
protected:

    FOffScreen* offscreen;
    FVRuler* vruler;
    FHRuler* hruler;

    CCoordinates* CurrentCoordsX;
    CCoordinates* CurrentCoordsY;

    COLORREF bkColor;
    COLORREF axisColor;  // Fx mod

	DWORD m_grflags;

    //zoom stuff
    BOOL bZoomActive;
    CRectTracker* zoomrect;
    //on-the-fly zoom rect creation
    BOOL bCreateZoomRect;
    CPoint new_zoom_1, new_zoom_2;

    //Panning support - thanks to Nigel Nunn (nnunn@ausport.gov.au)
    BOOL m_bPanning;
    CPoint m_PanStart, m_PanStop;

    //tooltips with coords info
    CToolTipCtrl tooltip;
    int tipCount;

    //property pages
    CGraphPropertyPage* graph_prop_page;

// Operations
public:

    void DoRedraw(CDC* dc, LPCRECT r);
    void DrawAxis(CDC* pDC, CRect& rect_to_draw);
    void UpdateGraphWindow(LPCRECT rect);
    void DrawToDC(CDC* dc_to_draw, CRect& rect_to_draw);
    int GetSquareSide(CDC* dest_dc);

    void InitCoords(double x1, double x2, double y1, double y2);
    void DrawPoints(CDC* dc, CRect& rect_to_draw);
    void DrawSquarePoint(CDC* dc, int x, int y);
    void AddPoint(SGraphChange* sgc);
    void UpdateTitles(BOOL bXAxis, BOOL bRedraw);
    void ApplyZoom();
    void GetToolTipString(CPoint pt, CString& tip_str);
    void ToggleGraphFlag(unsigned long graph_flag, BOOL bUpdate);
    void DoFit(UINT fitType);

    virtual void AppendMenuItems(CMenu* menu);
    virtual void OnRBMenuCommand(UINT command_id);
    virtual void AppendPropertyPage(CPropertySheet* prop_sheet);
    virtual void ReleasePropertyPage(UINT dialog_status);

    //graphix attributes
    DWORD GetGraphixFlags();
    DWORD SetGraphixFlags(DWORD new_flags, BOOL bRedraw);

    double GetX1();
    double GetX2();
    double GetY1();
    double GetY2();
    void GetWorldCoords(double* x1, double* x2, double* y1, double* y2);

    void SetMinX(double x, BOOL bRedraw);
    void SetMaxX(double x, BOOL bRedraw);
    void SetMinY(double y, BOOL bRedraw);
    void SetMaxY(double y, BOOL bRedraw);
    void SetWorldCoords(double _x1, double _x2, double _y1, double _y2, BOOL bRedraw);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraphPanel)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CGraphPanel();

	// Generated message map functions
protected:
	//{{AFX_MSG(CGraphPanel)
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CGraphPropertyPage dialog

class CGraphPropertyPage : public CPropertyPage
{
	DECLARE_DYNCREATE(CGraphPropertyPage)

// Construction
public:
	CGraphPropertyPage();
	~CGraphPropertyPage();

// Dialog Data
	//{{AFX_DATA(CGraphPropertyPage)
	enum { IDD = IDD_GRAPH_GRAPH_PROP_PAGE };
	double	m_x1;
	double	m_x2;
	double	m_y1;
	double	m_y2;
	BOOL	m_PointMarks;
	BOOL	m_MouseCoords;
	BOOL	m_ShowAxis;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CGraphPropertyPage)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CGraphPropertyPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GRAPHPANEL_H__6556B6BB_DE08_11D3_B4B4_00C04F89477F__INCLUDED_)
