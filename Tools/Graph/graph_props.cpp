/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#include "../../stdafx.h"
#include "graph_props.h"

/////////////////////////////////////////////////////////////////////////////
//class CPointsCollection
CGraphProps::CGraphProps(COLORREF newColor, const TCHAR* newTitle, BOOL VisibleFlag, 
	BOOL _bResources, int _index/* = -1*/, BOOL b_sort_x/* = TRUE*/, BOOL b_keep_same_x/* = FALSE*/) 
	: CPointsCollection(b_sort_x, b_keep_same_x)
{
	m_grcolor = newColor;
	m_grTitle = newTitle;
	m_bIsVisible = VisibleFlag;
	m_index = _index;
	bPrivateResources = _bResources;
	if (bPrivateResources)
	{
// Fx mod
//		pen = new CPen(PS_SOLID, 1, m_grcolor);
		pen = new CPen(PS_SOLID, 3, m_grcolor);
// end of fx mod
		brush = new CBrush(m_grcolor);
	} else
	{
		pen = NULL;
		brush = NULL;
	};
}

CGraphProps::~CGraphProps()
{
	if (pen!=NULL) delete pen;
	if (brush!=NULL) delete brush;
}

CGraphProps::CGraphProps(CGraphProps* grprop, BOOL bCopyPoints /*= TRUE*/)
{
	pen = NULL;
	brush = NULL;
	if (bCopyPoints) CPointsCollection(grprop);
		else CPointsCollection();
	SetGraphProps(grprop);
}

void CGraphProps::SetGraphProps(CGraphProps* grprop)
{
	bPrivateResources = grprop->bPrivateResources;
	SetGraphColor(grprop->m_grcolor);
	m_grTitle = grprop->m_grTitle;
	m_bIsVisible = grprop->m_bIsVisible;
	m_index = grprop->m_index;
}

void CGraphProps::SetGraphColor(COLORREF newColor)
{
	m_grcolor = newColor;
	//recreate pen and brush
	if (AreResourcesPrivate())
	{
		if (pen!=NULL) delete pen;
		if (brush!=NULL) delete brush;
		pen = new CPen(PS_SOLID, 1, m_grcolor);
		brush = new CBrush(m_grcolor);
	};
}

//CAxisProps**************
CAxisProps::CAxisProps()
{
	formatStrings[0] = formatStrings[1] = formatStrings[2] = NULL;
	m_Title = ""; 
	m_UOM = ""; 
	m_Precision = 3;
	CalculateFormatStrings();
}

CAxisProps::~CAxisProps()
{
	for (int i=0; i<3; i++) 
	{
		if (formatStrings[i]!=NULL) delete formatStrings[i];
		formatStrings[i] = NULL;
	};
}

void CAxisProps::GetFormattedOutput(double value, int formatLevel, CString* str)
{
	if (formatLevel<0 || formatLevel>2) 
	{
		*str = "";
	} else
	{
		str->Format(formatStrings[formatLevel], value);
	};
}

void CAxisProps::GetFullTitle(CString* str)
{
	*str = m_Title + ", " + m_UOM;
}

void CAxisProps::SetNewTitle(const TCHAR* title)
{
	m_Title = title;
	CalculateFormatStrings();
}

void CAxisProps::SetNewUOM(const TCHAR* newUOM)
{
	m_UOM = newUOM;
	CalculateFormatStrings();
}

void CAxisProps::SetNewPrecision(int newPrecision)
{
	m_Precision = newPrecision;
	CalculateFormatStrings();
}

void CAxisProps::CalculateFormatStrings()
{
	for (int i=0; i<3; i++) 
	{
		if (formatStrings[i]!=NULL) delete formatStrings[i];
		formatStrings[i] = NULL;
	};
	CString str;

	str.Format(_T("%%0.%df"), m_Precision);
	formatStrings[0] = new TCHAR[str.GetLength()+1];
	_tcscpy(formatStrings[0], (LPCTSTR)str);

	str.Format(_T("%%0.%df %s"), m_Precision, m_UOM);
	formatStrings[1] = new TCHAR[str.GetLength()+1];
	_tcscpy(formatStrings[1], (LPCTSTR)str);

	str.Format(_T("%s: %%0.%df %s"), m_Title, m_Precision, m_UOM);
	formatStrings[2] = new TCHAR[str.GetLength()+1];
	_tcscpy(formatStrings[2], (LPCTSTR)str);
}

