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

// DocEnumerator.h: interface for the CDocEnumerator class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DOCENUMERATOR_H__FF909DB2_F211_4B7E_86E0_46E4DC5FACCE__INCLUDED_)
#define AFX_DOCENUMERATOR_H__FF909DB2_F211_4B7E_86E0_46E4DC5FACCE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// CDocEnumerator: Enumerates all the documents in an MFC application.
// To use:
//
//    CDocEnumerator it;
//    CDocument* pdoc;
//    while ((pdoc=it.Next())!=NULL) {
//       // do something
//    }
//
class CDocEnumerator 
{
private:
   CPtrArray m_doclist; // array of all docs
   int m_iPos;          // current position in array
public:
   CDocEnumerator();
   CDocument* Next(); 
   void Reset() { m_iPos=-1; }
};


#endif // !defined(AFX_DOCENUMERATOR_H__FF909DB2_F211_4B7E_86E0_46E4DC5FACCE__INCLUDED_)
