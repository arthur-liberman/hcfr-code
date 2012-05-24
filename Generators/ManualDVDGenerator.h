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

// ManualDVDGenerator.h: interface for the CManualDVDGenerator class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MANUALDVDGENERATOR_H__2E70EC5C_71E2_41A9_8CC4_33F7F175439E__INCLUDED_)
#define AFX_MANUALDVDGENERATOR_H__2E70EC5C_71E2_41A9_8CC4_33F7F175439E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Generator.h"

class CManualDVDGenerator : public CGenerator  
{
public:
	DECLARE_SERIAL(CManualDVDGenerator) ;

public:
	CManualDVDGenerator();
	virtual ~CManualDVDGenerator();

	virtual	void Copy(CGenerator * p);
	virtual BOOL DisplayGray(double aLevel, MeasureType nPatternType, BOOL bChangePattern = TRUE);
	virtual BOOL DisplayRGBColor(const ColorRGBDisplay& aRGBColor , MeasureType nPatternType, UINT nPatternInfo = 0,  BOOL bChangePattern = TRUE,BOOL bSilentMode = FALSE);

	virtual BOOL DisplayAnsiBWRects(BOOL bInvert);
	virtual BOOL CanDisplayAnsiBWRects(); 
};

#endif // !defined(AFX_MANUALDVDGENERATOR_H__2E70EC5C_71E2_41A9_8CC4_33F7F175439E__INCLUDED_)
