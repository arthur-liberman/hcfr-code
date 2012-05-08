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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// DocTempl.h : document template class allowing window positions serialization
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_DocTempl_H__FDDEAC5A_8B9B_4AFD_BFDE_F48C28BF9899__INCLUDED_)
#define AFX_DocTempl_H__FDDEAC5A_8B9B_4AFD_BFDE_F48C28BF9899__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


/////////////////////////////////////////////////////////////////////////////
// Document template (for window positions serialization)

class CMyMultiDocTemplate : public CNewMultiDocTemplate
{
	DECLARE_DYNAMIC(CMyMultiDocTemplate)

// Constructors
public:
	CMyMultiDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
		: CNewMultiDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) {}

	virtual CDocument* OpenDocumentFile ( LPCTSTR lpszPathName, BOOL bMakeVisible = TRUE );
};


/////////////////////////////////////////////////////////////////////////////
// View with option saving capabilities

class CSavingView : public CView
{
	DECLARE_DYNAMIC(CSavingView)

// Constructor
protected:
	CSavingView()			{}
	virtual ~CSavingView()	{}

// Operations
public:
	virtual DWORD	GetUserInfo () = 0;
	virtual void	SetUserInfo ( DWORD dwUserInfo ) = 0;

};

/////////////////////////////////////////////////////////////////////////////
// Classes for window positions serialization

// Store view type and initialization data (main view only)
class CDataSetViewInfo : public CObject
{
 public:
	CDataSetViewInfo ();
	CDataSetViewInfo ( int nViewIndex, DWORD dwUserInfo );
	~CDataSetViewInfo ();

	DECLARE_SERIAL(CDataSetViewInfo)

	int			m_nViewIndex;
	DWORD		m_dwUserInfo;

	virtual void Serialize(CArchive& ar);
};

// Store frame position, list of included views, and current view index
class CDataSetFrameInfo : public CObject
{
 public:
	CDataSetFrameInfo ();
	CDataSetFrameInfo ( CFrameWnd * pFrame );
	~CDataSetFrameInfo ();

	DECLARE_SERIAL(CDataSetFrameInfo)

	WINDOWPLACEMENT	m_FramePlacement;
	int				m_nActiveView;
	CObList			m_ViewInfoList;	// List of view windows

	virtual void Serialize(CArchive& ar);
};

// Document windows positions, a list of frames with active one.
class CDataSetWindowPositions : public CObject
{
 public:
	CDataSetWindowPositions ();
	CDataSetWindowPositions ( CDataSetDoc * pDoc );
	~CDataSetWindowPositions ();

	DECLARE_SERIAL(CDataSetWindowPositions)

	int			m_nActiveFrame;
	CObList		m_FrameInfoList;	// List of frame windows

	virtual void Serialize(CArchive& ar);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DocTempl_H__FDDEAC5A_8B9B_4AFD_BFDE_F48C28BF9899__INCLUDED_)
