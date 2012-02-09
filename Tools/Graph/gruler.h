/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
// Changed from RULER.H by Nick
#ifndef __GRULER_H
#define __GRULER_H

#include "coords.h"

//FRuler object
class FRuler : public CWnd
{
protected:
	DECLARE_DYNCREATE(FRuler)

// Attributes
public:
	LOGFONT MainFontRec;
//	int MinShift,MaxShift;
	double MinValue, MaxValue;
	CString sUnit, sTitle;
	CToolTipCtrl tooltip;

// Operations
public:
	int ConvertLogPixToRealPix(CDC* dest_dc, double log_pix_num, BOOL b_x_axis);
	virtual int Width(CDC* dest_dc){return 0;};
	void SetMinMax(double minv, double maxv, BOOL bRedraw);
	void SetNewTitles(char* newUOM, char* newTitle);
	virtual int GetNMax(CDC* dc_to_draw, CRect rect_to_draw){return 0;};
	virtual void DrawRuler(CDC* dc_to_draw, CRect rect_to_draw){};

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(FRuler)
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

// Implementation
protected:
	FRuler();
	virtual ~FRuler();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(FRuler)
	afx_msg void OnPaint();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

class FHRuler : public FRuler
{
	DECLARE_DYNAMIC(FHRuler)
  public:
	FHRuler();
	virtual int Width(CDC* dest_dc);
	virtual int GetNMax(CDC* dc_to_draw, CRect rect_to_draw);
	virtual void DrawRuler(CDC* dc_to_draw, CRect rect_to_draw);

protected:
	//{{AFX_MSG(FHRuler)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class FVRuler : public FRuler
{
	DECLARE_DYNAMIC(FVRuler)
  public:
	FVRuler();
	virtual int Width(CDC* dest_dc);
	virtual int GetNMax(CDC* dc_to_draw, CRect rect_to_draw);
	virtual void DrawRuler(CDC* dc_to_draw, CRect rect_to_draw);
protected:
	//{{AFX_MSG(FVRuler)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif

