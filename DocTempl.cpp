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

// DocTempl.cpp : document template class allowing window positions serialization
//

#include "stdafx.h"
#include "ColorHCFR.h"

#include "DataSetDoc.h"

#include "Measure.h"
#include "MainView.h"
#include "MainFrm.h"
#include "MultiFrm.h"

#include "DocTempl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


extern CPtrList gOpenedFramesList;	// Implemented in MultiFrm.cpp


/////////////////////////////////////////////////////////////////////////////
// View with option saving capabilities

IMPLEMENT_DYNAMIC(CSavingView,CView)


/////////////////////////////////////////////////////////////////////////////
// Document template (for window positions serialization)

IMPLEMENT_DYNAMIC(CMyMultiDocTemplate,CNewMultiDocTemplate)

CDocument* CMyMultiDocTemplate::OpenDocumentFile(LPCTSTR lpszPathName,
	BOOL bMakeVisible)
{
	BOOL			bAutoDelete;
	CFrameWnd *		pFrame;
	CFrameWnd *		pActiveFrame = NULL;
	CDataSetDoc *	pDocument = (CDataSetDoc *) CreateNewDocument();
	
	if (pDocument == NULL)
	{
		TRACE0("CDocTemplate::CreateNewDocument returned NULL.\n");
		AfxMessageBox(AFX_IDP_FAILED_TO_CREATE_DOC);
		return NULL;
	}
	ASSERT_VALID(pDocument);

	if (lpszPathName == NULL)
	{
		bAutoDelete = pDocument->m_bAutoDelete;
		pDocument->m_bAutoDelete = FALSE;   // don't destroy if something goes wrong
		pFrame = CreateNewFrame(pDocument, NULL);
		pDocument->m_bAutoDelete = bAutoDelete;
		
		if (pFrame == NULL)
		{
			AfxMessageBox(AFX_IDP_FAILED_TO_CREATE_DOC);
			delete pDocument;       // explicit delete on error
			return NULL;
		}
		ASSERT_VALID(pFrame);

		// create a new document - with default document name
		SetDefaultTitle(pDocument);

		// avoid creating temporary compound file when starting up invisible
		if (!bMakeVisible)
			pDocument->m_bEmbedded = TRUE;

		if (!pDocument->OnNewDocument())
		{
			// user has be alerted to what failed in OnNewDocument
			TRACE0("CDocument::OnNewDocument returned FALSE.\n");
			pFrame->DestroyWindow();
			return NULL;
		}

		// it worked, now bump untitled count
		m_nUntitledCount++;

		InitialUpdateFrame(pFrame, pDocument, bMakeVisible);
	}
	else
	{
		// open an existing document
		CWaitCursor wait;
		if (!pDocument->OnOpenDocument(lpszPathName))
		{
			// user has be alerted to what failed in OnOpenDocument
			TRACE0("CDocument::OnOpenDocument returned FALSE.\n");
			return NULL;
		}
		
		pDocument->SetPathName(lpszPathName);

		if ( pDocument -> m_pWndPos )
		{
			// Create as many frames as necessary, and transmit them position/views information
			int	nCount = 0;
			POSITION	pos = pDocument -> m_pWndPos -> m_FrameInfoList.GetHeadPosition ();
			while ( pos )
			{
				// This internal data contains position info
				pDocument -> m_pFramePosInfo = ( CDataSetFrameInfo * ) pDocument -> m_pWndPos -> m_FrameInfoList.GetNext ( pos );

				// Create the frame, it will use above data
				bAutoDelete = pDocument->m_bAutoDelete;
				pDocument->m_bAutoDelete = FALSE;   // don't destroy if something goes wrong
				pFrame = CreateNewFrame(pDocument, NULL);
				pDocument->m_bAutoDelete = bAutoDelete;
				
				if (pFrame)
				{
					ASSERT_VALID(pFrame);
					InitialUpdateFrame(pFrame, pDocument, bMakeVisible);
					if ( pDocument -> m_pWndPos -> m_nActiveFrame == nCount )
						pActiveFrame = pFrame;
					nCount ++;
					
					// Minimized or maximized windows will be redrawn later (WM_SYSCOMMAND message posted)
					// This redraw avoid little trouble during multiple windows resizing during initialization
					if ( pDocument -> m_pFramePosInfo -> m_FramePlacement.showCmd != SW_SHOWMINIMIZED && pDocument -> m_pFramePosInfo -> m_FramePlacement.showCmd != SW_SHOWMAXIMIZED )
						pFrame -> RedrawWindow ();

				}
			}

			// Those data are not needed anymore
			pDocument -> m_pFramePosInfo = NULL;
			delete pDocument -> m_pWndPos;
			pDocument -> m_pWndPos = NULL;

			// At least one frame must exist
			if ( nCount == 0 )
			{
				AfxMessageBox(AFX_IDP_FAILED_TO_CREATE_DOC);
				delete pDocument;       // explicit delete on error
				return NULL;
			}

			// Activate the selected frame
			if ( pActiveFrame )
				( (CMainFrame *) AfxGetMainWnd () ) -> MDIActivate ( pActiveFrame );
		}
		else
		{
			// Standard creation, without window positionning
			bAutoDelete = pDocument->m_bAutoDelete;
			pDocument->m_bAutoDelete = FALSE;   // don't destroy if something goes wrong
			pFrame = CreateNewFrame(pDocument, NULL);
			pDocument->m_bAutoDelete = bAutoDelete;
			
			if (pFrame == NULL)
			{
				AfxMessageBox(AFX_IDP_FAILED_TO_CREATE_DOC);
				delete pDocument;       // explicit delete on error
				return NULL;
			}
			ASSERT_VALID(pFrame);
			InitialUpdateFrame(pFrame, pDocument, bMakeVisible);
		}

	}

	return pDocument;
}
/////////////////////////////////////////////////////////////////////////////
// Classes for window positions serialization

IMPLEMENT_SERIAL(CDataSetViewInfo,CObject,1)
IMPLEMENT_SERIAL(CDataSetFrameInfo,CObject,1)
IMPLEMENT_SERIAL(CDataSetWindowPositions,CObject,1)

CDataSetViewInfo::CDataSetViewInfo ()
{
	m_nViewIndex = 0;
	m_dwUserInfo = 0;
}
CDataSetViewInfo::CDataSetViewInfo ( int nViewIndex, DWORD dwUserInfo )
{
	m_nViewIndex = nViewIndex;
	m_dwUserInfo = dwUserInfo;
}
CDataSetViewInfo::~CDataSetViewInfo ()
{
}

void CDataSetViewInfo::Serialize(CArchive& ar)
{
	if ( ar.IsStoring () )
	{
		int	version = 1;
		ar << version;
		ar << m_nViewIndex;
		ar << m_dwUserInfo;
	}
	else
	{
		int	version;
		ar >> version;
		if ( version > 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		ar >> m_nViewIndex;
		ar >> m_dwUserInfo;
	}
}

CDataSetFrameInfo::CDataSetFrameInfo ()
{
	memset ( & m_FramePlacement, 0, sizeof ( m_FramePlacement ) );
	m_FramePlacement.length = sizeof ( m_FramePlacement );
	m_nActiveView = 0;
}

CDataSetFrameInfo::CDataSetFrameInfo ( CFrameWnd * pFrame )
{
	int				i;
	DWORD			nViewIndex;
	DWORD			dwUserInfo;
	CMultiFrame *	pMultiFrame = ( CMultiFrame * ) pFrame;

	m_FramePlacement.length = sizeof ( m_FramePlacement );
	pMultiFrame -> GetWindowPlacement ( & m_FramePlacement );

	for ( i = 0; i < pMultiFrame -> m_NbTabbedViews ; i ++ )
	{
		pMultiFrame -> m_TabCtrl.GetItemData ( i, nViewIndex );

		if ( pMultiFrame -> m_nTabbedViewIndex [ nViewIndex ] == IDS_DATASETVIEW_NAME )
		{
			ASSERT (pMultiFrame -> m_pTabbedView [ nViewIndex ]->IsKindOf(RUNTIME_CLASS(CMainView)));
			dwUserInfo = ( ( CMainView * ) pMultiFrame -> m_pTabbedView [ nViewIndex ] ) -> GetUserInfo ();
		}
		else
		{
			ASSERT ( pMultiFrame -> m_pTabbedView [ nViewIndex ] -> IsKindOf ( RUNTIME_CLASS(CSavingView) ) );
			dwUserInfo = ( ( CSavingView * ) pMultiFrame -> m_pTabbedView [ nViewIndex ] ) -> GetUserInfo ();
		}
		m_ViewInfoList.AddTail ( new CDataSetViewInfo ( pMultiFrame -> m_nTabbedViewIndex [ nViewIndex ], dwUserInfo ) );
	}
	m_nActiveView = pMultiFrame -> m_TabCtrl.GetCurSel ();
}

CDataSetFrameInfo::~CDataSetFrameInfo ()
{
	POSITION			pos;
	CDataSetViewInfo *	pViewInfo;

	pos = m_ViewInfoList.GetHeadPosition ();
	while ( pos )
	{
		pViewInfo = ( CDataSetViewInfo * ) m_ViewInfoList.GetNext ( pos );
		delete pViewInfo;
	}
	m_ViewInfoList.RemoveAll ();
}

void CDataSetFrameInfo::Serialize(CArchive& ar)
{
	POSITION			pos;
	CDataSetViewInfo *	pViewInfo;

	if ( ar.IsStoring () )
	{
		int	version = 1;
		ar << version;
		ar << m_FramePlacement.showCmd;
		ar << m_FramePlacement.ptMinPosition;
		ar << m_FramePlacement.ptMaxPosition;
		ar << m_FramePlacement.rcNormalPosition;
		ar << m_nActiveView;
		ar << m_ViewInfoList.GetCount ();

		pos = m_ViewInfoList.GetHeadPosition ();
		while ( pos )
		{
			pViewInfo = ( CDataSetViewInfo * ) m_ViewInfoList.GetNext ( pos );
			pViewInfo -> Serialize ( ar );
		}
	}
	else
	{
		int	i, nCount;
		int	version;

		ar >> version;
		if ( version > 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );

		// Clean actual data
		pos = m_ViewInfoList.GetHeadPosition ();
		while ( pos )
		{
			pViewInfo = ( CDataSetViewInfo * ) m_ViewInfoList.GetNext ( pos );
			delete pViewInfo;
		}
		m_ViewInfoList.RemoveAll ();

		ar >> m_FramePlacement.showCmd;
		ar >> m_FramePlacement.ptMinPosition;
		ar >> m_FramePlacement.ptMaxPosition;
		ar >> m_FramePlacement.rcNormalPosition;
		ar >> m_nActiveView;
		ar >> nCount;
		for ( i = 0; i < nCount ; i ++ )
		{
			pViewInfo = new CDataSetViewInfo;
			pViewInfo -> Serialize ( ar );
			m_ViewInfoList.AddTail ( pViewInfo );
		}
	}
}

CDataSetWindowPositions::CDataSetWindowPositions ()
{
	m_nActiveFrame = -1;
}

CDataSetWindowPositions::CDataSetWindowPositions ( CDataSetDoc * pDoc )
{
	int					i;
	POSITION			pos;
	CMultiFrame *		pFrame;
	CMultiFrame *		pActiveFrame = (CMultiFrame *) ( (CMainFrame *) AfxGetMainWnd () ) -> MDIGetActive();
	CDataSetFrameInfo * pFrameInfo;
	
	i = 0;
	m_nActiveFrame = -1;
	pos = gOpenedFramesList.GetHeadPosition ();
	while ( pos )
	{
		pFrame = ( CMultiFrame * ) gOpenedFramesList.GetNext ( pos );

		if ( pFrame -> GetDocument () == pDoc )
		{
			pFrameInfo = new CDataSetFrameInfo ( pFrame );
			m_FrameInfoList.AddTail ( pFrameInfo );

			if ( pActiveFrame == pFrame )
				m_nActiveFrame = i;

			i ++;
		}
	}
}

CDataSetWindowPositions::~CDataSetWindowPositions ()
{
	POSITION			pos;
	CDataSetFrameInfo * pFrameInfo;

	pos = m_FrameInfoList.GetHeadPosition ();
	while ( pos )
	{
		pFrameInfo = ( CDataSetFrameInfo * ) m_FrameInfoList.GetNext ( pos );
		delete pFrameInfo;
	}
	m_FrameInfoList.RemoveAll ();
}

void CDataSetWindowPositions::Serialize(CArchive& ar)
{
	POSITION			pos;
	CDataSetFrameInfo * pFrameInfo;

	if ( ar.IsStoring () )
	{
		int	version = 1;
		ar << version;
		ar << m_nActiveFrame;
		ar << m_FrameInfoList.GetCount ();

		pos = m_FrameInfoList.GetHeadPosition ();
		while ( pos )
		{
			pFrameInfo = ( CDataSetFrameInfo * ) m_FrameInfoList.GetNext ( pos );
			pFrameInfo -> Serialize ( ar );
		}
	}
	else
	{
		int	i, nCount;
		int	version;

		ar >> version;
		if ( version > 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );

		// Clean actual data
		pos = m_FrameInfoList.GetHeadPosition ();
		while ( pos )
		{
			pFrameInfo = ( CDataSetFrameInfo * ) m_FrameInfoList.GetNext ( pos );
			delete pFrameInfo;
		}
		m_FrameInfoList.RemoveAll ();

		ar >> m_nActiveFrame;
		ar >> nCount;
		for ( i = 0; i < nCount ; i ++ )
		{
			pFrameInfo = new CDataSetFrameInfo;
			pFrameInfo -> Serialize ( ar );
			m_FrameInfoList.AddTail ( pFrameInfo );
		}
	}
}



