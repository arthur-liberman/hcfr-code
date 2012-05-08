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

// DocEnumerator.cpp: implementation of the CDocEnumerator class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "DocEnumerator.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// CDocEnumerator Enumerates all the documents in an application.

//////////////////
// ctor loads all doc pointers into private array.
//
CDocEnumerator::CDocEnumerator() 
{
   CWinApp* pApp = AfxGetApp();
   POSITION pos = pApp->GetFirstDocTemplatePosition();
   while (pos!=NULL) 
   {
      CDocTemplate* pDocTemplate = pApp->GetNextDocTemplate(pos);
      POSITION pos2 = pDocTemplate->GetFirstDocPosition();
      while (pos2!=NULL) 
         m_doclist.Add(pDocTemplate->GetNextDoc(pos2));
   }
   Reset();
}

//////////////////
// Fetch next document.
//
CDocument* CDocEnumerator::Next() 
{
   if (++m_iPos < m_doclist.GetSize()) 
      return (CDocument*)m_doclist[m_iPos];

   return NULL;
}

