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
//	François-Xavier CHABOUD
//  Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Color.h"
#include <math.h>

#include "ColorHCFR.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/*
 *      Name:   XYZtoCorColorTemp.c
 *
 *      Author: Bruce Justin Lindbloom
 *
 *      Copyright (c) 2003 Bruce Justin Lindbloom. All rights reserved.
 *
 *      Input:  xyz = pointer to the input array of X, Y and Z color components (in that order).
 *              temp = pointer to where the computed correlated color temperature should be placed.
 *
 *      Output: *temp = correlated color temperature, if successful.
 *                    = unchanged if unsuccessful.
 *
 *      Return: 0 if successful, else -1.
 *
 *      Description:
 *              This is an implementation of Robertson's method of computing the correlated color
 *              temperature of an XYZ color. It can compute correlated color temperatures in the
 *              range [1666.7K, infinity].
 *
 *      Reference:
 *              "Color Science: Concepts and Methods, Quantitative Data and Formulae", Second Edition,
 *              Gunter Wyszecki and W. S. Stiles, John Wiley & Sons, 1982, pp. 227, 228.
 */

#include <float.h>
#include <math.h>
                        
/* LERP(a,b,c) = linear interpolation macro, is 'a' when c == 0.0 and 'b' when c == 1.0 */
#define LERP(a,b,c)     (((b) - (a)) * (c) + (a))

typedef struct UVT {
        double  u;
        double  v;
        double  t;
} UVT;

double rt[31] = {       /* reciprocal temperature (K) */
         DBL_MIN,  10.0e-6,  20.0e-6,  30.0e-6,  40.0e-6,  50.0e-6,
         60.0e-6,  70.0e-6,  80.0e-6,  90.0e-6, 100.0e-6, 125.0e-6,
        150.0e-6, 175.0e-6, 200.0e-6, 225.0e-6, 250.0e-6, 275.0e-6,
        300.0e-6, 325.0e-6, 350.0e-6, 375.0e-6, 400.0e-6, 425.0e-6,
        450.0e-6, 475.0e-6, 500.0e-6, 525.0e-6, 550.0e-6, 575.0e-6,
        600.0e-6
};
UVT uvt[31] = {
        {0.18006, 0.26352, -0.24341},
        {0.18066, 0.26589, -0.25479},
        {0.18133, 0.26846, -0.26876},
        {0.18208, 0.27119, -0.28539},
        {0.18293, 0.27407, -0.30470},
        {0.18388, 0.27709, -0.32675},
        {0.18494, 0.28021, -0.35156},
        {0.18611, 0.28342, -0.37915},
        {0.18740, 0.28668, -0.40955},
        {0.18880, 0.28997, -0.44278},
        {0.19032, 0.29326, -0.47888},
        {0.19462, 0.30141, -0.58204},
        {0.19962, 0.30921, -0.70471},
        {0.20525, 0.31647, -0.84901},
        {0.21142, 0.32312, -1.0182},
        {0.21807, 0.32909, -1.2168},
        {0.22511, 0.33439, -1.4512},
        {0.23247, 0.33904, -1.7298},
        {0.24010, 0.34308, -2.0637},
        {0.24702, 0.34655, -2.4681},
        {0.25591, 0.34951, -2.9641},
        {0.26400, 0.35200, -3.5814},
        {0.27218, 0.35407, -4.3633},
        {0.28039, 0.35577, -5.3762},
        {0.28863, 0.35714, -6.7262},
        {0.29685, 0.35823, -8.5955},
        {0.30505, 0.35907, -11.324},
        {0.31320, 0.35968, -15.628},
        {0.32129, 0.36011, -23.325},
        {0.32931, 0.36038, -40.770},
        {0.33724, 0.36051, -116.45}
};

int XYZtoCorColorTemp(double *xyz, double *temp)
{
        double us, vs, p, di, dm;
        int i;


        if ((xyz[0] < 1.0e-20) && (xyz[1] < 1.0e-20) && (xyz[2] < 1.0e-20))
                return(-1);     /* protect against possible divide-by-zero failure */
        us = (4.0 * xyz[0]) / (xyz[0] + 15.0 * xyz[1] + 3.0 * xyz[2]);
        vs = (6.0 * xyz[1]) / (xyz[0] + 15.0 * xyz[1] + 3.0 * xyz[2]);
        dm = 0.0;
        for (i = 0; i < 31; i++) {
                di = (vs - uvt[i].v) - uvt[i].t * (us - uvt[i].u);
                if ((i > 0) && (((di < 0.0) && (dm >= 0.0)) || ((di >= 0.0) && (dm < 0.0))))
                        break;  /* found lines bounding (us, vs) : i-1 and i */
                dm = di;
        }
        if (i == 31)
                return(-1);     /* bad XYZ input, color temp would be less than minimum of 1666.7 degrees, or too far towards blue */
        di = di / sqrt(1.0 + uvt[i    ].t * uvt[i    ].t);
        dm = dm / sqrt(1.0 + uvt[i - 1].t * uvt[i - 1].t);
        p = dm / (dm - di);     /* p = interpolation parameter, 0.0 : i-1, 1.0 : i */
        p = 1.0 / (LERP(rt[i - 1], rt[i], p));
        *temp = p;
        return(0);      /* success */
}

///////////////////////////////////////////////////////////////////
// Sensor references
///////////////////////////////////////////////////////////////////

double defaultSensorToXYZ[3][3] = {	{   7.79025E-05,  5.06389E-05,   6.02556E-05  }, 
									{   3.08665E-05,  0.000131285,   2.94813E-05  },
									{  -9.41924E-07, -4.42599E-05,   0.000271669  } };

//Matrix defaultSensorToXYZMatrix=IdentityMatrix(3);
Matrix defaultSensorToXYZMatrix(&defaultSensorToXYZ[0][0],3,3);
Matrix defaultXYZToSensorMatrix=defaultSensorToXYZMatrix.GetInverse();

CColor noDataColor(FX_NODATA,FX_NODATA,FX_NODATA);

///////////////////////////////////////////////////////////////////
// Color references
///////////////////////////////////////////////////////////////////

CRITICAL_SECTION MatrixCritSec;
static BOOL bMatrixCritSecInit = FALSE;

CColor illuminantA(0.447593,0.407539);
CColor illuminantB(0.348483,0.351747);
CColor illuminantC(0.310115,0.316311);
CColor illuminantE(0.333334,0.333333);
CColor illuminantD50(0.345669,0.358496);
CColor illuminantD55(0.332406,0.347551);
CColor illuminantD65(0.312712,0.329008);
CColor illuminantD75(0.299023,0.314963);

double primariesPAL[3][2] =	{	{ 0.6400, 0.3300 },
								{ 0.2900, 0.6000 },
								{ 0.1500, 0.0600 }	};

double primariesRec601[3][3] ={	{ 0.6300, 0.3400 },
								{ 0.3100, 0.5950 },
								{ 0.1550, 0.0700 }	};

double primariesRec709[3][2] ={	{ 0.6400, 0.3300 },
								{ 0.3000, 0.6000 },
								{ 0.1500, 0.0600 }	};

#define theColorReference	(*(GetColorApp()->m_pColorReference));

Matrix ComputeRGBtoXYZMatrix(Matrix primariesChromacities,CColor whiteChromacity)
{
	// Compute RGB to XYZ matrix
	Matrix coefJ(0.0,3,1);
	coefJ=primariesChromacities.GetInverse();
	coefJ=coefJ*whiteChromacity;

	Matrix scaling(0.0,3,3);
	scaling(0,0)=coefJ(0,0)/whiteChromacity(1,0);
	scaling(1,1)=coefJ(1,0)/whiteChromacity(1,0);
	scaling(2,2)=coefJ(2,0)/whiteChromacity(1,0);

	Matrix m=primariesChromacities*scaling;
	return m;
}

// For PAL/SECAM: EBU Tech 3213 
/*double PAL_to_XYZ[3][3] =	{	{ 0.430587,  0.222021,  0.0201837 },
								{ 0.341545,  0.706645,  0.129551  },
								{ 0.178336,  0.0713342, 0.939234  }	};

double PAL_to_XYZ[3][3] =	{	{ 0.430587,  0.341545,  0.178336 },
								{ 0.222021,  0.706645,  0.0713342  },
								{ 0.0201837,  0.129551, 0.939234  }	};

// For NTSC: NTSC RP145 (Not the old obsolete NTSC primaries) 
// double NTSC_to_XYZ[3][3] = {	{ 0.606734,  0.298839,  0.000000  },
//								{ 0.173564,  0.586811,  0.0661196 },
//								{ 0.200112,  0.114350,  1.11491   }	}; 

double NTSC_to_XYZ[3][3] = {	{ 0.606734,  0.173564,  0.200112  },
								{ 0.298839,  0.586811,  0.114350 },
								{ 0.000000,  0.0661196,  1.11491   }	};

// For HDTV and SRGB: Rec709
double Rec709_to_XYZ[3][3] = {	{ 0.412453,  0.357580,  0.180423  },
								{ 0.212671,  0.715160,  0.072169  },
								{ 0.019334,  0.119193,  0.950227  }	}; */


CColorReference::CColorReference(ColorStandard aColorStandard, WhiteTarget aWhiteTarget, double aGamma)
{
	CString	strModified;
	m_standard = aColorStandard;

	if ( ! bMatrixCritSecInit )
	{
		InitializeCriticalSection ( & MatrixCritSec );
		bMatrixCritSecInit = TRUE;
		srand((unsigned)time(NULL));
	}
	
	strModified.LoadString ( IDS_MODIFIED );
	
	switch(aColorStandard)
	{
		case PALSECAM:
			standardName="PAL/SECAM";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			redPrimary.  SetxyYValue(CColor(primariesPAL[0][0],primariesPAL[0][1],1));
			greenPrimary.SetxyYValue(CColor(primariesPAL[1][0],primariesPAL[1][1],1));
			bluePrimary. SetxyYValue(CColor(primariesPAL[2][0],primariesPAL[2][1],1));
			break;
		case SDTV:
			standardName="SDTV Rec601";
			whiteColor=illuminantD65;
			m_white=D65;
			whiteName="D65";
			redPrimary.  SetxyYValue(CColor(primariesRec601[0][0],primariesRec601[0][1],1));
			greenPrimary.SetxyYValue(CColor(primariesRec601[1][0],primariesRec601[1][1],1));
			bluePrimary. SetxyYValue(CColor(primariesRec601[2][0],primariesRec601[2][1],1));
			break;
		case HDTV:
			standardName="HDTV Rec709";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			redPrimary.  SetxyYValue(CColor(primariesRec709[0][0],primariesRec709[0][1],1));
			greenPrimary.SetxyYValue(CColor(primariesRec709[1][0],primariesRec709[1][1],1));
			bluePrimary. SetxyYValue(CColor(primariesRec709[2][0],primariesRec709[2][1],1));
			break;
		case sRGB:
			standardName="sRGB";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			redPrimary.  SetxyYValue(CColor(primariesRec709[0][0],primariesRec709[0][1],1));
			greenPrimary.SetxyYValue(CColor(primariesRec709[1][0],primariesRec709[1][1],1));
			bluePrimary. SetxyYValue(CColor(primariesRec709[2][0],primariesRec709[2][1],1));
			break;
		default:
			standardName.LoadString ( IDS_UNKNOWN );
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			redPrimary.  SetxyYValue(CColor(primariesRec601[0][0],primariesRec601[0][1],1));
			greenPrimary.SetxyYValue(CColor(primariesRec601[1][0],primariesRec601[1][1],1));
			bluePrimary. SetxyYValue(CColor(primariesRec601[2][0],primariesRec601[2][1],1));
	}

	if(aWhiteTarget != Default)
		m_white = aWhiteTarget;

	switch(aWhiteTarget)
	{
		case D65:
			break;
		case D55:
			standardName+=strModified;
			whiteColor=illuminantD55;
			whiteName="D55";
			break;
		case D50:
			standardName+=strModified;
			whiteColor=illuminantD50;
			whiteName="D50";
			break;
		case D75:
			standardName+=strModified;
			whiteColor=illuminantD75;
			whiteName="D75";
			break;
		case A:
			standardName+=strModified;
			whiteColor=illuminantA;
			whiteName="A";
			break;
		case B:
			standardName+=strModified;
			whiteColor=illuminantB;
			whiteName="B";
			break;
		case C:
			standardName+=strModified;
			whiteColor=illuminantC;
			whiteName="C";
			break;
		case E:
			if(aColorStandard == SDTV)
				break;
			standardName+=strModified;
			whiteColor=illuminantE;
			whiteName="E";
			break;
		case Default:
		default:
			break;
	}

	// Compute u'v' for current white
	double x = whiteColor.GetxyYValue () [ 0 ];
	double y = whiteColor.GetxyYValue () [ 1 ];
	u_white = 4.0*x / (-2.0*x + 12.0*y + 3.0); 
	v_white = 9.0*y / (-2.0*x + 12.0*y + 3.0); 

	// Compute transformation matrices
	Matrix primariesMatrix;
	primariesMatrix=redPrimary;
	primariesMatrix.CMAC(greenPrimary);
	primariesMatrix.CMAC(bluePrimary);
	RGBtoXYZMatrix=ComputeRGBtoXYZMatrix(primariesMatrix, whiteColor);
	XYZtoRGBMatrix=RGBtoXYZMatrix.GetInverse();

	// Adjust reference primary colors Y values relatively to white Y
	CColor aColor;

	aColor = redPrimary.GetxyYValue ();
	aColor.SetZ ( GetRedReferenceLuma () );
	redPrimary.SetxyYValue ( aColor );

	aColor = greenPrimary.GetxyYValue ();
	aColor.SetZ ( GetGreenReferenceLuma () );
	greenPrimary.SetxyYValue ( aColor );

	aColor = bluePrimary.GetxyYValue ();
	aColor.SetZ ( GetBlueReferenceLuma () );
	bluePrimary.SetxyYValue ( aColor );

	UpdateSecondary ( yellowSecondary, redPrimary, greenPrimary, bluePrimary );
	UpdateSecondary ( cyanSecondary, greenPrimary, bluePrimary, redPrimary );
	UpdateSecondary ( magentaSecondary, bluePrimary, redPrimary, greenPrimary );
}

CColorReference::~CColorReference()
{
}

void CColorReference::UpdateSecondary ( CColor & secondary, const CColor & primary1, const CColor & primary2, const CColor & primaryOpposite )
{
	// Compute intersection between line (primary1-primary2) and line (primaryOpposite-white)
	CColor	prim1 = primary1.GetxyYValue ();
	CColor	prim2 = primary2.GetxyYValue ();
	CColor	primOppo = primaryOpposite.GetxyYValue ();
	CColor	white = GetWhite ().GetxyYValue ();

	double x1 = prim1[0];
	double y1 = prim1[1];
	double x2 = white[0];
	double y2 = white[1];
	double dx1 = prim2[0] - x1;
	double dy1 = prim2[1] - y1;
	double dx2 = primOppo[0] - x2;
	double dy2 = primOppo[1] - y2;
	
	double k = ( ( ( x2 - x1 ) / dx1 ) + ( dx2 / ( dx1 * dy2 ) ) * ( y1 - y2 ) ) / ( 1.0 - ( ( dx2 * dy1 ) / ( dx1 * dy2 ) ) );

	CColor	aColor ( x1 + k * dx1, y1 + k * dy1, primary1.GetY() + primary2.GetY() );

	secondary.SetxyYValue ( aColor );
}

CColorReference & GetColorReference()
{
	return theColorReference;
}

CColorReference GetStandardColorReference(ColorStandard aColorStandard)
{
	CColorReference aStandardRef(aColorStandard);
	return aStandardRef;
}

void SetColorReference(ColorStandard aColorStandard, WhiteTarget aWhiteTarget, double gamma)
{
	*(GetColorApp()->m_pColorReference)=CColorReference(aColorStandard,aWhiteTarget,gamma);
}


/////////////////////////////////////////////////////////////////////
// CIRELevel Class
//
//////////////////////////////////////////////////////////////////////

CIRELevel::CIRELevel(double aIRELevel, BOOL bIRE, BOOL b16_235)
{
	m_b16_235 = b16_235;
	if ( bIRE )
	{
		// IRE levels: adjust 7.5-100 scale to percent scale
		if ( aIRELevel <= 7.5 )
			aIRELevel = 0;
		else 
			aIRELevel = (aIRELevel-7.5)/0.925;
	}

	m_redIRELevel=m_greenIRELevel=m_blueIRELevel=aIRELevel;
}

CIRELevel::CIRELevel(double aRedIRELevel,double aGreenIRELevel,double aBlueIRELevel,BOOL bIRE,BOOL b16_235)
{
	m_b16_235 = b16_235;
	if ( bIRE )
	{
		// IRE levels: adjust 7.5-100 scale to percent scale
		if ( aRedIRELevel <= 7.5 )
			aRedIRELevel = 0;
		else 
			aRedIRELevel = (aRedIRELevel-7.5)/0.925;

		if ( aGreenIRELevel <= 7.5 )
			aGreenIRELevel = 0;
		else 
			aGreenIRELevel = (aGreenIRELevel-7.5)/0.925;

		if ( aBlueIRELevel <= 7.5 )
			aBlueIRELevel = 0;
		else 
			aBlueIRELevel = (aBlueIRELevel-7.5)/0.925;
	}

	m_redIRELevel=aRedIRELevel;
	m_greenIRELevel=aGreenIRELevel;
	m_blueIRELevel=aBlueIRELevel;
}

CIRELevel::operator COLORREF()
{
	double coef;

	if ( m_b16_235 )
	{
		coef=(235.0-16.0)/100.0;
		return RGB((BYTE)(16.0+ceil(m_redIRELevel*coef)),(BYTE)(16.0+ceil(m_greenIRELevel*coef)),(BYTE)(16.0+ceil(m_blueIRELevel*coef)));
	}
	else
	{
		coef=255.0/100.0;
		return RGB((BYTE)(ceil(m_redIRELevel*coef)),(BYTE)(ceil(m_greenIRELevel*coef)),(BYTE)(ceil(m_blueIRELevel*coef)));
	}
}

/////////////////////////////////////////////////////////////////////
// ColorMeasure.cpp: implementation of the CColor class.
//
//////////////////////////////////////////////////////////////////////

CColor::CColor():Matrix(1.0,3,1)
{
	m_XYZtoSensorMatrix = defaultXYZToSensorMatrix;
	m_SensorToXYZMatrix = defaultSensorToXYZMatrix;
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(CColor &aColor):Matrix(aColor)
{
	m_XYZtoSensorMatrix = aColor.m_XYZtoSensorMatrix;
	m_SensorToXYZMatrix = aColor.m_SensorToXYZMatrix;
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;

	if ( aColor.m_pSpectrum )
		m_pSpectrum = new CSpectrum ( *aColor.m_pSpectrum );

	if ( aColor.m_pLuxValue )
		m_pLuxValue = new double ( *aColor.m_pLuxValue );
}

CColor::CColor(Matrix aMatrix):Matrix(aMatrix)
{
	ASSERT(aMatrix.GetColumns() == 1);
	ASSERT(aMatrix.GetRows() == 3);

	m_XYZtoSensorMatrix = defaultXYZToSensorMatrix;
	m_SensorToXYZMatrix = defaultSensorToXYZMatrix;
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(double ax,double ay):Matrix(0.0,3,1)
{
	SetxyYValue(CColor(ax,ay,1));

	m_XYZtoSensorMatrix = defaultXYZToSensorMatrix;
	m_SensorToXYZMatrix = defaultSensorToXYZMatrix;
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(double aX,double aY, double aZ):Matrix(0.0,3,1)
{
	SetX(aX);
	SetY(aY);
	SetZ(aZ);

	m_XYZtoSensorMatrix = defaultXYZToSensorMatrix;
	m_SensorToXYZMatrix = defaultSensorToXYZMatrix;
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::~CColor()
{
	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}

	if ( m_pLuxValue )
	{
		delete m_pLuxValue;
		m_pLuxValue = NULL;
	}
}

CColor& CColor::operator =(const CColor& aColor)
{
	if(&aColor == this)
		return *this;

	Matrix::operator=(aColor);

	m_XYZtoSensorMatrix = aColor.m_XYZtoSensorMatrix;
	m_SensorToXYZMatrix = aColor.m_SensorToXYZMatrix;

	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}

	if ( aColor.m_pSpectrum )
		m_pSpectrum = new CSpectrum ( *aColor.m_pSpectrum );

	if ( m_pLuxValue )
	{
		delete m_pLuxValue;
		m_pLuxValue = NULL;
	}

	if ( aColor.m_pLuxValue )
		m_pLuxValue = new double ( *aColor.m_pLuxValue );

	return *this;
}

void CColor::Serialize(CArchive& archive)
{
	if (archive.IsStoring())
	{
		int version=1;

		if ( m_pLuxValue )
		{
			if ( m_pSpectrum )
				version = 4;
			else
				version = 3;
		}
		else if ( m_pSpectrum )
		{
			version = 2;
		}

		archive << version;
		Matrix::Serialize(archive) ;
		m_XYZtoSensorMatrix.Serialize(archive);
		m_SensorToXYZMatrix.Serialize(archive);

		if ( m_pSpectrum )
		{
			archive << m_pSpectrum -> GetRows ();
			archive << m_pSpectrum -> m_WaveLengthMin;
			archive << m_pSpectrum -> m_WaveLengthMax;
			archive << m_pSpectrum -> m_BandWidth;
			m_pSpectrum -> Serialize(archive);
		}

		if ( m_pLuxValue )
			archive << (* m_pLuxValue);
	}
	else
	{
		int version;
		archive >> version;
		
		if ( version > 4 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		
		Matrix::Serialize(archive) ;
		m_XYZtoSensorMatrix.Serialize(archive);
		m_SensorToXYZMatrix.Serialize(archive);

		if ( m_pSpectrum )
		{
			delete m_pSpectrum;
			m_pSpectrum = NULL;
		}

		if ( m_pLuxValue )
		{
			delete m_pLuxValue;
			m_pLuxValue = NULL;
		}

		if ( version == 2 || version == 4 )
		{
			int NbBands, WaveLengthMin, WaveLengthMax, BandWidth;
			archive >> NbBands;
			archive >> WaveLengthMin;
			archive >> WaveLengthMax;
			archive >> BandWidth;
			m_pSpectrum = new CSpectrum ( NbBands, WaveLengthMin, WaveLengthMax, BandWidth );
			m_pSpectrum -> Serialize(archive);
		}

		if ( version == 3 || version == 4 )
		{
			m_pLuxValue = new double;
			archive >> (* m_pLuxValue);
		}
	}
}

double & CColor::operator[](const int nRow) const
{ 
	ASSERT(nRow < GetRows());
	return Matrix::operator ()(nRow,0); 
}

double CColor::GetLuminance() const
{
	return GetY();
}

double CColor::GetDeltaE(double YWhite) const
{
	return GetDeltaE ( YWhite, GetColorReference().GetWhite(), 1.0 );
}


double CColor::GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef) const
{
	double dE;
	double x=this->GetxyYValue()[0];
	double y=this->GetxyYValue()[1];
	double u = 4.0*x / (-2.0*x + 12.0*y + 3.0); 
	double v = 9.0*y / (-2.0*x + 12.0*y + 3.0); 
	double xRef=refColor.GetxyYValue()[0];
	double yRef=refColor.GetxyYValue()[1];
	double uRef = 4.0*xRef / (-2.0*xRef + 12.0*yRef + 3.0); 
	double vRef = 9.0*yRef / (-2.0*xRef + 12.0*yRef + 3.0); 
	
	if ( GetConfig()->m_bUseOldDeltaEFormula )
	{
		dE = 1300.0 * sqrt ( pow((u - uRef),2) + pow((v - vRef),2) );
	}
	else
	{
		double u_white = GetColorReference().GetWhite_uValue();
		double v_white = GetColorReference().GetWhite_vValue();
		double L = GetLValue ( YWhite );
		double LRef = refColor.GetLValue ( YWhiteRef );

		double uu = 13.0 * L * ( u - u_white );
		double vv = 13.0 * L * ( v - v_white );
		double uuRef = 13.0 * LRef * ( uRef - u_white );
		double vvRef = 13.0 * LRef * ( vRef - v_white );

		dE = sqrt ( pow ((L - LRef),2) + pow((uu - uuRef),2) + pow((vv - vvRef),2) );
	}

	return dE;
}

double CColor::GetDeltaE(const CColor & refColor) const
{
	double dE;
	double x=this->GetxyYValue()[0];
	double y=this->GetxyYValue()[1];
	double u = 4.0*x / (-2.0*x + 12.0*y + 3.0); 
	double v = 9.0*y / (-2.0*x + 12.0*y + 3.0); 
	double xRef=refColor.GetxyYValue()[0];
	double yRef=refColor.GetxyYValue()[1];
	double uRef = 4.0*xRef / (-2.0*xRef + 12.0*yRef + 3.0); 
	double vRef = 9.0*yRef / (-2.0*xRef + 12.0*yRef + 3.0); 
	
	dE = 1300.0 * sqrt ( pow((u - uRef),2) + pow((v - vRef),2) );

	return dE;
}

double CColor::GetDeltaxy(const CColor & refColor) const
{
	double x=this->GetxyYValue()[0];
	double y=this->GetxyYValue()[1];
	double xRef=refColor.GetxyYValue()[0];
	double yRef=refColor.GetxyYValue()[1];
	double dxy = sqrt ( pow((x - xRef),2) + pow((y - yRef),2) );

	return dxy;
}


int CColor::GetColorTemp() const
{
	double aXYZ[3];
	double aColorTemp;

	aXYZ[0]=GetX();
	aXYZ[1]=GetY();
	aXYZ[2]=GetZ();

	if(XYZtoCorColorTemp(aXYZ,&aColorTemp) == 0)
		return (int)aColorTemp;
	else
	{
		if(GetRGBValue()[0] >  GetRGBValue()[2]) // if red > blue then TC < 1500 else TC > 12000
			return 1499;
		else
			return 12001;
	}
}

CColor CColor::GetXYZValue() const 
{
	return *this;
}

CColor CColor::GetRGBValue() const 
{
	CColor clr;

	if(*this != noDataColor)
	{
		EnterCriticalSection ( & MatrixCritSec );
		clr = GetColorReference().GetXYZtoRGBMatrix()*(*this);
		LeaveCriticalSection ( & MatrixCritSec );
	}
	else 
		clr = noDataColor;

	return clr;
}

CColor CColor::GetSensorValue() const 
{
	CColor clr;

	if(*this != noDataColor)
	{
		EnterCriticalSection ( & MatrixCritSec );
		clr = m_XYZtoSensorMatrix*(*this);
		LeaveCriticalSection ( & MatrixCritSec );
	}
	else 
		clr = noDataColor;

	return clr;
}


CColor CColor::GetxyYValue() const 
{
	if(*this == noDataColor)
		return noDataColor;

	CColor aColor;
	double sum = GetX() + GetY() +GetZ();
	aColor.SetX(sum!=0.0?(GetX()/sum):0.0);
	aColor.SetY(sum!=0.0?(GetY()/sum):0.0);
	aColor.SetZ(GetY());

	return aColor;
}

double CColor::GetLValue(double YWhiteRef) const 
{
	if(*this == noDataColor)
		return 0.0;

	if ( YWhiteRef <= 0.0 )
	{
		// No white reference: use full luma value (use of default value for Delta E)
		return 100.0;
	}

	double var_Y = GetY()/YWhiteRef;

	if (var_Y > 0.008856)
		var_Y = pow(var_Y,1.0/3.0);
	else
		var_Y = (7.787*var_Y)+(16/116.0);

	return 116.0*var_Y-16.0;
}

CColor CColor::GetLabValue(double YWhiteRef) const 
{
	if(*this == noDataColor)
		return noDataColor;

	CColor aColor;
//	double var_X = GetX()/95.047;          //Observer = 2°, Illuminant = D65
//	double var_Y = GetY()/100.000;
//	double var_Z = GetZ()/108.883;

	double scaling = YWhiteRef/GetColorReference().GetWhite().GetY();

	double var_X = GetX()/(GetColorReference().GetWhite().GetX()*scaling);
	double var_Y = GetY()/(GetColorReference().GetWhite().GetY()*scaling);
	double var_Z = GetZ()/(GetColorReference().GetWhite().GetZ()*scaling);

	if ( var_X > 0.008856 ) 
		var_X = pow(var_X,1.0/3.0);
	else
		var_X = (7.787*var_X)+(16.0/116.0);

	if (var_Y > 0.008856)
		var_Y = pow(var_Y,1.0/3.0);
	else
		var_Y = (7.787*var_Y)+(16/116.0);

	if (var_Z > 0.008856)
		var_Z = pow(var_Z,1.0/3.0);
	else
		var_Z = (7.787*var_Z)+(16.0/116.0);

	aColor.SetX(116.0*var_Y-16.0);	 // CIE-L*
	aColor.SetY(500.0*(var_X-var_Y)); // CIE-a*
	aColor.SetZ(200.0*(var_Y-var_Z)); //CIE-b*

	return aColor;
}

CColor CColor::GetLCHValue(double YWhiteRef) const 
{
	if(*this == noDataColor)
		return noDataColor;

	CColor Lab = GetLabValue (YWhiteRef);

	double L = Lab[0];
	double C = sqrt ( (Lab[1]*Lab[1]) + (Lab[2]*Lab[2]) );
	double H = atan2 ( Lab[2], Lab[1] ) / acos(-1.0) * 180.0;

	if ( H < 0.0 )
		H += 360.0;

	CColor aColor;

	aColor.SetX(L);
	aColor.SetY(C);
	aColor.SetZ(H);

	return aColor;
}

void CColor::SetXYZValue(const CColor & aColor) 
{
	SetX(aColor.GetX()); 
	SetY(aColor.GetY()); 
	SetZ(aColor.GetZ()); 
}

void CColor::SetRGBValue(const CColor & aColor) 
{
	if(aColor == noDataColor)
	{
		*this=aColor;
		return;
	}

	CColor temp;

	EnterCriticalSection ( & MatrixCritSec );
	temp=GetColorReference().GetRGBtoXYZMatrix()*aColor;
	LeaveCriticalSection ( & MatrixCritSec );

	SetX(temp.GetX()); 
	SetY(temp.GetY()); 
	SetZ(temp.GetZ()); 
}

void CColor::SetSensorValue(const CColor & aColor) 
{
	if(aColor == noDataColor)
	{
		*this=aColor;
		return;
	}

	CColor temp;

	EnterCriticalSection ( & MatrixCritSec );
	temp=m_SensorToXYZMatrix*aColor;
	LeaveCriticalSection ( & MatrixCritSec );
	
	SetX(temp.GetX()); 
	SetY(temp.GetY()); 
	SetZ(temp.GetZ()); 

	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}

	if ( aColor.m_pSpectrum )
		m_pSpectrum = new CSpectrum ( *aColor.m_pSpectrum );

	if ( m_pLuxValue )
	{
		delete m_pLuxValue;
		m_pLuxValue = NULL;
	}

	if ( aColor.m_pLuxValue )
		m_pLuxValue = new double ( *aColor.m_pLuxValue );
}

void CColor::SetxyYValue(const CColor & aColor) 
{
	if(aColor == noDataColor)
	{
		*this=aColor;
		return;
	}

	double aX,aY,aZ;
	if(aColor.GetY() != 0)
	{
		aX=(aColor.GetX()/aColor.GetY())*aColor.GetZ();
		aY=aColor.GetZ();
		aZ=((1.0 - aColor.GetX() - aColor.GetY())*aColor.GetZ())/aColor.GetY();
	}
	else
	{
		aX=aY=aZ=0.0;
	}

	SetX(aX); 
	SetY(aY); 
	SetZ(aZ); 
}

void CColor::SetLabValue(const CColor & aColor) 
{
	if(aColor == noDataColor)
	{
		*this=aColor;
		return;
	}

	double var_Y = ( aColor.GetX() + 16.0 ) / 116.0;
	double var_X = aColor.GetY() / 500.0 + var_Y;
	double var_Z = var_Y - aColor.GetZ() / 200.0;

	if ( pow(var_Y,3) > 0.008856 ) 
		var_Y = pow(var_Y,3);
	else                      
		var_Y = ( var_Y - 16.0 / 116.0 ) / 7.787;

	if ( pow(var_X,3) > 0.008856 ) 
		var_X = pow(var_X,3);
	else                      
		var_X = ( var_X - 16.0 / 116.0 ) / 7.787;

	if ( pow(var_Z,3) > 0.008856 ) 
		var_Z = pow(var_Z,3);
	else                      
		var_Z = ( var_Z - 16.0 / 116.0 ) / 7.787;

	SetX(GetColorReference().GetWhite().GetX() * var_X);    
	SetY(GetColorReference().GetWhite().GetY() * var_Y);    
	SetZ(GetColorReference().GetWhite().GetZ() * var_Z);    
}

void CColor::SetSensorToXYZMatrix(const Matrix & aMatrix) 
{ 
	m_SensorToXYZMatrix=aMatrix; 

	EnterCriticalSection ( & MatrixCritSec );
	m_XYZtoSensorMatrix = m_SensorToXYZMatrix.GetInverse();
	LeaveCriticalSection ( & MatrixCritSec );
}

Matrix CColor::GetSensorToXYZMatrix() const
{
	return m_SensorToXYZMatrix; 
}

void CColor::SetSpectrum ( CSpectrum & aSpectrum )
{
	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}

	m_pSpectrum = new CSpectrum ( aSpectrum );
}

void CColor::ResetSpectrum ()
{
	if ( m_pSpectrum )
	{
		delete m_pSpectrum;
		m_pSpectrum = NULL;
	}
}

BOOL CColor::HasSpectrum () const
{
	return ( m_pSpectrum != NULL );
}

CSpectrum CColor::GetSpectrum () const
{
	if ( m_pSpectrum )
		return CSpectrum ( *m_pSpectrum );
	else
		return CSpectrum ( 0, 0, 0, 0 );
}

void CColor::SetLuxValue ( double LuxValue )
{
	if ( m_pLuxValue == NULL )
		m_pLuxValue = new double;

	* m_pLuxValue = LuxValue;
}

void CColor::ResetLuxValue ()
{
	if ( m_pLuxValue )
	{
		delete m_pLuxValue;
		m_pLuxValue = NULL;
	}
}

BOOL CColor::HasLuxValue () const
{
	return ( m_pLuxValue != NULL );
}

double CColor::GetLuxValue () const
{
	return ( m_pLuxValue ? (*m_pLuxValue) : 0.0 );
}

double CColor::GetLuxOrLumaValue () const
{
	// Return Luxmeter value when option authorize it and luxmeter value exists.
	return ( GetConfig () -> m_nLuminanceCurveMode == 1 && m_pLuxValue ? (*m_pLuxValue) : GetY() );
}

double CColor::GetPreferedLuxValue () const
{
	// Return Luxmeter value when option authorize it and luxmeter value exists.
	return ( GetConfig () -> m_bPreferLuxmeter && m_pLuxValue ? (*m_pLuxValue) : GetY() );
}


/////////////////////////////////////////////////////////////////////
// Implementation of the CSpectrum class.

CSpectrum::CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, int BandWidth) : Matrix(0.0,NbBands,1)
{
	m_WaveLengthMin	= WaveLengthMin;
	m_WaveLengthMax = WaveLengthMax;
	m_BandWidth = BandWidth;    
}

CSpectrum::CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, int BandWidth, double * pValues) : Matrix(0.0,NbBands,1)
{
	m_WaveLengthMin	= WaveLengthMin;
	m_WaveLengthMax = WaveLengthMax;
	m_BandWidth = BandWidth;    

	for ( int i = 0; i < NbBands ; i ++ )
		Matrix::operator()(i,0) = pValues [ i ];
}

CSpectrum::CSpectrum(CSpectrum &aSpectrum):Matrix(aSpectrum)
{
	m_WaveLengthMin	= aSpectrum.m_WaveLengthMin;
	m_WaveLengthMax = aSpectrum.m_WaveLengthMax;
	m_BandWidth = aSpectrum.m_BandWidth;    
}

CSpectrum::~CSpectrum()
{
}

double & CSpectrum::operator[](const int nRow) const
{ 
	ASSERT(nRow < GetRows());
	return Matrix::operator ()(nRow,0); 
}

void CSpectrum::Serialize(CArchive& archive)
{
	if (archive.IsStoring())
	{
		int version=1;
		archive << version;
		Matrix::Serialize(archive);
	}
	else
	{
		int version;
		archive >> version;
		if ( version > 1 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		Matrix::Serialize(archive);
	}
}

CColor CSpectrum::GetXYZValue() const
{
	// Not implemented yet
	return noDataColor;
}

void GenerateSaturationColors ( COLORREF * GenColors, int nSteps, BOOL bRed, BOOL bGreen, BOOL bBlue, BOOL b16_235 )
{
	// Retrieve color luma coefficients matching actual reference
	const double KR = GetColorReference().GetRedReferenceLuma ();  
	const double KG = GetColorReference().GetGreenReferenceLuma ();
	const double KB = GetColorReference().GetBlueReferenceLuma (); 

	double K = ( bRed ? KR : 0.0 ) + ( bGreen ? KG : 0.0 ) + ( bBlue ? KB : 0.0 );
	double luma = K * 255.0;	// Luma for pure color

	// Compute vector between neutral gray and saturated color in CIExy space
	CColor Clr1, Clr2, Clr3;
	double	xstart, ystart, xend, yend;

	// Retrieve gray xy coordinates
	xstart = GetColorReference().GetWhite().GetxyYValue().GetX();
	ystart = GetColorReference().GetWhite().GetxyYValue().GetY();

	// Define target color in RGB mode
	Clr1[0] = ( bRed ? 255.0 : 0.0 );
	Clr1[1] = ( bGreen ? 255.0 : 0.0 );
	Clr1[2] = ( bBlue ? 255.0 : 0.0 );

	// Compute xy coordinates of 100% saturated color
	Clr2.SetRGBValue(Clr1);
	Clr3=Clr2.GetxyYValue();
	xend=Clr3[0];
	yend=Clr3[1];

	for ( int i = 0; i < nSteps ; i++ )
	{
		double	clr, comp;

		if ( i == 0 )
		{
			clr = luma;
			comp = luma;
		}
		else if ( i == nSteps - 1 )
		{
			clr = 255.0;
			comp = 0.0;
		}
		else
		{
			double x, y;

			x = xstart + ( (xend - xstart) * (double) i / (double) (nSteps - 1) );
			y = ystart + ( (yend - ystart) * (double) i / (double)(nSteps - 1) );

			CColor UnsatClr_xyY(x,y,luma);

			CColor UnsatClr;
			UnsatClr.SetxyYValue (UnsatClr_xyY);

			CColor UnsatClr_rgb = UnsatClr.GetRGBValue ();

			// Both components are theoretically equal, get medium value
			clr = ( ( ( bRed ? UnsatClr_rgb[0] : 0.0 ) + ( bGreen ? UnsatClr_rgb[1] : 0.0 ) + ( bBlue ? UnsatClr_rgb[2] : 0.0 ) ) / (double) ( bRed + bGreen + bBlue ) );
			comp = ( ( luma - ( K * (double) clr ) ) / ( 1.0 - K ) );

			if ( clr < 0.0 )
				clr = 0.0;
			else if ( clr > 255.0 )
				clr = 255.0;

			if ( comp < 0.0 )
				comp = 0.0;
			else if ( comp > 255.0 )
				comp = 255.0;
		}

		// adjust "color gamma"
		int	clr2, comp2;

		if ( b16_235 )
		{
			clr2 = 16 + (int) ( 219.0 * pow ( clr / 255.0, 0.45 ) );
			comp2 = 16 + (int) ( 219.0 * pow ( comp / 255.0, 0.45 ) );
		}
		else
		{
			clr2 = (int) ( 255.0 * pow ( clr / 255.0, 0.45 ) );
			comp2 = (int) ( 255.0 * pow ( comp / 255.0, 0.45 ) );
		}

		GenColors [ i ] = RGB ( ( bRed ? clr2 : comp2 ), ( bGreen ? clr2 : comp2 ), ( bBlue ? clr2 : comp2 ) );

#if 0
	char szBuf [ 256 ];
	sprintf(szBuf, "Color %d : R%3d, G%3d, B%3d\n", i, ( bRed ? clr2 : comp2 ), ( bGreen ? clr2 : comp2 ), ( bBlue ? clr2 : comp2 ) );
	OutputDebugString ( szBuf );
#endif
	}
}

Matrix ComputeConversionMatrix(Matrix & measures, Matrix & references, CColor & WhiteTest, CColor & WhiteRef )
{
	Matrix	m;
	BOOL	bUseOnlyPrimaries = GetConfig () -> m_bUseOnlyPrimaries;

	if ( WhiteTest == noDataColor || WhiteRef == noDataColor )
	{
		// TODO: add a messagebox
		bUseOnlyPrimaries = TRUE;
	}

	if ( ! bUseOnlyPrimaries )
	{
		if(references.Determinant() == 0) // check that reference matrix is inversible
		{
			// TODO: we can also exit with an error message... should check what is the best solution...
			bUseOnlyPrimaries = TRUE;
		}
	}

	if ( bUseOnlyPrimaries )
	{
		// Use only primary colors to compute transformation matrix
		Matrix invT=measures.GetInverse();
		m=references*invT;
	}
	else
	{
		// Use primary colors and white to compute transformation matrix
		// Compute component gain for reference white
		Matrix invR=references.GetInverse();
		Matrix RefWhiteGain=invR*WhiteRef;

		// Compute component gain for measured white
		Matrix invM=measures.GetInverse();
		Matrix MeasWhiteGain=invM*WhiteTest;

		// Transform component gain matrix into a diagonal matrix
		Matrix RefDiagWhite(0.0,3,3);
		RefDiagWhite(0,0)=RefWhiteGain(0,0);
		RefDiagWhite(1,1)=RefWhiteGain(1,0);
		RefDiagWhite(2,2)=RefWhiteGain(2,0);

		// Transform component gain matrix into a diagonal matrix
		Matrix MeasDiagWhite(0.0,3,3);
		MeasDiagWhite(0,0)=MeasWhiteGain(0,0);
		MeasDiagWhite(1,1)=MeasWhiteGain(1,0);
		MeasDiagWhite(2,2)=MeasWhiteGain(2,0);

		// Compute component distribution for reference white
		Matrix RefWhiteComponentMatrix=references*RefDiagWhite;

		// Compute component distribution for measured white
		Matrix MeasWhiteComponentMatrix=measures*MeasDiagWhite;

		// Compute XYZ transformation matrix
		Matrix	invT=MeasWhiteComponentMatrix.GetInverse();
		m=RefWhiteComponentMatrix*invT;
	}

	return m;
}

double ArrayIndexToGrayLevel ( int nCol, int nSize, BOOL bIRE )
{
	if ( bIRE )
	{
		// IRE levels: return value between 7.5 and 100
		// Level 0 (black) is always 7.5,
		// When there is less than 10 measure points, values are like percents (except black)
		// When there is at least 11 points, level 1 is always 10, and other levels are regular
		if ( nCol == 0 )
			return 7.5;
		else if ( nSize <= 11 )
			return ( (double)nCol*100.0/(double)(nSize-1) );
		else 
			return ( 10.0 + ( (double)(nCol-1)*90.0/(double)(nSize-2) ) );
	}
	else
	{
		// Gray percent: return a value between 0 and 100
		return ( (double)nCol*100.0/(double)(nSize-1) );
	}
}

double GrayLevelToGrayProp ( double Level, BOOL bIRE )
{
	if ( bIRE )
	{
		// IRE levels: return value between 0 and 1 from value between 7.5 and 100
		if ( Level <= 7.5 )
			return 0.0;
		else
			return ( Level - 7.5 ) / 92.5;
	}
	else
	{
		// Gray Level: return a value between 0 and 1
		return Level / 100.0;
	}
}

