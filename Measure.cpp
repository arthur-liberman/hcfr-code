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
//	Fran�ois-Xavier CHABOUD
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

#include <math.h>

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
	m_primariesArray.SetSize(3);
	m_secondariesArray.SetSize(3);
	m_grayMeasureArray.SetSize(11);
	m_nearBlackMeasureArray.SetSize(5);
	m_nearWhiteMeasureArray.SetSize(5);
	m_redSatMeasureArray.SetSize(5);
	m_greenSatMeasureArray.SetSize(5);
	m_blueSatMeasureArray.SetSize(5);
	m_yellowSatMeasureArray.SetSize(5);
	m_cyanSatMeasureArray.SetSize(5);
	m_magentaSatMeasureArray.SetSize(5);
	m_cc24SatMeasureArray.SetSize(1000);

	m_primariesArray[0]=m_primariesArray[1]=m_primariesArray[2]=noDataColor;
	m_secondariesArray[0]=m_secondariesArray[1]=m_secondariesArray[2]=noDataColor;

	for(int i=0;i<m_grayMeasureArray.GetSize();i++)	// Init default values: by default m_grayMeasureArray init to D65, Y=1
		m_grayMeasureArray[i]=noDataColor;	

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

	m_OnOffBlack=m_OnOffWhite=noDataColor;
	m_AnsiBlack=m_AnsiWhite=noDataColor;
	m_infoStr="";
	
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
			for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
			{
				m_redSatMeasureArray[i]=p->m_redSatMeasureArray[i];
				m_greenSatMeasureArray[i]=p->m_greenSatMeasureArray[i];
				m_blueSatMeasureArray[i]=p->m_blueSatMeasureArray[i];
			}
			break;

		case DUPLSECONDARIESSAT:		// Secondaries saturation measure
			m_redSatMeasureArray.SetSize(p->m_redSatMeasureArray.GetSize());
			m_greenSatMeasureArray.SetSize(p->m_greenSatMeasureArray.GetSize());
			m_blueSatMeasureArray.SetSize(p->m_blueSatMeasureArray.GetSize());
			m_yellowSatMeasureArray.SetSize(p->m_yellowSatMeasureArray.GetSize());
			m_cyanSatMeasureArray.SetSize(p->m_cyanSatMeasureArray.GetSize());
			m_magentaSatMeasureArray.SetSize(p->m_magentaSatMeasureArray.GetSize());
			m_cc24SatMeasureArray.SetSize(p->m_magentaSatMeasureArray.GetSize());
			for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
			{
				m_yellowSatMeasureArray[i]=p->m_yellowSatMeasureArray[i];
				m_cyanSatMeasureArray[i]=p->m_cyanSatMeasureArray[i];
				m_magentaSatMeasureArray[i]=p->m_magentaSatMeasureArray[i];
			}
			for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
				m_cc24SatMeasureArray[i]=p->m_cc24SatMeasureArray[i];
			break;

		case DUPLPRIMARIESCOL:		// Primaries measure
			for(int i=0;i<m_primariesArray.GetSize();i++)
				m_primariesArray[i]=p->m_primariesArray[i];	

			break;

		case DUPLSECONDARIESCOL:		// Secondaries measure
			for(int i=0;i<m_secondariesArray.GetSize();i++)
				m_secondariesArray[i]=p->m_secondariesArray[i];
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
	    int version=8;
		ar << version;

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

		ar << m_infoStr;

		ar << m_bIREScaleMode;
	}
	else
	{
	    int version;
		ar >> version;

		if ( version > 8 )
			AfxThrowArchiveException ( CArchiveException::badSchema );

		int size;

		ar >> size;
		m_grayMeasureArray.SetSize(size);
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

			if ( version > 7)
			{
				ar >> size;
				m_cc24SatMeasureArray.SetSize(size);
				for(int i=0;i<m_cc24SatMeasureArray.GetSize();i++)
					m_cc24SatMeasureArray[i].Serialize(ar);
			}

		}
		else
		{
			m_redSatMeasureArray.SetSize(5);
			m_greenSatMeasureArray.SetSize(5);
			m_blueSatMeasureArray.SetSize(5);
			m_yellowSatMeasureArray.SetSize(5);
			m_cyanSatMeasureArray.SetSize(5);
			m_magentaSatMeasureArray.SetSize(5);
			m_cc24SatMeasureArray.SetSize(1000);
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
		m_OnOffWhite.Serialize(ar);
		m_AnsiBlack.Serialize(ar);
		m_AnsiWhite.Serialize(ar);

		ar >> m_infoStr;

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

BOOL CMeasure::MeasureGrayScale(CSensor *pSensor, CGenerator *pGenerator)
{
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_grayMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor, lastColor;

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

	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayGray(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown),CGenerator::MT_IRE ,!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown));
				
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

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		GetColorApp()->InMeasureMessageBox(pGenerator->GetRetryMessage(), NULL, MB_OK | MB_ICONWARNING);

	for(int i=0;i<size;i++)
	{
		m_grayMeasureArray[i] = measuredColor[i];

		if ( bUseLuxValues )
			m_grayMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_grayMeasureArray[i].ResetLuxValue ();
	}

	m_isModified=TRUE;
	return TRUE;
}



BOOL CMeasure::MeasureGrayScaleAndColors(CSensor *pSensor, CGenerator *pGenerator)
{
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_grayMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor, lastColor;

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

	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayGray(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown),CGenerator::MT_IRE ,!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(ArrayIndexToGrayLevel ( i, size, GetConfig () -> m_bUseRoundDown ));
				
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
	if (GetColorReference().m_standard == CC6 || GetColorReference().m_standard == CC6a) //CC6
	{
		if (GetConfig()->m_CCMode == GCD)
		{
			GenColors [ 0 ] = ColorRGBDisplay(75.80,58.90,51.14);
			GenColors [ 1 ] = ColorRGBDisplay(36.99,47.95,61.19);
			GenColors [ 2 ] = ColorRGBDisplay(35.16,42.01,26.03);
			GenColors [ 3 ] = ColorRGBDisplay(29.22,36.07,63.93);
			GenColors [ 4 ] = ColorRGBDisplay(62.1,73.06,25.11);
			GenColors [ 5 ] = ColorRGBDisplay(89.95,63.01,17.81);
		} else
		{
			GenColors [ 0 ] = ColorRGBDisplay(75.8,58.45,50.68); //light skin
			GenColors [ 1 ] = ColorRGBDisplay(36.99,47.95,60.73); //blue sky
			GenColors [ 2 ] = ColorRGBDisplay(34.7,42.47,26.48); //foliage
			GenColors [ 3 ] = ColorRGBDisplay(28.77,36.61,64.39);
			GenColors [ 4 ] = ColorRGBDisplay(62.1,73.06,24.66);
			GenColors [ 5 ] = ColorRGBDisplay(89.95,63.47,18.36);
		}
	}
	else if (GetColorReference().m_standard == HDTVa) //75%
	{ 
		GenColors [ 0 ] = ColorRGBDisplay(68.0,20.0,20.0);
		GenColors [ 1 ] = ColorRGBDisplay(28.0,73.0,28.0);
		GenColors [ 2 ] = ColorRGBDisplay(19.0,19.0,50.0);
		GenColors [ 3 ] = ColorRGBDisplay(75.0,75.0,33.0);
		GenColors [ 4 ] = ColorRGBDisplay(36.0,73.0,73.0);
		GenColors [ 5 ] = ColorRGBDisplay(64.0,29.0,64.0);
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
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SECONDARY,0,TRUE,TRUE) )

		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[size+i]=pSensor->MeasureColor(MeasColors[i]);
				
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

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_OnOffWhite = measuredColor[size+6];                
		if ( bUseLuxValues )
			m_OnOffWhite.SetLuxValue ( measuredLux[size+6] );
		else
			m_OnOffWhite.ResetLuxValue ();
	}
		
	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureNearBlackScale(CSensor *pSensor, CGenerator *pGenerator)
{
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_nearBlackMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor, lastColor;

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


	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayGray((ArrayIndexToGrayLevel ( i, 101., GetConfig () -> m_bUseRoundDown)),CGenerator::MT_NEARBLACK,!bRetry) )
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(i);
				
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
		m_nearBlackMeasureArray[i] = measuredColor[i];
		if ( bUseLuxValues )
			m_nearBlackMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_nearBlackMeasureArray[i].ResetLuxValue ();
	}

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureNearWhiteScale(CSensor *pSensor, CGenerator *pGenerator)
{
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	int		size=m_nearWhiteMeasureArray.GetSize();
	CString	strMsg, Title;
	double	dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor, lastColor;

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

	
	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayGray((ArrayIndexToGrayLevel ( 101-size+i, 101., GetConfig () -> m_bUseRoundDown)),CGenerator::MT_NEARWHITE,!bRetry ) )
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(101-size+i);
				
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureRedSatScale(CSensor *pSensor, CGenerator *pGenerator)
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
	CColor previousColor, lastColor;

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
	double gamma=GetConfig()->m_GammaAvg;

	GenerateSaturationColors (GetColorReference(), GenColors,size, true, false, false, gamma);
	
    for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_RED,100*i/(size - 1),!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureGreenSatScale(CSensor *pSensor, CGenerator *pGenerator)
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
	CColor previousColor, lastColor;

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
	double gamma=GetConfig()->m_GammaAvg;
	// Generate saturation colors for green
	GenerateSaturationColors (GetColorReference(), GenColors,size, false, true, false, gamma );

	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_GREEN,100*i/(size - 1),!bRetry) )
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureBlueSatScale(CSensor *pSensor, CGenerator *pGenerator)
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
	CColor previousColor, lastColor;

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
 	double gamma=GetConfig()->m_GammaAvg;

	// Generate saturation colors for blue
		GenerateSaturationColors (GetColorReference(), GenColors,size, false, false, true, gamma );

	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_BLUE,100*i/(size - 1),!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureYellowSatScale(CSensor *pSensor, CGenerator *pGenerator)
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
	CColor previousColor, lastColor;

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
	double gamma=GetConfig()->m_GammaAvg;

	// Generate saturation colors for yellow
	GenerateSaturationColors (GetColorReference(), GenColors,size, true, true, false, gamma );

	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_YELLOW,100*i/(size - 1),!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureCyanSatScale(CSensor *pSensor, CGenerator *pGenerator)
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
	CColor previousColor, lastColor;

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
	double gamma=GetConfig()->m_GammaAvg;

	// Generate saturation colors for cyan
	GenerateSaturationColors (GetColorReference(), GenColors,size, false, true, true, gamma );

	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_CYAN,100*i/(size - 1),!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureMagentaSatScale(CSensor *pSensor, CGenerator *pGenerator)
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
	CColor previousColor, lastColor;

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
	double gamma=GetConfig()->m_GammaAvg;

	// Generate saturation colors for magenta
	GenerateSaturationColors (GetColorReference(), GenColors,size, true, false, true, gamma );

	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SAT_MAGENTA,100*i/(size - 1) ,!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
				
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureCC24SatScale(CSensor *pSensor, CGenerator *pGenerator)
{
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetConfig()->m_CCMode == CCSG?96:24;
	CString		strMsg, Title;
	ColorRGBDisplay	GenColors [ 256 ];
	double		dLuxValue;

	CArray<CColor,int> measuredColor;
	CColor previousColor, lastColor;

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
	case AXIS:
		 nPattern=CGenerator::MT_SAT_CC24_MCD;		
		 break;
	case SKIN:
		 nPattern=CGenerator::MT_SAT_CC24_MCD;
		 break;
	case CCSG:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;
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

    if(pGenerator->GetName() == str&&(GetConfig()->m_CCMode==AXIS || GetConfig()->m_CCMode==SKIN) )
	{		
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	GenerateCC24Colors (GenColors, GetConfig()->m_CCMode);

	for(int i=0;i<size;i++)
	{
		if( pGenerator->DisplayRGBColor(GenColors[i], nPattern ,i,!bRetry))
		{
			bEscape = WaitForDynamicIris ();
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureAllSaturationScales(CSensor *pSensor, CGenerator *pGenerator, BOOL bPrimaryOnly)
{
	int			i, j;
	MSG			Msg;
	BOOL		bEscape;
	BOOL		bPatternRetry = FALSE;
	BOOL		bRetry = FALSE;
	int			size = GetSaturationSize ();
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
	case AXIS:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;		
		 break;
	case SKIN:
		 nPattern=CGenerator::MT_SAT_CC24_GCD;		
	case CCSG:
		 nPattern=CGenerator::MT_SAT_CC24_CCSG;		
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
	CColor previousColor, lastColor;

	BOOL	bUseLuxValues = TRUE;
	CArray<double,int> measuredLux;

    measuredColor.SetSize(size*6+(GetConfig()->m_CCMode==CCSG?96:24));
	measuredLux.SetSize(size*6+(GetConfig()->m_CCMode==CCSG?96:24));

	if(pGenerator->Init(size*(bPrimaryOnly?3:6) + (GetConfig()->m_CCMode==CCSG?96:24)) != TRUE)
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

	CString str;
	str.LoadString(IDS_MANUALDVDGENERATOR_NAME);
	if(pGenerator->GetName() == str&&(GetConfig()->m_CCMode==AXIS || GetConfig()->m_CCMode==SKIN))
	{		
		Title.LoadString ( IDS_ERROR );
		strMsg.LoadString ( IDS_ERRINITGENERATOR );
		GetColorApp()->InMeasureMessageBox(strMsg,Title,MB_ICONERROR | MB_OK);
		pGenerator->Release();
		return FALSE;
	}

	double gamma=GetConfig()->m_GammaAvg;

	// Generate saturations for all colors
	GenerateSaturationColors (GetColorReference(), GenColors, size, true, false, false, gamma );				// Red
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 1 ], size, false, true, false, gamma );	// Green
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 2 ], size, false, false, true, gamma );	// Blue
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 3 ], size, true, true, false, gamma );	// Yellow
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 4 ], size, false, true, true, gamma );	// Cyan
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 5 ], size, true, false, true, gamma );	// Magenta
	GenerateCC24Colors (& GenColors [ size * 6 ], GetConfig()->m_CCMode); //color checker

	for ( j = 0 ; j < ( bPrimaryOnly ? 3 : 7 ) ; j ++ )
	{
		for ( i = 0 ; i < ( j == 6 ? (GetConfig()->m_CCMode==CCSG?96:24) : size ) ; i ++ )
		{
			if( pGenerator->DisplayRGBColor(GenColors[(j*size)+i],SaturationType[j],(j == 6 ? i:100*i/(size - 1)),!bRetry,(j>0)) )
			{
				bEscape = WaitForDynamicIris ();
				bRetry = FALSE;

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();

					measuredColor[(j*size)+i]=pSensor->MeasureColor(GenColors[(j*size)+i]);
					
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
	for ( i = 0 ; i < (GetConfig()->m_CCMode==CCSG?96:24) ; i ++ )
	{
		if ( ! bPrimaryOnly )
		{
				if (GetConfig()->m_CCMode != MCD)
				{
//					measuredColor[i]=pSensor->MeasureColor(GenColors[i]);
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasurePrimarySecondarySaturationScales(CSensor *pSensor, CGenerator *pGenerator, BOOL bPrimaryOnly)
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
	CColor previousColor, lastColor;

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

	double gamma=GetConfig()->m_GammaAvg;

	// Generate saturations for all colors
	GenerateSaturationColors (GetColorReference(), GenColors, size, true, false, false, gamma );				// Red
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 1 ], size, false, true, false, gamma );	// Green
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 2 ], size, false, false, true, gamma );	// Blue
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 3 ], size, true, true, false, gamma );	// Yellow
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 4 ], size, false, true, true, gamma );	// Cyan
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 5 ], size, true, false, true, gamma );	// Magenta

	for ( j = 0 ; j < ( bPrimaryOnly ? 3 : 6 ) ; j ++ )
	{
		for ( i = 0 ; i < size  ; i ++ )
		{
			if( pGenerator->DisplayRGBColor(GenColors[(j*size)+i],SaturationType[j],100*i/(size - 1),!bRetry,(j>0)) )
			{
				bEscape = WaitForDynamicIris ();
				bRetry = FALSE;

				if ( ! bEscape )
				{
					if ( bUseLuxValues )
						StartLuxMeasure ();

					measuredColor[(j*size)+i]=pSensor->MeasureColor(GenColors[(j*size)+i]);
					
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

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasurePrimaries(CSensor *pSensor, CGenerator *pGenerator)
{
	int		i;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	CColor	measuredColor[5];
	CString	strMsg, Title;
	CColor previousColor, lastColor;
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

	if ( GetColorReference().m_standard == CC6 || GetColorReference().m_standard == CC6a ) //CC6
	{
		if ( GetConfig()->m_CCMode == GCD )
		{
			GenColors [ 0 ] = ColorRGBDisplay(75.80,58.90,51.14);
			GenColors [ 1 ] = ColorRGBDisplay(36.99,47.95,61.19);
			GenColors [ 2 ] = ColorRGBDisplay(35.16,42.01,26.03);
			GenColors [ 3 ] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
			GenColors [ 4 ] = ColorRGBDisplay(0,0,0);
		} else
		{
			GenColors [ 0 ] = ColorRGBDisplay(75.8,58.45,50.68); //light skin
			GenColors [ 1 ] = ColorRGBDisplay(36.99,47.95,60.73); //blue sky
			GenColors [ 2 ] = ColorRGBDisplay(34.7,42.47,26.48); //foliage
			GenColors [ 3 ] = ColorRGBDisplay(primaryIRELevel,primaryIRELevel,primaryIRELevel);
			GenColors [ 4 ] = ColorRGBDisplay(0,0,0);
		}
	}
	else if ( GetColorReference().m_standard == HDTVa ) //75%
	{ 
		GenColors [ 0 ] = ColorRGBDisplay(68.0,20.0,20.0);
		GenColors [ 1 ] = ColorRGBDisplay(28.0, 73.0,28.0);
		GenColors [ 2 ] = ColorRGBDisplay(19.0,19.0,50.0);
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

	for ( i = 0; i < ( 3 + GetConfig () -> m_BWColorsToAdd ) ; i ++ )
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_PRIMARY) )
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(MeasColors[i]);
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
		m_OnOffWhite = measuredColor[3];                
		if ( bUseLuxValues )
			m_OnOffWhite.SetLuxValue ( measuredLux[3] );
		else
			m_OnOffWhite.ResetLuxValue ();
	}
	else
	{
		m_OnOffWhite=noDataColor;
	}

	if ( GetConfig () -> m_BWColorsToAdd > 1 )
	{
		m_OnOffBlack = measuredColor[4];                
		if ( bUseLuxValues )
			m_OnOffBlack.SetLuxValue ( measuredLux[4] );
		else
			m_OnOffBlack.ResetLuxValue ();
	}
	else
	{
		m_OnOffBlack=noDataColor;
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

	m_isModified=TRUE;
	return TRUE;
}

BOOL CMeasure::MeasureSecondaries(CSensor *pSensor, CGenerator *pGenerator)
{
	int		i;
	MSG		Msg;
	BOOL	bEscape;
	BOOL	bPatternRetry = FALSE;
	BOOL	bRetry = FALSE;
	CColor	measuredColor[8];
	CString	strMsg, Title;
	CColor previousColor, lastColor;
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
	if (GetColorReference().m_standard == CC6 || GetColorReference().m_standard == CC6a) //CC6
	{
		if ( GetConfig()->m_CCMode == GCD )
		{
			GenColors [ 0 ] = ColorRGBDisplay(75.80,58.90,51.14);
			GenColors [ 1 ] = ColorRGBDisplay(36.99,47.95,61.19);
			GenColors [ 2 ] = ColorRGBDisplay(35.16,42.01,26.03);
			GenColors [ 3 ] = ColorRGBDisplay(29.22,36.07,63.93);
			GenColors [ 4 ] = ColorRGBDisplay(62.1,73.06,25.11);
			GenColors [ 5 ] = ColorRGBDisplay(89.95,63.01,17.81);
			GenColors [ 6 ] = ColorRGBDisplay(IRELevel,IRELevel,IRELevel);
			GenColors [ 7 ] = ColorRGBDisplay(0,0,0);
		} else
		{
			GenColors [ 0 ] = ColorRGBDisplay(75.8,58.45,50.68); //light skin
			GenColors [ 1 ] = ColorRGBDisplay(36.99,47.95,60.73); //blue sky
			GenColors [ 2 ] = ColorRGBDisplay(34.7,42.47,26.48); //foliage
			GenColors [ 3 ] = ColorRGBDisplay(28.77,36.61,64.39);
			GenColors [ 4 ] = ColorRGBDisplay(62.1,73.06,24.66);
			GenColors [ 5 ] = ColorRGBDisplay(89.95,63.47,18.36);
			GenColors [ 6 ] = ColorRGBDisplay(IRELevel,IRELevel,IRELevel);
			GenColors [ 7 ] = ColorRGBDisplay(0,0,0);
		}
	}
	else if (GetColorReference().m_standard == 3) //75%
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
	for ( i = 0; i < ( 6 + GetConfig () -> m_BWColorsToAdd ); i ++ )
	{
		if( pGenerator->DisplayRGBColor(GenColors[i],CGenerator::MT_SECONDARY) )
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureColor(MeasColors[i]);
				
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
		m_OnOffWhite = measuredColor[6];                
		if ( bUseLuxValues )
			m_OnOffWhite.SetLuxValue ( measuredLux[6] );
		else
			m_OnOffWhite.ResetLuxValue ();
	}
	else
	{
		m_OnOffWhite=noDataColor;
	}

	if ( GetConfig () -> m_BWColorsToAdd > 1 )
	{
		m_OnOffBlack = measuredColor[7];                
		if ( bUseLuxValues )
			m_OnOffBlack.SetLuxValue ( measuredLux[7] );
		else
			m_OnOffBlack.ResetLuxValue ();
	}
	else
	{
		m_OnOffBlack=noDataColor;
	}

	pSensor->Release();
	pGenerator->Release();

	if (bPatternRetry)
		AfxMessageBox(pGenerator->GetRetryMessage(), MB_OK | MB_ICONWARNING);

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
	CColor	lastColor;
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
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measure=pSensor->MeasureColor(ColorRGBDisplay(NearWhiteIRELevel));	// Assume White
				
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

	m_isModified=TRUE;
	return TRUE;
}

double CMeasure::GetOnOffContrast ()
{
	double	black = m_OnOffBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	double	white = m_OnOffWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	
	if ( black > 0.000001 && white > black )
		return ( white / black );
	else
		return -1.0;
}

double CMeasure::GetAnsiContrast ()
{
	double	black = m_AnsiBlack.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	double	white = m_AnsiWhite.GetPreferedLuxValue (GetConfig () -> m_bPreferLuxmeter);
	
	if ( black > 0.000001 && white > black )
		return ( white / black );
	else
		return -1.0;
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
	m_OnOffBlack = noDataColor;
	m_OnOffWhite = noDataColor;
	m_AnsiBlack = noDataColor;
	m_AnsiWhite = noDataColor;

	m_isModified=TRUE; 
}

BOOL CMeasure::AddMeasurement(CSensor *pSensor, CGenerator *pGenerator)
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
		bOk = pGenerator->DisplayRGBColor(ColorRGBDisplay(clr),CGenerator::MT_ACTUAL);
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
	
	FreeMeasurementAppended();

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
	for(int i=0;i<3;i++)
	{
		m_primariesArray[i].applyAdjustmentMatrix(aMatrix);
	}
	for(int i=0;i<3;i++)
	{
		m_secondariesArray[i].applyAdjustmentMatrix(aMatrix);
	}
	
	m_OnOffBlack.applyAdjustmentMatrix(aMatrix);
	
	m_OnOffWhite.applyAdjustmentMatrix(aMatrix);

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

		while((!bEscape) && ((dwNow - dwStart) < nLatencyTime))
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
		
		FreeMeasurementAppended ();
	}

	// Close background thread and event objects
	CancelBackgroundMeasures ();

	return bOk;
}

void CMeasure::FreeMeasurementAppended()
{
	if ( GetConfig ()->m_bDetectPrimaries )
	{
		CColor LastMeasure;

		int	n = GetMeasurementsSize();
		if ( n > 0 )
		{
			LastMeasure=GetMeasurement(n-1);

			if ( LastMeasure.GetDeltaxy ( GetColorReference().GetRed (), GetColorReference()) < 0.05 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetRedPrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetColorReference().GetGreen (), GetColorReference() ) < 0.05 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetGreenPrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetColorReference().GetBlue (), GetColorReference() ) < 0.05 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetBluePrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetColorReference().GetYellow (), GetColorReference() ) < 0.05 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetYellowSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetColorReference().GetCyan (), GetColorReference() ) < 0.05 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetCyanSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaxy ( GetColorReference().GetMagenta (), GetColorReference() ) < 0.05 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetMagentaSecondary ( m_measurementsArray[n-1] );
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
			// Store reference white.
			m_OnOffWhite = (*m_pBkMeasuredColor)[3];

			if ( bUseLuxValues )
				m_OnOffWhite.SetLuxValue ( pLuxValues[3] );
			else
				m_OnOffWhite.ResetLuxValue ();
		}
		else
			m_OnOffWhite = noDataColor;

		if ( m_nBkMeasureStepCount >= 5 )
		{
			// Store reference white.
			m_OnOffBlack = (*m_pBkMeasuredColor)[4];

			if ( bUseLuxValues )
				m_OnOffBlack.SetLuxValue ( pLuxValues[4] );
			else
				m_OnOffBlack.ResetLuxValue ();
		}
		else
			m_OnOffBlack = noDataColor;
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
			// Store reference white.
			m_OnOffWhite = (*m_pBkMeasuredColor)[6];

			if ( bUseLuxValues )
				m_OnOffWhite.SetLuxValue ( pLuxValues[6] );
			else
				m_OnOffWhite.ResetLuxValue ();
		}
		else
			m_OnOffWhite = noDataColor;

		if ( m_nBkMeasureStepCount >= 8 )
		{
			// Store reference white.
			m_OnOffBlack = (*m_pBkMeasuredColor)[7];

			if ( bUseLuxValues )
				m_OnOffBlack.SetLuxValue ( pLuxValues[7] );
			else
				m_OnOffBlack.ResetLuxValue ();
		}
		else
			m_OnOffBlack = noDataColor;
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

		m_OnOffWhite = (*m_pBkMeasuredColor)[m_nBkMeasureStepCount-7];
		m_OnOffBlack = (*m_pBkMeasuredColor)[0];

		if ( bUseLuxValues )
		{
			m_OnOffWhite.SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-7] );
			m_OnOffBlack.SetLuxValue ( pLuxValues[0] );
		}
		else
		{
			m_OnOffWhite.ResetLuxValue ();
			m_OnOffBlack.ResetLuxValue ();
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
	return m_cc24SatMeasureArray[i]; 
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

CColor CMeasure::GetMeasurement(int i) const 
{ 
	return m_measurementsArray[i]; 
} 

void CMeasure::AppendMeasurements(const CColor & aColor) 
{
	if (m_measurementsArray.GetSize() >= m_nbMaxMeasurements )
		m_measurementsArray.RemoveAt(0,1); 
	
	// Using a pointer here is a workaround for a VC++ 6.0 bug, which uses Matrix copy constructor instead of CColor one when using a const CColor ref.
	CColor * pColor = (CColor *) & aColor;
	m_measurementsArray.InsertAt(m_measurementsArray.GetSize(),*pColor); 

	m_isModified=TRUE; 

	FreeMeasurementAppended(); 
}

CColor CMeasure::GetRefPrimary(int i) const
{
	double gamma=GetConfig()->m_GammaAvg;
	bool isSpecial = (GetColorReference().m_standard==HDTVa||GetColorReference().m_standard==CC6||GetColorReference().m_standard==CC6a);
	CColorReference cRef=GetColorReference();
	CColor	aColorr,aColorg,aColorb;
	aColorr.SetXYZValue (cRef.GetRed());
	aColorg.SetXYZValue (cRef.GetGreen());
	aColorb.SetXYZValue (cRef.GetBlue());
	ColorRGB rgbr=aColorr.GetRGBValue ( GetColorReference() );
	ColorRGB rgbg=aColorg.GetRGBValue ( GetColorReference() );
	ColorRGB rgbb=aColorb.GetRGBValue ( GetColorReference() );
	double r,g,b;
	r=min(max(rgbr[0],0.00001),.99999);
	g=min(max(rgbr[1],0.00001),.99999);
	b=min(max(rgbr[2],0.00001),.99999);
	aColorr.SetRGBValue (ColorRGB(pow(pow(r,1./2.22),gamma),pow(pow(g,1./2.22),gamma),pow(pow(b,1./2.22),gamma)), GetColorReference());	
	r=min(max(rgbg[0],0.00001),.99999);
	g=min(max(rgbg[1],0.00001),.99999);
	b=min(max(rgbg[2],0.00001),.99999);
	aColorg.SetRGBValue (ColorRGB(pow(pow(r,1./2.22),gamma),pow(pow(g,1./2.22),gamma),pow(pow(b,1./2.22),gamma)), GetColorReference());	
	r=min(max(rgbb[0],0.00001),.99999);
	g=min(max(rgbb[1],0.00001),.99999);
	b=min(max(rgbb[2],0.00001),.99999);
	aColorb.SetRGBValue (ColorRGB(pow(pow(r,1./2.22),gamma),pow(pow(g,1./2.22),gamma),pow(pow(b,1./2.22),gamma)), GetColorReference());	
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
	double gamma=GetConfig()->m_GammaAvg;
	bool isSpecial = (GetColorReference().m_standard==HDTVa||GetColorReference().m_standard==CC6||GetColorReference().m_standard==CC6a);
	CColorReference cRef=GetColorReference();
	CColor	aColory,aColorc,aColorm;
	aColory.SetXYZValue (cRef.GetYellow());
	aColorc.SetXYZValue (cRef.GetCyan());
	aColorm.SetXYZValue (cRef.GetMagenta());
	ColorRGB rgby=aColory.GetRGBValue ( GetColorReference() );
	ColorRGB rgbc=aColorc.GetRGBValue ( GetColorReference() );
	ColorRGB rgbm=aColorm.GetRGBValue ( GetColorReference() );
	double r,g,b;
	r=min(max(rgby[0],0.00001),.99999);
	g=min(max(rgby[1],0.00001),.99999);
	b=min(max(rgby[2],0.00001),.99999);
	aColory.SetRGBValue (ColorRGB(pow(pow(r,1./2.22),gamma),pow(pow(g,1./2.22),gamma),pow(pow(b,1./2.22),gamma)), GetColorReference());	
	r=min(max(rgbc[0],0.00001),.99999);
	g=min(max(rgbc[1],0.00001),.99999);
	b=min(max(rgbc[2],0.00001),.99999);
	aColorc.SetRGBValue (ColorRGB(pow(pow(r,1./2.22),gamma),pow(pow(g,1./2.22),gamma),pow(pow(b,1./2.22),gamma)), GetColorReference());	
	r=min(max(rgbm[0],0.00001),.99999);
	g=min(max(rgbm[1],0.00001),.99999);
	b=min(max(rgbm[2],0.00001),.99999);
	aColorm.SetRGBValue (ColorRGB(pow(pow(r,1./2.22),gamma),pow(pow(g,1./2.22),gamma),pow(pow(b,1./2.22),gamma)), GetColorReference());	

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

CColor CMeasure::GetRefSat(int i, double sat_percent) const
{
	CColor	refColor;
	ColorxyY	refWhite(GetColorReference().GetWhite());
	
	double	x, y;
	double	xstart = refWhite[0];
	double	ystart = refWhite[1];
	double	YLuma;
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
	if (!(m_cRef == 3 || m_cRef == 4 || m_cRef == 5))
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

	if ( i < 3 )
		YLuma = GetRefPrimary(i) [ 1 ];
	else
		YLuma = GetRefSecondary(i-3) [ 1 ];
	
	double	xend = refColor.GetxyYValue()[0];
	double	yend = refColor.GetxyYValue()[1];

	x = xstart + ( (xend - xstart) * sat_percent );
	y = ystart + ( (yend - ystart) * sat_percent );

	CColor	aColor;
	aColor.SetxyYValue (x, y, YLuma);
	ColorRGB rgb=aColor.GetRGBValue ( GetColorReference() );
	double r,g,b;
	r=min(max(rgb[0],0.00001),.99999);
	g=min(max(rgb[1],0.00001),.99999);
	b=min(max(rgb[2],0.00001),.99999);
	double gamma=GetConfig()->m_GammaAvg;
	aColor.SetRGBValue (ColorRGB(pow(pow(r,1./2.22),gamma),pow(pow(g,1./2.22),gamma),pow(pow(b,1./2.22),gamma)), GetColorReference());	
	return aColor;
}

CColor CMeasure::GetRefCC24Sat(int i) const
{
	CColor aColor;
	int m_cRef=GetColorReference().m_standard;
    CColorReference cRef = GetColorReference();
	//switch over to user gamma
	CColor ccRef;
    ColorRGB RGB[100];
	double gamma=GetConfig()->m_GammaAvg;
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
		//axis steps
	case AXIS:
		{
			RGB[0] = ColorRGB(0.12, 0, 0 );
			RGB[1] = ColorRGB(.24, 0, 0 );
			RGB[2] = ColorRGB(.36, 0, 0 );
			RGB[3] = ColorRGB(.48, 0, 0 );
			RGB[4] = ColorRGB(.60, 0, 0 );
			RGB[5] = ColorRGB(.72, 0, 0 );
			RGB[6] = ColorRGB(.84, 0, 0 );
			RGB[7] = ColorRGB(.96, 0, 0 );
			RGB[8] = ColorRGB(  0, 0.12, 0 );
			RGB[9] = ColorRGB(  0, .24, 0 );
			RGB[10] = ColorRGB(  0, .36, 0 );
			RGB[11] = ColorRGB(  0, .48, 0 );
			RGB[12] = ColorRGB(  0, .60, 0 );
			RGB[13] = ColorRGB(  0, .72, 0 );
			RGB[14] = ColorRGB(  0, .84, 0 );
			RGB[15] = ColorRGB(  0, .96, 0 );
			RGB[16] = ColorRGB(  0, 0, 0.12);
			RGB[17] = ColorRGB(  0, 0, 0.24 );
			RGB[18] = ColorRGB(  0, 0, .36 );
			RGB[19] = ColorRGB(  0, 0, .48 );
			RGB[20] = ColorRGB(  0, 0, .60 );
			RGB[21] = ColorRGB(  0, 0, .72 );
			RGB[22] = ColorRGB(  0, 0, .84 );
			RGB[23] = ColorRGB(  0, 0, .96 );
		break;		}
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
	} 
    ccRef.SetRGBValue(ColorRGB(pow(RGB[i][0],gamma),pow(RGB[i][1],gamma),pow(RGB[i][2],gamma)),cRef);
	return ccRef;
}
