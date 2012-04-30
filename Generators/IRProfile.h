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
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////
// IRProfile.h: interface for the CIRProfile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IRPROFILE_H__B55DAFD3_E108_43EF_8BEE_2F5614B9E637__INCLUDED_)
#define AFX_IRPROFILE_H__B55DAFD3_E108_43EF_8BEE_2F5614B9E637__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CIRProfile : public CObject  
{
public:
	DECLARE_SERIAL(CIRProfile) ;
	enum CodeType
	{
		CT_NEXT = 0,
		CT_OK = 1,
		CT_DOWN = 2,
		CT_RIGHT,
	};
		
	CIRProfile();
	~CIRProfile();
	void Serialize(CArchive& archive);
	int GetBinaryIRCode (CodeType nType, LPBYTE BinCode );
	void BuildBinaryIRCode ( LPBYTE BinCode, int * pBinCodeLength, CString & str );
	static LPCSTR GetIhcFileSubDir() { return "Profils_IR";}
	void Clear();
	BOOL Load(CString m_IRProfileName);

	BOOL	m_ReadOnly;
	CString	m_NextCode;
	CString	m_OkCode;
	CString	m_DownCode;
	CString	m_RightCode;
	int		m_NavInMenuLatency;
	int		m_ValidMenuLatency;
	int		m_NextChapterLatency;

/*
	BYTE	m_NextCode [ 512 ];
	int		m_NextCodeLength;
	BYTE	m_OkCode [ 512 ];
	int		m_OkCodeLength;
	BYTE	m_DownCode [ 512 ];
	int		m_DownCodeLength;
	BYTE	m_RightCode [ 512 ];
	int		m_RightCodeLength;


*/
};

#endif // !defined(AFX_IRPROFILE_H__B55DAFD3_E108_43EF_8BEE_2F5614B9E637__INCLUDED_)
