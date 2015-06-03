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

// Export.h: interface for the CExport class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_EXPORT_H__0CE11478_BEDB_4D05_AD97_0088BBD1AFA0__INCLUDED_)
#define AFX_EXPORT_H__0CE11478_BEDB_4D05_AD97_0088BBD1AFA0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DataSetDoc.h"

extern bool IsExcelDriverInsalled();

class CExport  
{
public:
	enum ExportType 
	{
		XLS=0,
		CSV=1,
		ICC=2,
		PDF=3
	};

private:
	CDataSetDoc *m_pDoc;
	ExportType m_type;
	bool m_doReplace;
	bool m_doBackup;
	CString m_fileName;
	CString m_separator;
	CString m_errorStr;
	int m_numToReplace;

	bool SaveSheets();
	bool SaveGeneralSheet();
	bool SaveGrayScaleSheet();
	bool SavePrimariesSheet();
	bool SavePDF();
	bool SaveCCSheet();

public:
	CExport(CDataSetDoc *pDoc, ExportType type);
	virtual ~CExport();

	bool Save();
};

#endif // !defined(AFX_EXPORT_H__0CE11478_BEDB_4D05_AD97_0088BBD1AFA0__INCLUDED_)
