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

	m_OnOffBlack=m_OnOffWhite=noDataColor;
	m_AnsiBlack=m_AnsiWhite=noDataColor;
	m_infoStr="";
	
	m_bUseAdjustmentMatrix = FALSE;
	m_XYZAdjustmentMatrix = IdentityMatrix ( 3 );

	m_bIREScaleMode = GetConfig()->GetProfileInt("References","IRELevels",FALSE);

	m_hThread = NULL;
	m_hEventRun = NULL;
	m_hEventDone = NULL;
	m_bTerminateThread = FALSE;
	m_bErrorOccurred = FALSE;
	m_nBkMeasureStep = 0;
	m_clrToMeasure = 0;
	m_nBkMeasureStepCount = 0;
	m_pBkMeasureSensor = NULL;
	m_pBkMeasuredColor = NULL;

	m_nbMaxMeasurements = GetConfig()->GetProfileInt("General","MaxMeasurements",250);
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
			for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
			{
				m_yellowSatMeasureArray[i]=p->m_yellowSatMeasureArray[i];
				m_cyanSatMeasureArray[i]=p->m_cyanSatMeasureArray[i];
				m_magentaSatMeasureArray[i]=p->m_magentaSatMeasureArray[i];
			}
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

		case DUPLXYZADJUST:
			m_XYZAdjustmentMatrix=p->m_XYZAdjustmentMatrix;
			m_bUseAdjustmentMatrix=p->m_bUseAdjustmentMatrix;
			m_XYZAdjustmentComment=p->m_XYZAdjustmentComment;
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
	    int version=6;
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

		ar << m_bUseAdjustmentMatrix;
		m_XYZAdjustmentMatrix.Serialize(ar);
		ar << m_XYZAdjustmentComment;

		ar << m_bIREScaleMode;
	}
	else
	{
	    int version;
		ar >> version;

		if ( version > 6 )
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
		}
		else
		{
			m_redSatMeasureArray.SetSize(5);
			m_greenSatMeasureArray.SetSize(5);
			m_blueSatMeasureArray.SetSize(5);
			m_yellowSatMeasureArray.SetSize(5);
			m_cyanSatMeasureArray.SetSize(5);
			m_magentaSatMeasureArray.SetSize(5);
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

		if ( version > 4 )
		{
			ar >> m_bUseAdjustmentMatrix;
			m_XYZAdjustmentMatrix.Serialize(ar);
			ar >> m_XYZAdjustmentComment;
		}
		else
		{
			m_bUseAdjustmentMatrix = FALSE;
			m_XYZAdjustmentMatrix = IdentityMatrix ( 3 );
			m_XYZAdjustmentComment.Empty ();
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
		if( pGenerator->DisplayGray(ArrayIndexToGrayLevel ( i, size, m_bIREScaleMode ),m_bIREScaleMode,CGenerator::MT_IRE ,!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(ArrayIndexToGrayLevel ( i, size, m_bIREScaleMode ),m_bIREScaleMode);
				
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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_grayMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_grayMeasureArray[i].SetSensorValue(measuredColor[i]);

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

	measuredColor.SetSize(size+8);
	measuredLux.SetSize(size+8);

	if(pGenerator->Init(size+8) != TRUE)
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
		if( pGenerator->DisplayGray(ArrayIndexToGrayLevel ( i, size, m_bIREScaleMode ),m_bIREScaleMode,CGenerator::MT_IRE ,!bRetry))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(ArrayIndexToGrayLevel ( i, size, m_bIREScaleMode ),m_bIREScaleMode);
				
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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
	COLORREF	GenColors [ 6 ] = 
								{	
									CIRELevel(primaryIRELevel,0,0,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,primaryIRELevel,0,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,0,primaryIRELevel,FALSE,pGenerator->m_b16_235),
									CIRELevel(primaryIRELevel,primaryIRELevel,0,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,primaryIRELevel,primaryIRELevel,FALSE,pGenerator->m_b16_235),
									CIRELevel(primaryIRELevel,0,primaryIRELevel,FALSE,pGenerator->m_b16_235)
								};
	COLORREF	MeasColors [ 6 ] = 
								{	
									CIRELevel(primaryIRELevel,0,0,FALSE,FALSE),
									CIRELevel(0,primaryIRELevel,0,FALSE,FALSE),
									CIRELevel(0,0,primaryIRELevel,FALSE,FALSE),
									CIRELevel(primaryIRELevel,primaryIRELevel,0,FALSE,FALSE),
									CIRELevel(0,primaryIRELevel,primaryIRELevel,FALSE,FALSE),
									CIRELevel(primaryIRELevel,0,primaryIRELevel,FALSE,FALSE)
								};
	for (int i = 0; i < 6 ; i ++ )
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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[size+i]);
	
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
		m_grayMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_grayMeasureArray[i].SetSensorValue(measuredColor[i]);

		if ( bUseLuxValues )
			m_grayMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_grayMeasureArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_primariesArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_primariesArray[i].SetSensorValue(measuredColor[i+size]);

		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i+size] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_secondariesArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_secondariesArray[i].SetSensorValue(measuredColor[i+size+3]);

		if ( bUseLuxValues )
			m_secondariesArray[i].SetLuxValue ( measuredLux[i+size+3] );
		else
			m_secondariesArray[i].ResetLuxValue ();
	}

	m_OnOffWhite.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
	m_OnOffWhite.SetSensorValue(measuredColor[size-1]);                
	m_OnOffBlack.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
	m_OnOffBlack.SetSensorValue(measuredColor[0]);                

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
		if( pGenerator->DisplayGray(i,FALSE,CGenerator::MT_NEARBLACK,!bRetry) )
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(i,FALSE);
				
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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_nearBlackMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_nearBlackMeasureArray[i].SetSensorValue(measuredColor[i]);
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
		if( pGenerator->DisplayGray(101-size+i,FALSE,CGenerator::MT_NEARWHITE,!bRetry ) )
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measuredColor[i]=pSensor->MeasureGray(101-size+i,FALSE);
				
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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_nearWhiteMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_nearWhiteMeasureArray[i].SetSensorValue(measuredColor[i]);
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
	COLORREF	GenColors [ 256 ];
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
	GenerateSaturationColors (GetColorReference(), GenColors, size, TRUE, FALSE, FALSE, pGenerator->m_b16_235 );

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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_redSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_redSatMeasureArray[i].SetSensorValue(measuredColor[i]);
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
	COLORREF	GenColors [ 256 ];
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

	// Generate saturation colors for green
	GenerateSaturationColors (GetColorReference(), GenColors, size, FALSE, TRUE, FALSE, pGenerator->m_b16_235 );

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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_greenSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_greenSatMeasureArray[i].SetSensorValue(measuredColor[i]);
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
	COLORREF	GenColors [ 256 ];
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

	// Generate saturation colors for blue
	GenerateSaturationColors (GetColorReference(), GenColors, size, FALSE, FALSE, TRUE, pGenerator->m_b16_235 );

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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_blueSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_blueSatMeasureArray[i].SetSensorValue(measuredColor[i]);
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
	COLORREF	GenColors [ 256 ];
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

	// Generate saturation colors for yellow
	GenerateSaturationColors (GetColorReference(), GenColors, size, TRUE, TRUE, FALSE, pGenerator->m_b16_235 );

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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_yellowSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_yellowSatMeasureArray[i].SetSensorValue(measuredColor[i]);
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
	COLORREF	GenColors [ 256 ];
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

	// Generate saturation colors for cyan
	GenerateSaturationColors (GetColorReference(), GenColors, size, FALSE, TRUE, TRUE, pGenerator->m_b16_235 );

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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_cyanSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_cyanSatMeasureArray[i].SetSensorValue(measuredColor[i]);
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
	COLORREF	GenColors [ 256 ];
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

	// Generate saturation colors for magenta
	GenerateSaturationColors (GetColorReference(), GenColors, size, TRUE, FALSE, TRUE, pGenerator->m_b16_235 );

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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_magentaSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_magentaSatMeasureArray[i].SetSensorValue(measuredColor[i]);
		if ( bUseLuxValues )
			m_magentaSatMeasureArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_magentaSatMeasureArray[i].ResetLuxValue ();
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
	COLORREF	GenColors [ 6 * 256 ];
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

	if(pGenerator->Init(size*6) != TRUE)
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
	GenerateSaturationColors (GetColorReference(), GenColors, size, TRUE, FALSE, FALSE, pGenerator->m_b16_235 );				// Red
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 1 ], size, FALSE, TRUE, FALSE, pGenerator->m_b16_235 );	// Green
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 2 ], size, FALSE, FALSE, TRUE, pGenerator->m_b16_235 );	// Blue
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 3 ], size, TRUE, TRUE, FALSE, pGenerator->m_b16_235 );	// Yellow
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 4 ], size, FALSE, TRUE, TRUE, pGenerator->m_b16_235 );	// Cyan
	GenerateSaturationColors (GetColorReference(), & GenColors [ size * 5 ], size, TRUE, FALSE, TRUE, pGenerator->m_b16_235 );	// Magenta

	for ( j = 0 ; j < ( bPrimaryOnly ? 3 : 6 ) ; j ++ )
	{
		for ( i = 0 ; i < size ; i ++ )
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
					lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
					lastColor.SetSensorValue(measuredColor[(j*size)+i]);
		
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
		m_redSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_redSatMeasureArray[i].SetSensorValue(measuredColor[i]);

		m_greenSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_greenSatMeasureArray[i].SetSensorValue(measuredColor[size+i]);

		m_blueSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_blueSatMeasureArray[i].SetSensorValue(measuredColor[(2*size)+i]);

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
			m_yellowSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
			m_yellowSatMeasureArray[i].SetSensorValue(measuredColor[(3*size)+i]);

			m_cyanSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
			m_cyanSatMeasureArray[i].SetSensorValue(measuredColor[(4*size)+i]);

			m_magentaSatMeasureArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
			m_magentaSatMeasureArray[i].SetSensorValue(measuredColor[(5*size)+i]);

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

	if(pGenerator->Init(5) != TRUE)
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
	COLORREF	GenColors [ 5 ] = 
								{	
									CIRELevel(IRELevel,0,0,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,IRELevel,0,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,0,IRELevel,FALSE,pGenerator->m_b16_235),
									CIRELevel(IRELevel,IRELevel,IRELevel,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,0,0,FALSE,pGenerator->m_b16_235)
								};
	COLORREF	MeasColors [ 5 ] = 
								{	
									CIRELevel(IRELevel,0,0,FALSE,FALSE),
									CIRELevel(0,IRELevel,0,FALSE,FALSE),
									CIRELevel(0,0,IRELevel,FALSE,FALSE),
									CIRELevel(IRELevel,IRELevel,IRELevel,FALSE,FALSE),
									CIRELevel(0,0,0,FALSE,FALSE)
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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_primariesArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_primariesArray[i].SetSensorValue(measuredColor[i]);
		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_OnOffWhite.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_OnOffWhite.SetSensorValue(measuredColor[3]);                
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
		m_OnOffBlack.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_OnOffBlack.SetSensorValue(measuredColor[4]);                
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

	if(pGenerator->Init(8) != TRUE)
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
	COLORREF	GenColors [ 8 ] = 
								{	
									CIRELevel(IRELevel,0,0,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,IRELevel,0,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,0,IRELevel,FALSE,pGenerator->m_b16_235),
									CIRELevel(IRELevel,IRELevel,0,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,IRELevel,IRELevel,FALSE,pGenerator->m_b16_235),
									CIRELevel(IRELevel,0,IRELevel,FALSE,pGenerator->m_b16_235),
									CIRELevel(IRELevel,IRELevel,IRELevel,FALSE,pGenerator->m_b16_235),
									CIRELevel(0,0,0,FALSE,pGenerator->m_b16_235)
								};
	COLORREF	MeasColors [ 8 ] = 
								{	
									CIRELevel(IRELevel,0,0,FALSE,FALSE),
									CIRELevel(0,IRELevel,0,FALSE,FALSE),
									CIRELevel(0,0,IRELevel,FALSE,FALSE),
									CIRELevel(IRELevel,IRELevel,0,FALSE,FALSE),
									CIRELevel(0,IRELevel,IRELevel,FALSE,FALSE),
									CIRELevel(IRELevel,0,IRELevel,FALSE,FALSE),
									CIRELevel(IRELevel,IRELevel,IRELevel,FALSE,FALSE),
									CIRELevel(0,0,0,FALSE,FALSE)
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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measuredColor[i]);
	
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
		m_primariesArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_primariesArray[i].SetSensorValue(measuredColor[i]);
		if ( bUseLuxValues )
			m_primariesArray[i].SetLuxValue ( measuredLux[i] );
		else
			m_primariesArray[i].ResetLuxValue ();
	}

	for(int i=0;i<3;i++)
	{
		m_secondariesArray[i].SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_secondariesArray[i].SetSensorValue(measuredColor[i+3]);
		if ( bUseLuxValues )
			m_secondariesArray[i].SetLuxValue ( measuredLux[i+3] );
		else
			m_secondariesArray[i].ResetLuxValue ();
	}

	if ( GetConfig () -> m_BWColorsToAdd > 0 )
	{
		m_OnOffWhite.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_OnOffWhite.SetSensorValue(measuredColor[6]);                
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
		m_OnOffBlack.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
		m_OnOffBlack.SetSensorValue(measuredColor[7]);                
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
		if( pGenerator->DisplayGray(BlackIRELevel,FALSE,CGenerator::MT_CONTRAST,!bRetry ) )
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measure=pSensor->MeasureColor(CIRELevel(BlackIRELevel,FALSE));
				
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
				m_OnOffBlack.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				m_OnOffBlack.SetSensorValue(measure);
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
		if( pGenerator->DisplayGray(WhiteIRELevel,FALSE,CGenerator::MT_CONTRAST,!bRetry ))
		{
			bEscape = WaitForDynamicIris ();
			bRetry = FALSE;

			if ( ! bEscape )
			{
				if ( bUseLuxValues )
					StartLuxMeasure ();

				measure=pSensor->MeasureColor(CIRELevel(WhiteIRELevel,FALSE));
				
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
				lastColor.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
				lastColor.SetSensorValue(measure);
	
				if (!pGenerator->HasPatternChanged(CGenerator::MT_CONTRAST,m_OnOffBlack,lastColor))
				{
					i--;
					bPatternRetry = TRUE;
				}
				else
				{
					m_OnOffWhite.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
					m_OnOffWhite.SetSensorValue(measure);

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

					measure=pSensor->MeasureColor(CIRELevel(NearBlackIRELevel,FALSE));	// Assume Black
					
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
					m_AnsiBlack.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
					m_AnsiBlack.SetSensorValue(measure);
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

				measure=pSensor->MeasureColor(CIRELevel(NearWhiteIRELevel,FALSE));	// Assume White
				
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
					m_AnsiWhite.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
					m_AnsiWhite.SetSensorValue(measure);

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
		bOk = pGenerator->DisplayRGBColor(clr,CGenerator::MT_ACTUAL);
		if ( bOk )
			WaitForDynamicIris ( TRUE );
	}
	else
	{
		clr = RGB(128,128,128);
		bOk = TRUE;
	}

	if ( bOk )
	{
		measuredColor=pSensor->MeasureColor(clr);  // don't know the color to measure so default value to mid gray...
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
	measurement.SetSensorToXYZMatrix(pSensor->GetSensorMatrix());
	measurement.SetSensorValue(measuredColor);
	m_measurementsArray.InsertAt(m_measurementsArray.GetSize(),measurement);
	
	FreeMeasurementAppended();

	pSensor->Release();
	if ( bDisplayColor )
		pGenerator->Release();

	m_isModified=TRUE;
	return TRUE;
}

void CMeasure::SetSensorMatrix(const Matrix & aMatrix, BOOL doPreserveSensorValues)
{
	if(doPreserveSensorValues)
	{
		CColor color;

		for(int i=0;i<m_grayMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_grayMeasureArray[i].GetSensorValue();
			m_grayMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_grayMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_nearBlackMeasureArray[i].GetSensorValue();
			m_nearBlackMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_nearBlackMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_nearWhiteMeasureArray[i].GetSensorValue();
			m_nearWhiteMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_nearWhiteMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<m_redSatMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_redSatMeasureArray[i].GetSensorValue();
			m_redSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_redSatMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_greenSatMeasureArray[i].GetSensorValue();
			m_greenSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_greenSatMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_blueSatMeasureArray[i].GetSensorValue();
			m_blueSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_blueSatMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_yellowSatMeasureArray[i].GetSensorValue();
			m_yellowSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_yellowSatMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_cyanSatMeasureArray[i].GetSensorValue();
			m_cyanSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_cyanSatMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)  // Preserve sensor values 
		{
			color=m_magentaSatMeasureArray[i].GetSensorValue();
			m_magentaSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
			m_magentaSatMeasureArray[i].SetSensorValue(color);
		}
		for(int i=0;i<3;i++)
		{
			color=m_primariesArray[i].GetSensorValue();
			m_primariesArray[i].SetSensorToXYZMatrix(aMatrix);
			m_primariesArray[i].SetSensorValue(color);
		}
		for(int i=0;i<3;i++)
		{
			color=m_secondariesArray[i].GetSensorValue();
			m_secondariesArray[i].SetSensorToXYZMatrix(aMatrix);
			m_secondariesArray[i].SetSensorValue(color);
		}
		
		color=m_OnOffBlack.GetSensorValue();
		m_OnOffBlack.SetSensorToXYZMatrix(aMatrix);
		m_OnOffBlack.SetSensorValue(color);        
		
		color=m_OnOffWhite.GetSensorValue();
		m_OnOffWhite.SetSensorToXYZMatrix(aMatrix);
		m_OnOffWhite.SetSensorValue(color);        

		color=m_AnsiBlack.GetSensorValue();
		m_AnsiBlack.SetSensorToXYZMatrix(aMatrix);
		m_AnsiBlack.SetSensorValue(color);        
		
		color=m_AnsiWhite.GetSensorValue();
		m_AnsiWhite.SetSensorToXYZMatrix(aMatrix);
		m_AnsiWhite.SetSensorValue(color);        
	}
	else
	{
		for(int i=0;i<m_grayMeasureArray.GetSize();i++)
			m_grayMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<m_nearBlackMeasureArray.GetSize();i++)
			m_nearBlackMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<m_nearWhiteMeasureArray.GetSize();i++)
			m_nearWhiteMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<m_redSatMeasureArray.GetSize();i++)
			m_redSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<m_greenSatMeasureArray.GetSize();i++)
			m_greenSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<m_blueSatMeasureArray.GetSize();i++)
			m_blueSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<m_yellowSatMeasureArray.GetSize();i++)
			m_yellowSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<m_cyanSatMeasureArray.GetSize();i++)
			m_cyanSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<m_magentaSatMeasureArray.GetSize();i++)
			m_magentaSatMeasureArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<3;i++)
			m_primariesArray[i].SetSensorToXYZMatrix(aMatrix);
		for(int i=0;i<3;i++)
			m_secondariesArray[i].SetSensorToXYZMatrix(aMatrix);

		m_OnOffBlack.SetSensorToXYZMatrix(aMatrix);
		m_OnOffWhite.SetSensorToXYZMatrix(aMatrix);
		m_AnsiBlack.SetSensorToXYZMatrix(aMatrix);
		m_AnsiWhite.SetSensorToXYZMatrix(aMatrix);
	}
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
	m_clrToMeasure = 0;
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

BOOL CMeasure::BackgroundMeasureColor ( int nCurStep, COLORREF aRGBValue )
{
	if ( m_hThread )
	{
		// Reset event indicating that a measure is ready
		ResetEvent ( m_hEventDone );
		
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
			m_grayMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_grayMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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

		measurement.SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
		measurement.SetSensorValue((*m_pBkMeasuredColor)[0]);

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

			if ( LastMeasure.GetDeltaE ( GetColorReference().GetRed ()) < 120 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetRedPrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaE ( GetColorReference().GetGreen () ) < 50 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetGreenPrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaE ( GetColorReference().GetBlue () ) < 200 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetBluePrimary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaE ( GetColorReference().GetYellow () ) < 40 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetYellowSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaE ( GetColorReference().GetCyan () ) < 40 )
			{
				// Copy real color to primary (not LastMeasure which may have been adjusted)
				SetCyanSecondary ( m_measurementsArray[n-1] );
			}
			else if ( LastMeasure.GetDeltaE ( GetColorReference().GetMagenta () ) < 100 )
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
			m_nearBlackMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_nearBlackMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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
			m_nearWhiteMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_nearWhiteMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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
			m_primariesArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_primariesArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		if ( m_nBkMeasureStepCount >= 4 )
		{
			// Store reference white.
			m_OnOffWhite.SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_OnOffWhite.SetSensorValue((*m_pBkMeasuredColor)[3]);

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
			m_OnOffBlack.SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_OnOffBlack.SetSensorValue((*m_pBkMeasuredColor)[4]);

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
			m_primariesArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_primariesArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_secondariesArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_secondariesArray[i].SetSensorValue((*m_pBkMeasuredColor)[i+3]);

			if ( bUseLuxValues )
				m_secondariesArray[i].SetLuxValue ( pLuxValues[i+3] );
			else
				m_secondariesArray[i].ResetLuxValue ();
		}

		if ( m_nBkMeasureStepCount >= 7 )
		{
			// Store reference white.
			m_OnOffWhite.SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_OnOffWhite.SetSensorValue((*m_pBkMeasuredColor)[6]);

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
			m_OnOffBlack.SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_OnOffBlack.SetSensorValue((*m_pBkMeasuredColor)[7]);

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
			m_grayMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_grayMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

			if ( bUseLuxValues )
				m_grayMeasureArray[i].SetLuxValue ( pLuxValues[i] );
			else
				m_grayMeasureArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_primariesArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_primariesArray[i].SetSensorValue((*m_pBkMeasuredColor)[m_nBkMeasureStepCount-6+i]);

			if ( bUseLuxValues )
				m_primariesArray[i].SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-6+i] );
			else
				m_primariesArray[i].ResetLuxValue ();
		}
		for (int i = 0; i < 3 ; i++ )
		{
			m_secondariesArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_secondariesArray[i].SetSensorValue((*m_pBkMeasuredColor)[m_nBkMeasureStepCount-3+i]);

			if ( bUseLuxValues )
				m_secondariesArray[i].SetLuxValue ( pLuxValues[m_nBkMeasureStepCount-3+i] );
			else
				m_secondariesArray[i].ResetLuxValue ();
		}

		m_OnOffWhite.SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
		m_OnOffWhite.SetSensorValue((*m_pBkMeasuredColor)[m_nBkMeasureStepCount-7]);
		m_OnOffBlack.SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
		m_OnOffBlack.SetSensorValue((*m_pBkMeasuredColor)[0]);

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
			m_redSatMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_redSatMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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
			m_greenSatMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_greenSatMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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
			m_blueSatMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_blueSatMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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
			m_yellowSatMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_yellowSatMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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
			m_cyanSatMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_cyanSatMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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
			m_magentaSatMeasureArray[i].SetSensorToXYZMatrix(m_pBkMeasureSensor->GetSensorMatrix());
			m_magentaSatMeasureArray[i].SetSensorValue((*m_pBkMeasuredColor)[i]);

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

CColor CMeasure::GetGray(int i) const 
{
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_grayMeasureArray[i]);
	else
		return m_grayMeasureArray[i]; 
} 

CColor CMeasure::GetNearBlack(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_nearBlackMeasureArray[i]);
	else
		return m_nearBlackMeasureArray[i]; 
} 

CColor CMeasure::GetNearWhite(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_nearWhiteMeasureArray[i]);
	else
		return m_nearWhiteMeasureArray[i]; 
} 

CColor CMeasure::GetRedSat(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_redSatMeasureArray[i]);
	else
		return m_redSatMeasureArray[i]; 
} 

CColor CMeasure::GetGreenSat(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_greenSatMeasureArray[i]);
	else
		return m_greenSatMeasureArray[i]; 
} 

CColor CMeasure::GetBlueSat(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_blueSatMeasureArray[i]);
	else
		return m_blueSatMeasureArray[i]; 
} 

CColor CMeasure::GetYellowSat(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_yellowSatMeasureArray[i]);
	else
		return m_yellowSatMeasureArray[i]; 
} 

CColor CMeasure::GetCyanSat(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_cyanSatMeasureArray[i]);
	else
		return m_cyanSatMeasureArray[i]; 
} 

CColor CMeasure::GetMagentaSat(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_magentaSatMeasureArray[i]);
	else
		return m_magentaSatMeasureArray[i]; 
} 

CColor CMeasure::GetPrimary(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_primariesArray[i]);
	else
		return m_primariesArray[i]; 
} 

CColor CMeasure::GetRedPrimary(BOOL bBypassAdjust) const 
{ 
	if ( bBypassAdjust )
		return m_primariesArray[0];
	else
		return GetPrimary(0);
} 

CColor CMeasure::GetGreenPrimary(BOOL bBypassAdjust) const 
{ 
	if ( bBypassAdjust )
		return m_primariesArray[1];
	else
		return GetPrimary(1);
} 

CColor CMeasure::GetBluePrimary(BOOL bBypassAdjust) const 
{ 
	if ( bBypassAdjust )
		return m_primariesArray[2];
	else
		return GetPrimary(2);
} 

CColor CMeasure::GetSecondary(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_secondariesArray[i]);
	else
		return m_secondariesArray[i]; 
} 

CColor CMeasure::GetYellowSecondary(BOOL bBypassAdjust) const 
{ 
	if ( bBypassAdjust )
		return m_secondariesArray[0];
	else
		return GetSecondary(0);
} 

CColor CMeasure::GetCyanSecondary(BOOL bBypassAdjust) const 
{ 
	if ( bBypassAdjust )
		return m_secondariesArray[1];
	else
		return GetSecondary(1);
} 

CColor CMeasure::GetMagentaSecondary(BOOL bBypassAdjust) const 
{ 
	if ( bBypassAdjust )
		return m_secondariesArray[2];
	else
		return GetSecondary(2);
} 

CColor CMeasure::GetAnsiBlack(BOOL bBypassAdjust) const 
{ 
	CColor clr;

	if ( m_bUseAdjustmentMatrix && ! bBypassAdjust )
		clr = ComputeAdjustedColor(m_AnsiBlack);
	else
		clr = m_AnsiBlack; 

	return clr;
} 

CColor CMeasure::GetAnsiWhite(BOOL bBypassAdjust) const 
{ 
	CColor clr;

	if ( m_bUseAdjustmentMatrix && ! bBypassAdjust )
		clr = ComputeAdjustedColor(m_AnsiWhite);
	else
		clr = m_AnsiWhite; 

	return clr;
} 

CColor CMeasure::GetOnOffBlack(BOOL bBypassAdjust) const 
{ 
	CColor clr;

	if ( m_bUseAdjustmentMatrix && ! bBypassAdjust )
		clr = ComputeAdjustedColor(m_OnOffBlack);
	else
		clr = m_OnOffBlack; 

	return clr;
} 

CColor CMeasure::GetOnOffWhite(BOOL bBypassAdjust) const 
{ 
	CColor clr;

	if ( m_bUseAdjustmentMatrix && ! bBypassAdjust )
		clr = ComputeAdjustedColor(m_OnOffWhite);
	else
		clr = m_OnOffWhite; 

	return clr;
} 

CColor CMeasure::GetMeasurement(int i) const 
{ 
	if ( m_bUseAdjustmentMatrix )
		return ComputeAdjustedColor(m_measurementsArray[i]);
	else
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

CColor CMeasure::ComputeAdjustedColor(const CColor & aColor) const 
{
	CColor clr;

	if ( aColor != noDataColor )
	{
		clr = m_XYZAdjustmentMatrix*aColor;

		if ( aColor.HasLuxValue () )
			clr.SetLuxValue ( aColor.GetLuxValue () );

		if ( aColor.HasSpectrum () )
			clr.SetSpectrum ( aColor.GetSpectrum () );
	}
	else
	{
		clr = noDataColor;
	}
	return clr;
}

CColor CMeasure::GetRefPrimary(int i) const
{
	switch ( i )
	{
		case 0:	// red
			 return GetColorReference().GetRed();
			 break;

		case 1:	// green
			 return GetColorReference().GetGreen();
			 break;

		case 2:	// blue
			 return GetColorReference().GetBlue();
			 break;
	}

	// Cannot execute this if "i" is OK.
	ASSERT(0);
	return noDataColor;
}

CColor CMeasure::GetRefSecondary(int i) const
{
	switch ( i )
	{
		case 0: // Yellow
			 return GetColorReference().GetYellow();
			 break;

		case 1:	// Cyan
			 return GetColorReference().GetCyan();
			 break;

		case 2:	// Magenta
			 return GetColorReference().GetMagenta();
			 break;
	}

	// Cannot execute this if "i" is OK.
	ASSERT(0);
	return noDataColor;
}

CColor CMeasure::GetRefSat(int i, double sat_percent) const
{
	CColor	refColor;
	CColor	refWhite = GetColorReference().GetWhite().GetxyYValue();
	
	double	x, y;
	double	xstart = refWhite[0];
	double	ystart = refWhite[1];
	double	YLuma;

	if ( i < 3 )
		refColor = GetPrimary(i);
	else
		refColor = GetSecondary(i-3);

	if ( refColor == noDataColor )
	{
		if ( i < 3 )
			refColor = GetRefPrimary(i);
		else
			refColor = GetRefSecondary(i-3);
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
	
	aColor.SetxyYValue ( CColor(x, y, YLuma) );
	
	return aColor;
}

