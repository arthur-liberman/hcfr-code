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
/////////////////////////////////////////////////////////////////////////////

// Measure.cpp: implementation of the CMeasure class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ColorHCFR.h"
#include "MainFrm.h"
#include "Measure.h"
#include "Generator.h"
#include "LuxScaleAdvisor.h"
#include "DataSetDoc.h"
#include "Views\MainView.h"

#include <math.h>
#include <sstream>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_SERIAL(CMeasure, CObject, 1)

CMeasure::CMeasure()
{
	m_isModified = FALSE;
	m_bpreV10 = FALSE;
	m_binMeasure = FALSE;
	m_primariesArray.SetSize(3);
	m_secondariesArray.SetSize(3);
	m_grayMeasureArray.SetSize(	GetConfig()->GetProfileInt("Scale Sizes","Gray",10)+1);
	m_nearBlackMeasureArray.SetSize( GetConfig()->GetProfileInt("Scale Sizes","Near Black",4)+1);
	m_nearWhiteMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Near White",4)+1);
	m_redSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_greenSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_blueSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_yellowSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_cyanSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
	m_magentaSatMeasureArray.SetSize(GetConfig()->GetProfileInt("Scale Sizes","Saturations",4)+1);
    m_cc24SatMeasureArray.SetSize(1000);
	m_cc24SatMeasureArray_master.SetSize(5000);

	m_primariesArray[0]=m_primariesArray[1]=m_primariesArray[2]=noDataColor;
	m_secondariesArray[0]=m_secondariesArray[1]=m_secondariesArray[2]=noDataColor;

	for(int i=0;i<m_grayMeasureArray.GetSize();i++)	// Init default values: by default m_grayMeasureArray init to D65, Y=1
		m_grayMeasureArray[i]=GetPrimary(0);	

	for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
		m_nearBlackMeasureArray[i]=noDataColor;	

	for(int  i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
		m_nearWhiteMeasureArray[i]=noDataColor;	

	for(int  i=0;i<m_redSatMeasureArray.GetSize();i++)
	{
		m_redSatMeasureArray[i]=noDataColor;
		m_greenSatMeasureArray[i]=noDataColor;
		m_blueSatMeasureArray[i]=noDataColor;
		m_yellowSatMeasureArray[i]=noDataColor;
		m_cyanSatMeasureArray[i]=noDataColor;
		m_magentaSatMeasureArray[i]=noDataColor;
	}	
	for ( int i=0;i<m_cc24SatMeasureArray.GetSize();i++ )	m_cc24SatMeasureArray[i]=noDataColor;
	for ( int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++ )	m_cc24SatMeasureArray_master[i]=noDataColor;
	m_OnOffWhite.SetXYZValue(GetColorReference().GetWhite());
	m_PrimeWhite.SetXYZValue(GetColorReference().GetWhite());
	m_PrimeWhite.SetY(100.);
	m_OnOffWhite.SetY(100.);
	m_OnOffBlack.SetXYZValue(GetColorReference().GetWhite());
	m_OnOffBlack.SetY(0.012345);
	m_AnsiBlack=m_AnsiWhite=noDataColor;
	m_CCStr = (CString)"";
	SetInfoString((CString)"Calibration by: \r\nDisplay: \r\nNote: \r\n");
	m_currentIndex = 0;
	m_bIREScaleMode = GetConfig()->GetProfileInt("References","IRELevels",FALSE);

	m_hThread = NULL;
	m_hEventRun = NULL;
	m_hEventDone = NULL;
	m_bTerminateThread = FALSE;
	m_bErrorOccurred = FALSE;
	m_nBkMeasureStep = 0;
	m_clrToMeasure = ColorRGBDisplay(0.0);
	m_nBkMeasureStepCount = 0;
	m_pBkMeasureSensor = NULL;
	m_pBkMeasuredColor = NULL;

	m_nbMaxMeasurements = GetConfig()->GetProfileInt("General","MaxMeasurements",2500);
	if ( m_nbMaxMeasurements < 100 )
		m_nbMaxMeasurements = 100;
	else if ( m_nbMaxMeasurements > 30000 )
		m_nbMaxMeasurements = 30000;

	m_bOverRideBlack = GetConfig()->GetProfileDouble("References","Use Black Level",0);
	double YBlack = GetConfig()->GetProfileDouble("References","Manual Black Level",0);
	m_userBlack = CColor(ColorXYZ(YBlack*.95047,YBlack,YBlack*1.0883));

}

CMeasure::~CMeasure()
{

}

void CMeasure::Copy(CMeasure * p,UINT nId)
{
	switch (nId)
	{
		case DUPLGRAYLEVEL:		// Gray scale measure
			m_grayMeasureArray.SetSize(p->m_grayMeasureArray.GetSize());
			m_bIREScaleMode=p->m_bIREScaleMode;
			m_OnOffWhite=p->m_OnOffWhite;
			for(int i=0;i<m_grayMeasureArray.GetSize();i++)
    			m_grayMeasureArray[i]=p->m_grayMeasureArray[i];	
			break;

		case DUPLNEARBLACK:		// Near black measure
			m_nearBlackMeasureArray.SetSize(p->m_nearBlackMeasureArray.GetSize());
			for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
	    		m_nearBlackMeasureArray[i]=p->m_nearBlackMeasureArray[i];
			break;

		case DUPLNEARWHITE:		// Near white measure
			m_nearWhiteMeasureArray.SetSize(p->m_nearWhiteMeasureArray.GetSize());
			for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
		    	m_nearWhiteMeasureArray[i]=p->m_nearWhiteMeasureArray[i];
			break;

		case DUPLPRIMARIESSAT:		// Primaries saturation measure
			m_redSatMeasureArray.SetSize(p->m_redSatMeasureArray.GetSize());
			m_greenSatMeasureArray.SetSize(p->m_greenSatMeasureArray.GetSize());
			m_blueSatMeasureArray.SetSize(p->m_blueSatMeasureArray.GetSize());
			m_yellowSatMeasureArray.SetSize(p->m_yellowSatMeasureArray.GetSize());
			m_cyanSatMeasureArray.SetSize(p->m_cyanSatMeasureArray.GetSize());
			m_magentaSatMeasureArray.SetSize(p->m_magentaSatMeasureArray.GetSize());
			m_cc24SatMeasureArray.SetSize(p->m_cc24SatMeasureArray.GetSize());
			m_cc24SatMeasureArray_master.SetSize(p->m_cc24SatMeasureArray_master.GetSize());
			for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
			{
				m_redSatMeasureArray[i]=p->m_redSatMeasureArray[i];
				m_greenSatMeasureArray[i]=p->m_greenSatMeasureArray[i];
				m_blueSatMeasureArray[i]=p->m_blueSatMeasureArray[i];
			}
			for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
				m_cc24SatMeasureArray[i]=p->m_cc24SatMeasureArray[i];
			for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)
				m_cc24SatMeasureArray_master[i]=p->m_cc24SatMeasureArray_master[i];
			break;

		case DUPLSECONDARIESSAT:		// Secondaries saturation measure
			m_redSatMeasureArray.SetSize(p->m_redSatMeasureArray.GetSize());
			m_greenSatMeasureArray.SetSize(p->m_greenSatMeasureArray.GetSize());
			m_blueSatMeasureArray.SetSize(p->m_blueSatMeasureArray.GetSize());
			m_yellowSatMeasureArray.SetSize(p->m_yellowSatMeasureArray.GetSize());
			m_cyanSatMeasureArray.SetSize(p->m_cyanSatMeasureArray.GetSize());
			m_magentaSatMeasureArray.SetSize(p->m_magentaSatMeasureArray.GetSize());
			m_cc24SatMeasureArray.SetSize(p->m_cc24SatMeasureArray.GetSize());
			m_cc24SatMeasureArray_master.SetSize(p->m_cc24SatMeasureArray_master.GetSize());
			for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
			{
				m_yellowSatMeasureArray[i]=p->m_yellowSatMeasureArray[i];
				m_cyanSatMeasureArray[i]=p->m_cyanSatMeasureArray[i];
				m_magentaSatMeasureArray[i]=p->m_magentaSatMeasureArray[i];
			}
			for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
				m_cc24SatMeasureArray[i]=p->m_cc24SatMeasureArray[i];
			for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)
				m_cc24SatMeasureArray_master[i]=p->m_cc24SatMeasureArray_master[i];
			break;

		case DUPLPRIMARIESCOL:		// Primaries measure
			for(int i=0;i<m_primariesArray.GetSize();i++)
				m_primariesArray[i]=p->m_primariesArray[i];	
				m_PrimeWhite = p->m_PrimeWhite;
			break;

		case DUPLSECONDARIESCOL:		// Secondaries measure
			for(int i=0;i<m_secondariesArray.GetSize();i++)
				m_secondariesArray[i]=p->m_secondariesArray[i];
				m_PrimeWhite = p->m_PrimeWhite;
			break;

		case DUPLCONTRAST:		// Contrast measure
			m_OnOffBlack=p->m_OnOffBlack;
			m_OnOffWhite=p->m_OnOffWhite;
			m_AnsiBlack=p->m_AnsiBlack;
			m_AnsiBlack=p->m_AnsiBlack;
			break;

		case DUPLINFO:		// Info
			m_infoStr=p->m_infoStr;
			break;

		default:
			break;
	}
}

void CMeasure::Serialize(CArchive& ar)
{
	CObject::Serialize(ar) ;

	if (ar.IsStoring())
	{
	    int version = 12;
		ar << version;

		ar << (int)GetConfig()->m_whiteTarget; //new in 12
		ar << (int)GetConfig()->m_CCMode;
		ar << (int)GetConfig()->m_colorStandard;
		ar << GetConfig()->m_dE_form;
		ar << GetConfig()->m_dE_gray;
		ar << GetConfig()->gw_Weight;
		ar << GetConfig()->m_GammaOffsetType;
		ar << GetConfig()->m_GammaRef;
		ar << GetConfig()->m_GammaRel;
		ar << GetConfig()->m_Split;
		ar << GetConfig()->m_manualWhitex;
		ar << GetConfig()->m_manualWhitey;
		ar << GetConfig()->m_useMeasuredGamma;
		ar << GetConfig()->m_manualBluex;
		ar << GetConfig()->m_manualRedx;
		ar << GetConfig()->m_manualGreenx;
		ar << GetConfig()->m_manualBluey;
		ar << GetConfig()->m_manualRedy;
		ar << GetConfig()->m_manualGreeny;

		ar << m_bOverRideBlack; //new in 11
		m_userBlack.Serialize(ar);

		ar << m_grayMeasureArray.GetSize();
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)
			m_grayMeasureArray[i].Serialize(ar);

		// Version 3: near black and near white added
		ar << m_nearBlackMeasureArray.GetSize();
		for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
			m_nearBlackMeasureArray[i].Serialize(ar);

		ar << m_nearWhiteMeasureArray.GetSize();
		for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
			m_nearWhiteMeasureArray[i].Serialize(ar);

		// Version 2: color saturation added
		ar << m_redSatMeasureArray.GetSize();
		for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
			m_redSatMeasureArray[i].Serialize(ar);

		ar << m_greenSatMeasureArray.GetSize();
		for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)
			m_greenSatMeasureArray[i].Serialize(ar);

		ar << m_blueSatMeasureArray.GetSize();
		for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)
			m_blueSatMeasureArray[i].Serialize(ar);

		ar << m_yellowSatMeasureArray.GetSize();
		for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
			m_yellowSatMeasureArray[i].Serialize(ar);

		ar << m_cyanSatMeasureArray.GetSize();
		for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)
			m_cyanSatMeasureArray[i].Serialize(ar);

		ar << m_magentaSatMeasureArray.GetSize();
		for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)
			m_magentaSatMeasureArray[i].Serialize(ar);

		ar << m_cc24SatMeasureArray.GetSize();
		for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
			m_cc24SatMeasureArray[i].Serialize(ar);

		ar << m_cc24SatMeasureArray_master.GetSize();
		for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)
			m_cc24SatMeasureArray_master[i].Serialize(ar);

		// Version 1 again
		ar << m_measurementsArray.GetSize();
		for(int i=0;i<m_measurementsArray.GetSize();i++)
			m_measurementsArray[i].Serialize(ar);

		m_primariesArray[0].Serialize(ar);
		m_primariesArray[1].Serialize(ar);
		m_primariesArray[2].Serialize(ar);

		m_secondariesArray[0].Serialize(ar);
		m_secondariesArray[1].Serialize(ar);
		m_secondariesArray[2].Serialize(ar);

		m_OnOffBlack.Serialize(ar);
		m_OnOffWhite.Serialize(ar);
		m_AnsiBlack.Serialize(ar);
		m_AnsiWhite.Serialize(ar);
//version 9
		m_PrimeWhite.Serialize(ar);

		ar << m_infoStr;

		ar << m_bIREScaleMode;
	}
	else
	{
	    int version;
		ar >> version;

		if ( version > 12)
			AfxThrowArchiveException ( CArchiveException::badSchema );

		if ( version > 11)
		{
			int in;
			double in2;
			ar >> in;
			GetConfig()->m_whiteTarget = WhiteTarget(in);
			ar >> in;
			GetConfig()->m_CCMode = CCPatterns(in);
			ar >> in;
			GetConfig()->m_colorStandard = ColorStandard(in);
			ar >> in;
			GetConfig()->m_dE_form = in;
			ar >> in;
			GetConfig()->m_dE_gray = in;
			ar >> in;
			GetConfig()->gw_Weight = in;
			ar >> in;
			GetConfig()->m_GammaOffsetType = in;
			ar >> in2;
			GetConfig()->m_GammaRef = in2;
			ar >> in2;
			GetConfig()->m_GammaRel = in2;
			ar >> in2;
			GetConfig()->m_Split = in2;
			ar >> in2;
			GetConfig()->m_manualWhitex = in2;
			ar >> in2;
			GetConfig()->m_manualWhitey = in2;
			ar >> in;
			GetConfig()->m_useMeasuredGamma = in;
			ar >> in2;
			GetConfig()->m_manualBluex = in2;
			ar >> in2;
			GetConfig()->m_manualRedx = in2;
			ar >> in2;
			GetConfig()->m_manualGreenx = in2;
			ar >> in2;
			GetConfig()->m_manualBluey = in2;
			ar >> in2;
			GetConfig()->m_manualRedy = in2;
			ar >> in2;
			GetConfig()->m_manualGreeny = in2;
		}

		if (version > 10)
		{
			ar >> m_bOverRideBlack;
			m_userBlack.Serialize(ar);
		}

		int size, gsize;

		ar >> gsize;
		m_grayMeasureArray.SetSize(gsize);
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)
			m_grayMeasureArray[i].Serialize(ar);
					
		if ( version > 2 )
		{
			ar >> size;
			m_nearBlackMeasureArray.SetSize(size);
			for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
				m_nearBlackMeasureArray[i].Serialize(ar);

			ar >> size;
			m_nearWhiteMeasureArray.SetSize(size);
			for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
				m_nearWhiteMeasureArray[i].Serialize(ar);
		}
		else
		{
			m_nearBlackMeasureArray.SetSize(5);
			m_nearWhiteMeasureArray.SetSize(5);
			for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
			{
				m_nearBlackMeasureArray[i]=noDataColor;
				m_nearWhiteMeasureArray[i]=noDataColor;
			}
		}

		if ( version > 1 )
		{
			ar >> size;
			m_redSatMeasureArray.SetSize(size);
			for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
				m_redSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_greenSatMeasureArray.SetSize(size);
			for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)
				m_greenSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_blueSatMeasureArray.SetSize(size);
			for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)
				m_blueSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_yellowSatMeasureArray.SetSize(size);
			for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
				m_yellowSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_cyanSatMeasureArray.SetSize(size);
			for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)
				m_cyanSatMeasureArray[i].Serialize(ar);

			ar >> size;
			m_magentaSatMeasureArray.SetSize(size);
			for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)
				m_magentaSatMeasureArray[i].Serialize(ar);

			if ( version >= 8)
			{

				ar >> size;
				m_cc24SatMeasureArray.SetSize(1000);
				for(int i=0;i<size;i++)
					m_cc24SatMeasureArray[i].Serialize(ar);
				if ( version >= 10)
				{
					ar >> size;
					m_cc24SatMeasureArray_master.SetSize(5000);
					for(int i=0;i<size;i++)
						m_cc24SatMeasureArray_master[i].Serialize(ar);
					m_CCStr=GetCCStr();
				}
				else
					m_bpreV10 = TRUE;

			}
			else
				m_bpreV10 = TRUE;			
		}
		else
		{
			m_redSatMeasureArray.SetSize(5);
			m_greenSatMeasureArray.SetSize(5);
			m_blueSatMeasureArray.SetSize(5);
			m_yellowSatMeasureArray.SetSize(5);
			m_cyanSatMeasureArray.SetSize(5);
			m_magentaSatMeasureArray.SetSize(5);
			m_cc24SatMeasureArray.SetSize(24);
			for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++) 
				m_cc24SatMeasureArray[i]=noDataColor;
			for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
			{
				m_redSatMeasureArray[i]=noDataColor;
				m_greenSatMeasureArray[i]=noDataColor;
				m_blueSatMeasureArray[i]=noDataColor;
				m_yellowSatMeasureArray[i]=noDataColor;
				m_cyanSatMeasureArray[i]=noDataColor;
				m_magentaSatMeasureArray[i]=noDataColor;
			}
		}

		if (m_bpreV10)
		{
			CString msg;
			msg.SetString("Loading old color checker data, rerun series before saving new file.");
//			GetColorApp()->InMeasureMessageBox(msg,"Warning",MB_OK | MB_ICONWARNING);
		}
		
		ar >> size;
		m_measurementsArray.SetSize(size);
		for(int i=0;i<m_measurementsArray.GetSize();i++)
			m_measurementsArray[i].Serialize(ar);

		m_primariesArray[0].Serialize(ar);
		m_primariesArray[1].Serialize(ar);
		m_primariesArray[2].Serialize(ar);

		m_secondariesArray[0].Serialize(ar);
		m_secondariesArray[1].Serialize(ar);
		m_secondariesArray[2].Serialize(ar);

		m_OnOffBlack.Serialize(ar);
		if (!m_OnOffBlack.isValid() && gsize > 0)
			m_OnOffBlack = 	m_grayMeasureArray[0];
		m_OnOffWhite.Serialize(ar);
		m_AnsiBlack.Serialize(ar);
		m_AnsiWhite.Serialize(ar);
		if ( version > 8 )
			m_PrimeWhite.Serialize(ar);
		else
		{
			m_PrimeWhite = m_OnOffWhite;
			if (gsize > 0)
				m_OnOffWhite = m_grayMeasureArray[gsize-1];
		}

		ar >> m_infoStr;
		if (m_infoStr.Find("\n") < 1)
			SetInfoString((CString)"Calibration by: \r\nDisplay: \r\nNote: \r\n");

		if ( version > 4 && version < 7 )
		{
            BOOL bUseAdjustmentMatrix;
            Matrix XYZAdjustmentMatrix;
            CString XYZAdjustmentComment;
			ar >> bUseAdjustmentMatrix;
			XYZAdjustmentMatrix.Serialize(ar);
			ar >> XYZAdjustmentComment;
		}

		if ( version > 5 )
			ar >> m_bIREScaleMode;
		else
			m_bIREScaleMode = FALSE;

	}
	m_isModified = FALSE;
}

void CMeasure::SetGrayScaleSize(int steps)
{
	int OldSize = m_grayMeasureArray.GetSize ();

	m_grayMeasureArray.SetSize(steps);

	if ( steps != OldSize )
	{
		// Purge all actual results
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)	// Init default values: by default m_grayMeasureArray init to D65, Y=1
		{
			m_grayMeasureArray[i]=noDataColor;	
		}
	}
}

void CMeasure::SetIREScaleMode(BOOL bIRE)
{
	if ( bIRE != m_bIREScaleMode )
	{
		m_bIREScaleMode = bIRE;

		// Purge all actual results
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)	// Init default values: by default m_grayMeasureArray init to D65, Y=1
		{
			m_grayMeasureArray[i]=noDataColor;	
		}
	}
}

void CMeasure::SetNearBlackScaleSize(int steps)
{
	int OldSize = m_nearBlackMeasureArray.GetSize ();

	m_nearBlackMeasureArray.SetSize(steps);

	if ( steps != OldSize )
	{
		// Purge all actual results
		for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
		{
			m_nearBlackMeasureArray[i]=noDataColor;	
		}
	}
}

void CMeasure::SetNearWhiteScaleSize(int steps)
{
	int OldSize = m_nearWhiteMeasureArray.GetSize ();

	m_nearWhiteMeasureArray.SetSize(steps);

	if ( steps != OldSize )
	{
		// Purge all actual results
		for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
		{
			m_nearWhiteMeasureArray[i]=noDataColor;	
		}
	}
}

void CMeasure::SetSaturationSize(int steps)
{
	int OldSize = m_redSatMeasureArray.GetSize ();

	m_redSatMeasureArray.SetSize(steps);
	m_greenSatMeasureArray.SetSize(steps);
	m_blueSatMeasureArray.SetSize(steps);
	m_yellowSatMeasureArray.SetSize(steps);
	m_cyanSatMeasureArray.SetSize(steps);
	m_magentaSatMeasureArray.SetSize(steps);

	if ( steps != OldSize )
	{
		// Purge all actual results
		for(int i=0;i<m_redSatMeasureArray.GetSize();i++)	// Init default values
		{
			m_redSatMeasureArray[i]=noDataColor;
			m_greenSatMeasureArray[i]=noDataColor;
			m_blueSatMeasureArray[i]=noDataColor;
			m_yellowSatMeasureArray[i]=noDataColor;
			m_cyanSatMeasureArray[i]=noDataColor;
			m_magentaSatMeasureArray[i]=noDataColor;
		}
	}
}


void CMeasure::StartLuxMeasure ()
{
	GetColorApp () -> BeginLuxMeasure ();
}

UINT CMeasure::GetLuxMeasure ( double * pValue )
{
	UINT	nRet = LUX_NOMEASURE;
	UINT	nLuxRetCode;
	BOOL	bContinue;
	double	dLuxValue;
	CString	Msg, Title;
	CLuxScaleAdvisor *	pDlg = NULL;

	do
	{
		bContinue = FALSE;
		nLuxRetCode = GetColorApp () -> GetLuxMeasure ( & dLuxValue );
		switch ( nLuxRetCode )
		{
			case LUXMETER_OK:
				 * pValue = dLuxValue;
				 nRet = LUX_OK;
				 break;
			
			case LUXMETER_NOT_RUNNING:
				 * pValue = 0.0;
				 nRet = LUX_NOMEASURE;
				 break;

			case LUXMETER_SCALE_TOO_HIGH:
			case LUXMETER_SCALE_TOO_LOW:
				 // Request scale adjustment
				 bContinue = TRUE;
				 if ( pDlg == NULL )
				 {
					// Create advisor dialog
					pDlg = new CLuxScaleAdvisor;
					pDlg -> Create ( pDlg -> IDD, NULL );
					pDlg -> UpdateWindow ();
				 }
				 else
				 {
					// Test if advisor button has been clicked
					if ( pDlg -> m_bCancel )
					{
						 * pValue = 0.0;
						 nRet = LUX_CANCELED;
						 bContinue = FALSE;
					}
					else if ( pDlg -> m_bContinue )
					{
						 * pValue = 0.0;
						 nRet = LUX_NOMEASURE;
						 bContinue = FALSE;
					}
				 }
				 break;

			case LUXMETER_SCALE_TOO_HIGH_MIN:
			case LUXMETER_SCALE_TOO_LOW_MAX:
				 Title.LoadString ( IDS_ERROR );
				 Msg.LoadString ( IDS_LUXMETER_OUTOFRANGE );
				 GetColorApp()->InMeasureMessageBox(Msg,Title,MB_OK | MB_ICONINFORMATION);
				 * pValue = 0.0;
				 nRet = LUX_NOMEASURE;
				 break;

			case LUXMETER_NEW_SCALE_OK:
				 bContinue = TRUE;
				 if ( pDlg )
				 {
					// Advisor dialog has been opened: close it
					pDlg -> DestroyWindow ();
					delete pDlg;
					pDlg = NULL;
					GetColorApp()->BringPatternWindowToTop ();
				 }
				 
				 // Reset measures
				 * pValue = 0.0;
				 WaitForDynamicIris ( TRUE );
				 GetColorApp () -> BeginLuxMeasure ();
				 break;
		}

		if ( bContinue )
		{
			// Sleep 50 ms while dispatching messages to the advisor dialog
			Sleep(50);
			
			MSG	Msg;
			while(PeekMessage(&Msg, NULL, NULL, NULL, PM_REMOVE))
			{
				TranslateMessage( &Msg );
				DispatchMessage( &Msg );
			}
		}
	} while ( bContinue );

	if ( pDlg )
	{
		// This case should not occur
		ASSERT ( 0 );

		// Advisor dialog has been opened: close it
		pDlg -> DestroyWindow ();
		delete pDlg;
		pDlg = NULL;
		GetColorApp()->BringPatternWindowToTop ();
		GetColorApp () -> BeginLuxMeasure ();
	}

	return nRet;
}
bool doSettling = FALSE;

BOOL CMeasure::MeasureGrayScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_grayMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_IRE, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	
	m_binMeasure = TRUE;
	for(int i=(CheckBlackOverride()?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayGray(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown),CGenerator::MT_IRE ,!bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown));

					m_grayMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			if ( bUseLuxValues && ! bEscape && i == 0 )
			{
				int		nNbLoops = 0;
				BOOL	bContinue;
				
				// Measuring black: ask and reask luxmeter value until it stabilizes
				do
				{
					bContinue = FALSE;
					nNbLoops ++;
					
					StartLuxMeasure ();
					
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 if ( dLuxValue < measuredLux[0] )
							 {
								measuredLux[0] = dLuxValue;
								bContinue = TRUE;
							 }
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				} while ( bContinue && nNbLoops < 10 );
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 0);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_IRE,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}
	GetConfig()->m_isSettling = doSettling;

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		GetColorApp()->InMeasureMessageBox(pGenerator->GetRetryMessage(), NULL, MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		if (!i && m_bOverRideBlack)
			m_grayMeasureArray[i] = m_userBlack;
		else
			m_grayMeasureArray[i] = measuredColor[i];

		if ( bUseLuxValues )
			m_grayMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_grayMeasureArray[i].ResetLuxValue ();
	}

	m_OnOffWhite = measuredColor[size-1];
	if (m_bOverRideBlack)
		m_OnOffBlack = m_userBlack;
	else		
		m_OnOffBlack = measuredColor[0];

	if ( bUseLuxValues )
	{
		m_OnOffWhite.SetLuxValue ( measuredLux[size-1] );
		m_OnOffBlack.SetLuxValue ( measuredLux[0] );
	}
	else
	{
		m_OnOffWhite.ResetLuxValue ();
		m_OnOffBlack.ResetLuxValue ();
	}

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureGrayScaleAndColors(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_grayMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size+6+GetConfig()->m_BWColorsToAdd);
	measuredLux.SetSize(size+6+GetConfig()->m_BWColorsToAdd);

	if(pGenerator->Init(size+6+GetConfig()->m_BWColorsToAdd) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayGrayAndColorsSeries() != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	m_binMeasure = TRUE;
	for(int i=(CheckBlackOverride()?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayGray(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown),CGenerator::MT_IRE ,!bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown ));
				m_grayMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			if ( bUseLuxValues && ! bEscape && i == 0 )
			{
				int		nNbLoops = 0;
				BOOL	bContinue;
				
				// Measuring black: ask and reask luxmeter value until it stabilizes
				do
				{
					bContinue = FALSE;
					nNbLoops ++;
					
					StartLuxMeasure ();
					
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 if ( dLuxValue < measuredLux[0] )
							 {
								measuredLux[0] = dLuxValue;
								bContinue = TRUE;
							 }
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				} while ( bContinue && nNbLoops < 10 );
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release(size - 1 - i);
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				UpdateViews(pDoc, 0);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_IRE,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}
	
	// Generator init to change pattern series if necessary
	if ( !pGenerator->ChangePatternSeries())
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	double primaryIRELevel=100.0;	
	// Measure primary and secondary colors
	ColorRGBDisplay	GenColors [ 8 ] = 
								{	
									ColorRGBDisplay(primaryIRELevel,0,0),
									ColorRGBDisplay(0,primaryIRELevel,0),
									ColorRGBDisplay(0,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,0),
									ColorRGBDisplay(0,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(0,0,0)
								};
	if (GetColorReference().m_standard == HDTVb)
	{
			GenColors [ 0 ] = ColorRGBDisplay(79.9087,10.0457,10.0457); 
			GenColors [ 1 ] = ColorRGBDisplay(30.137,79.9087,30.137); 
			GenColors [ 2 ] = ColorRGBDisplay(50.2283,50.2283,79.9087); 
			GenColors [ 3 ] = ColorRGBDisplay(79.9087,79.9087,10.0457);
			GenColors [ 4 ] = ColorRGBDisplay(10.0457,79.9087,79.9087);
			GenColors [ 5 ] = ColorRGBDisplay(79.9087,10.0457,79.9087);
	}
	else if (GetColorReference().m_standard == HDTVa) //75%
	{ 
		GenColors [ 0 ] = ColorRGBDisplay(68.04,20.09,20.09);
		GenColors [ 1 ] = ColorRGBDisplay(27.85,73.06,27.85);
		GenColors [ 2 ] = ColorRGBDisplay(19.18,19.18,50.22);
		GenColors [ 3 ] = ColorRGBDisplay(74.89,74.89,32.88);
		GenColors [ 4 ] = ColorRGBDisplay(36.07,73.06,73.06);
		GenColors [ 5 ] = ColorRGBDisplay(63.92,29.22,63.93);
		GenColors [ 6 ] = ColorRGBDisplay(75.0,75.0,75.0);
	}

	ColorRGBDisplay	MeasColors [ 8 ] = 
								{	
									ColorRGBDisplay(primaryIRELevel,0,0),
									ColorRGBDisplay(0,primaryIRELevel,0),
									ColorRGBDisplay(0,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,0),
									ColorRGBDisplay(0,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(0,0,0)
								};
	for (int i = 0; i < 6 + GetConfig()->m_BWColorsToAdd ; i ++ )
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SECONDARY,i,TRUE,TRUE) )
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[size+i]=pSensor->MeasureColor(MeasColors[i]);
				if (i<3)
					m_primariesArray[i] = measuredColor[size+i];
				if (i>=3&&i<6)
					m_secondariesArray[i-3] = measuredColor[size+i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[size+i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[size+i];
				m_currentIndex = i;
				UpdateViews(pDoc, 1);
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_SECONDARY,previousColor,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		if (!i && m_bOverRideBlack)
			m_grayMeasureArray[i] = m_userBlack;
		else
			m_grayMeasureArray[i] = measuredColor[i];

		if ( bUseLuxValues )
			m_grayMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_grayMeasureArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_primariesArray[i] = measuredColor[i+size];

		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i+size] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_secondariesArray[i] = measuredColor[i+size+3];

		if ( bUseLuxValues )
			m_secondariesArray[i].SetLuxValue ( measuredLux[i+size+3] );
		else
			m_secondariesArray[i].ResetLuxValue ();
	}

	m_OnOffWhite = measuredColor[size-1];
	if (m_bOverRideBlack)
		m_OnOffBlack = m_userBlack;
	else
		m_OnOffBlack = measuredColor[0];

	if ( bUseLuxValues )
	{
		m_OnOffWhite.SetLuxValue ( measuredLux[size-1] );
		m_OnOffBlack.SetLuxValue ( measuredLux[0] );
	}
	else
	{
		m_OnOffWhite.ResetLuxValue ();
//		m_OnOffBlack.ResetLuxValue ();
	}

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_PrimeWhite = measuredColor[size+6];                
		if ( bUseLuxValues )
			m_PrimeWhite.SetLuxValue ( measuredLux[size+6] );
		else
			m_PrimeWhite.ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;
		
	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureNearBlackScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_nearBlackMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_NEARBLACK, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}


	m_binMeasure = TRUE;
	for(int i=(CheckBlackOverride()?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayGray((ArrayIndexToGrayLevel ( i, 101, GetConfig () -> m_bUseRoundDown)),CGenerator::MT_NEARBLACK,!bRetry) )
		{

			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(i);
				m_nearBlackMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			if ( bUseLuxValues && ! bEscape && i == 0 )
			{
				int		nNbLoops = 0;
				BOOL	bContinue;
				
				// Measuring black: ask and reask luxmeter value until it stabilizes
				do
				{
					bContinue = FALSE;
					nNbLoops ++;
					
					StartLuxMeasure ();
					
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 if ( dLuxValue < measuredLux[0] )
							 {
								measuredLux[0] = dLuxValue;
								bContinue = TRUE;
							 }
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				} while ( bContinue && nNbLoops < 10 );
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				UpdateViews(pDoc, 3);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_NEARBLACK,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		if (!i && m_bOverRideBlack)
			m_nearBlackMeasureArray[i] = m_userBlack;
		else
			m_nearBlackMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_nearBlackMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_nearBlackMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureNearWhiteScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_nearWhiteMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_NEARWHITE, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	
	m_binMeasure = TRUE;
	for(int i=0;i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayGray((ArrayIndexToGrayLevel ( 101-size+i, 101, GetConfig () -> m_bUseRoundDown)),CGenerator::MT_NEARWHITE,!bRetry ) )
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(101-size+i);
				m_nearWhiteMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				UpdateViews(pDoc, 4);

				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_NEARWHITE,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_nearWhiteMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_nearWhiteMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_nearWhiteMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureRedSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_RED, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	// Generate saturation colors for red
	GenerateSaturationColors (GetColorReference(), GenColors,size, true, false, false);
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;

		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_RED,100*i/(size - 1),!bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				m_redSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 5);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_RED,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_redSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_redSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_redSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureGreenSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_GREEN, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	// Generate saturation colors for green
	GenerateSaturationColors (GetColorReference(), GenColors,size, false, true, false );
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
	for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_GREEN,100*i/(size - 1),!bRetry) )
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				m_greenSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 6);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_GREEN,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_greenSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_greenSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_greenSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureBlueSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_BLUE, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	// Generate saturation colors for blue
		GenerateSaturationColors (GetColorReference(), GenColors,size, false, false, true );
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_BLUE,100*i/(size - 1),!bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				m_blueSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 7);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_BLUE,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_blueSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_blueSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_blueSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureYellowSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_YELLOW, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	// Generate saturation colors for yellow
	GenerateSaturationColors (GetColorReference(), GenColors,size, true, true, false );
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_YELLOW,100*i/(size - 1),!bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				m_yellowSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 8);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_YELLOW,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}

		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_yellowSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_yellowSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_yellowSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureCyanSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_CYAN, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	// Generate saturation colors for cyan
	GenerateSaturationColors (GetColorReference(), GenColors,size, false, true, true );
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_CYAN,100*i/(size - 1),!bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				m_cyanSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 9);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_CYAN,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}

		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_cyanSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_cyanSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_cyanSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureMagentaSatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_MAGENTA, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	// Generate saturation colors for magenta
	GenerateSaturationColors (GetColorReference(), GenColors,size, true, false, true );
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	m_binMeasure = TRUE;
    for(int i=((GetConfig()->m_CCMode == MCD && pGenerator->GetName() == str)?1:0);i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_MAGENTA,100*i/(size - 1) ,!bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				m_magentaSatMeasureArray[i] = measuredColor[i];
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 10);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(CGenerator::MT_SAT_MAGENTA,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_magentaSatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_magentaSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_magentaSatMeasureArray[i].ResetLuxValue ();
	}
	GetConfig()->m_isSettling = doSettling;
	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureCC24SatScale(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	CCPatterns	ccPat = GetConfig()->m_CCMode;
	int			size = ccPat == CCSG?96:(ccPat == CMS || ccPat ==CPS)?19:(ccPat==AXIS?71:24);
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 1010 ];
	double		dLuxValue;
	BOOL isExtPat =( GetConfig()->m_CCMode == USER || GetConfig()->m_CCMode == CM10SAT || GetConfig()->m_CCMode == CM10SAT75 || GetConfig()->m_CCMode == CM5SAT || GetConfig()->m_CCMode == CM5SAT75 || GetConfig()->m_CCMode == CM4SAT || GetConfig()->m_CCMode == CM4SAT75 || GetConfig()->m_CCMode == CM4LUM || GetConfig()->m_CCMode == CM5LUM || GetConfig()->m_CCMode == CM10LUM || GetConfig()->m_CCMode == RANDOM250 || GetConfig()->m_CCMode == RANDOM500 || GetConfig()->m_CCMode == CM6NB || GetConfig()->m_CCMode == CMDNR);

    if (isExtPat) size = GetConfig()->GetCColorsSize();

    CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size);
	measuredLux.SetSize(size);

	if(pGenerator->Init(size) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}
	// Generate saturation colors for color checker
	CGenerator::MeasureType nPattern;
	switch (GetConfig()->m_CCMode)
	{
	case GCD:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;
		 break;
	case MCD:
		 nPattern=CGenerator::MT_SAT_CC24_MCD;		
		 break;
	case CMC:
		 nPattern=CGenerator::MT_SAT_CC24_CMC;		
		 break;
	case CMS:
		 nPattern=CGenerator::MT_SAT_CC24_CMS;		
		 break;
	case CPS:
		 nPattern=CGenerator::MT_SAT_CC24_CPS;		
		 break;
	case SKIN:
		 nPattern=CGenerator::MT_SAT_CC24_MCD;
		 break;
	case AXIS:
		 nPattern=CGenerator::MT_SAT_CC24_AXIS;
		 break;
	case CCSG:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;
		 break;
	default:
		 nPattern=CGenerator::MT_SAT_CC24_USER;
		 break;
	}

	if(pGenerator->CanDisplayScale ( nPattern, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

    if(pGenerator->GetName() == str&&( GetConfig()->m_CCMode==SKIN || (GetConfig()->m_CCMode==USER && size > 100) ) )
	{		
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		strMsg.Append(" not a supported DVD sequence.");
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

    if (!GenerateCC24Colors (GenColors, GetConfig()->m_CCMode))
	{		
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	for (int i=0;i<1000;i++) 
		m_cc24SatMeasureArray[i] = noDataColor;
	m_binMeasure = TRUE;
	for(int i=0;i<size;i++)
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayRGBColor(GenColors[i], nPattern , i, !bRetry))
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();
				if (GetConfig()->m_CCMode != MCD)
				{
					measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				}
				else
				{
					if (i < 18)
						measuredColor[i+6]=pSensor->MeasureColor(GenColors[i]);	
					else
						measuredColor[23-i]=pSensor->MeasureColor(GenColors[i]);	
				}

				m_cc24SatMeasureArray[i] = measuredColor[i];

				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 11);
	
				if(i != 0)
				{
					if (!pGenerator->HasPatternChanged(nPattern,previousColor,lastColor))
					{
						i--;
						bPatternRetry = TRUE;
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_cc24SatMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_cc24SatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_cc24SatMeasureArray[i].ResetLuxValue ();
	}

	GetConfig()->m_isSettling = doSettling;
	int iCC=GetConfig()->m_CCMode;
	if (iCC < 19)
		for (int i=0+100*iCC;i<100*(iCC+1);i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-iCC*100];
	else if (iCC == 19) 
		for (int i=1900;i<1900+250;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-1900];
	else if (iCC == 20) 
		for (int i=1900+250;i<1900+250+500;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(1900+250)];
	else if (iCC == 21) 
		for (int i=1900+250+500;i<1900+250+500+1000;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(1900+250+500)];

	m_isModified=TRUE;
	m_binMeasure = FALSE;
	m_CCStr=GetCCStr();
	UpdateViews(pDoc, 11);
	return TRUE;
}

BOOL CMeasure::MeasureAllSaturationScales(CSensor *pSensor, CGenerator *pGenerator, BOOL bPrimaryOnly, CDataSetDoc *pDoc)
{
	int			i, j;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CCPatterns	ccPat = GetConfig()->m_CCMode;
	int			ccSize = ccPat == CCSG?96:(ccPat == CMS || ccPat == CPS)?19:(ccPat==AXIS?71:24);
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 7 * 256 ];

	double		dLuxValue;
	CGenerator::MeasureType nPattern;
	switch (GetConfig()->m_CCMode)
	{
	case MCD:
		 nPattern=CGenerator::MT_SAT_CC24_MCD;
		 break;
	case GCD:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;		
		 break;
	case CMC:
		 nPattern=CGenerator::MT_SAT_CC24_CMC;		
		 break;
	case CMS:
		 nPattern=CGenerator::MT_SAT_CC24_CMS;		
		 break;
	case CPS:
		 nPattern=CGenerator::MT_SAT_CC24_CPS;		
		 break;
	case SKIN:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;
         break;
	case AXIS:
		 nPattern=CGenerator::MT_SAT_CC24_AXIS;
         break;
	case CCSG:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;		
         break;
	default:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;		
         ccSize = GetConfig()->GetCColorsSize();
		 break;
	}
	
	CGenerator::MeasureType	SaturationType [ 7 ] =
							{
								CGenerator::MT_SAT_RED,
								CGenerator::MT_SAT_GREEN,
								CGenerator::MT_SAT_BLUE,
								CGenerator::MT_SAT_YELLOW,
								CGenerator::MT_SAT_CYAN,
								CGenerator::MT_SAT_MAGENTA,
								nPattern
							};

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

    measuredColor.SetSize(size*6+(ccSize));
	measuredLux.SetSize(size*6+(ccSize));

	if(pGenerator->Init(size*(bPrimaryOnly?3:6) + ccSize) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_ALL, ccSize ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	if(pGenerator->GetName() == str&&( GetConfig()->m_CCMode==SKIN || (GetConfig()->m_CCMode==USER && ccSize > 100) ))
	{		
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		strMsg.Append(" not a supported DVD sequence.");
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}


	// Generate saturations for all colors
	GenerateSaturationColors (GetColorReference(), GenColors, size, true, false, false );				// Red
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 1 ], size, false, true, false );	// Green
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 2 ], size, false, false, true );	// Blue
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 3 ], size, true, true, false );	// Yellow
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 4 ], size, false, true, true );	// Cyan
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 5 ], size, true, false, true );	// Magenta

	if (!GenerateCC24Colors (& GenColors [ size * 6 ], GetConfig()->m_CCMode)) //color checker
	{		
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
	for (i=0;i<1000;i++) 
	m_cc24SatMeasureArray[i] = noDataColor;

	m_binMeasure = TRUE;
	for ( j = 0 ; j < ( bPrimaryOnly ? 3 : 7 ) ; j ++ )
	{
		for ( i = 0 ; i < ( j == 6 ? ccSize : size ) ; i ++ )
		{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
			if( pGenerator->DisplayRGBColor(GenColors[(j*size)+i],SaturationType[j],(j == 6 ? i:100*i/(size - 1)),!bRetry,(j>0)) )
			{
				bEscape = WaitForDynamicIris (FALSE);
				bRetry = FALSE;

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();

					measuredColor[(j*size)+i]=pSensor->MeasureColor(GenColors[(j*size)+i]);
					if ((i+j*size)<size)
						m_redSatMeasureArray[i] = measuredColor[j*size+i];
					if ((i+j*size)<2*size&&(i+j*size)>=size)
						m_greenSatMeasureArray[i] = measuredColor[j*size+i];
					if ((i+j*size)<3*size&&(i+j*size)>=2*size)
						m_blueSatMeasureArray[i] = measuredColor[j*size+i];
					if (!bPrimaryOnly)
					{
						if ((i+j*size)<4*size&&(i+j*size)>=3*size)
							m_yellowSatMeasureArray[i] = measuredColor[j*size+i];
						if ((i+j*size)<5*size&&(i+j*size)>=4*size)
							m_cyanSatMeasureArray[i] = measuredColor[j*size+i];
						if ((i+j*size)<6*size&&(i+j*size)>=5*size)
							m_magentaSatMeasureArray[i] = measuredColor[j*size+i];
						if ((i+j*size)>=6*size)
							m_cc24SatMeasureArray[i] = measuredColor[j*size+i];
					} else
					{
						if ((i+j*size)>=3*size)
							m_cc24SatMeasureArray[i] = measuredColor[j*size+i];
					}
					if ( bUseLuxValues )
					{
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 measuredLux[(j*size)+i] = dLuxValue;
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					}
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release(size - i - 1);
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						pSensor->Release();
						pGenerator->Release();
						return FALSE;
					}
					if(result == IDRETRY)
					{
						i--;
						bRetry = TRUE;
					}
				}
				else
				{
					previousColor = lastColor;			
					lastColor = measuredColor[(j*size)+i];
					m_currentIndex = i;
					if (bPrimaryOnly)
					{
						if (j < 3)
							UpdateViews(pDoc, j+5);
						else
							UpdateViews(pDoc, 11);
					}
					else
					{
						if (j < 6)
							UpdateViews(pDoc, j+5);
						else
							UpdateViews(pDoc, 11);
					}

					if(i != 0)
					{
						if (!pGenerator->HasPatternChanged(SaturationType[j],previousColor,lastColor))
						{
							i--;
							bPatternRetry = TRUE;
						}
					}
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}
		// Generator init to change pattern series if necessary
		if (j < ( bPrimaryOnly ? 3 : 6 ) - 1)
		{
			if ( !pGenerator->ChangePatternSeries())
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}

	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for ( i = 0 ; i < size ; i ++ )
	{
		m_redSatMeasureArray[i] = measuredColor[i];

		m_greenSatMeasureArray[i] = measuredColor[size+i];

		m_blueSatMeasureArray[i] = measuredColor[(2*size)+i];

		if ( bUseLuxValues )
		{
			m_redSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
			m_greenSatMeasureArray[i].SetLuxValue ( measuredLux[size+i] );
			m_blueSatMeasureArray[i].SetLuxValue ( measuredLux[(2*size)+i] );
		}
		else
		{
			m_redSatMeasureArray[i].ResetLuxValue ();
			m_greenSatMeasureArray[i].ResetLuxValue ();
			m_blueSatMeasureArray[i].ResetLuxValue ();
		}

		if ( ! bPrimaryOnly )
		{
			m_yellowSatMeasureArray[i] = measuredColor[(3*size)+i];

			m_cyanSatMeasureArray[i] = measuredColor[(4*size)+i];

			m_magentaSatMeasureArray[i] = measuredColor[(5*size)+i];

			m_cc24SatMeasureArray[i] = measuredColor[(6*size)+i];

			if ( bUseLuxValues )
			{
				m_yellowSatMeasureArray[i].SetLuxValue ( measuredLux[(3*size)+i] );
				m_cyanSatMeasureArray[i].SetLuxValue ( measuredLux[(4*size)+i] );
				m_magentaSatMeasureArray[i].SetLuxValue ( measuredLux[(5*size)+i] );
				m_cc24SatMeasureArray[i].SetLuxValue ( measuredLux[(6*size)+i] );
			}
			else
			{
				m_yellowSatMeasureArray[i].ResetLuxValue ();
				m_cyanSatMeasureArray[i].ResetLuxValue ();
				m_magentaSatMeasureArray[i].ResetLuxValue ();
				m_cc24SatMeasureArray[i].ResetLuxValue ();
			}
		}
	}
	for ( i = 0 ; i < ccSize ; i ++ )
	{
		if ( ! bPrimaryOnly )
		{
				if (GetConfig()->m_CCMode != MCD)
				{
					m_cc24SatMeasureArray[i] = measuredColor[(6*size)+i];
				}
				else
				{
					if (i < 18)
					    m_cc24SatMeasureArray[i+6] = measuredColor[(6*size)+i];
					else
						m_cc24SatMeasureArray[23-i] = measuredColor[(6*size)+i];
				}

			if ( bUseLuxValues )
			{
				if (GetConfig()->m_CCMode != MCD)
				{
					m_cc24SatMeasureArray[i].SetLuxValue ( measuredLux[(6*size)+i] );
				}
				else
				{
					if (i < 18)
						m_cc24SatMeasureArray[i+6].SetLuxValue ( measuredLux[(6*size)+i] );
					else
						m_cc24SatMeasureArray[23-i].SetLuxValue ( measuredLux[(6*size)+i] );
				}
			}
			else
			{
				m_cc24SatMeasureArray[i].ResetLuxValue ();
			}
		}
	}
	GetConfig()->m_isSettling = doSettling;
	int iCC=GetConfig()->m_CCMode;
	if (iCC < 19)
		for (int i=0+100*iCC;i<100*(iCC+1);i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-iCC*100];
	else if (iCC == 19) 
		for (int i=1900;i<1900+250;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-1900];
	else if (iCC == 20) 
		for (int i=1900+250;i<1900+250+500;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(1900+250)];
	else if (iCC == 21) 
		for (int i=1900+250+500;i<1900+250+500+1000;i++)
				m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(1900+250+500)];

	m_isModified=TRUE;
	m_binMeasure = FALSE;
	m_CCStr=GetCCStr();
	UpdateViews(pDoc, 11);
	return TRUE;
}

BOOL CMeasure::MeasurePrimarySecondarySaturationScales(CSensor *pSensor, CGenerator *pGenerator, BOOL bPrimaryOnly, CDataSetDoc *pDoc)
{
	int			i, j;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 6 * 256 ];

	double		dLuxValue;
	
	CGenerator::MeasureType	SaturationType [ 6 ] =
							{
								CGenerator::MT_SAT_RED,
								CGenerator::MT_SAT_GREEN,
								CGenerator::MT_SAT_BLUE,
								CGenerator::MT_SAT_YELLOW,
								CGenerator::MT_SAT_CYAN,
								CGenerator::MT_SAT_MAGENTA
							};

	CArray<CColor,int> measuredColor;
	CColor previousColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

	measuredColor.SetSize(size*6);
	measuredLux.SetSize(size*6);

	if(pGenerator->Init(size*(bPrimaryOnly?3:6)) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pGenerator->CanDisplayScale ( CGenerator::MT_SAT_ALL, size ) != TRUE)
	{
		pGenerator->Release();
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}


	// Generate saturations for all colors
	GenerateSaturationColors (GetColorReference(), GenColors, size, true, false, false );				// Red
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 1 ], size, false, true, false );	// Green
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 2 ], size, false, false, true );	// Blue
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 3 ], size, true, true, false );	// Yellow
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 4 ], size, false, true, true );	// Cyan
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 5 ], size, true, false, true );	// Magenta

	m_binMeasure = TRUE;
	for ( j = 0 ; j < ( bPrimaryOnly ? 3 : 6 ) ; j ++ )
	{
		for ( i = 0 ; i < size  ; i ++ )
		{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
			if( pGenerator->DisplayRGBColor(GenColors[(j*size)+i],SaturationType[j],100*i/(size - 1),!bRetry,(j>0)) )
			{
				bEscape = WaitForDynamicIris (FALSE);
				bRetry = FALSE;

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();

					measuredColor[(j*size)+i]=pSensor->MeasureColor(GenColors[(j*size)+i]);
					if ((i+j*size)<size)
						m_redSatMeasureArray[i] = measuredColor[i];
					if ((i+j*size)<size*2&&(i+j*size)>=size)
						m_greenSatMeasureArray[i] = measuredColor[size+i];
					if ((i+j*size)<size*3&&(i+j*size)>=2*size)
						m_blueSatMeasureArray[i] = measuredColor[2*size+i];
					if (!bPrimaryOnly)
					{
						if ((i+j*size)<size*4&&(i+j*size)>=3*size)
							m_yellowSatMeasureArray[i] = measuredColor[3*size+i];
						if ((i+j*size)<size*5&&(i+j*size)>=4*size)
							m_cyanSatMeasureArray[i] = measuredColor[4*size+i];
						if ((i+j*size)<size*6&&(i+j*size)>=5*size)
							m_magentaSatMeasureArray[i] = measuredColor[5*size+i];
					}
					
					if ( bUseLuxValues )
					{
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 measuredLux[(j*size)+i] = dLuxValue;
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					}
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release(size - i - 1);
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						pSensor->Release();
						pGenerator->Release();
						return FALSE;
					}
					if(result == IDRETRY)
					{
						i--;
						bRetry = TRUE;
					}
				}
				else
				{
					previousColor = lastColor;			
					lastColor = measuredColor[(j*size)+i];
					m_currentIndex = i;
					UpdateViews(pDoc, j+5);
		
					if(i != 0)
					{
						if (!pGenerator->HasPatternChanged(SaturationType[j],previousColor,lastColor))
						{
							i--;
							bPatternRetry = TRUE;
						}
					}
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}
		// Generator init to change pattern series if necessary
		if (j < ( bPrimaryOnly ? 3 : 6 ) - 1)
		{
			if ( !pGenerator->ChangePatternSeries())
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}

	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	for ( i = 0 ; i < size ; i ++ )
	{
		m_redSatMeasureArray[i] = measuredColor[i];

		m_greenSatMeasureArray[i] = measuredColor[size+i];

		m_blueSatMeasureArray[i] = measuredColor[(2*size)+i];

		if ( bUseLuxValues )
		{
			m_redSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
			m_greenSatMeasureArray[i].SetLuxValue ( measuredLux[size+i] );
			m_blueSatMeasureArray[i].SetLuxValue ( measuredLux[(2*size)+i] );
		}
		else
		{
			m_redSatMeasureArray[i].ResetLuxValue ();
			m_greenSatMeasureArray[i].ResetLuxValue ();
			m_blueSatMeasureArray[i].ResetLuxValue ();
		}

		if ( ! bPrimaryOnly )
		{
			m_yellowSatMeasureArray[i] = measuredColor[(3*size)+i];

			m_cyanSatMeasureArray[i] = measuredColor[(4*size)+i];

			m_magentaSatMeasureArray[i] = measuredColor[(5*size)+i];

			if ( bUseLuxValues )
			{
				m_yellowSatMeasureArray[i].SetLuxValue ( measuredLux[(3*size)+i] );
				m_cyanSatMeasureArray[i].SetLuxValue ( measuredLux[(4*size)+i] );
				m_magentaSatMeasureArray[i].SetLuxValue ( measuredLux[(5*size)+i] );
			}
			else
			{
				m_yellowSatMeasureArray[i].ResetLuxValue ();
				m_cyanSatMeasureArray[i].ResetLuxValue ();
				m_magentaSatMeasureArray[i].ResetLuxValue ();
			}
		}
	}
	GetConfig()->m_isSettling = doSettling;

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasurePrimaries(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	int		i;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	CColor	measuredColor[5];
	CString	strMsg, Title;
	CColor previousColor;
	double	dLuxValue;

	BOOL	bUseLuxValues = TRUE;
	double	measuredLux[5];

	if(pGenerator->Init(3 + GetConfig () -> m_BWColorsToAdd) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);

	if(pGenerator->GetName() == str&&GetConfig()->m_colorStandard == HDTVb)
	{		
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		strMsg.Append(" not a supported DVD sequence.");
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}


	// Measure primary and secondary colors
	double		primaryIRELevel=100.0;
	ColorRGBDisplay	GenColors [ 5 ] = 
								{	
									ColorRGBDisplay(primaryIRELevel,0,0),
									ColorRGBDisplay(0,primaryIRELevel,0),
									ColorRGBDisplay(0,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(0,0,0)
								};

	if ( GetColorReference().m_standard == HDTVb )
	{
			GenColors [ 0 ] = ColorRGBDisplay(79.9087,10.0457,10.0457); 
			GenColors [ 1 ] = ColorRGBDisplay(30.137,79.9087,30.137); 
			GenColors [ 2 ] = ColorRGBDisplay(50.2283,50.2283,79.9087); 
			GenColors [ 3 ] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
			GenColors [ 4 ] = ColorRGBDisplay(0,0,0);
	}
	else if ( GetColorReference().m_standard == HDTVa ) //75%
	{ 
		GenColors [ 0 ] = ColorRGBDisplay(68.04,20.09,20.09);
		GenColors [ 1 ] = ColorRGBDisplay(27.85,73.06,27.85);
		GenColors [ 2 ] = ColorRGBDisplay(19.18,19.18,50.22);
		GenColors [ 3 ] = ColorRGBDisplay(75.0,75.0,75.0);
		GenColors [ 4 ] = ColorRGBDisplay(0,0,0);
	
	}

	ColorRGBDisplay	MeasColors [ 5 ] = 
								{	
									ColorRGBDisplay(primaryIRELevel,0,0),
									ColorRGBDisplay(0,primaryIRELevel,0),
									ColorRGBDisplay(0,0,primaryIRELevel),
									ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel),
									ColorRGBDisplay(0,0,0)
								};

	m_binMeasure = TRUE;
	for ( i = 0; i < ( 3 + GetConfig () -> m_BWColorsToAdd ) ; i ++ )
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_PRIMARY, i) )
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(MeasColors[i]);
				if (i < 3)
					m_primariesArray[i] = measuredColor[i];
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				if (i<3)
					m_currentIndex = i;
				else
					m_currentIndex = i + 3;

				UpdateViews(pDoc, 1);
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_PRIMARY,previousColor,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}

	for(int i=0;i<3;i++)
	{
		m_primariesArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_PrimeWhite = measuredColor[3];                
		if ( bUseLuxValues )
			m_PrimeWhite.SetLuxValue ( measuredLux[3] );
		else
			m_PrimeWhite.ResetLuxValue ();
	}
	else
	{
		m_PrimeWhite=noDataColor;
	}

	if ( GetConfig () -> m_BWColorsToAdd > 1 )
	{
		if (m_bOverRideBlack)
			m_OnOffBlack = m_userBlack;
		else
			m_OnOffBlack = measuredColor[4];                
		if ( bUseLuxValues )
			m_OnOffBlack.SetLuxValue ( measuredLux[4] );
		else
			m_OnOffBlack.ResetLuxValue ();
	}
	else
	{
//		m_OnOffBlack=noDataColor;
	}
	GetConfig()->m_isSettling = doSettling;

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureSecondaries(CSensor *pSensor, CGenerator *pGenerator, CDataSetDoc *pDoc)
{
	int		i;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	CColor	measuredColor[8];
	CString	strMsg, Title;
	CColor previousColor;
	double	dLuxValue;

	BOOL	bUseLuxValues = TRUE;
	double	measuredLux[8];

	if(pGenerator->Init(6+GetConfig()->m_BWColorsToAdd) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}
		CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	// Measure primary and secondary colors
	double		IRELevel=100.0;	
	ColorRGBDisplay	GenColors [ 8 ] = 
								{	
									ColorRGBDisplay(IRELevel,0,0),
									ColorRGBDisplay(0,IRELevel,0),
									ColorRGBDisplay(0,0,IRELevel),
									ColorRGBDisplay(IRELevel,IRELevel,0),
									ColorRGBDisplay(0,IRELevel,IRELevel),
									ColorRGBDisplay(IRELevel,0,IRELevel),
									ColorRGBDisplay(IRELevel,IRELevel,IRELevel),
									ColorRGBDisplay(0,0,0)
								};
	if (GetColorReference().m_standard == HDTVb)
	{
			GenColors [ 0 ] = ColorRGBDisplay(79.9087,10.0457,10.0457); 
			GenColors [ 1 ] = ColorRGBDisplay(30.137,79.9087,30.137); 
			GenColors [ 2 ] = ColorRGBDisplay(50.2283,50.2283,79.9087); 
			GenColors [ 3 ] = ColorRGBDisplay(79.9087,79.9087,10.0457);
			GenColors [ 4 ] = ColorRGBDisplay(10.0457,79.9087,79.9087);
			GenColors [ 5 ] = ColorRGBDisplay(79.9087,10.0457,79.9087);
			GenColors [ 6 ] = ColorRGBDisplay(IRELevel,IRELevel,IRELevel);
			GenColors [ 7 ] = ColorRGBDisplay(0,0,0);
	}
	else if (GetColorReference().m_standard == HDTVa) //75%
	{ 
		GenColors [ 0 ] = ColorRGBDisplay(68.0,20.0,20.0);
		GenColors [ 1 ] = ColorRGBDisplay(28.0,73.0,28.0);
		GenColors [ 2 ] = ColorRGBDisplay(19.0,19.0,50.0);
		GenColors [ 3 ] = ColorRGBDisplay(75.0,75.0,33.0);
		GenColors [ 4 ] = ColorRGBDisplay(36.0,73.0,73.0);
		GenColors [ 5 ] = ColorRGBDisplay(64.0,29.0,64.0);
		GenColors [ 6 ] = ColorRGBDisplay(75.0,75.0,75.0);
		GenColors [ 7 ] = ColorRGBDisplay(0,0,0);	
	}
	ColorRGBDisplay	MeasColors [ 8 ] = 
								{	
									ColorRGBDisplay(IRELevel,0,0),
									ColorRGBDisplay(0,IRELevel,0),
									ColorRGBDisplay(0,0,IRELevel),
									ColorRGBDisplay(IRELevel,IRELevel,0),
									ColorRGBDisplay(0,IRELevel,IRELevel),
									ColorRGBDisplay(IRELevel,0,IRELevel),
									ColorRGBDisplay(IRELevel,IRELevel,IRELevel),
									ColorRGBDisplay(0,0,0)
								};

	m_binMeasure = TRUE;
	for ( i = 0; i < ( 6 + GetConfig () -> m_BWColorsToAdd ); i ++ )
	{
		if (i>0)
			GetConfig()->m_isSettling=FALSE;
		else
			doSettling = GetConfig()->m_isSettling;
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SECONDARY, i) )
		{
			bEscape = WaitForDynamicIris (FALSE);
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(MeasColors[i]);
				
				if (i<3)
					m_primariesArray[i] = measuredColor[i];
				if (i>=3&&i<6)
					m_secondariesArray[i-3] = measuredColor[i];
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[i] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				previousColor = lastColor;			
				lastColor = measuredColor[i];
				m_currentIndex = i;
				UpdateViews(pDoc, 1);
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_SECONDARY,previousColor,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}
	for(int i=0;i<3;i++)
	{
		m_primariesArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_secondariesArray[i] = measuredColor[i+3];
		if ( bUseLuxValues )
			m_secondariesArray[i].SetLuxValue ( measuredLux[i+3] );
		else
			m_secondariesArray[i].ResetLuxValue ();
	}

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_PrimeWhite = measuredColor[6];                
		if ( bUseLuxValues )
			m_PrimeWhite.SetLuxValue ( measuredLux[6] );
		else
			m_PrimeWhite.ResetLuxValue ();
	}
	else
	{
		m_PrimeWhite=noDataColor;
	}

	if ( GetConfig () -> m_BWColorsToAdd > 1 )
	{
		if (m_bOverRideBlack)
			m_OnOffBlack = m_userBlack;
		else
			m_OnOffBlack = measuredColor[7];
		if ( bUseLuxValues )
			m_OnOffBlack.SetLuxValue ( measuredLux[7] );
		else
			m_OnOffBlack.ResetLuxValue ();
	}
	else
	{
//		m_OnOffBlack=noDataColor;
	}
	GetConfig()->m_isSettling = doSettling;

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureContrast(CSensor *pSensor, CGenerator *pGenerator)
{
	int		i;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	CString	strMsg, Title;
	double	dLuxValue;

	BOOL	bUseLuxValues = TRUE;
	double	measuredLux[4];

	if(pGenerator->Init(4) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		return FALSE;
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	double BlackIRELevel=0.0;	
	double WhiteIRELevel=100.0;	
	double NearBlackIRELevel=10.0;	
	double NearWhiteIRELevel=90.0;	
	
	CColor measure;
	// Measure black for on/off contrast
	m_binMeasure = TRUE;

	if (!CheckBlackOverride())
	{
		for ( i = 0; i < 1 ; i ++ )
		{
			if( pGenerator->DisplayGray(BlackIRELevel,CGenerator::MT_CONTRAST,!bRetry ) )
			{
				bEscape = WaitForDynamicIris ();
				bRetry = FALSE;

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();

	//				if (!m_bOverRideBlack)
						measure=pSensor->MeasureColor(ColorRGBDisplay(BlackIRELevel));
				
					if ( bUseLuxValues )
					{
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 measuredLux[0] = dLuxValue;
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					}
				}

				if ( bUseLuxValues && ! bEscape )
				{
					int		nNbLoops = 0;
					BOOL	bContinue;
				
					// Measuring black: ask and reask luxmeter value until it stabilizes
					do
					{
						bContinue = FALSE;
						nNbLoops ++;
					
						StartLuxMeasure ();
					
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 if ( dLuxValue < measuredLux[0] )
								 {
									measuredLux[0] = dLuxValue;
									bContinue = TRUE;
								 }
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					} while ( bContinue && nNbLoops < 10 );
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						pSensor->Release();
						pGenerator->Release();
						return FALSE;
					}
					if(result == IDRETRY)
					{
						i--;
						bRetry = TRUE;
					}
				}
				else
				{
						m_OnOffBlack = measure;
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}
	} else
		m_OnOffBlack = m_userBlack;

	// Measure white for on/off contrast
	for ( i = 0; i < 1 ; i ++ )
	{
		if( pGenerator->DisplayGray(WhiteIRELevel,CGenerator::MT_CONTRAST,!bRetry ))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measure=pSensor->MeasureColor(ColorRGBDisplay(WhiteIRELevel));
				
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[1] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}
			}

			while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
			{
				if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
					bEscape = TRUE;
			}

			if ( bEscape )
			{
				pSensor->Release();
				pGenerator->Release();
				strMsg.LoadString ( IDS_MEASURESCANCELED );
				GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
				return FALSE;
			}

			if(!pSensor->IsMeasureValid())
			{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
				if(result == IDABORT)
				{
					pSensor->Release();
					pGenerator->Release();
					return FALSE;
				}
				if(result == IDRETRY)
				{
					i--;
					bRetry = TRUE;
				}
			}
			else
			{
				lastColor = measure;
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_CONTRAST,m_OnOffBlack,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
				else
				{
					m_OnOffWhite = measure;

					if ( bUseLuxValues )
					{
						m_OnOffBlack.SetLuxValue ( measuredLux[0] );
						m_OnOffWhite.SetLuxValue ( measuredLux[1] );
					}
					else
					{
						m_OnOffBlack.ResetLuxValue ();
						m_OnOffWhite.ResetLuxValue ();
					}
				}
			}
		}
		else
		{
			pSensor->Release();
			pGenerator->Release();
			return FALSE;
		}
	}
		
	if(pGenerator->CanDisplayAnsiBWRects())
	{
		// Measure a color for ansi contrast (black or white, don't know)
		for ( i = 0; i < 1 ; i ++ )
		{
			if( pGenerator->DisplayAnsiBWRects(FALSE) )
			{
				bEscape = WaitForDynamicIris ();

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();
						measure=pSensor->MeasureColor(ColorRGBDisplay(NearBlackIRELevel));	// Assume Black
					
					if ( bUseLuxValues )
					{
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 measuredLux[2] = dLuxValue;
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					}
				}

				if ( bUseLuxValues && ! bEscape )
				{
					int		nNbLoops = 0;
					BOOL	bContinue;
					
					// Measuring black: ask and reask luxmeter value until it stabilizes
					do
					{
						bContinue = FALSE;
						nNbLoops ++;
						
						StartLuxMeasure ();
						
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 if ( dLuxValue < measuredLux[2] )
								 {
									measuredLux[2] = dLuxValue;
									bContinue = TRUE;
								 }
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					} while ( bContinue && nNbLoops < 10 );
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
					Title.LoadString ( IDS_ERROR );
					strMsg.LoadString ( IDS_ANERROROCCURED );
					int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
						if(result == IDABORT)
						{
							pSensor->Release();
							pGenerator->Release();
							return FALSE;
						}
						if(result == IDRETRY)
							i--;
				}
				else
				{
					m_AnsiBlack = measure;
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();

				return FALSE;
			}
		}
		
		// Measure the other color for ansi contrast (black or white, don't know)
		for ( i = 0; i < 1 ; i ++ )
		{
			if( pGenerator->DisplayAnsiBWRects(TRUE) )
			{
				bEscape = WaitForDynamicIris ();

				if ( bUseLuxValues )
					StartLuxMeasure ();

				measure=pSensor->MeasureColor(ColorRGBDisplay(NearWhiteIRELevel));	// Assume White
				
				pGenerator->DisplayGray(0, CGenerator::MT_NEARBLACK, FALSE); //flush ccast

				bEscape = FALSE;
			
				if ( bUseLuxValues )
				{
					switch ( GetLuxMeasure ( & dLuxValue ) )
					{
						case LUX_NOMEASURE:
							 bUseLuxValues = FALSE;
							 break;

						case LUX_OK:
							 measuredLux[3] = dLuxValue;
							 break;

						case LUX_CANCELED:
							 bEscape = TRUE;
							 break;
					}
				}

				if ( bUseLuxValues && ! bEscape )
				{
					int		nNbLoops = 0;
					BOOL	bContinue;
					
					// Measuring black: ask and reask luxmeter value until it stabilizes
					do
					{
						bContinue = FALSE;
						nNbLoops ++;
						
						StartLuxMeasure ();
						
						switch ( GetLuxMeasure ( & dLuxValue ) )
						{
							case LUX_NOMEASURE:
								 bUseLuxValues = FALSE;
								 break;

							case LUX_OK:
								 if ( dLuxValue < measuredLux[3] )
								 {
									measuredLux[3] = dLuxValue;
									bContinue = TRUE;
								 }
								 break;

							case LUX_CANCELED:
								 bEscape = TRUE;
								 break;
						}
					} while ( bContinue && nNbLoops < 10 );
				}

				while ( PeekMessage ( & Msg, NULL, WM_KEYDOWN, WM_KEYUP, TRUE ) )
				{
					if ( Msg.message == WM_KEYDOWN && Msg.wParam == VK_ESCAPE )
						bEscape = TRUE;
				}

				if ( bEscape )
				{
					pSensor->Release();
					pGenerator->Release();
					strMsg.LoadString ( IDS_MEASURESCANCELED );
					GetColorApp()->InMeasureMessageBox ( strMsg, NULL, MB_OK | MB_ICONINFORMATION );
					return FALSE;
				}

				if(!pSensor->IsMeasureValid())
				{
				Title.LoadString ( IDS_ERROR );
				strMsg.LoadString ( IDS_ANERROROCCURED );
				int result=GetColorApp()->InMeasureMessageBox(strMsg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
					if(result == IDABORT)
					{
						pSensor->Release();
						pGenerator->Release();
						return FALSE;
					}
					if(result == IDRETRY)
						i--;
				}
				else
				{
					m_AnsiWhite = measure;

					if ( bUseLuxValues )
					{
						m_AnsiBlack.SetLuxValue ( measuredLux[2] );
						m_AnsiWhite.SetLuxValue ( measuredLux[3] );
					}
					else
					{
						m_AnsiBlack.ResetLuxValue ();
						m_AnsiWhite.ResetLuxValue ();
					}
				}
			}
			else
			{
				pSensor->Release();
				pGenerator->Release();
				return FALSE;
			}
		}

		if ( m_AnsiBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter) > m_AnsiWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter) )
		{
			// Exchange colors
			measure = m_AnsiBlack;
			m_AnsiBlack = m_AnsiWhite;
			m_AnsiWhite = measure;
		}
	}
	
	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	m_binMeasure = FALSE;
	m_isModified=TRUE;
	return TRUE;
}

double CMeasure::GetOnOffContrast ()
{
	double	black = m_OnOffBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	double	white = m_OnOffWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	
	if ( black > 0.0 && white > black )
		return ( white / black );
	else
		if (black == 0)
			return -1.0;
		else
			return -2.0;
}

double CMeasure::GetAnsiContrast ()
{
	double	black = m_AnsiBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	double	white = m_AnsiWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	
	if ( black > 0.000001 && white > black )
		return ( white / black );
	else
		if (black == 0)
			return -1.0;
		else
			return -2.0;
}

double CMeasure::GetContrastMinLum ()
{
	double	black = m_OnOffBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	if ( black > 0.000001 )
		return black;
	else
		return -1.0;
}

double CMeasure::GetContrastMaxLum ()
{
	double	white = m_OnOffWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);

	if ( white > 0.000001 )
		return white;
	else
		return -1.0;
}

void CMeasure::DeleteContrast ()
{
	m_AnsiBlack = noDataColor;
	m_AnsiWhite = noDataColor;

	m_isModified=TRUE; 
}

BOOL CMeasure::AddMeasurement(CSensor *pSensor, CGenerator *pGenerator,  CGenerator::MeasureType MT, bool isPrimary)
{
	BOOL		bDisplayColor = GetConfig () -> m_bDisplayTestColors;
	BOOL		bOk;
	COLORREF	clr;
	CColor		measuredColor;
	CString	Msg, Title;

	if ( bDisplayColor )
	{
		if(pGenerator->Init() != TRUE)
		{
			Title.LoadString ( IDS_ERROR );
			Msg.LoadString ( IDS_ERRINITGENERATOR );
			GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
			return FALSE;
		}
	}

	if(pSensor->Init(FALSE) != TRUE)
	{
		Title.LoadString ( IDS_ERROR );
		Msg.LoadString ( IDS_ERRINITSENSOR );
		GetColorApp()->InMeasureMessageBox(Msg,Title,MB_ICONERROR | MB_OK);
		if ( bDisplayColor )
			pGenerator->Release();
		return FALSE;
	}

	if ( bDisplayColor ) 
	{
		// Display test color
		clr = ( (CMainFrame *) ( AfxGetApp () -> m_pMainWnd ) ) ->m_wndTestColorWnd.m_colorPicker.GetColor ();
		clr &= 0x00FFFFFF;
		bOk = pGenerator->DisplayRGBColor(ColorRGBDisplay(clr), MT);
		if ( bOk )
			WaitForDynamicIris ( TRUE );
	}
	else
	{
        // don't know the color to measure so default value to mid gray...
		clr = RGB(128,128,128);
		bOk = TRUE;
	}

	if ( bOk )
	{
		measuredColor=pSensor->MeasureColor(ColorRGBDisplay(clr));
		if(!pSensor->IsMeasureValid())
		{
			Title.LoadString ( IDS_ERROR );
			Msg.LoadString ( IDS_ANERROROCCURED );
			int result=GetColorApp()->InMeasureMessageBox(Msg+pSensor->GetErrorString(),Title,MB_ABORTRETRYIGNORE | MB_ICONERROR);
			if(result == IDABORT)
			{
				pSensor->Release();
				if ( bDisplayColor )
					pGenerator->Release();
				return FALSE;
			}
		}
	}
	else
	{
		pSensor->Release();
		if ( bDisplayColor )
			pGenerator->Release();
		return FALSE;
	}

	CColor measurement;
	measurement = measuredColor;
	m_measurementsArray.InsertAt(m_measurementsArray.GetSize(),measurement);
	
	FreeMeasurementAppended(isPrimary);

	pSensor->Release();
	if ( bDisplayColor )
		pGenerator->Release();

	m_isModified=TRUE;
	return TRUE;
}

void CMeasure::ApplySensorAdjustmentMatrix(const Matrix& aMatrix)
{
	for(int i=0;i<m_grayMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
				if (!i && m_bOverRideBlack)
					m_grayMeasureArray[i] = m_userBlack;
				else
					m_grayMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}

	for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_nearBlackMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_nearWhiteMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_redSatMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_redSatMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_greenSatMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_blueSatMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_yellowSatMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_cyanSatMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_magentaSatMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)  // Preserve sensor values 
	{
		m_cc24SatMeasureArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<m_cc24SatMeasureArray_master.GetSize();i++)  // Preserve sensor values 
	{
		m_cc24SatMeasureArray_master[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<3;i++)
	{
		m_primariesArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<3;i++)
	{
		m_secondariesArray[i].applyAdjustmentMatrix(aMatrix);
	}
	
	if (!m_bOverRideBlack)
		m_OnOffBlack.applyAdjustmentMatrix(aMatrix);
	
	m_OnOffWhite.applyAdjustmentMatrix(aMatrix);

	m_PrimeWhite.applyAdjustmentMatrix(aMatrix);

	m_AnsiBlack.applyAdjustmentMatrix(aMatrix);
	
	m_AnsiWhite.applyAdjustmentMatrix(aMatrix);
}

BOOL CMeasure::WaitForDynamicIris ( BOOL bIgnoreEscape )
{
	BOOL bEscape = FALSE;
	DWORD nLatencyTime = (DWORD)GetConfig()->m_latencyTime;

	if ( nLatencyTime > 0 )
	{
		// Sleep nLatencyTime ms while dispatching messages
		MSG	Msg;
		DWORD dwStart = GetTickCount();
		DWORD dwNow = dwStart;

		while((!bEscape) && ((dwNow - dwStart) < nLatencyTime + 100))
		{
			Sleep(0);
			while(PeekMessage(&Msg, NULL, NULL, NULL, TRUE ))
			{
				if (!bIgnoreEscape )
				{
					if ( Msg.message == WM_KEYDOWN )
					{
						if ( Msg.wParam == VK_ESCAPE )
							bEscape = TRUE;
					}
				}

				TranslateMessage ( & Msg );
				DispatchMessage ( & Msg );
			}
			dwNow = GetTickCount();
		}
	}

	if ( GetConfig () -> m_bLatencyBeep && ! bEscape )
		MessageBeep (-1);

	return bEscape;
}
BOOL CMeasure::CheckBlackOverride ( )
{
	m_bOverRideBlack = GetConfig()->GetProfileDouble("References","Use Black Level",0);
	double YBlack = GetConfig()->GetProfileDouble("References","Manual Black Level",0);
	m_userBlack = CColor(ColorXYZ(YBlack*.95047,YBlack,YBlack*1.0883));
	return m_bOverRideBlack;
}

void CMeasure::UpdateViews ( CDataSetDoc *pDoc, int Sequence )
{
	if (pDoc)
	{
		POSITION pos = pDoc -> GetFirstViewPosition ();
		CView *pView = pDoc->GetNextView(pos);
		((CMainView*)pView)->SetSelectedColor(lastColor);
		pDoc ->SetModifiedFlag(TRUE);
		pDoc ->UpdateAllViews(NULL, UPD_REALTIME + Sequence);
	}

}

static DWORD WINAPI BkGndMeasureThreadFunc ( LPVOID lpParameter )
{
    CrashDump useInThisThread;
    try
    {
	    CMeasure *		pMeasure = (CMeasure *) lpParameter;
	    CSensor *		pSensor = pMeasure -> m_pBkMeasureSensor;	// Assume sensor is initialized
    	
	    do
	    {
		    // Wait for execution request
		    WaitForSingleObject ( pMeasure -> m_hEventRun, INFINITE );
		    ResetEvent ( pMeasure -> m_hEventRun );

		    if ( pMeasure -> m_bTerminateThread )
		    {
			    // Exit thread
			    break;
		    }

		    // Perform one measure
		    ( * pMeasure -> m_pBkMeasuredColor ) [ pMeasure -> m_nBkMeasureStep ] = pSensor -> MeasureColor ( pMeasure -> m_clrToMeasure );

		    if ( ! pSensor -> IsMeasureValid () )
		    {
			    // Register error
			    pMeasure -> m_bErrorOccurred = TRUE;
		    }

		    // Indicate measure is done
		    SetEvent ( pMeasure -> m_hEventDone );

	    } while ( TRUE );

	    // Release sensor
	    pSensor -> Release ();
    }
    catch(std::exception& e)
    {
        std::cerr << "Exception in measurement thread : " << e.what() << std::endl;
    }
    catch(...)
    {
        std::cerr << "Unexpected Exception in measurement thread" << std::endl;
    }
	return 0;
}

HANDLE CMeasure::InitBackgroundMeasures ( CSensor *pSensor, int nSteps )
{
	DWORD	dw;

	m_bTerminateThread = FALSE;
	m_bErrorOccurred = FALSE;
	m_nBkMeasureStep = 0;
	m_clrToMeasure = ColorRGBDisplay(0.0);
	m_hEventRun = NULL;
	m_hEventDone = NULL;
	
	// Initialise sensor
	if ( pSensor -> Init (TRUE) )
	{
		m_nBkMeasureStepCount = nSteps;
		m_pBkMeasureSensor = pSensor;

		m_pBkMeasuredColor = new CArray<CColor,int>;
		m_pBkMeasuredColor -> SetSize ( m_nBkMeasureStepCount );

		// Create manual event in non signaled state
		m_hEventRun = CreateEvent ( NULL, TRUE, FALSE, NULL );

		// Create manual event in non signaled state
		m_hEventDone = CreateEvent ( NULL, TRUE, FALSE, NULL );

		if ( m_hEventRun && m_hEventDone )
		{
            ResetEvent ( m_hEventDone );
            ResetEvent ( m_hEventRun );
            m_hThread = CreateThread ( NULL, 0, BkGndMeasureThreadFunc, this, 0, & dw );

			if ( ! m_hThread )
			{
				CloseHandle ( m_hEventDone );
				CloseHandle ( m_hEventRun );
				m_hEventDone = NULL;
				m_hEventRun = NULL;
			}
		}

		if ( ! m_hThread )
		{
			delete m_pBkMeasuredColor;
			m_pBkMeasuredColor = NULL;
			m_pBkMeasureSensor = NULL;

			pSensor -> Release ();
		}
	}

	// Return NULL when initialization failed, or the event to wait for after measure request
	return m_hEventDone;
}

BOOL CMeasure::BackgroundMeasureColor ( int nCurStep, const ColorRGBDisplay& aRGBValue )
{
	if ( m_hThread )
	{
		// Reset event indicating that a measure is ready
		ResetEvent ( m_hEventDone );
		ResetEvent ( m_hEventRun );
		
		// Request a new background measure
		m_nBkMeasureStep = nCurStep;
		m_clrToMeasure = aRGBValue;
		
		SetEvent ( m_hEventRun );
		return TRUE;
	}

	return FALSE;
}

void CMeasure::CancelBackgroundMeasures ()
{
	if ( m_hThread )
	{
		// Terminate thread
		m_bTerminateThread = TRUE;
		SetEvent ( m_hEventRun );

		// Close thread
		WaitForSingleObject ( m_hThread, INFINITE );
		CloseHandle ( m_hThread );
		m_hThread = NULL;

		// Close events
		CloseHandle ( m_hEventDone );
		CloseHandle ( m_hEventRun );
		m_hEventDone = NULL;
		m_hEventRun = NULL;

		// Delete temporary list of measures
		delete m_pBkMeasuredColor;
		m_pBkMeasuredColor = NULL;

		m_pBkMeasureSensor = NULL;
	}
}

BOOL CMeasure::ValidateBackgroundGrayScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetGrayScaleSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			if (!i && m_bOverRideBlack)
				m_grayMeasureArray[i] = m_userBlack;
			else
				m_grayMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_grayMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_grayMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundSingleMeasurement ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		CColor measurement;

		measurement = (*m_pBkMeasuredColor)[0];

		if ( bUseLuxValues )
			measurement.SetLuxValue ( pLuxValues[0] );
		else
			measurement.ResetLuxValue ();

		m_measurementsArray.InsertAt(m_measurementsArray.GetSize(),measurement);
		
		FreeMeasurementAppended (false);
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

void CMeasure::FreeMeasurementAppended(bool isPrimary)
{
	if ( GetConfig () -> m_bDetectPrimaries && isPrimary )
	{
		CColor LastMeasure;

		int	n = GetMeasurementsSize();
		if ( n > 0 )
		{
			LastMeasure=GetMeasurement(n-1);

			if ( LastMeasure.GetDeltaxy ( GetRefPrimary(0), GetColorReference()) < 0.03 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetRedPrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetRefPrimary(1), GetColorReference() ) < 0.03 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetGreenPrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetRefPrimary(2), GetColorReference() ) < 0.03 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetBluePrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy (GetRefSecondary(0), GetColorReference() ) < 0.03)
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetYellowSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetRefSecondary(1), GetColorReference() ) < 0.03)
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetCyanSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetRefSecondary(2), GetColorReference() ) < 0.03)
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetMagentaSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetColorReference().GetWhite(), GetColorReference() ) < 0.03 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
//				SetPrimeWhite ( m_measurementsArray[n-1] ); //do not futz with white reference during measures
			}
		}
	}
}

BOOL CMeasure::ValidateBackgroundNearBlack ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetNearBlackScaleSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			if (!i && m_bOverRideBlack)
				m_nearBlackMeasureArray[i] = m_userBlack;
			else
				m_nearBlackMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_nearBlackMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_nearBlackMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundNearWhite ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetNearWhiteScaleSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_nearWhiteMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_nearWhiteMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_nearWhiteMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundPrimaries ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		ASSERT ( m_nBkMeasureStepCount == 3 || m_nBkMeasureStepCount == 4 || m_nBkMeasureStepCount == 5 );
		for ( int i = 0; i < 3 ; i++ )
		{
			m_primariesArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		if ( m_nBkMeasureStepCount >= 4 )
		{
			// Store reference white for primaries
			m_PrimeWhite = (*m_pBkMeasuredColor)[3];

			if ( bUseLuxValues )
				m_PrimeWhite.SetLuxValue ( pLuxValues[3] );
			else
				m_PrimeWhite.ResetLuxValue ();
		}
		else
			m_PrimeWhite = noDataColor;

		if ( m_nBkMeasureStepCount >= 5 )
		{
			if (m_bOverRideBlack)
				m_OnOffBlack = m_userBlack;
			else
				m_OnOffBlack = (*m_pBkMeasuredColor)[4];

			if ( bUseLuxValues )
				m_OnOffBlack.SetLuxValue ( pLuxValues[4] );
			else
				m_OnOffBlack.ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundSecondaries ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		ASSERT ( m_nBkMeasureStepCount == 6 || m_nBkMeasureStepCount == 7 || m_nBkMeasureStepCount == 8 );
		for ( int i = 0; i < 3 ; i++ )
		{
			m_primariesArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_secondariesArray[i] = (*m_pBkMeasuredColor)[i+3];

			if ( bUseLuxValues )
				m_secondariesArray[i].SetLuxValue ( pLuxValues[i+3] );
			else
				m_secondariesArray[i].ResetLuxValue ();
		}

		if ( m_nBkMeasureStepCount >= 7 )
		{
			// Store reference white for primaries
			m_PrimeWhite = (*m_pBkMeasuredColor)[6];

			if ( bUseLuxValues )
				m_PrimeWhite.SetLuxValue ( pLuxValues[6] );
			else
				m_PrimeWhite.ResetLuxValue ();
		}
		else
			m_PrimeWhite = noDataColor;

		if ( m_nBkMeasureStepCount >= 8 )
		{
			if (m_bOverRideBlack)
				m_OnOffBlack = m_userBlack;				
			else
				m_OnOffBlack = (*m_pBkMeasuredColor)[7];

			if ( bUseLuxValues )
				m_OnOffBlack.SetLuxValue ( pLuxValues[7] );
			else
				m_OnOffBlack.ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundGrayScaleAndColors ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetGrayScaleSize(m_nBkMeasureStepCount-6);
		for ( int i = 0; i < m_nBkMeasureStepCount-6 ; i++ )
		{
			if (!i && m_bOverRideBlack)
				m_grayMeasureArray[i] = m_userBlack;
			else
				m_grayMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_grayMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_grayMeasureArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_primariesArray[i] = (*m_pBkMeasuredColor)[m_nBkMeasureStepCount-6+i];

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-6+i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_secondariesArray[i] = (*m_pBkMeasuredColor)[m_nBkMeasureStepCount-3+i];

			if ( bUseLuxValues )
				m_secondariesArray[i].SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-3+i] );
			else
				m_secondariesArray[i].ResetLuxValue ();
		}

		m_PrimeWhite = (*m_pBkMeasuredColor)[m_nBkMeasureStepCount-7];
		if (m_bOverRideBlack)
			m_OnOffBlack = m_userBlack;
		else
			m_OnOffBlack = (*m_pBkMeasuredColor)[0];

		if ( bUseLuxValues )
		{
			m_PrimeWhite.SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-7] );
			m_OnOffBlack.SetLuxValue ( pLuxValues[0] );
		}
		else
		{
			m_PrimeWhite.ResetLuxValue ();
//			m_OnOffBlack.ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundRedSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_redSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_redSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_redSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundGreenSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_greenSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_greenSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_greenSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundBlueSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_blueSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_blueSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_blueSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundYellowSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_yellowSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_yellowSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_yellowSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundCyanSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_cyanSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_cyanSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_cyanSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundMagentaSatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i < m_nBkMeasureStepCount ; i++ )
		{
			m_magentaSatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_magentaSatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_magentaSatMeasureArray[i].ResetLuxValue ();
		}
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

BOOL CMeasure::ValidateBackgroundCC24SatScale ( BOOL bUseLuxValues, double * pLuxValues )
{
	BOOL	bOk = FALSE;

	if ( ! m_bErrorOccurred )
	{
		bOk = TRUE;
		m_isModified=TRUE;

//		SetSaturationSize(m_nBkMeasureStepCount);
		for ( int i = 0; i<m_cc24SatMeasureArray.GetSize() ; i++ )
		{
			m_cc24SatMeasureArray[i] = (*m_pBkMeasuredColor)[i];

			if ( bUseLuxValues )
				m_cc24SatMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_cc24SatMeasureArray[i].ResetLuxValue ();
		}
		int iCC=GetConfig()->m_CCMode;
		if (iCC < 19)
			for (int i=0+100*iCC;i<100*(iCC+1);i++)
					m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-iCC*100];
		else if (iCC == 19) 
			for (int i=1900;i<1900+250;i++)
					m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-1900];
		else if (iCC == 20) 
			for (int i=1900+250;i<1900+250+500;i++)
					m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(1900+250)];
		else if (iCC == 21) 
			for (int i=1900+250+500;i<1900+250+500+1000;i++)
					m_cc24SatMeasureArray_master[i] = m_cc24SatMeasureArray[i-(1900+250+500)];
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

CColor CMeasure::GetGray(int i) const 
{
	return m_grayMeasureArray[i]; 
} 

CColor CMeasure::GetNearBlack(int i) const 
{ 
	return m_nearBlackMeasureArray[i]; 
} 

CColor CMeasure::GetNearWhite(int i) const 
{ 
	return m_nearWhiteMeasureArray[i]; 
} 

CColor CMeasure::GetRedSat(int i) const 
{ 
	return m_redSatMeasureArray[i]; 
} 

CColor CMeasure::GetGreenSat(int i) const 
{ 
	return m_greenSatMeasureArray[i]; 
} 

CColor CMeasure::GetBlueSat(int i) const 
{ 
	return m_blueSatMeasureArray[i]; 
} 

CColor CMeasure::GetYellowSat(int i) const 
{ 
	return m_yellowSatMeasureArray[i]; 
} 

CColor CMeasure::GetCyanSat(int i) const 
{ 
	return m_cyanSatMeasureArray[i]; 
} 

CColor CMeasure::GetMagentaSat(int i) const 
{ 
	return m_magentaSatMeasureArray[i]; 
} 

CColor CMeasure::GetCC24Sat(int i) const 
{ 
	int iCC=GetConfig()->m_CCMode;

	if (m_bpreV10 || m_binMeasure)
		return m_cc24SatMeasureArray[i]; 
	else
	return m_cc24SatMeasureArray_master[i + (iCC<=19?(iCC * 100):(iCC==20?1900+250:1900+250+500))];  //index increments by 100 until slot 19 (then 250 & 500)
} 

CString CMeasure::GetCCStr() const
{
	CString mStr("Color Checker sweeps active:\r\n");
	char *sweeps[22]={"GCD Classic \r\n","MCD Classic \r\n","Pantone Skin \r\n",
		"CalMAN Classic \r\n",
		"CalMAN Skin \r\n",
		"Chromapure Skin \r\n",
		"CalMAN SG \r\n",
		"CalMAN 10 pt Lum. \r\n",
		"CalMAN 4 pt Lum. \r\n",
		"CalMAN 5 pt Lum. \r\n",
		"CalMAN 10 pt Lum. \r\n",
		"CalMAN 4 pt Sat.(100AMP) \r\n",
		"CalMAN 4 pt Sat.(75AMP) \r\n",
		"CalMAN 5 pt Sat.(100AMP) \r\n",
		"CalMAN 5 pt Sat.(75AMP) \r\n",
		"CalMAN 10 pt Sat.(100AMP) \r\n",
		"CalMAN 10 pt Sat.(75AMP) \r\n",
		"CalMAN near black \r\n",
		"CalMan dynamic range \r\n",
		"Random 250 \r\n",
		"Random 500 \r\n",
		"User\r\n"
	};
	for (int i=0;i<=22;i++)
	{
		if (i<=19)
			if (m_cc24SatMeasureArray_master[i*100].isValid())
				mStr+=sweeps[i];
		if (i==20)
			if (m_cc24SatMeasureArray_master[1900+250].isValid())
				mStr+=sweeps[i];
		if (i==21)
			if (m_cc24SatMeasureArray_master[1900+250+500].isValid())
				mStr+=sweeps[i];
	}
	return mStr+="\r\n";
}

CColor CMeasure::GetPrimary(int i) const 
{ 
	return m_primariesArray[i]; 
} 

CColor CMeasure::GetRedPrimary() const 
{ 
	return GetPrimary(0);
} 

CColor CMeasure::GetGreenPrimary() const 
{ 
	return GetPrimary(1);
} 

CColor CMeasure::GetBluePrimary() const 
{ 
	return GetPrimary(2);
} 

CColor CMeasure::GetSecondary(int i) const 
{ 
	return m_secondariesArray[i]; 
} 

CColor CMeasure::GetYellowSecondary() const 
{ 
	return GetSecondary(0);
} 

CColor CMeasure::GetCyanSecondary() const 
{ 
	return GetSecondary(1);
} 

CColor CMeasure::GetMagentaSecondary() const 
{ 
	return GetSecondary(2);
} 

CColor CMeasure::GetAnsiBlack() const 
{ 
	CColor clr;

	clr = m_AnsiBlack; 

	return clr;
} 

CColor CMeasure::GetAnsiWhite() const 
{ 
	CColor clr;

	clr = m_AnsiWhite; 

	return clr;
} 

CColor CMeasure::GetOnOffBlack() const 
{ 
	CColor clr;

	clr = m_OnOffBlack; 

	return clr;
} 

CColor CMeasure::GetOnOffWhite() const 
{ 
	CColor clr;

	clr = m_OnOffWhite; 

	return clr;
} 

CColor CMeasure::GetPrimeWhite() const 
{ 
	CColor clr;

	clr = m_PrimeWhite; 

	return clr;
} 

CColor CMeasure::GetMeasurement(int i) const 
{ 
	return m_measurementsArray[i]; 
} 

void CMeasure::AppendMeasurements(const CColor & aColor, bool isPrimary) 
{
	if (m_measurementsArray.GetSize() >= m_nbMaxMeasurements )
		m_measurementsArray.RemoveAt(0,1); 
	
	// Using a pointer here is a workaround for a VC++ 6.0 bug, which uses Matrix copy constructor instead of CColor one when using a const CColor ref.
	CColor * pColor = (CColor *) & aColor;
	m_measurementsArray.InsertAt(m_measurementsArray.GetSize(),*pColor); 

	m_isModified=TRUE; 

	FreeMeasurementAppended(isPrimary); 
}

CColor CMeasure::GetRefPrimary(int i) const
{
    double gamma=(GetConfig()->m_useMeasuredGamma)?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);
	bool isSpecial = (GetColorReference().m_standard==HDTVa||GetColorReference().m_standard==CC6||GetColorReference().m_standard==HDTVb);
	CColorReference cRef=GetColorReference();
	CColor	aColor,aColorr,aColorg,aColorb,White,Black;
	aColorr.SetXYZValue (cRef.GetRed());
	aColorg.SetXYZValue (cRef.GetGreen());
	aColorb.SetXYZValue (cRef.GetBlue());
	ColorRGB rgbr=aColorr.GetRGBValue ( GetColorReference() );
	ColorRGB rgbg=aColorg.GetRGBValue ( GetColorReference() );
	ColorRGB rgbb=aColorb.GetRGBValue ( GetColorReference() );
	double r,g,b;
    r=rgbr[0];
    g=rgbr[1];
    b=rgbr[2];
    if (CMeasure::GetGray(0).isValid())
    {
        White = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );
	    Black = CMeasure::GetGray ( 0 );
    }
    aColor.SetRGBValue(ColorRGB(r,g,b), GetColorReference() );
	int mode = GetConfig()->m_GammaOffsetType;
	if (GetConfig()->m_colorStandard == sRGB) mode = 7;
	if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
       gamma = log(getEOTF(pow(aColor.GetY(),1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(pow(aColor.GetY(),1.0/2.22));
    if (isSpecial)
    {
        r=(r<=0.0||r>=1.0)?min(max(r,0),1):pow(pow(r,1.0/2.22),gamma);
        g=(g<=0.0||g>=1.0)?min(max(g,0),1):pow(pow(g,1.0/2.22),gamma);
        b=(b<=0.0||b>=1.0)?min(max(b,0),1):pow(pow(b,1.0/2.22),gamma);
    }
    aColorr.SetRGBValue (ColorRGB(r,g,b), GetColorReference());	

    r=rgbg[0];
    g=rgbg[1];
    b=rgbg[2];
    aColor.SetRGBValue(ColorRGB(r,g,b), GetColorReference() );
	if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
       gamma = log(getEOTF(pow(aColor.GetY(),1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(pow(aColor.GetY(),1.0/2.22));
    if (isSpecial)
    {
        r=(r<=0.0||r>=1.0)?min(max(r,0),1):pow(pow(r,1.0/2.22),gamma);
        g=(g<=0.0||g>=1.0)?min(max(g,0),1):pow(pow(g,1.0/2.22),gamma);
        b=(b<=0.0||b>=1.0)?min(max(b,0),1):pow(pow(b,1.0/2.22),gamma);
    }
    aColorg.SetRGBValue (ColorRGB(r,g,b), GetColorReference());	

    r=rgbb[0];
    g=rgbb[1];
    b=rgbb[2];
	if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
    if (GetConfig()->m_GammaOffsetType == 4 && White.isValid() && Black.isValid())
       gamma = log(getEOTF(pow(aColor.GetY(),1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(pow(aColor.GetY(),1.0/2.22));
    if (isSpecial)
    {
        r=(r<=0.0||r>=1.0)?min(max(r,0),1):pow(pow(r,1.0/2.22),gamma);
        g=(g<=0.0||g>=1.0)?min(max(g,0),1):pow(pow(g,1.0/2.22),gamma);
        b=(b<=0.0||b>=1.0)?min(max(b,0),1):pow(pow(b,1.0/2.22),gamma);
    }
    aColorb.SetRGBValue (ColorRGB(r,g,b), GetColorReference());	

    switch ( i )
	{
		case 0:	// red
			return  isSpecial?aColorr.GetXYZValue():cRef.GetRed();
			 break;

		case 1:	// green
			return  isSpecial?aColorg.GetXYZValue():cRef.GetGreen();
			 break;

		case 2:	// blue
			return  isSpecial?aColorb.GetXYZValue():cRef.GetBlue();
			 break;
	}

	// Cannot execute this if "i" is OK.
	ASSERT(0);
	return noDataColor;
}

CColor CMeasure::GetRefSecondary(int i) const
{
    double gamma=(GetConfig()->m_useMeasuredGamma)?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);

    bool isSpecial = (GetColorReference().m_standard==HDTVa||GetColorReference().m_standard==CC6||GetColorReference().m_standard==HDTVb);
	CColorReference cRef=GetColorReference();
	CColor	aColor,aColory,aColorc,aColorm,White,Black;
	aColory.SetXYZValue (cRef.GetYellow());
	aColorc.SetXYZValue (cRef.GetCyan());
	aColorm.SetXYZValue (cRef.GetMagenta());
	ColorRGB rgby=aColory.GetRGBValue ( GetColorReference() );
	ColorRGB rgbc=aColorc.GetRGBValue ( GetColorReference() );
	ColorRGB rgbm=aColorm.GetRGBValue ( GetColorReference() );
    if (CMeasure::GetGray(0).isValid())
    {
        White = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );
	    Black = CMeasure::GetGray ( 0 );
    }	
    double r,g,b;
    r=rgby[0];
    g=rgby[1];
    b=rgby[2];
    aColor.SetRGBValue(ColorRGB(r,g,b), GetColorReference() );
	int mode = GetConfig()->m_GammaOffsetType;
	if (GetConfig()->m_colorStandard == sRGB) mode = 7;
	if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
       gamma = log(getEOTF(pow(aColor.GetY(),1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(pow(aColor.GetY(),1.0/2.22));
    if (isSpecial)
    {
        r=(r<=0.0||r>=1.0)?min(max(r,0),1):pow(pow(r,1.0/2.22),gamma);
        g=(g<=0.0||g>=1.0)?min(max(g,0),1):pow(pow(g,1.0/2.22),gamma);
        b=(b<=0.0||b>=1.0)?min(max(b,0),1):pow(pow(b,1.0/2.22),gamma);
    }
    aColory.SetRGBValue (ColorRGB(r,g,b), GetColorReference());	

    r=rgbc[0];
    g=rgbc[1];
    b=rgbc[2];
    aColor.SetRGBValue(ColorRGB(r,g,b), GetColorReference() );
	if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
       gamma = log(getEOTF(pow(aColor.GetY(),1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(pow(aColor.GetY(),1.0/2.22));
    if (isSpecial)
    {
        r=(r<=0.0||r>=1.0)?min(max(r,0),1):pow(pow(r,1.0/2.22),gamma);
        g=(g<=0.0||g>=1.0)?min(max(g,0),1):pow(pow(g,1.0/2.22),gamma);
        b=(b<=0.0||b>=1.0)?min(max(b,0),1):pow(pow(b,1.0/2.22),gamma);
    }
    aColorc.SetRGBValue (ColorRGB(r,g,b), GetColorReference());	

    r=rgbm[0];
    g=rgbm[1];
    b=rgbm[2];
    aColor.SetRGBValue(ColorRGB(r,g,b), GetColorReference() );
	if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
       gamma = log(getEOTF(pow(aColor.GetY(),1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(pow(aColor.GetY(),1.0/2.22));
    if (isSpecial)
    {
        r=(r<=0.0||r>=1.0)?min(max(r,0),1):pow(pow(r,1.0/2.22),gamma);
        g=(g<=0.0||g>=1.0)?min(max(g,0),1):pow(pow(g,1.0/2.22),gamma);
        b=(b<=0.0||b>=1.0)?min(max(b,0),1):pow(pow(b,1.0/2.22),gamma);
    }
    aColorm.SetRGBValue (ColorRGB(r,g,b), GetColorReference());	

	switch ( i )
	{
		case 0: // Yellow
			return isSpecial?aColory.GetXYZValue():cRef.GetYellow();
			 break;

		case 1:	// Cyan
			return isSpecial?aColorc.GetXYZValue():cRef.GetCyan();
			 break;

		case 2:	// Magenta
			return isSpecial?aColorm.GetXYZValue():cRef.GetMagenta();
			 break;
	}

	// Cannot execute this if "i" is OK.
	ASSERT(0);
	return noDataColor;
}

CColor CMeasure::GetRefSat(int i, double sat_percent, bool special) const
{
	CColor	refColor;
	ColorxyY	refWhite(GetColorReference().GetWhite());
	double	x, y;
	double	xstart = refWhite[0];
	double	ystart = refWhite[1];
	double	YLuma;
	double Intensity=GetConfig()->GetProfileInt("GDIGenerator","Intensity",100) / 100.;
	CColor pRef[3];
	pRef[0].SetxyYValue(ColorxyY(0.6400, 0.3300));
	pRef[1].SetxyYValue(ColorxyY(0.3000, 0.6000));
	pRef[2].SetxyYValue(ColorxyY(0.1500, 0.0600));
	CColor sRef[3];
	sRef[0].SetxyYValue(ColorxyY(0.4193, 0.5053));
	sRef[1].SetxyYValue(ColorxyY(0.2246, 0.3287));
	sRef[2].SetxyYValue(ColorxyY(0.3209, 0.1542));
	int m_cRef=GetColorReference().m_standard;

	//display rec709 sat points in special colorspace modes	
	if (!special)
	{
		if ( i < 3 )
			refColor = GetRefPrimary(i);
		else
			refColor = GetRefSecondary(i-3);
	}
	else
	{
		if ( i < 3 )
			refColor = pRef[i];
		else
			refColor = sRef[i-3];
	}
		switch (i)
		{
		case 0:
			YLuma = CColorReference(special?HDTV:GetColorReference()).GetRedReferenceLuma(true);
			break;
		case 1:
			YLuma = CColorReference(special?HDTV:GetColorReference()).GetGreenReferenceLuma(true);
			break;
		case 2:
			YLuma = CColorReference(special?HDTV:GetColorReference()).GetBlueReferenceLuma(true);
			break;
		case 3:
			YLuma = CColorReference(special?HDTV:GetColorReference()).GetYellowReferenceLuma(true);
			break;
		case 4:
			YLuma = CColorReference(special?HDTV:GetColorReference()).GetCyanReferenceLuma(true);
			break;
		case 5:
			YLuma = CColorReference(special?HDTV:GetColorReference()).GetMagentaReferenceLuma(true);
			break;
		}
	
	double	xend = refColor.GetxyYValue()[0];
	double	yend = refColor.GetxyYValue()[1];

	x = xstart + ( (xend - xstart) * sat_percent );
	y = ystart + ( (yend - ystart) * sat_percent );

	CColor	aColor;
    CColor White = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );
	CColor Black = CMeasure::GetGray ( 0 );
    double gamma=GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);

	aColor.SetxyYValue (x, y, YLuma );

	if (sat_percent < 1 )
	{
		int mode = GetConfig()->m_GammaOffsetType;
		if (GetConfig()->m_colorStandard == sRGB) mode = 7;
		if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
		   gamma = log(getEOTF(pow(aColor.GetY() * pow(Intensity,2.22),1.0/2.22),White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(pow(aColor.GetY() * pow(Intensity,2.22),1.0/2.22));

		ColorRGB rgb;
		if (!special)
			rgb=aColor.GetRGBValue (GetColorReference());
		else
			rgb=aColor.GetRGBValue(CColorReference(HDTV));
		double r=rgb[0],g=rgb[1],b=rgb[2];

		r=(r<=0||r>=1)?min(max(r,0),1):pow(pow(r,1.0/2.22),gamma);
		g=(g<=0||g>=1)?min(max(g,0),1):pow(pow(g,1.0/2.22),gamma);
		b=(b<=0||b>=1)?min(max(b,0),1):pow(pow(b,1.0/2.22),gamma);

		if (!special)
			aColor.SetRGBValue (ColorRGB(r,g,b), GetColorReference());	
		else
			aColor.SetRGBValue (ColorRGB(r,g,b), CColorReference(HDTV));	
	}

	return aColor;
}

CColor CMeasure::GetRefCC24Sat(int i) const
{
	CColor aColor;
	int m_cRef=GetColorReference().m_standard;
    CColorReference cRef = GetColorReference();
	CColor ccRef;
    ColorRGB RGB[1000];
	switch (GetConfig()->m_CCMode)
	{
//GCD
    	case GCD:
		{
            RGB[0] = ColorRGB(0, 0, 0);
			RGB[1] = ColorRGB(.6210,.6210,.6210) ;
			RGB[2] = ColorRGB(.7306,.7306,.7306) ;
			RGB[3] = ColorRGB(.8219,.8219,.8219) ;
			RGB[4] = ColorRGB(.8995,.8995,.8995) ;
			RGB[5] = ColorRGB(1.0,1.0,1.0) ;
			RGB[6] = ColorRGB(.452,.3196,.2603);
			RGB[7] = ColorRGB(.758,.589,.5114);
			RGB[8] = ColorRGB(.3699,.4795,.6119);
			RGB[9] = ColorRGB(.3516,.4201,.2603);
			RGB[10] = ColorRGB(.5114,.5023,.6895);
			RGB[11] = ColorRGB(.3881,.7397,.6621);
			RGB[12] = ColorRGB(.8493,.4703,.1598);
			RGB[13] = ColorRGB(.2922,.3607,.6393);
			RGB[14] = ColorRGB(.7580,.3288,.3790);
			RGB[15] = ColorRGB(.3607,.2420,.4201);
			RGB[16] = ColorRGB(.6210,.7306,.2511);
			RGB[17] = ColorRGB(.8995,.6301,.1781);
			RGB[18] = ColorRGB(.2009,.2420,.5890);
			RGB[19] = ColorRGB(.2785,.5799,.2785);
			RGB[20] = ColorRGB(.6895,.1918,.2283);
			RGB[21] = ColorRGB(.9315,.7808,.1279);
			RGB[22] = ColorRGB(.7306,.3288,.5708);
			RGB[23] = ColorRGB(  0.0, .5205,.6393);
		break;
		}
//MCD
    	case MCD:
		{
			RGB[0] = ColorRGB(.21,.2055,.21);
			RGB[1] = ColorRGB(.3288,.3288,.3288);
			RGB[2] = ColorRGB(.4749,.4749,.4703);
			RGB[3] = ColorRGB(.6256,.6256,.6256);
			RGB[4] = ColorRGB(.7854,.7854,.7808);
			RGB[5] = ColorRGB(.9498,.9452,.9269);
			RGB[6] = ColorRGB(.4474,.3151,.2603);
			RGB[7] = ColorRGB(.7580,.5845,.5068);
			RGB[8] = ColorRGB(.3699,.4795,.6073);
			RGB[9] = ColorRGB(.3470,.4247,.2648);
			RGB[10] = ColorRGB(.5068,.5022,.6849);
			RGB[11] = ColorRGB(.3927,.7397,.6621);
			RGB[12] = ColorRGB(.8447,.4749,.1644);
			RGB[13] = ColorRGB(.2877,.3562,.6438);
			RGB[14] = ColorRGB(.7580,.3333,.3836);
			RGB[15] = ColorRGB(.3607,.2420,.4201);
			RGB[16] = ColorRGB(.6210,.7306,.2466);
			RGB[17] = ColorRGB(.8995,.6347,.1826);
			RGB[18] = ColorRGB(.1963,.2420,.5936);
			RGB[19] = ColorRGB(.2831,.5799,.2785);
			RGB[20] = ColorRGB(.6895,.1918,.2283);
			RGB[21] = ColorRGB(.9315,.7808,.1279);
			RGB[22] = ColorRGB(.7261,.3242,.5753);
			RGB[23] = ColorRGB(.1187,.5160,.5982);
		break;
		}
		//CalMAN classic steps
	    case CMC:
		{
			RGB[0] = ColorRGB(1, 1, 1 );
			RGB[1] = ColorRGB(0.8995, 0.8995, 0.8995 );
			RGB[2] = ColorRGB(0.8329, 0.8329, 0.8329 );
			RGB[3] = ColorRGB(0.7306, 0.7306, 0.7306 );
			RGB[4] = ColorRGB(0.6210, 0.6210, 0.6210 );
			RGB[5] = ColorRGB(0.0, 0.0, 0.0 );
			RGB[6] = ColorRGB(0.4521,	0.3196,	0.2603);
			RGB[7] = ColorRGB(0.7580,	0.5890,	0.5114);
			RGB[8] = ColorRGB(  0.3699,	0.4795,	0.6119);
			RGB[9] = ColorRGB(  0.3516,	0.4201,	0.2603);
			RGB[10] = ColorRGB(  0.5114,	0.5023,	0.6895);
			RGB[11] = ColorRGB(  0.3881,	0.7397,	0.6621);
			RGB[12] = ColorRGB(  0.8493,	0.4703,	0.1598);
			RGB[13] = ColorRGB(  0.2922,	0.3607,	0.6393);
			RGB[14] = ColorRGB(  0.7580,	0.3288,	0.3790);
			RGB[15] = ColorRGB(  0.3607,	0.2420,	0.4201);
			RGB[16] = ColorRGB(  0.6210,	0.7306,	0.2511);
			RGB[17] = ColorRGB(  0.8995,	0.6301,	0.1781);
			RGB[18] = ColorRGB(  0.2009,	0.2420,	0.5890);
			RGB[19] = ColorRGB(  0.2785,	0.5799,	0.2785);
			RGB[20] = ColorRGB(  0.6895,	0.1918,	0.2283);
			RGB[21] = ColorRGB(  0.9315,	0.7808,	0.1279);
			RGB[22] = ColorRGB(  0.7306,	0.3288,	0.5708);
			RGB[23] = ColorRGB(  0.0000,	0.5205,	0.6393);
		break;		
        }
	    case CMS:
		{
			RGB[0] = ColorRGB(1.0000,	1.0000,	1.0000);
			RGB[1] = ColorRGB(0.0000,	0.0000,	0.0000);
			RGB[2] = ColorRGB(0.4384,	0.2511,	0.1507);
			RGB[3] = ColorRGB(0.7991,	0.5388,	0.4018);
			RGB[4] = ColorRGB(1.0000,	0.7808,	0.5982);
			RGB[5] = ColorRGB(1.0000,	0.7808,	0.6712);
			RGB[6] = ColorRGB(0.9680,	0.6712,	0.4886);
			RGB[7] = ColorRGB(0.7808,	0.5479,	0.3607);
			RGB[8] = ColorRGB( 0.5616,	0.3607,	0.2009);
			RGB[9] = ColorRGB(0.8082,	0.5890,	0.4521);
			RGB[10] = ColorRGB( 0.6301,	0.3379,	0.1279);
			RGB[11] = ColorRGB( 0.8402,	0.5205,	0.3607);
			RGB[12] = ColorRGB( 0.8219,	0.5388,	0.4110);
			RGB[13] = ColorRGB( 0.9817,	0.5982,	0.4521);
			RGB[14] = ColorRGB( 0.7808,	0.5616,	0.4201);
			RGB[15] = ColorRGB( 0.7900,	0.5479,	0.4201);
			RGB[16] = ColorRGB( 0.7991,	0.5616,	0.4110);
			RGB[17] = ColorRGB( 0.4795,	0.2922,	0.1507);
			RGB[18] = ColorRGB( 0.8493,	0.5479,	0.3699);
		break;		
        }
	    case CPS:
		{
			RGB[0] = ColorRGB(1, 1, 1 );
			RGB[1] = ColorRGB(0.8447,	0.4932,	0.3425);
			RGB[2] = ColorRGB(0.7900,	0.5525,	0.4840);
			RGB[3] = ColorRGB(0.9361,	0.6758,	0.5799);
			RGB[4] = ColorRGB(0.9452,	0.6119,	0.5297);
			RGB[5] = ColorRGB(0.7534,	0.5616,	0.4292);
			RGB[6] = ColorRGB(0.7489,	0.5708,	0.4932);
			RGB[7] = ColorRGB(0.5525,	0.3653,	0.2466);
			RGB[8] = ColorRGB(0.7626,	0.5662,	0.4977);
			RGB[9] = ColorRGB(0.7580,	0.5890,	0.5114);
			RGB[10] = ColorRGB(0.7717,	0.5662,	0.4886);
			RGB[11] = ColorRGB(0.6210,	0.3425,	0.1781);
			RGB[12] = ColorRGB(0.4703,	0.2968,	0.1872);
			RGB[13] = ColorRGB(0.8174,	0.5297,	0.4247);
			RGB[14] = ColorRGB(0.8265,	0.5616,	0.4384);
			RGB[15] = ColorRGB(0.9315,	0.6849,	0.4840);
			RGB[16] = ColorRGB(0.8174,	0.6027,	0.4247);
			RGB[17] = ColorRGB(0.4521,	0.3196,	0.2648);
			RGB[18] = ColorRGB(0.7626,	0.5890,	0.5114);
		break;		
        }
        //Pantone skin set
	    case SKIN:
		{
			RGB[0] = ColorRGB(1,0.8745,0.7686);
			RGB[1] = ColorRGB(0.9412,0.8353,0.7451);
			RGB[2] = ColorRGB(0.9333,0.8078,0.7020);
			RGB[3] = ColorRGB(0.8824,0.7216,0.6000);
			RGB[4] = ColorRGB(0.8980,0.7608,0.5961);
			RGB[5] = ColorRGB(1,0.8627,0.6980);
			RGB[6] = ColorRGB(0.8980,0.7215,0.5608);
			RGB[7] = ColorRGB(0.8980,0.6274,0.4510);
			RGB[8] = ColorRGB(0.9059,0.6196,0.4275);
			RGB[9] = ColorRGB(0.8588,0.5647,0.3961);
			RGB[10] = ColorRGB(0.8078,0.5882,0.4863);
			RGB[11] = ColorRGB(0.7765,0.4706,0.3373);
			RGB[12] = ColorRGB(0.7294,0.4235,0.2863);
			RGB[13] = ColorRGB(0.6471,0.4471,0.3411);
			RGB[14] = ColorRGB(0.9412,0.7843,0.7882);
			RGB[15] = ColorRGB(0.8666,0.6588,0.6275);
			RGB[16] = ColorRGB(0.7255,0.4863,0.4275);
			RGB[17] = ColorRGB(0.6588,0.4588,0.4235);
			RGB[18] = ColorRGB(0.6784,0.3922,0.3216);
			RGB[19] = ColorRGB(0.3608,0.2196,0.2118);
			RGB[20] = ColorRGB(0.7961,0.5176,0.2588);
			RGB[21] = ColorRGB(0.7412,0.4471,0.2353);
			RGB[22] = ColorRGB(0.4392,0.2549,0.2235);
			RGB[23] = ColorRGB(0.6392,0.5255,0.4157);
            break;
        }
	    case AXIS:
		{
			int j;
			RGB[0] = ColorRGB(0,0,0);
			for (j=0;j<10;j++) {RGB[j+1] = ColorRGB((j+1) * 0.1,(j+1) * 0.1,(j+1) * 0.1);}
			for (j=0;j<10;j++) {RGB[j+11] = ColorRGB((j+1) * 0.1,0.0,0.0);}
			for (j=0;j<10;j++) {RGB[j+21] = ColorRGB(0.0, (j+1) * 0.1,0.0);}
			for (j=0;j<10;j++) {RGB[j+31] = ColorRGB(0.0, 0.0, (j+1) * 0.1);}
			for (j=0;j<10;j++) {RGB[j+61] = ColorRGB((j+1) * 0.1, (j+1) * 0.1, 0.0);}
			for (j=0;j<10;j++) {RGB[j+41] = ColorRGB(0.0, (j+1) * 0.1, (j+1) * 0.1);}
			for (j=0;j<10;j++) {RGB[j+51] = ColorRGB((j+1) * 0.1, 0.0, (j+1) * 0.1);}
            break;
        }
        //Color checker SG 96 colors
		case CCSG:
		{
            RGB[0] = ColorRGB(1,1,1);
            RGB[1] = ColorRGB(0.872146119,0.872146119,0.872146119);
            RGB[2] = ColorRGB(0.771689498,0.771689498,0.771689498);
            RGB[3] = ColorRGB(0.721461187,0.721461187,0.721461187);
            RGB[4] = ColorRGB(0.671232877,0.671232877,0.671232877);
            RGB[5] = ColorRGB(0.611872146,0.611872146,0.611872146);
            RGB[6] = ColorRGB(0.561643836,0.561643836,0.561643836);
            RGB[7] = ColorRGB(0.461187215,0.461187215,0.461187215);
            RGB[8] = ColorRGB(0.420091324,0.420091324,0.420091324);
            RGB[9] = ColorRGB(0.369863014,0.369863014,0.369863014);
            RGB[10] = ColorRGB(0.328767123,0.328767123,0.328767123);
            RGB[11] = ColorRGB(0.292237443,0.292237443,0.292237443);
            RGB[12] = ColorRGB(0.210045662,0.210045662,0.210045662);
            RGB[13] = ColorRGB(0.168949772,0.168949772,0.168949772);
            RGB[14] = ColorRGB(0,0,0);
            RGB[15] = ColorRGB(0.570776256,0.109589041,0.328767123);
            RGB[16] = ColorRGB(0.292237443,0.178082192,0.278538813);
            RGB[17] = ColorRGB(0.849315068,0.821917808,0.757990868);
            RGB[18] = ColorRGB(0.438356164,0.251141553,0.150684932);
            RGB[19] = ColorRGB(0.799086758,0.538812785,0.401826484);
            RGB[20] = ColorRGB(0.351598174,0.438356164,0.511415525);
            RGB[21] = ColorRGB(0.328767123,0.378995434,0.118721461);
            RGB[22] = ColorRGB(0.502283105,0.461187215,0.570776256);
            RGB[23] = ColorRGB(0.420091324,0.707762557,0.561643836);
            RGB[24] = ColorRGB(1,0.780821918,0.598173516);
            RGB[25] = ColorRGB(0.388127854,0.109589041,0.159817352);
            RGB[26] = ColorRGB(0.748858447,0.118721461,0.292237443);
            RGB[27] = ColorRGB(0.739726027,0.502283105,0.611872146);
            RGB[28] = ColorRGB(0.429223744,0.351598174,0.538812785);
            RGB[29] = ColorRGB(0.99086758,0.780821918,0.707762557);
            RGB[30] = ColorRGB(0.908675799,0.438356164,0);
            RGB[31] = ColorRGB(0.242009132,0.310502283,0.561643836);
            RGB[32] = ColorRGB(0.780821918,0.251141553,0.269406393);
            RGB[33] = ColorRGB(0.319634703,0.150684932,0.319634703);
            RGB[34] = ColorRGB(0.662100457,0.707762557,0);
            RGB[35] = ColorRGB(0.917808219,0.589041096,0);
            RGB[36] = ColorRGB(0.840182648,0.908675799,0.707762557);
            RGB[37] = ColorRGB(0.799086758,0,0.082191781);
            RGB[38] = ColorRGB(0.337899543,0.127853881,0.210045662);
            RGB[39] = ColorRGB(0.461187215,0.118721461,0.438356164);
            RGB[40] = ColorRGB(0,0.210045662,0.351598174);
            RGB[41] = ColorRGB(0.739726027,0.858447489,0.721461187);
            RGB[42] = ColorRGB(0.050228311,0.168949772,0.461187215);
            RGB[43] = ColorRGB(0.260273973,0.547945205,0.159817352);
            RGB[44] = ColorRGB(0.698630137,0,0.100456621);
            RGB[45] = ColorRGB(0.96803653,0.748858447,0);
            RGB[46] = ColorRGB(0.757990868,0.260273973,0.479452055);
            RGB[47] = ColorRGB(0,0.502283105,0.547945205);
            RGB[48] = ColorRGB(0.908675799,0.799086758,0.748858447);
            RGB[49] = ColorRGB(0.840182648,0.461187215,0.470319635);
            RGB[50] = ColorRGB(0.748858447,0,0.150684932);
            RGB[51] = ColorRGB(0,0.488584475,0.680365297);
            RGB[52] = ColorRGB(0.328767123,0.598173516,0.680365297);
            RGB[53] = ColorRGB(1,0.780821918,0.671232877);
            RGB[54] = ColorRGB(0.748858447,0.840182648,0.757990868);
            RGB[55] = ColorRGB(0.872146119,0.461187215,0.401826484);
            RGB[56] = ColorRGB(0.940639269,0.200913242,0.150684932);
            RGB[57] = ColorRGB(0.191780822,0.630136986,0.648401826);
            RGB[58] = ColorRGB(0,0.228310502,0.269406393);
            RGB[59] = ColorRGB(0.858447489,0.831050228,0.520547945);
            RGB[60] = ColorRGB(1,0.401826484,0);
            RGB[61] = ColorRGB(1,0.630136986,0);
            RGB[62] = ColorRGB(0,0.228310502,0.200913242);
            RGB[63] = ColorRGB(0.461187215,0.579908676,0.689497717);
            RGB[64] = ColorRGB(0.858447489,0.479452055,0.278538813);
            RGB[65] = ColorRGB(0.96803653,0.671232877,0.488584475);
            RGB[66] = ColorRGB(0.780821918,0.547945205,0.360730594);
            RGB[67] = ColorRGB(0.561643836,0.360730594,0.200913242);
            RGB[68] = ColorRGB(0.808219178,0.589041096,0.452054795);
            RGB[69] = ColorRGB(0.630136986,0.337899543,0.127853881);
            RGB[70] = ColorRGB(0.840182648,0.520547945,0.360730594);
            RGB[71] = ColorRGB(0.780821918,0.698630137,0);
            RGB[72] = ColorRGB(1,0.748858447,0);
            RGB[73] = ColorRGB(0,0.630136986,0.561643836);
            RGB[74] = ColorRGB(0,0.547945205,0.461187215);
            RGB[75] = ColorRGB(0.821917808,0.538812785,0.410958904);
            RGB[76] = ColorRGB(0.98173516,0.598173516,0.452054795);
            RGB[77] = ColorRGB(0.780821918,0.561643836,0.420091324);
            RGB[78] = ColorRGB(0.789954338,0.547945205,0.420091324);
            RGB[79] = ColorRGB(0.799086758,0.561643836,0.410958904);
            RGB[80] = ColorRGB(0.479452055,0.292237443,0.150684932);
            RGB[81] = ColorRGB(0.849315068,0.547945205,0.369863014);
            RGB[82] = ColorRGB(0.721461187,0.561643836,0.091324201);
            RGB[83] = ColorRGB(0.730593607,0.698630137,0);
            RGB[84] = ColorRGB(0.251141553,0.210045662,0.141552511);
            RGB[85] = ColorRGB(0.351598174,0.630136986,0.351598174);
            RGB[86] = ColorRGB(0,0.538812785,0.310502283);
            RGB[87] = ColorRGB(0.118721461,0.251141553,0.159817352);
            RGB[88] = ColorRGB(0.228310502,0.630136986,0.429223744);
            RGB[89] = ColorRGB(0.479452055,0.611872146,0.219178082);
            RGB[90] = ColorRGB(0.200913242,0.538812785,0.100456621);
            RGB[91] = ColorRGB(0.292237443,0.662100457,0.159817352);
            RGB[92] = ColorRGB(0.789954338,0.520547945,0.168949772);
            RGB[93] = ColorRGB(0.621004566,0.589041096,0.100456621);
            RGB[94] = ColorRGB(0.648401826,0.730593607,0);
            RGB[95] = ColorRGB(0.301369863,0.168949772,0.100456621);
            break;
        } 
        //Custom color checker as default
		default:
			int r=0, g=0, b=0;			
			RGB [ i ] = GetConfig()->GetCColorsT(i);
			break;
    } 
    CColor White = CMeasure::GetGray ( CMeasure::GetGrayScaleSize() - 1 );
	CColor Black = CMeasure::GetGray ( 0 );
    double gamma=GetConfig()->m_useMeasuredGamma?(GetConfig()->m_GammaAvg):(GetConfig()->m_GammaRef);
    double inr=RGB[i][0],ing=RGB[i][1],inb=RGB[i][2];
	int mode = GetConfig()->m_GammaOffsetType;
	if (GetConfig()->m_colorStandard == sRGB) mode = 7;
	if (  (mode == 4 && White.isValid() && Black.isValid()) || mode > 4)
    {
        inr=(inr<=0||inr>=1)?min(max(inr,0),1):pow(RGB[i][0],log(getEOTF(RGB[i][0],White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(RGB[i][0]));
        ing=(ing<=0||ing>=1)?min(max(ing,0),1):pow(RGB[i][1],log(getEOTF(RGB[i][1],White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(RGB[i][1]));
        inb=(inb<=0||inb>=1)?min(max(inb,0),1):pow(RGB[i][2],log(getEOTF(RGB[i][2],White,Black,GetConfig()->m_GammaRel, GetConfig()->m_Split, mode))/log(RGB[i][2]));
    }
    else
    {
        inr=(inr<=0||inr>=1)?min(max(inr,0),1):pow(RGB[i][0],gamma);
        ing=(ing<=0||ing>=1)?min(max(ing,0),1):pow(RGB[i][1],gamma);
        inb=(inb<=0||inb>=1)?min(max(inb,0),1):pow(RGB[i][2],gamma);
    }
    ccRef.SetRGBValue(ColorRGB(inr,ing,inb),cRef);
	return ccRef;
}