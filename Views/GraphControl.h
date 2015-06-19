/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2011 Association Homecinema Francophone.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.htm. If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
//  Author(s):
//	François-Xavier CHABOUD
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_GRAPHCONTROL_H__A5F56589_1DF3_4759_AF94_C8FA6CD76575__INCLUDED_)
#define AFX_GRAPHCONTROL_H__A5F56589_1DF3_4759_AF94_C8FA6CD76575__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// graphcontrol.h : header file
//

#include "PPTooltip.h" 

/////////////////////////////////////////////////////////////////////////////
// CGraph
/////////////////////////////////////////////////////////////////////////////
class CDecimalPoint
{
// Construction
public:
	CDecimalPoint::CDecimalPoint(double aX=0, double aY=0, LPCSTR lpszTextInfo=NULL) { x=aX; y=aY; if ( lpszTextInfo ) m_text=lpszTextInfo; }

	// Attributes
public:
	double x,y;
	CString m_text;
};

class CGraph
{
// Construction
public:
	CGraph::CGraph();
	CGraph(COLORREF aColor, char* apTitle=NULL, int aPenWidth=2,int aPenStyle=PS_SOLID);
	CGraph(const CGraph & aGraph);
	CGraph& operator =(const CGraph& obj);

	// Attributes
public:
	COLORREF m_color; 
	int m_penStyle;
	int m_penWidth;
	CString m_Title;
	BOOL m_doShowPoints;
	BOOL m_doShowToolTips;
	CArray<CDecimalPoint,CDecimalPoint> m_pointArray;
};

/////////////////////////////////////////////////////////////////////////////
// CGraphControl window
/////////////////////////////////////////////////////////////////////////////
class CGraphControl : public CWnd
{
// Construction
public:
	CGraphControl();

// Attributes
public:
	BOOL m_doShowAxis;
	BOOL m_doGradientBg;
	BOOL m_doUserScales;
	BOOL m_doSpectrumBg;
	BOOL m_doShowAllPoints;
	BOOL m_doShowAllToolTips;

	BOOL m_doShowXLabel;
	BOOL m_doShowYLabel;
	BOOL m_doShowDataLabel;

	CArray<CGraph,CGraph> m_graphArray;
	double m_xScale;
	double m_xAxisStep;
	double m_minXGrow,m_maxXGrow;
	double m_yScale;
	double m_yAxisStep;
	double m_minYGrow,m_maxYGrow;
	double m_minX,m_minY;
	double m_maxX,m_maxY;
	LPSTR  m_pXUnitStr;
	LPSTR  m_pYUnitStr;
	
protected:
	CPPToolTip m_tooltip;
	COLORREF *	m_pSpectrumColors;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraphControl)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CGraphControl();

    BOOL Create(LPCTSTR lpszWindowName, const RECT& rect, CWnd* pParentWnd, UINT nID, DWORD dwStyle = WS_CHILD | WS_VISIBLE);

	int AddGraph(COLORREF newColor = 0, char* title = "", int aPenWidth=2, int aPenStyle=PS_SOLID);
	int AddGraph(CGraph & aGraph);
	int RemoveGraph(int index);
	int AddPoint(int graphnum, double x, double y, LPCSTR lpszTextInfo = NULL );
	int RemovePoint(int graphnum, int index);
	void ClearGraph(int graphnum);

	void ReadSettings(LPSTR aConfigStr, BOOL bReadGraphSettings=TRUE);
	void WriteSettings(LPSTR aConfigStr);
	void ChangeSettings();
	void CopySettings(const CGraphControl & aGraphControl, int aGraphSrcIndex, int aGraphDestIndex);
	void ChangeScale();

	void SetScale(double minX, double maxX, double minY, double maxY);
	void SetXScale(double minX, double maxX) { SetScale(minX,maxX,m_minY,m_maxY); }
	void SetYScale(double minY, double maxY) { SetScale(m_minX,m_maxX,minY,maxY); }
	void SetXAxisProps(LPSTR pUnitStr, double axisStep, double minGrow, double maxGrow);
	void SetYAxisProps(LPSTR pUnitStr, double axisStep, double minGrow, double maxGrow);
	void ShiftXScale(double deltaX);
	void ShiftYScale(double deltaY);
	void GrowXScale(double deltaMinX, double deltaMaxX);
	void GrowYScale(double deltaMinY, double deltaMaxY);
	void FitXScale(BOOL doRound=FALSE, double roundStep=10.0);
	void FitYScale(BOOL doRound=FALSE, double roundStep=10.0);

	void DrawAxis(CDC *pDC, CRect rect, BOOL bWhiteBkgnd);
	void DrawBackground(CDC *pDC, CRect rect, BOOL bForFile);
	void DrawSpectrumColors(CDC *pDC, CRect rect);
	void DrawGraphs(CDC *pDC, CRect rect);

	static void DrawFiligree(CDC *pDC, CRect rect, COLORREF clr);
	void SaveGraphs(CGraphControl *pGraphToAppend=NULL, CGraphControl *pGraphToAppend2=NULL, CGraphControl *pGraphToAppend3=NULL, bool do_Dialog = TRUE, int nSequence = 0);
	void SaveGraphFile ( CSize ImageSize, LPCSTR lpszPathName, int ImageFormat = 0, int ImageQuality = 95, CGraphControl * * pOtherGraphs = NULL, int NbOtherGraphs = 0, bool do_Gradient = FALSE );

protected:
	int GetGraphX(double x,CRect rect);
	int GetGraphY(double y,CRect rect);
	CPoint GetGraphPoint(CDecimalPoint aPoint,CRect rect);

	// Generated message map functions
protected:
	//{{AFX_MSG(CGraphControl)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GRAPHCONTROL_H__A5F56589_1DF3_4759_AF94_C8FA6CD76575__INCLUDED_)
