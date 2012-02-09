//
//	Class:		CShadeButtonST
//
//	Compiler:	Visual C++
//				eMbedded Visual C++
//	Tested on:	Visual C++ 6.0
//				Windows CE 3.0
//
//	Created:	14/June/2001
//	Updated:	25/November/2002
//
//	Author:		Davide Calabro'		davide_calabro@yahoo.com
//									http://www.softechsoftware.it
//
//	Disclaimer
//	----------
//	THIS SOFTWARE AND THE ACCOMPANYING FILES ARE DISTRIBUTED "AS IS" AND WITHOUT
//	ANY WARRANTIES WHETHER EXPRESSED OR IMPLIED. NO REPONSIBILITIES FOR POSSIBLE
//	DAMAGES OR EVEN FUNCTIONALITY CAN BE TAKEN. THE USER MUST ASSUME THE ENTIRE
//	RISK OF USING THIS SOFTWARE.
//
//	Terms of use
//	------------
//	THIS SOFTWARE IS FREE FOR PERSONAL USE OR FREEWARE APPLICATIONS.
//	IF YOU USE THIS SOFTWARE IN COMMERCIAL OR SHAREWARE APPLICATIONS YOU
//	ARE GENTLY ASKED TO DONATE 5$ (FIVE U.S. DOLLARS) TO THE AUTHOR:
//
//		Davide Calabro'
//		P.O. Box 65
//		21019 Somma Lombardo (VA)
//		Italy
//
#ifndef _SHADEBUTTONST_H_
#define _SHADEBUTTONST_H_

#ifdef _WIN32_WCE
#define	BASE_BTNST_CLASS	CCeButtonST
#define	INC_BTNST			"CeBtnST.h"
#else
#define	BASE_BTNST_CLASS	CButtonST
#define	INC_BTNST			"BtnST.h"
#endif

#include INC_BTNST
#include "CeXDib.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CShadeButtonST : public BASE_BTNST_CLASS
{
public:
	CShadeButtonST();
	virtual ~CShadeButtonST();

	enum	{	SHS_NOISE = 0,
				SHS_DIAGSHADE,
				SHS_HSHADE,
				SHS_VSHADE,
				SHS_HBUMP,
				SHS_VBUMP,
				SHS_SOFTBUMP,
				SHS_HARDBUMP,
				SHS_METAL	};

	void SetShade(UINT shadeID=0,BYTE granularity=8,BYTE highlight=10,BYTE coloring=0,COLORREF color=0);

private:
	CCeXDib m_dNormal,m_dDown,m_dDisabled,m_dOver,m_dh,m_dv;

protected:
	virtual DWORD OnDrawBorder(CDC* pDC, CRect* pRect);
	virtual DWORD OnDrawBackground(CDC* pDC, CRect* pRect);
};

#endif
