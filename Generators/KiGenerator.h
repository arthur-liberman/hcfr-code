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
//	Georges GALLERAND
//  Patrice AFFLATET
//  Benoit SEGUIN
/////////////////////////////////////////////////////////////////////////////

// KIGenerator.h: interface for the CKIGenerator class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_KIGENERATOR_H__2E70EC5C_71E2_41A9_8CC4_33F7F175439E__INCLUDED_)
#define AFX_KIGENERATOR_H__2E70EC5C_71E2_41A9_8CC4_33F7F175439E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Generator.h"
#include "KiGenePropPage.h"


class CKiGenerator : public CGenerator  
{
public:
	DECLARE_SERIAL(CKiGenerator) ;

	CString	m_comPort;

protected:
	BOOL m_showDialog;
	CString	m_RealComPort;
	CKiGenePropPage m_KiGenePropertiesPage;


	CString m_IRProfileName;
	BOOL	m_bUseIRCodes;
	BYTE	m_NextCode [ 512 ];
	int		m_NextCodeLength;
	BYTE	m_OkCode [ 512 ];
	int		m_OkCodeLength;
	BYTE	m_DownCode [ 512 ];
	int		m_DownCodeLength;
	BYTE	m_RightCode [ 512 ];
	int		m_RightCodeLength;

	int		m_NavInMenuLatency;
	int		m_ValidMenuLatency;
	int		m_NextChapterLatency;

	BOOL	m_bBeforeFirstDisplay;

	UINT	m_patternCheckMaxRetryByPattern;
	UINT	m_patternCheckMaxRetryBySeries;
	UINT	m_patternCheckMaxThreshold;
	BOOL	m_patternCheckOnGrayscale;
	BOOL	m_patternCheckOnSaturations;
	BOOL	m_patternCheckOnPrimaries;
	BOOL	m_patternCheckOnSecondaries;
	UINT	m_patternRetry;
	UINT	m_totalPatternRetry;
	double	m_minDif;
	CString m_retryList;
	COLORREF	m_lastColor;
	UINT	m_lastPatternInfo;

	int		m_NbNextCodeToAdd;

public:
	CKiGenerator();
	virtual ~CKiGenerator();

	virtual	void Copy(CGenerator * p);

	virtual void Serialize(CArchive& archive); 

	virtual BOOL Init(UINT nbMeasure = 0);
	virtual BOOL InitRealComPort();
	virtual BOOL Release(INT nbNext = -1);
	virtual BOOL ChangePatternSeries();
	virtual BOOL DisplayGray(double aLevel, BOOL bIRE, MeasureType nPatternType, BOOL bChangePattern = TRUE);
	virtual BOOL DisplayRGBColor(COLORREF aRGBColor , MeasureType nPatternType, UINT nPatternInfo = 0, BOOL bChangePattern = TRUE,BOOL bSilentMode = FALSE);
	virtual BOOL DisplayAnsiBWRects(BOOL bInvert);
	virtual BOOL CanDisplayAnsiBWRects(); 
	virtual BOOL CanDisplayGrayAndColorsSeries();
	virtual BOOL CanDisplayScale ( MeasureType nScaleType, int nbLevels, BOOL bMute = FALSE );
	virtual	BOOL HasPatternChanged( MeasureType nScaleType,CColor previousColor,CColor measuredColor);
	virtual BOOL setMire(int timeout, char command, char *sensVal);
	virtual BOOL SendIRCode(int timeout, LPBYTE Code, int CodeLength, char *sensVal);
	virtual CString	AcquireIRCode ();
	virtual CString GetRetryMessage ();

	virtual void SetPropertiesSheetValues();
	virtual void GetPropertiesSheetValues();

	virtual CString GetIRProfileName() {return m_IRProfileName;}
};

#endif // !defined(AFX_KIGENERATOR_H__2E70EC5C_71E2_41A9_8CC4_33F7F175439E__INCLUDED_)
