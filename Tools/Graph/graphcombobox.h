/*
This file was created by Paul Barvinko (pbarvinko@yahoo.com).
This file is distributed "as is", e.g. there are no warranties 
and obligations and you could use it in your applications on your
own risk. 
Your comments and questions are welcome.
If using in your applications, please mention author in credits for your app.
*/
#if !defined(AFX_GRAPHCOMBOBOX_H__F0A75FD4_DEE1_11D3_B4B4_00C04F89477F__INCLUDED_)
#define AFX_GRAPHCOMBOBOX_H__F0A75FD4_DEE1_11D3_B4B4_00C04F89477F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// graphcombobox.h : header file
//

#include "graph_general.h"

/////////////////////////////////////////////////////////////////////////////
// CGraphComboBox window

class CGraphComboBox : public CComboBox, public virtual CGraphBaseClass
{
// Construction
public:
	CGraphComboBox();

// Attributes
public:

    BOOL Create( DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID );
    void DrawComboItem(COLORREF color, const TCHAR* title, LPDRAWITEMSTRUCT lpDrawItemStruct);

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGraphComboBox)
	public:
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CGraphComboBox();

	// Generated message map functions
protected:
	//{{AFX_MSG(CGraphComboBox)
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GRAPHCOMBOBOX_H__F0A75FD4_DEE1_11D3_B4B4_00C04F89477F__INCLUDED_)
