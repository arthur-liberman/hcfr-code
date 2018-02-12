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
//  Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// graphcontrol.cpp : implementation file
//

#include "stdafx.h"
#include "..\ColorHCFR.h"
#include "ximage.h"
#include "graphcontrol.h"
#include "graphsettingsdialog.h"
#include "graphscalepropdialog.h"
#include "savegraphdialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGraph

CGraph::CGraph(COLORREF aColor, char* apTitle, int aPenWidth,int aPenStyle)
{
	m_color=aColor;
	m_Title=apTitle;
	m_penWidth=aPenWidth;
	m_penStyle=aPenStyle;
	m_doShowPoints=TRUE;
	m_doShowToolTips=TRUE;
}

CGraph::CGraph()
{
	m_color=RGB(128,128,128);
	m_penWidth=2;
	m_penStyle=PS_SOLID;
	m_doShowPoints=TRUE;
	m_doShowToolTips=TRUE;
}

CGraph::CGraph(const CGraph & aGraph)
{
	m_Title=aGraph.m_Title;
	m_color=aGraph.m_color;
	m_penWidth=aGraph.m_penWidth;
	m_penStyle=aGraph.m_penStyle;
	m_doShowPoints=aGraph.m_doShowPoints;
	m_doShowToolTips=aGraph.m_doShowToolTips;
	m_pointArray.Copy(aGraph.m_pointArray);
}

CGraph& CGraph::operator =(const CGraph& obj)
{
	if(&obj == this) return *this;
	m_Title=obj.m_Title;
	m_color=obj.m_color;
	m_penWidth=obj.m_penWidth;
	m_penStyle=obj.m_penStyle;
	m_doShowPoints=obj.m_doShowPoints;
	m_doShowToolTips=obj.m_doShowToolTips;
	m_pointArray.Copy(obj.m_pointArray);

	return *this;
}

/////////////////////////////////////////////////////////////////////////////
// CGraphControl

CGraphControl::CGraphControl()
{
	m_doGradientBg=TRUE;
	m_doUserScales=FALSE;
	m_doSpectrumBg=FALSE;
	m_doShowAxis=TRUE;
	m_doShowXLabel=TRUE;
	m_doShowYLabel=TRUE;
	m_doShowAllPoints=FALSE;
	m_doShowAllToolTips=TRUE;
	m_doShowDataLabel = FALSE;
	
	m_pSpectrumColors = NULL;

	m_minXGrow=0;
	m_maxXGrow=100;
	m_xAxisStep=10.0;
	m_pXUnitStr=NULL;
	m_minYGrow=0;
	m_maxYGrow=100;
	m_yAxisStep=10.0;
	m_pYUnitStr=NULL;
	SetScale(0,100,0,100);
}

CGraphControl::~CGraphControl()
{
	if ( m_pSpectrumColors )
	{
		delete m_pSpectrumColors;
		m_pSpectrumColors = NULL;
	}
}

BOOL CGraphControl::Create(LPCTSTR lpszWindowName, const RECT& rect, CWnd* pParentWnd, UINT nID, 
    DWORD dwStyle/* = WS_CHILD | WS_VISIBLE*/)
{
    return CWnd::Create(NULL, lpszWindowName, dwStyle, rect, pParentWnd, nID);
}

int CGraphControl::AddGraph(COLORREF aColor, char* aTitle, int aPenWidth, int aPenStyle)
{
	int index=m_graphArray.Add(CGraph(aColor,aTitle,aPenWidth,aPenStyle));
	return index;
}

int CGraphControl::AddGraph(CGraph & aGraph)
{
	int index=m_graphArray.Add(aGraph);
	return index;
}

int CGraphControl::RemoveGraph(int index)
{
	m_graphArray.RemoveAt(index);
	return index;
}

int CGraphControl::AddPoint(int graphnum, double x, double y, LPCSTR lpszTextInfo, double Y)
{
	if(graphnum != -1 && graphnum < m_graphArray.GetSize())
		return m_graphArray[graphnum].m_pointArray.Add(CDecimalPoint(x,y,lpszTextInfo,Y));
	else
		return -1;
}

int CGraphControl::RemovePoint(int graphnum, int index)
{
	if(graphnum != -1 && graphnum < m_graphArray.GetSize())
		m_graphArray[graphnum].m_pointArray.RemoveAt(index);
	return index;
}

void CGraphControl::ClearGraph(int graphnum)
{
	if(graphnum != -1 && graphnum < m_graphArray.GetSize())
		m_graphArray[graphnum].m_pointArray.RemoveAll();
}

bool CGraphControl::ReadSettings(LPSTR aConfigStr, BOOL bReadGraphSettings)
{
	m_doGradientBg=GetConfig()->GetProfileInt(aConfigStr,"Show Gradient",TRUE);
	m_doUserScales=GetConfig()->GetProfileInt(aConfigStr,"User Scales",FALSE);
	m_doShowAxis=GetConfig()->GetProfileInt(aConfigStr,"Show Axis",TRUE);
	m_doShowDataLabel=GetConfig()->GetProfileInt(aConfigStr,"Show Data Labels",FALSE);
	m_doShowXLabel=GetConfig()->GetProfileInt(aConfigStr,"Show X Labels",TRUE);
	m_doShowYLabel=GetConfig()->GetProfileInt(aConfigStr,"Show Y Labels",TRUE);
	m_doShowAllPoints=GetConfig()->GetProfileInt(aConfigStr,"Show All Points",TRUE);
	m_doShowAllToolTips=GetConfig()->GetProfileInt(aConfigStr,"Show All ToolTips",TRUE);
	bool userScale = FALSE;

/* GGA: Do not use saved scale data */
	// do not read settings if not in config => avoid to overwrite defaults set by parent
	if(GetConfig()->IsProfileEntryDefined(aConfigStr,"Max Y") )
	{
		userScale = TRUE;
		m_minX=GetConfig()->GetProfileDouble(aConfigStr,"Min X",0);
		m_maxX=GetConfig()->GetProfileDouble(aConfigStr,"Max X",100);
		m_minXGrow=GetConfig()->GetProfileDouble(aConfigStr,"Min Grow X",0);
		m_maxXGrow=GetConfig()->GetProfileDouble(aConfigStr,"Max Grow X",100);
		m_xAxisStep=GetConfig()->GetProfileDouble(aConfigStr,"Axis Step X",10);
		m_minY=GetConfig()->GetProfileDouble(aConfigStr,"Min Y",0);
		m_maxY=GetConfig()->GetProfileDouble(aConfigStr,"Max Y",100);
		m_minYGrow=GetConfig()->GetProfileDouble(aConfigStr,"Min Grow Y",0);
		m_maxYGrow=GetConfig()->GetProfileDouble(aConfigStr,"Max Grow Y",100);
		m_yAxisStep=GetConfig()->GetProfileDouble(aConfigStr,"Axis Step Y",10);
		
		SetScale(m_minX,m_maxX,m_minY,m_maxY);
	}
//*/

	if(bReadGraphSettings)
		for(int i=0;i<m_graphArray.GetSize();i++)
		{
			CString aStr = CString(aConfigStr) + ": Graph " + m_graphArray[i].m_Title;
			
			// do not read settings if not in config => avoid to overwrite defaults set by parent
			if(!GetConfig()->IsProfileEntryDefined(aStr,"Color") )  
				break;

			m_graphArray[i].m_color=GetConfig()->GetProfileColor(aStr,"Color");
			m_graphArray[i].m_penStyle=GetConfig()->GetProfileInt(aStr,"Pen Style",PS_SOLID);	
			m_graphArray[i].m_penWidth=GetConfig()->GetProfileInt(aStr,"Pen Width",2);	
			m_graphArray[i].m_doShowPoints=GetConfig()->GetProfileInt(aStr,"Show Points",TRUE);	
			m_graphArray[i].m_doShowToolTips=GetConfig()->GetProfileInt(aStr,"Show ToolTips",TRUE);	
		}
	return userScale;
}

void CGraphControl::WriteSettings(LPSTR aConfigStr)
{
	GetConfig()->WriteProfileInt(aConfigStr,"Show Gradient",m_doGradientBg);
	GetConfig()->WriteProfileInt(aConfigStr,"User Scales",m_doUserScales);
	GetConfig()->WriteProfileInt(aConfigStr,"Show Axis",m_doShowAxis);
	GetConfig()->WriteProfileInt(aConfigStr,"Show Data Labels",m_doShowDataLabel);
	GetConfig()->WriteProfileInt(aConfigStr,"Show X Labels",m_doShowXLabel);
	GetConfig()->WriteProfileInt(aConfigStr,"Show Y Labels",m_doShowYLabel);
	GetConfig()->WriteProfileInt(aConfigStr,"Show All Points",m_doShowAllPoints);
	GetConfig()->WriteProfileInt(aConfigStr,"Show All ToolTips",m_doShowAllToolTips);

	GetConfig()->WriteProfileDouble(aConfigStr,"Min X",m_minX);
	GetConfig()->WriteProfileDouble(aConfigStr,"Max X",m_maxX);
	GetConfig()->WriteProfileDouble(aConfigStr,"Min Grow X",m_minXGrow);
	GetConfig()->WriteProfileDouble(aConfigStr,"Max Grow X",m_maxXGrow);
	GetConfig()->WriteProfileDouble(aConfigStr,"Axis Step X",m_xAxisStep);
	GetConfig()->WriteProfileDouble(aConfigStr,"Min Y",m_minY);
	GetConfig()->WriteProfileDouble(aConfigStr,"Max Y",m_maxY);
	GetConfig()->WriteProfileDouble(aConfigStr,"Min Grow Y",m_minYGrow);
	GetConfig()->WriteProfileDouble(aConfigStr,"Max Grow Y",m_maxYGrow);
	GetConfig()->WriteProfileDouble(aConfigStr,"Axis Step Y",m_yAxisStep);

	for(int i=0;i<m_graphArray.GetSize();i++)
	{
		CString aStr = CString(aConfigStr) + ": Graph " + m_graphArray[i].m_Title;
		GetConfig()->WriteProfileColor(aStr,"Color",m_graphArray[i].m_color);
		GetConfig()->WriteProfileInt(aStr,"Pen Style",m_graphArray[i].m_penStyle);	
		GetConfig()->WriteProfileInt(aStr,"Pen Width",m_graphArray[i].m_penWidth);	
		GetConfig()->WriteProfileInt(aStr,"Show Points",m_graphArray[i].m_doShowPoints);	
		GetConfig()->WriteProfileInt(aStr,"Show ToolTips",m_graphArray[i].m_doShowToolTips);	
	}
}

void CGraphControl::ChangeSettings()
{
	CGraphSettingsDialog dialog;

	dialog.m_doGradientBg=m_doGradientBg;
	dialog.m_doUserScales=m_doUserScales;
	dialog.m_doShowAxis=m_doShowAxis;
	dialog.m_doShowDataLabel=m_doShowDataLabel;
	dialog.m_doShowXLabel=m_doShowXLabel;
	dialog.m_doShowYLabel=m_doShowYLabel;
	dialog.m_doShowAllPoints=m_doShowAllPoints;
	dialog.m_doShowAllToolTips=m_doShowAllToolTips;
	dialog.m_graphArray.Copy(m_graphArray);

	if(dialog.DoModal() == IDOK)
	{
		m_graphArray.Copy(dialog.m_graphArray);
		m_doGradientBg=dialog.m_doGradientBg;
		m_doUserScales=dialog.m_doUserScales;
		m_doShowAxis=dialog.m_doShowAxis;
		m_doShowDataLabel=dialog.m_doShowDataLabel;
		m_doShowXLabel=dialog.m_doShowXLabel;
		m_doShowYLabel=dialog.m_doShowYLabel;
		m_doShowAllPoints=dialog.m_doShowAllPoints;
		m_doShowAllToolTips=dialog.m_doShowAllToolTips;
		Invalidate(TRUE);
	}
}

void CGraphControl::CopySettings(const CGraphControl & aGraphControl, int aGraphSrcIndex, int aGraphDestIndex)
{
	m_doGradientBg=aGraphControl.m_doGradientBg;
	m_doUserScales=aGraphControl.m_doUserScales;
	m_doShowAxis=aGraphControl.m_doShowAxis;
	m_doShowDataLabel=aGraphControl.m_doShowDataLabel;
	m_doShowXLabel=aGraphControl.m_doShowXLabel;
	m_doShowYLabel=aGraphControl.m_doShowYLabel;
	m_doShowAllPoints=aGraphControl.m_doShowAllPoints;
	m_doShowAllToolTips=aGraphControl.m_doShowAllToolTips;
	m_graphArray.SetAtGrow(aGraphDestIndex,aGraphControl.m_graphArray.GetAt(aGraphSrcIndex));
}

void CGraphControl::ChangeScale()
{
	CGraphScalePropDialog dialog;

	dialog.m_minX=m_minX;
	dialog.m_maxX=m_maxX;
	dialog.m_minXGrow=m_minXGrow;
	dialog.m_maxXGrow=m_maxXGrow;
	dialog.m_xAxisStep=m_xAxisStep;
	dialog.m_minY=m_minY;
	dialog.m_maxY=m_maxY;
	dialog.m_minYGrow=m_minYGrow;
	dialog.m_maxYGrow=m_maxYGrow;
	dialog.m_yAxisStep=m_yAxisStep;

	if(dialog.DoModal() == IDOK)
	{
		m_minX=dialog.m_minX;
		m_maxX=dialog.m_maxX;
		m_minXGrow=dialog.m_minXGrow;
		m_maxXGrow=dialog.m_maxXGrow;
		m_xAxisStep=dialog.m_xAxisStep;
		m_minY=dialog.m_minY;
		m_maxY=dialog.m_maxY;
		m_minYGrow=dialog.m_minYGrow;
		m_maxYGrow=dialog.m_maxYGrow;
		m_yAxisStep=dialog.m_yAxisStep;
		SetScale(m_minX,m_maxX,m_minY,m_maxY);
		Invalidate(TRUE);
	}
}

BEGIN_MESSAGE_MAP(CGraphControl, CWnd)
	//{{AFX_MSG_MAP(CGraphControl)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CGraphControl message handlers

void CGraphControl::SetScale(double minX, double maxX, double minY, double maxY)
{
	if(maxX==minX || maxY==minY)
		return;

	m_xScale=1.0/(maxX-minX);
	m_yScale=1.0/(maxY-minY);

	m_minX=minX;
	m_minY=minY;
	m_maxX=maxX;
	m_maxY=maxY;
}

void CGraphControl::SetXAxisProps(LPSTR pUnitStr, double axisStep, double minGrow, double maxGrow)
{
	m_pXUnitStr=pUnitStr;
	m_xAxisStep=axisStep;
	m_minXGrow=minGrow;
	m_maxXGrow=maxGrow;
}

void CGraphControl::SetYAxisProps(LPSTR pUnitStr, double axisStep, double minGrow, double maxGrow)
{
	m_pYUnitStr=pUnitStr;
	m_yAxisStep=axisStep;
	m_minYGrow=minGrow;
	m_maxYGrow=maxGrow;
}

void CGraphControl::GrowXScale(double deltaMinX, double deltaMaxX)
{
	double minX=m_minX+deltaMinX;
	double maxX=m_maxX+deltaMaxX;

	if(maxX == minX || minX > maxX)
		return;

//	if(minX < m_minXGrow || maxX > m_maxXGrow || minX > maxX)
//		return;

	if(minX >= m_minXGrow)
		m_minX=minX;
	if(maxX <= m_maxXGrow)
		m_maxX=maxX;
	m_xScale=1.0/(m_maxX-m_minX);
}

void CGraphControl::GrowYScale(double deltaMinY, double deltaMaxY)
{
	double minY=m_minY+deltaMinY;
	double maxY=m_maxY+deltaMaxY;

	if(maxY == minY )
		return;

	if(minY < m_minYGrow || maxY > m_maxYGrow || minY > maxY)
		return;

	m_maxY=maxY;
	m_minY=minY;
	m_yScale=1.0/(m_maxY-m_minY);
}

void CGraphControl::ShiftXScale(double deltaX)
{
	double minX=m_minX+deltaX;
	double maxX=m_maxX+deltaX;

	if(minX < m_minXGrow || maxX > m_maxXGrow)
		return;

	m_minX=minX;
	m_maxX=maxX;
	m_xScale=1.0/(m_maxX-m_minX);
}

void CGraphControl::ShiftYScale(double deltaY)
{
	double minY=m_minY+deltaY;
	double maxY=m_maxY+deltaY;

	if(minY < m_minYGrow || maxY > m_maxYGrow)
		return;

	m_minY=minY;
	m_maxY=maxY;
	m_yScale=1.0/(m_maxY-m_minY);
}

void CGraphControl::FitXScale(BOOL doRound, double roundStep)
{
	double	minX=9999999.99;
	double	maxX=-9999999.99;

	for(int j=0;j<m_graphArray.GetSize();j++)
		for(int i=0; i<m_graphArray[j].m_pointArray.GetSize(); i++)
		{
			if(m_graphArray[j].m_pointArray[i].x < minX)
				minX=m_graphArray[j].m_pointArray[i].x;
			if(m_graphArray[j].m_pointArray[i].x > maxX)
				maxX=m_graphArray[j].m_pointArray[i].x;
		}
	if(doRound)
	{
		minX=(floor(minX/roundStep)-1) * roundStep;
		maxX=(floor(maxX/roundStep)+1) * roundStep;
	}

	if(minX >= m_minXGrow)
		m_minX=minX;
	else
		m_minX=m_minXGrow;

	if(maxX <= m_maxXGrow)
		m_maxX=maxX;
	else
		m_maxX=m_maxXGrow;

	SetScale(m_minX,m_maxX,m_minY,m_maxY);
}

void CGraphControl::FitYScale(BOOL doRound, double roundStep, bool isGamma)
{
	double	minY=9999999.99;
	double	maxY=-9999999.99;

	for(int j=0;j<m_graphArray.GetSize();j++)
		for(int i=0; i<m_graphArray[j].m_pointArray.GetSize(); i++)
		{
			if(m_graphArray[j].m_pointArray[i].x >= m_minX && m_graphArray[j].m_pointArray[i].x <= m_maxX )
			{
				if(m_graphArray[j].m_pointArray[i].y < minY)
					minY=m_graphArray[j].m_pointArray[i].y;
				if(m_graphArray[j].m_pointArray[i].y > maxY)
					maxY=m_graphArray[j].m_pointArray[i].y;
			}
		}

	if(doRound)
	{
		minY=(floor(minY/roundStep)-1) * roundStep;
		maxY=(floor(maxY/roundStep)+1) * roundStep;
	}

	//force all luminance plots to minY=0
	if (!isGamma)
	{
		minY = max(minY,0);
		m_minYGrow=min(minY,m_minYGrow);
		m_maxYGrow=max(maxY,m_maxYGrow);
		double delta = (maxY - minY);
		if ( delta <= 30)
			SetYAxisProps(m_pYUnitStr,maxY<0.5?0.05:(maxY<1||delta<5)?0.1:maxY<15?1.0:2.0,m_minYGrow,m_maxYGrow);
		else	
			SetYAxisProps(m_pYUnitStr,maxY<100?10:maxY<200?20:maxY<500?25:maxY<1000?50:100,m_minYGrow,m_maxYGrow);
	} else
	{
		if (GetConfig()->m_GammaOffsetType == 5 || GetConfig()->m_GammaOffsetType == 7)
		{

		double delta = (maxY - minY);
			if ( delta <= 30)
				SetYAxisProps(m_pYUnitStr,maxY<0.5?0.05:(maxY<1||delta<5)?0.1:maxY<15?1.0:2.0,m_minYGrow,m_maxYGrow);
			else	
				SetYAxisProps(m_pYUnitStr,maxY<100?10:maxY<200?20:maxY<500?25:maxY<1000?50:100,m_minYGrow,m_maxYGrow);
		}
	}

	SetScale(m_minX,m_maxX,minY,maxY);
}

int CGraphControl::GetGraphX(double x,CRect rect) 
{
	return (int)((x-m_minX)*(double)rect.Width()*m_xScale);	// graph is from 0 to m_xScale in width
}

int CGraphControl::GetGraphY(double y,CRect rect) 
{
	return (int)((double)rect.bottom-(y-m_minY)*(double)rect.Height()*m_yScale);	// graph is from 0 to m_yScale in height
}

CPoint CGraphControl::GetGraphPoint(CDecimalPoint aPoint,CRect rect)
{
	return CPoint(GetGraphX(aPoint.x,rect),GetGraphY(aPoint.y,rect));
}

void CGraphControl::DrawBackground(CDC *pDC, CRect rect, BOOL bForFile)
{
	if ( bForFile ? GetConfig()->m_bWhiteBkgndOnFile : GetConfig()->m_bWhiteBkgndOnScreen )
		pDC->FillSolidRect(rect,RGB(255,255,255));
	else if(m_doGradientBg)
		DrawGradient(pDC,rect,RGB(32+m_minX,32+m_minX,32+m_minX),RGB(m_maxX*2.55,m_maxX*2.55,m_maxX*2.55),true);
	else
		pDC->FillSolidRect(rect,RGB(0,0,0));
}

void CGraphControl::DrawSpectrumColors(CDC *pDC, CRect rect)
{
static double spectral_chromaticities[][2] = {
    { 0.1741, 0.0050 },			/* 380 nm */
    { 0.1740, 0.0050 },			/* 385 nm */
    { 0.1738, 0.0049 },			/* 390, and so on */
    { 0.1736, 0.0049 },
    { 0.1733, 0.0048 },
    { 0.1730, 0.0048 },
    { 0.1726, 0.0048 },
    { 0.1721, 0.0048 },
    { 0.1714, 0.0051 },
    { 0.1703, 0.0058 },
    { 0.1689, 0.0069 },
    { 0.1669, 0.0086 },
    { 0.1644, 0.0109 },
    { 0.1611, 0.0138 },
    { 0.1566, 0.0177 },
    { 0.1510, 0.0227 },
    { 0.1440, 0.0297 },
    { 0.1355, 0.0399 },
    { 0.1241, 0.0578 },
    { 0.1096, 0.0868 },
    { 0.0913, 0.1327 },
    { 0.0687, 0.2007 },
    { 0.0454, 0.2950 },
    { 0.0235, 0.4127 },
    { 0.0082, 0.5384 },
    { 0.0039, 0.6548 },
    { 0.0139, 0.7502 },
    { 0.0389, 0.8120 },
    { 0.0743, 0.8338 },
    { 0.0943, 0.8310 },
    { 0.1142, 0.8262 },
    { 0.1547, 0.8059 },
    { 0.1929, 0.7816 },
    { 0.2296, 0.7543 },
    { 0.2658, 0.7243 },
    { 0.3016, 0.6923 },
    { 0.3373, 0.6589 },
    { 0.3731, 0.6245 },
    { 0.4087, 0.5896 },
    { 0.4441, 0.5547 },
    { 0.4788, 0.5202 },
    { 0.5125, 0.4866 },
    { 0.5448, 0.4544 },
    { 0.5752, 0.4242 },
    { 0.6029, 0.3965 },
    { 0.6270, 0.3725 },
    { 0.6482, 0.3514 },
    { 0.6658, 0.3340 },
    { 0.6801, 0.3197 },
    { 0.6915, 0.3083 },
    { 0.7006, 0.2993 },
    { 0.7079, 0.2920 },
    { 0.7140, 0.2859 },
    { 0.7190, 0.2809 },
    { 0.7230, 0.2770 },
    { 0.7260, 0.2740 },
    { 0.7283, 0.2717 },
    { 0.7300, 0.2700 },
    { 0.7311, 0.2689 },
    { 0.7320, 0.2680 },
    { 0.7327, 0.2673 },
    { 0.7334, 0.2666 },
    { 0.7340, 0.2660 },
    { 0.7344, 0.2656 },
    { 0.7346, 0.2654 },
    { 0.7347, 0.2653 },
    { 0.7347, 0.2653 },
    { 0.7347, 0.2653 },
    { 0.7347, 0.2653 },
    { 0.7347, 0.2653 },
    { 0.7347, 0.2653 },
    { 0.7347, 0.2653 }         /* 730 nm */
};

	int	x, nWaveIndex;
	int	nR = 0, nG = 0, nB = 0;
	double WaveLength, WaveIndex;
	double xCIE, yCIE;
	double xCIE2, yCIE2;
	double	r, g, b;
	double	cmax;
	double	base, coef;
	double	gamma = 1.0/3.0;
	CColor	RGBColor;
	POINT	ptOrg;
	POINT *	pts;
	CRgn	Rgn;

	if ( m_pSpectrumColors == NULL )
	{
		// Create initial spectrum colors
		m_pSpectrumColors = new COLORREF [ 1000 ];

		for ( x = 0; x < 1000 ; x ++ )
		{
			WaveLength = m_minX + ( (double) x / ( (double) 1000.0 * m_xScale ) );
			WaveIndex = ( WaveLength - 380.0 ) / 5.0;
			nWaveIndex = (int)floor(WaveIndex);
			WaveIndex = WaveIndex - (double) nWaveIndex;

			if ( nWaveIndex < 0 )
			{
				nWaveIndex = 0;
				WaveIndex = 0.0;
			}
			else if ( nWaveIndex > sizeof ( spectral_chromaticities ) / sizeof ( spectral_chromaticities [ 0 ] ) - 1 )
			{
				nWaveIndex = sizeof ( spectral_chromaticities ) / sizeof ( spectral_chromaticities [ 0 ] ) - 1;
				WaveIndex = 1.0;
			}

			xCIE = spectral_chromaticities [ nWaveIndex ] [ 0 ];
			yCIE = spectral_chromaticities [ nWaveIndex ] [ 1 ];
			xCIE2 = spectral_chromaticities [ nWaveIndex + 1 ] [ 0 ];
			yCIE2 = spectral_chromaticities [ nWaveIndex + 1 ] [ 1 ];
			
			xCIE += (xCIE2 - xCIE)*WaveIndex;
			yCIE += (yCIE2 - yCIE)*WaveIndex;

			CColor	WaveColor ( xCIE, yCIE );
			ColorRGB RGBColor = WaveColor.GetRGBValue ((GetColorReference()));

			r = pow(max(0.0,RGBColor[0]),gamma);
			g = pow(max(0.0,RGBColor[1]),gamma);
			b = pow(max(0.0,RGBColor[2]),gamma);

			cmax = max(r,g);
			if ( b>cmax)
				cmax=b;

			base = 0.0;
			coef = 192.0;

			nR = (int) (r/cmax*coef+base);
			nG = (int) (g/cmax*coef+base);
			nB = (int) (b/cmax*coef+base);

			m_pSpectrumColors [ x ] = RGB(nR,nG,nB);
		}
	}

	ptOrg = pDC ->GetWindowOrg ();
	pts = new POINT [ m_graphArray[0].m_pointArray.GetSize() + 2 ];
	
	pts [ 0 ].x = 0 - ptOrg.x;
	pts [ 0 ].y = rect.bottom - ptOrg.y;
	for ( int i = 0; i < m_graphArray[0].m_pointArray.GetSize() ; i ++ )
	{
		pts [ i + 1 ] = GetGraphPoint(m_graphArray[0].m_pointArray[i],rect);
		pts [ i + 1 ].x -= ptOrg.x;
		pts [ i + 1 ].y -= ptOrg.y;

	}
	pts [ m_graphArray[0].m_pointArray.GetSize() + 1 ].x = rect.right - ptOrg.x;
	pts [ m_graphArray[0].m_pointArray.GetSize() + 1 ].y = rect.bottom - ptOrg.y;

	Rgn.CreatePolygonRgn ( pts, m_graphArray[0].m_pointArray.GetSize() + 2, ALTERNATE );
	delete [] pts;

	pDC -> SelectClipRgn ( & Rgn );

	for ( x = 0; x < rect.right ; x += 3 )
	{
		nWaveIndex = ( x + 1 ) * 1000 / rect.right;

		CBrush	br(m_pSpectrumColors [ nWaveIndex ]);
		CRect	r(x, 0, x + 3, rect.bottom);
		pDC ->FillRect ( & r, & br );

	}

	pDC -> SelectStockObject ( BLACK_PEN );
	pDC -> SelectClipRgn ( NULL );
}

void CGraphControl::DrawGraphs(CDC *pDC, CRect rect)
{
	int lineWidth=2;
	m_tooltip.RemoveAllTools();

	for(int j=0;j<m_graphArray.GetSize();j++)
	{
		COLORREF graphColor=m_graphArray[j].m_color;

		CPen graphPen(m_graphArray[j].m_penStyle,m_graphArray[j].m_penWidth,graphColor);
	    CPen *pOldPen = pDC->SelectObject(&graphPen); 
	
		if(m_graphArray[j].m_pointArray.GetSize())
			pDC->MoveTo(GetGraphPoint(m_graphArray[j].m_pointArray[0],rect));

		for(int i=0; i<m_graphArray[j].m_pointArray.GetSize(); i++)
		{
			pDC->LineTo(GetGraphPoint(m_graphArray[j].m_pointArray[i],rect));
			if(m_doShowAllPoints && m_graphArray[j].m_doShowPoints)
			{
				CSize pointSize(4*lineWidth,4*lineWidth);
				CPoint pointPos(GetGraphPoint(m_graphArray[j].m_pointArray[i],rect)-CPoint(pointSize.cx/2,pointSize.cy/2));
				CRect pointRect(pointPos,pointSize);
				pDC->FillSolidRect(pointRect,graphColor);
			}

			if(m_doShowAllToolTips && m_graphArray[j].m_doShowToolTips)
			{
				CString str,str2;
				str.Format("%.2f %s: %.3f ", m_graphArray[j].m_pointArray[i].x, ( m_pXUnitStr ? m_pXUnitStr : (GetConfig()->m_PercentGray) ), m_graphArray[j].m_pointArray[i].y);
				//add absolute luminance for luminance plots
				if(m_pYUnitStr)
					str+=m_pYUnitStr;

				if (m_graphArray[j].m_pointArray[i].Y > 0.0)
				{
					str2.Format("\n%.3f cd/m^2 ", m_graphArray[j].m_pointArray[i].y / 100. *  m_graphArray[j].m_pointArray[i].Y);
				} else
					str2.Format("%s","\n");
				
				str+=str2;

				if ( ! m_graphArray[j].m_pointArray[i].m_text.IsEmpty () )
				{
					str += "\n";
					str += m_graphArray[j].m_pointArray[i].m_text;
				}

				CSize pointSize(4*lineWidth,4*lineWidth);
				CPoint pointPos(GetGraphPoint(m_graphArray[j].m_pointArray[i],rect)-CPoint(pointSize.cx/2,pointSize.cy/2));
				CRect pointRect(pointPos,pointSize);
				m_tooltip.AddTool(this, "<b>"+m_graphArray[j].m_Title +"</b> \n" +str,&pointRect);

				if (this->m_doShowDataLabel && (j==0 || this->m_graphArray[0].m_Title == "Red") && j<6 && this->m_graphArray[0].m_Title != "RGB Reference") //only label 1st graph of series unless Luminance from sat sweep
				{
					CFont font;
				if (GetConfig()->isHighDPI)
					font.CreateFont(24,0,300,300,FW_SEMIBOLD,FALSE,FALSE,FALSE,0,OUT_STRING_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
				else
					font.CreateFont(16,0,300,300,FW_SEMIBOLD,FALSE,FALSE,FALSE,0,OUT_STRING_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
					CFont* pOldFont = pDC->SelectObject(&font);
					char outStr[10];
					double y = m_graphArray[j].m_pointArray[i].y;
					if (y < 100)
						sprintf(outStr,"%0.2f",y);
					else
						sprintf(outStr,"%0.0f",y);
					if (y < 1)
						sprintf(outStr,"%0.3f",y);

					pDC->SetTextColor(RGB(220,240,0));
					pDC->TextOutA(pointPos.x,pointPos.y,outStr);
					pDC->SelectObject(pOldFont);
				}
			}

		}
		pDC->SelectObject(pOldPen);
	}
}

void CGraphControl::DrawAxis(CDC *pDC, CRect rect, BOOL bWhiteBkgnd)
{
    CPen axisPen(PS_SOLID,1,RGB(0,64,64));
    CPen *pOldPen = pDC->SelectObject(&axisPen); 

	pDC->SetTextAlign(TA_BOTTOM);
	pDC->SetBkMode(TRANSPARENT);

	int pointSize=80;
	CFont font;
	if (GetConfig()->isHighDPI)
		font.CreateFont(24,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));
	else
		font.CreateFont(16,0,0,0,FW_THIN,FALSE,FALSE,FALSE,0,OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_MODERN,_T("Garamond"));

	CFont* pOldFont = pDC->SelectObject(&font);

	// Draw X axis
	pDC->SetTextColor(bWhiteBkgnd?RGB(64,64,64):RGB(192,192,192));
	double xVal=m_minX;
	double lastStrEndX=0;
	for(int i=0;i<(m_maxX-m_minX)/m_xAxisStep;i++)
	{
		int x=GetGraphX(xVal,rect);
		// Draw axis line
		if(m_doShowAxis)
		{
			pDC->MoveTo(x,0);
			pDC->LineTo(x,rect.bottom);
		}
		// Draw Label
		if(m_doShowXLabel)
		{
			CString str;
			str.Format("%g",xVal);
			if(m_pXUnitStr)
				str+=m_pXUnitStr;
			if ( ! bWhiteBkgnd && m_doGradientBg )
				pDC->SetTextColor(RGB(255-xVal*2.55,255-xVal*2.55,255-xVal*2.55));
			if(i &&  x > lastStrEndX)
			{
				pDC->TextOut(x+2,rect.bottom,str); // Draw axis label
				lastStrEndX=x+pDC->GetTextExtent(str).cx+2;
			}
		}
		xVal+=m_xAxisStep;
	}

	// Draw Y axis
	pDC->SetTextColor(bWhiteBkgnd?RGB(64,64,64):RGB(192,192,192));
	double yVal=m_minY;
	double lastStrEndY=GetGraphY(yVal,rect);
	for(int i=0;i<(m_maxY-m_minY)/m_yAxisStep;i++)
	{
		int y=GetGraphY(yVal,rect);
		if(m_doShowAxis)
		{
			pDC->MoveTo(0,y);
			pDC->LineTo(rect.right,y);
		}
		if(m_doShowYLabel)
		{
			CString str;
			str.Format("%g",yVal);
			if(m_pYUnitStr)
				str+=m_pYUnitStr;
			if(i && y < lastStrEndY)
			{
				pDC->TextOut(2,y,str);
				lastStrEndY=y-pDC->GetTextExtent(str).cy+2;
			}
		}
		yVal+=m_yAxisStep;
	}

	pDC->SelectObject(pOldPen);
	pDC->SelectObject(pOldFont);
}

void CGraphControl::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	CHMemDC MemDC(&dc);
	CHMemDC * pDC = & MemDC;

    CRect rect;
    GetClientRect(&rect);

    CRect parentrect, windowrect;
    GetWindowRect ( & windowrect );
	GetParent () -> GetClientRect ( & parentrect );
	GetParent () -> ScreenToClient ( & windowrect );

	DrawBackground(pDC,rect,FALSE);
	
	if ( m_doSpectrumBg )
	{
		DrawSpectrumColors(pDC,rect);
	}

	if ( windowrect.bottom >= parentrect.bottom - 2 )
	{
		int			i;
		COLORREF	clr;
		
		if ( GetConfig()->m_bWhiteBkgndOnScreen )
			clr = RGB ( 192, 192, 192 );
		else if(m_doGradientBg)
		{
			if ( m_maxX*2.55 < 128 )
				i = (int)( m_maxX*2.55 ) + 64;
			else
				i = (int)( m_maxX*2.55 ) - 96;
			clr = RGB ( i, i, i );
		}
		else
			clr = RGB ( 64, 64, 64 );
		
		DrawFiligree ( pDC, rect, clr );
	}
	
	DrawAxis(pDC,rect,GetConfig()->m_bWhiteBkgndOnScreen);
	DrawGraphs(pDC,rect);
}

void CGraphControl::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
}

void CGraphControl::DrawFiligree(CDC *pDC, CRect rect, COLORREF clr)
{
	CFont	font;
	CSize	TextSize;
	LOGFONT logfont;
	static const char * szFiliText = "hcfr.sourceforge.net";

	logfont.lfHeight = -16;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 0;
	logfont.lfWeight = FW_BOLD;
	logfont.lfItalic = FALSE;
	logfont.lfUnderline = FALSE;
	logfont.lfStrikeOut = FALSE;
	logfont.lfCharSet = ANSI_CHARSET;
	logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logfont.lfQuality = PROOF_QUALITY;
	logfont.lfPitchAndFamily = VARIABLE_PITCH;
	strcpy(logfont.lfFaceName,"Arial");

	font.CreateFontIndirect (&logfont);

	CFont* pOldFont = pDC -> SelectObject ( & font );

	TextSize = pDC -> GetTextExtent ( szFiliText, strlen ( szFiliText ) );

	pDC -> SetTextColor ( clr );
	pDC -> SetBkMode ( TRANSPARENT );
	
	pDC -> TextOut ( rect.right - TextSize.cx - 10, rect.bottom - ( TextSize.cy * 3 ), szFiliText, strlen ( szFiliText ) );

	pDC -> SelectObject ( pOldFont );
}

void CGraphControl::SaveGraphs(CGraphControl *pGraphToAppend, CGraphControl *pGraphToAppend2, CGraphControl *pGraphToAppend3, bool do_Dialog, int nSequence)
{
	int				i, NbOtherGraphs = 0;
	CGraphControl *	pOtherGraphs [ 3 ];
	
	if (!do_Dialog)
	{
		switch (nSequence)
		{
		case 1:
			if (this->m_doUserScales)
				this->ReadSettings("RGB Histo");
			else
			{
				this->FitYScale(FALSE);
				this->m_minY = 80; //RGB
				this->m_maxY = 120;
				this->m_yAxisStep = 5;
				this->m_yScale = 1.0 / (40.0);
			}
			if (pGraphToAppend->m_doUserScales)
				pGraphToAppend->ReadSettings("RGB Histo2");
			else
			{
				pGraphToAppend->FitYScale(FALSE); //dE
				pGraphToAppend->m_minY = 0; 
				pGraphToAppend->m_maxY = (pGraphToAppend->m_maxY > 3 ? (pGraphToAppend->m_maxY >5?10:5):3);
				pGraphToAppend->m_yAxisStep = (pGraphToAppend->m_maxY > 3 ? (pGraphToAppend->m_maxY >5?2:1):0.5);
				pGraphToAppend->m_yScale = (pGraphToAppend->m_maxY > 3 ? (pGraphToAppend->m_maxY >5?1.0/10.0:1.0/5.0):1.0/3.0);
				pGraphToAppend->m_doShowDataLabel = TRUE;
			}
			break;
		case 2:
			if (this->m_doUserScales)
				this->ReadSettings("ColorTemp Histo");
			else
			{
				this->FitYScale(FALSE);
				this->m_minY = 5000; //CCT
				this->m_maxY = this->m_maxY > 7500?8500:7500;
				this->m_yAxisStep = 500;
				this->m_yScale = this->m_maxY > 7500?1.0/3500.0:1.0/2500.0;
				this->m_doShowDataLabel = TRUE; 
			}
			if (pGraphToAppend->m_doUserScales)
				pGraphToAppend->ReadSettings("Gamma Histo");
			else
			{
				pGraphToAppend->FitYScale(FALSE);
				pGraphToAppend->m_minY = 1.8; //Gamma
				pGraphToAppend->m_maxY = 2.6;
				pGraphToAppend->m_yAxisStep = 0.1;
				pGraphToAppend->m_yScale = 1.0/0.8;
				pGraphToAppend->m_doShowDataLabel = TRUE;
			}
			break;
		case 3:
			if (this->m_doUserScales)
				this->ReadSettings("Saturation Luminance Histo");
			if (pGraphToAppend->m_doUserScales)
				pGraphToAppend->ReadSettings("Saturation Shift Sat");
			else
			{
				pGraphToAppend->FitYScale(FALSE);
				pGraphToAppend->m_yAxisStep = 1; //Sat shift 
				pGraphToAppend->m_minY = -10.0;
				pGraphToAppend->m_maxY = 10.0;
				pGraphToAppend->m_yScale = 1.0/20.0;
			}
			if (pGraphToAppend2->m_doUserScales)
				pGraphToAppend2->ReadSettings("Saturation Shift Color");
			else
			{
				pGraphToAppend2->FitYScale(FALSE);
				pGraphToAppend2->m_minY = 0; //dE
				pGraphToAppend2->m_maxY = (pGraphToAppend2->m_maxY > 3 ? (pGraphToAppend2->m_maxY >5?10:5):3);
				pGraphToAppend2->m_yAxisStep = (pGraphToAppend2->m_maxY > 3 ? (pGraphToAppend2->m_maxY >5?2:1):0.5);
				pGraphToAppend2->m_yScale = (pGraphToAppend2->m_maxY > 3 ? (pGraphToAppend2->m_maxY >5?1.0/10.0:1.0/5.0):1.0/3.0);
			}
		case 4:
			if (this->m_doUserScales)
				this->ReadSettings("ColorTemp Histo");
			else
			{
				this->FitYScale(FALSE);
				this->m_minY = 5000; //CCT
				this->m_maxY = this->m_maxY > 7500?8500:7500;
				this->m_yAxisStep = 500;
				this->m_yScale = this->m_maxY > 7500?1.0/3500.0:1.0/2500.0;
				this->m_doShowDataLabel = TRUE; 
			}
			if (pGraphToAppend->m_doUserScales)
				pGraphToAppend->ReadSettings("Luminance Histo");
			else
			{
				pGraphToAppend->FitYScale(TRUE);
				pGraphToAppend->m_doShowDataLabel = TRUE;
			}
			break;
		}
	}

	if ( pGraphToAppend )
		pOtherGraphs [ NbOtherGraphs ++ ] = pGraphToAppend;

	if ( pGraphToAppend2 )
		pOtherGraphs [ NbOtherGraphs ++ ] = pGraphToAppend2;

	if ( pGraphToAppend3 )
		pOtherGraphs [ NbOtherGraphs ++ ] = pGraphToAppend3;

	CRect rect;
	CSize size;

	if (do_Dialog)
	{
		CSaveGraphDialog dialog;
		char *defExt;
		char *filter;

		if(dialog.DoModal()!=IDOK)
			return;

		switch(dialog.m_sizeType)
		{
			case 0:
				GetClientRect(&rect);
				size = CSize(rect.Width(),rect.Height());
				for ( i = 0; i < NbOtherGraphs ; i ++ )
				{
					CRect rect2;
					pOtherGraphs [ i ] -> GetClientRect(&rect2);
					size = CSize(size.cx,size.cy+rect2.Height());
				}
				break;
			case 1:
				size = CSize(300,200);
				break;
			case 2:
				size = CSize(600,400);
				break;
			case 3:
				size = CSize(dialog.m_saveWidth,dialog.m_saveHeight);
				break;

		}
 
		switch(dialog.m_fileType)
		{
			case 0:
				defExt="jpg";
				filter="Jpeg File (*.jpg)|*.jpg||";
				break;
			case 1:
				defExt="bmp";
				filter="Bitmap File (*.bmp)|*.bmp||";
				break;
			case 2:
				defExt="png";
				filter="Portable Network Graphic File (*.png)|*.png||";
				break;
		}

		CFileDialog fileSaveDialog ( FALSE, defExt, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter );

		if ( fileSaveDialog.DoModal () == IDOK )
		{
			// Save the file
			SaveGraphFile ( size, fileSaveDialog.GetPathName(), dialog.m_fileType, dialog.m_jpegQuality, pOtherGraphs, NbOtherGraphs );
		}
	} else
	{
		char * path;
		char filename1[255];
		path = getenv("APPDATA");
		strcpy(filename1, path);
		strcat(filename1, "\\");
		strcat(filename1, "color\\temp.png");
		size = CSize(900,600);
		SaveGraphFile ( size, filename1, 2, 95, pOtherGraphs, NbOtherGraphs, do_Dialog );
	}
}

void CGraphControl::SaveGraphFile ( CSize ImageSize, LPCSTR lpszPathName, int ImageFormat, int ImageQuality, CGraphControl * * pOtherGraphs, int NbOtherGraphs, bool do_Gradient )
{
	int				i, j;
	COLORREF		clr;
	int				format;

	switch ( ImageFormat )
	{
		case 0: format = CXIMAGE_FORMAT_JPG; break;
		case 1: format = CXIMAGE_FORMAT_BMP; break;
		case 2: format = CXIMAGE_FORMAT_PNG; break;
		default: format = CXIMAGE_FORMAT_JPG; break;
	}

    CRect rect(0,0,ImageSize.cx,ImageSize.cy);

	CDC ScreenDC;
	ScreenDC.CreateDC ( "DISPLAY", NULL, NULL, NULL );

	CDC MemDC;
    MemDC.CreateCompatibleDC ( & ScreenDC );

	CBitmap bitmap; 
    bitmap.CreateCompatibleBitmap ( & ScreenDC, rect.Width(), rect.Height() );

	ScreenDC.DeleteDC ();

    CBitmap *pOldBitmap=MemDC.SelectObject(&bitmap);

	if(NbOtherGraphs)
	{
		CRect halfRect1(CPoint(0,0),CSize(rect.Width(),(rect.Height()/(NbOtherGraphs+1))+1));
		DrawBackground(&MemDC,halfRect1, do_Gradient?FALSE:TRUE);
		DrawAxis(&MemDC,halfRect1,GetConfig()->m_bWhiteBkgndOnFile && !do_Gradient);
		DrawGraphs(&MemDC,halfRect1);

		for (i = 0; i < NbOtherGraphs ; i ++ )
		{
			CRect halfRect2(CPoint(0,rect.Height()*(i+1)/(NbOtherGraphs+1)),CSize(rect.Width(),(rect.Height()/(NbOtherGraphs+1))+1));
			CRgn clipRgn;
			clipRgn.CreateRectRgnIndirect(halfRect2);
			MemDC.SelectClipRgn(&clipRgn);
			pOtherGraphs [ i ]->DrawBackground(&MemDC,halfRect2,do_Gradient?FALSE:TRUE);

			if ( i == NbOtherGraphs - 1 )
			{
				if ( GetConfig()->m_bWhiteBkgndOnFile )
					clr = RGB ( 192, 192, 192 );
				else if ( pOtherGraphs [ i ] -> m_doGradientBg )
				{
					if ( pOtherGraphs [ i ] -> m_maxX*2.55 < 128 )
						j = (int)( pOtherGraphs [ i ] -> m_maxX*2.55 ) + 64;
					else
						j = (int)( pOtherGraphs [ i ] -> m_maxX*2.55 ) - 96;
					clr = RGB ( j, j, j );
				}
				else
					clr = RGB ( 64, 64, 64 );
				
				DrawFiligree ( &MemDC, halfRect2, clr );
			}
			pOtherGraphs [ i ]->DrawAxis(&MemDC,halfRect2,GetConfig()->m_bWhiteBkgndOnFile& !do_Gradient);
			pOtherGraphs [ i ]->DrawGraphs(&MemDC,halfRect2);
		}
	}
	else
	{
		DrawBackground(&MemDC,rect,do_Gradient?FALSE:TRUE);
		
		if ( GetConfig()->m_bWhiteBkgndOnFile && !do_Gradient )
			clr = RGB ( 192, 192, 192 );
		else if(m_doGradientBg)
		{
			if ( m_maxX*2.55 < 128 )
				i = (int)( m_maxX*2.55 ) + 64;
			else
				i = (int)( m_maxX*2.55 ) - 96;
			clr = RGB ( i, i, i );
		}
		else
			clr = RGB ( 64, 64, 64 );
		
		DrawFiligree ( &MemDC, rect, clr );

		DrawAxis(&MemDC,rect,GetConfig()->m_bWhiteBkgndOnFile && !do_Gradient);
		DrawGraphs(&MemDC,rect);
	}
	MemDC.SelectObject(pOldBitmap);

	CxImage *pImage = new CxImage();
	pImage->CreateFromHBITMAP(bitmap);

	if (pImage->IsValid())
	{
		pImage->SetJpegQuality(ImageQuality);
		pImage->Save(lpszPathName,format);
	}

	delete pImage;
}

BOOL CGraphControl::PreTranslateMessage(MSG* pMsg) 
{
	m_tooltip.RelayEvent(pMsg);
	
	return CWnd::PreTranslateMessage(pMsg);
}

int CGraphControl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_tooltip.Create(this);	
	m_tooltip.SetBehaviour(PPTOOLTIP_MULTIPLE_SHOW);
	m_tooltip.SetNotify(TRUE);
	m_tooltip.SetColorBk(RGB(255,165,0),RGB(0,128,128));
	m_tooltip.SetEffectBk(CPPDrawManager::EFFECT_HGRADIENT);
	m_tooltip.SetBorder(::CreateSolidBrush(RGB(212,175,55)),1,1);
	
	return 0;
}
