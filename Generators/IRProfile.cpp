/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2008 Association Homecinema Francophone.  All rights reserved.
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
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////
// IRProfile.cpp: implementation of the CIRProfile class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "colorhcfr.h"
#include "IRProfile.h"
#include <io.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIRProfile::CIRProfile()
{
	m_ReadOnly = FALSE;
	
	m_NextCode = "";
	m_OkCode = "";
	m_DownCode = "";
	m_RightCode = "";
	m_NavInMenuLatency = 0;
	m_ValidMenuLatency = 0;
	m_NextChapterLatency = 0;
}

CIRProfile::~CIRProfile()
{

}

void CIRProfile::Serialize(CArchive& archive)
{
	if (archive.IsStoring())
	{
		int version=1;
		archive << version;
		archive << m_ReadOnly;
		archive << m_NextCode;
		archive << m_OkCode;
		archive << m_DownCode;
		archive << m_RightCode;

		archive << m_NavInMenuLatency;
		archive << m_ValidMenuLatency;
		archive << m_NextChapterLatency;
	}
	else
	{
		int version;
		archive >> version;
		archive >> m_ReadOnly;
		archive >> m_NextCode;
		archive >> m_OkCode;
		archive >> m_DownCode;
		archive >> m_RightCode;
		archive >> m_NavInMenuLatency;
		archive >> m_ValidMenuLatency;
		archive >> m_NextChapterLatency;
	}
}
IMPLEMENT_SERIAL(CIRProfile, CObject, 1) ;

int CIRProfile::GetBinaryIRCode (CodeType nType, LPBYTE BinCode )
{
	int nCodeLength;
	switch ( nType )
			{
				case CT_NEXT:
					BuildBinaryIRCode(BinCode, & nCodeLength, m_NextCode);
					break;
				case CT_OK:
					BuildBinaryIRCode(BinCode, & nCodeLength,m_OkCode);
					break;
				case CT_DOWN:
					BuildBinaryIRCode(BinCode, & nCodeLength,m_DownCode);
					break;
				case CT_RIGHT:
					BuildBinaryIRCode(BinCode, & nCodeLength,m_RightCode);
					break;
			}
	return nCodeLength;
}

void CIRProfile::Clear()
{
	m_NextCode = "";
	m_OkCode = "";
	m_DownCode = "";
	m_RightCode = "";
	m_NavInMenuLatency = 300;
	m_ValidMenuLatency = 1500;
	m_NextChapterLatency = 2000;
}


void CIRProfile::BuildBinaryIRCode ( LPBYTE BinCode, int * pBinCodeLength, CString & str )
{
	* pBinCodeLength = 0;
	if ( ! str.IsEmpty () )
	{
		int		i, j;
		char	c;
		WORD	w = 0;
		WORD	wHead = 0;
		WORD	wLen1 = 0;
		WORD	wLen2 = 0;

		for ( i = 0, j = 0; i < str.GetLength (); i ++ )
		{
			c = str [i];
			
			if ( c == ' ' || c == '\t' )
			{
				// Separator
			}
			else if ( isxdigit ( c ) )
			{
				// Hexadecimal digit
				w <<= 4;
				
				if ( isdigit (c) )
					w += (WORD) c - (WORD) '0';
				else
					w += (WORD) toupper(c) - (WORD) 'A' + 10;

				j ++;

				if ( j % 4 == 0 )
				{
					BinCode [ (*pBinCodeLength) ++ ] = LOBYTE (w);
					BinCode [ (*pBinCodeLength) ++ ] = HIBYTE (w);

					if ( j == 4 )
						wHead = w;
					else if ( j == 12 )
						wLen1 = w;
					else if ( j == 16 )
						wLen2 = w;

					w = 0;
				}
			}
			else
			{
				// Not a recognized digit
				j = 1;
				break;
			}
		}

		// Check validity
		if ( j % 4 != 0 || ( wHead != 0x0000 && wHead != 0x5000 && wHead != 0x6000 ) ) 
		{
			// Wrong number of digits or bad header
			* pBinCodeLength = 0;
		}
		else if ( wHead == 0x5000 || wHead == 0x6000 )
		{
			if ( j != 24 )
			{
				// Wrong number of codes
				* pBinCodeLength = 0;
			}
		}
		else
		{
			ASSERT ( wHead == 0x0000 );

			if ( wLen1 > 255 || wLen2 > 255 || ( wLen2 + wLen1 ) > 200 )
			{
				// Too long code
				* pBinCodeLength = 0;
			}
			else
			{
				if ( j != ( ( wLen2 + wLen1 ) * 8 ) + 16 )
				{
					// Not the right number of words
					* pBinCodeLength = 0;
				}
			}
		}
	}
}

BOOL CIRProfile::Load(CString nIRProfileName)
{
	if((_access( nIRProfileName, 0 )) != -1 )
	{
		CFile IhcFile ( nIRProfileName, CFile::modeRead | CFile::shareDenyNone );
		CArchive ar ( & IhcFile, CArchive::load );
		Serialize(ar);
		return TRUE;
	}
	else
	{
		Clear();
		return FALSE;
	}
}