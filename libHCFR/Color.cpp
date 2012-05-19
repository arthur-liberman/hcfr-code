/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2007 Association Homecinema Francophone.  All rights reserved.
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
//	FranÁois-Xavier CHABOUD
//  Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////
//  Modifications:
//  JÈrÙme Duquennoy -> made OS agnostic
//    ASSERT -> assert
//    CRITICAL_SECTION -> pthreads mutex
//    CString -> std::string
//      CString.LoadString -> string passÈe en argument
//    BOOL, FALSE, TRUE -> bool, false, true
//    const added to the param of the copy constructor
//    GetColorReference linked to global app var -> replaced by arguments in impacted functions

#include "Color.h"
#include "Endianness.h"
#include "CriticalSection.h"
#include "LockWhileInScope.h"
#include <math.h>
#include <assert.h>

// critical section to be used in this file to
// ensure config matrices don't get changed mid-calculation
static CriticalSection m_matrixSection;

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
// Color references
///////////////////////////////////////////////////////////////////

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

Matrix ComputeRGBtoXYZMatrix(Matrix primariesChromacities,Matrix whiteChromacity)
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


CColorReference::CColorReference(ColorStandard aColorStandard, WhiteTarget aWhiteTarget, double aGamma, string	strModified)
{
	m_standard = aColorStandard;

	switch(aColorStandard)
	{
		case PALSECAM:
		{
			standardName="PAL/SECAM";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			redPrimary.  SetxyYValue(primariesPAL[0][0],primariesPAL[0][1],1);
			greenPrimary.SetxyYValue(primariesPAL[1][0],primariesPAL[1][1],1);
			bluePrimary. SetxyYValue(primariesPAL[2][0],primariesPAL[2][1],1);
			break;
		}
		case SDTV:
		{
			standardName="SDTV Rec601";
			whiteColor=illuminantD65;
			m_white=D65;
			whiteName="D65";
			redPrimary.  SetxyYValue(primariesRec601[0][0],primariesRec601[0][1],1);
			greenPrimary.SetxyYValue(primariesRec601[1][0],primariesRec601[1][1],1);
			bluePrimary. SetxyYValue(primariesRec601[2][0],primariesRec601[2][1],1);
			break;
		}
		case HDTV:
		{
			standardName="HDTV Rec709";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			redPrimary.  SetxyYValue(primariesRec709[0][0],primariesRec709[0][1],1);
			greenPrimary.SetxyYValue(primariesRec709[1][0],primariesRec709[1][1],1);
			bluePrimary. SetxyYValue(primariesRec709[2][0],primariesRec709[2][1],1);
			break;
		}
		case sRGB:
		{
			standardName="sRGB";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			redPrimary.  SetxyYValue(primariesRec709[0][0],primariesRec709[0][1],1);
			greenPrimary.SetxyYValue(primariesRec709[1][0],primariesRec709[1][1],1);
			bluePrimary. SetxyYValue(primariesRec709[2][0],primariesRec709[2][1],1);
			break;
		}
		default:
		{
            standardName="Unknown";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			redPrimary.  SetxyYValue(primariesRec601[0][0],primariesRec601[0][1],1);
			greenPrimary.SetxyYValue(primariesRec601[1][0],primariesRec601[1][1],1);
			bluePrimary. SetxyYValue(primariesRec601[2][0],primariesRec601[2][1],1);
		}
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
	double x = whiteColor.GetxyYValue ()[0];
	double y = whiteColor.GetxyYValue ()[1];
	u_white = 4.0*x / (-2.0*x + 12.0*y + 3.0); 
	v_white = 9.0*y / (-2.0*x + 12.0*y + 3.0); 
  
	// Compute transformation matrices
	Matrix primariesMatrix(redPrimary.GetXYZValue());
	primariesMatrix.CMAC(greenPrimary.GetXYZValue());
	primariesMatrix.CMAC(bluePrimary.GetXYZValue());
	RGBtoXYZMatrix=ComputeRGBtoXYZMatrix(primariesMatrix, whiteColor.GetXYZValue());
	XYZtoRGBMatrix=RGBtoXYZMatrix.GetInverse();

	// Adjust reference primary colors Y values relatively to white Y
	ColorxyY aColor = redPrimary.GetxyYValue ();
	aColor[2] = GetRedReferenceLuma ();
	redPrimary.SetxyYValue ( aColor );

	aColor = greenPrimary.GetxyYValue ();
	aColor[2] = GetGreenReferenceLuma ();
	greenPrimary.SetxyYValue ( aColor );

	aColor = bluePrimary.GetxyYValue ();
	aColor[2] = GetBlueReferenceLuma ();
	bluePrimary.SetxyYValue ( aColor );

    UpdateSecondary ( yellowSecondary, redPrimary, greenPrimary, bluePrimary );
    UpdateSecondary ( cyanSecondary, greenPrimary, bluePrimary, redPrimary );
    UpdateSecondary ( magentaSecondary, bluePrimary, redPrimary, greenPrimary );
}

CColorReference::~CColorReference()
{
}

CColorReference GetStandardColorReference(ColorStandard aColorStandard)
{
	CColorReference aStandardRef(aColorStandard);
	return aStandardRef;
}

void CColorReference::UpdateSecondary ( CColor & secondary, const CColor & primary1, const CColor & primary2, const CColor & primaryOpposite )
{
	// Compute intersection between line (primary1-primary2) and line (primaryOpposite-white)
	ColorxyY	prim1 = primary1.GetxyYValue ();
	ColorxyY	prim2 = primary2.GetxyYValue ();
	ColorxyY	primOppo = primaryOpposite.GetxyYValue ();
	ColorxyY	white = GetWhite ().GetxyYValue ();

	double x1 = prim1[0];
	double y1 = prim1[1];
	double x2 = white[0];
	double y2 = white[1];
	double dx1 = prim2[0] - x1;
	double dy1 = prim2[1] - y1;
	double dx2 = primOppo[0] - x2;
	double dy2 = primOppo[1] - y2;
	
	double k = ( ( ( x2 - x1 ) / dx1 ) + ( dx2 / ( dx1 * dy2 ) ) * ( y1 - y2 ) ) / ( 1.0 - ( ( dx2 * dy1 ) / ( dx1 * dy2 ) ) );

	ColorxyY aColor ( x1 + k * dx1, y1 + k * dy1, primary1.GetY() + primary2.GetY() );

	secondary.SetxyYValue ( aColor );
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

/////////////////////////////////////////////////////////////////////
// CIRELevel Class
//
//////////////////////////////////////////////////////////////////////

CIRELevel::CIRELevel(double aIRELevel, bool bIRE, bool b16_235)
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

CIRELevel::CIRELevel(double aRedIRELevel,double aGreenIRELevel,double aBlueIRELevel,bool bIRE,bool b16_235)
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

#ifdef LIBHCFR_HAS_WIN32_API
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
#endif // #ifdef LIBHCFR_HAS_WIN32_API

/////////////////////////////////////////////////////////////////////
// implementation of the ColorTriplet class.
//////////////////////////////////////////////////////////////////////
ColorTriplet::ColorTriplet() :
    Matrix(FX_NODATA, 3, 1)
{
}

ColorTriplet::ColorTriplet(const Matrix& matrix) :
    Matrix(matrix)
{
    if(GetRows() != 3 || GetColumns() != 1)
    {
        throw std::logic_error("matrix not right shape to create color");
    }
}

ColorTriplet::ColorTriplet(double a, double b, double c) :
    Matrix(FX_NODATA, 3, 1)
{
    (*this)[0] = a;
    (*this)[1] = b;
    (*this)[2] = c;
}

const double& ColorTriplet::operator[](const int nRow) const
{
    return (*this)(nRow, 0);
}

double& ColorTriplet::operator[](const int nRow)
{
    return (*this)(nRow, 0);
}

bool ColorTriplet::isValid() const
{
    return ((*this)[0] != FX_NODATA) && ((*this)[1] != FX_NODATA) && ((*this)[2] != FX_NODATA);
}

/////////////////////////////////////////////////////////////////////
// implementation of the ColorXYZ class.
//////////////////////////////////////////////////////////////////////
ColorXYZ::ColorXYZ()
{
}

ColorXYZ::ColorXYZ(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorXYZ::ColorXYZ(const ColorRGB& RGB, CColorReference colorReference)
{
    if(RGB.isValid())
    {
        CLockWhileInScope dummy(m_matrixSection);
        *this = colorReference.RGBtoXYZMatrix*RGB;
    }
}

ColorXYZ::ColorXYZ(const ColorxyY& xyY)
{
    if(xyY.isValid())
    {
        if(xyY[1] != 0)
        {
            (*this)[0] = (xyY[0]*xyY[2])/xyY[1];
            (*this)[1] = xyY[2];
            (*this)[2] = ((1.0 - xyY[0] - xyY[1])*xyY[2])/xyY[1];
        }
    }
}

ColorXYZ::ColorXYZ(double X, double Y, double Z) :
    ColorTriplet(X, Y, Z)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorxyY class.
//////////////////////////////////////////////////////////////////////
ColorxyY::ColorxyY()
{
}

ColorxyY::ColorxyY(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorxyY::ColorxyY(const ColorXYZ& XYZ)
{
    if(XYZ.isValid())
    {
        double sum = XYZ[0] + XYZ[1] + XYZ[2];
        if(sum > 0.0)
        {
            (*this)[0] = XYZ[0] / sum;
            (*this)[1] = XYZ[1] / sum;
            (*this)[2] = XYZ[1];
        }
    }
}

ColorxyY::ColorxyY(double x, double y, double YY) :
    ColorTriplet(x, y, YY)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the Colorxyz class.
//////////////////////////////////////////////////////////////////////
Colorxyz::Colorxyz()
{
}

Colorxyz::Colorxyz(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

Colorxyz::Colorxyz(const ColorXYZ& XYZ)
{
    if(XYZ.isValid())
    {
        double sum = XYZ[0] + XYZ[1] + XYZ[2];
        if(sum > 0.0)
        {
            (*this)[0] = XYZ[0] / sum;
            (*this)[1] = XYZ[1] / sum;
            (*this)[2] = XYZ[2] / sum;
        }
    }
}

Colorxyz::Colorxyz(double x, double y, double z) :
    ColorTriplet(x, y, z)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorRGB class.
//////////////////////////////////////////////////////////////////////
ColorRGB::ColorRGB()
{
}

ColorRGB::ColorRGB(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorRGB::ColorRGB(const ColorXYZ& XYZ, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
        CLockWhileInScope dummy(m_matrixSection);
        *this = colorReference.XYZtoRGBMatrix*XYZ;
    }
}

ColorRGB::ColorRGB(double r, double g, double b) :
    ColorTriplet(r, g, b)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorLab class.
//////////////////////////////////////////////////////////////////////
ColorLab::ColorLab()
{
}

ColorLab::ColorLab(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorLab::ColorLab(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
        double scaling = YWhiteRef/colorReference.GetWhite().GetY();

        double var_X = XYZ[0]/(colorReference.GetWhite().GetX()*scaling);
        double var_Y = XYZ[1]/(colorReference.GetWhite().GetY()*scaling);
        double var_Z = XYZ[2]/(colorReference.GetWhite().GetZ()*scaling);

        if ( var_X > 0.008856 )
        {
            var_X = pow(var_X,1.0/3.0);
        }
        else
        {
            var_X = (7.787*var_X)+(16.0/116.0);
        }

        if (var_Y > 0.008856)
        {
            var_Y = pow(var_Y,1.0/3.0);
        }
        else
        {
            var_Y = (7.787*var_Y)+(16/116.0);
        }

        if (var_Z > 0.008856)
        {
            var_Z = pow(var_Z,1.0/3.0);
        }
        else
        {
            var_Z = (7.787*var_Z)+(16.0/116.0);
        }

        (*this)[0] = 116.0*var_Y-16.0;	 // CIE-L*
        (*this)[1] = 500.0*(var_X-var_Y); // CIE-a*
        (*this)[2] = 200.0*(var_Y-var_Z); //CIE-b*
    }
}

ColorLab::ColorLab(double l, double a, double b) :
    ColorTriplet(l, a, b)
{
}

////////////////////////////////////////////////////////////////////
// implementation of the ColorLCH class.
//////////////////////////////////////////////////////////////////////
ColorLCH::ColorLCH()
{
}

ColorLCH::ColorLCH(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorLCH::ColorLCH(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
        ColorLab Lab(XYZ, YWhiteRef, colorReference);

        double L = Lab[0];
        double C = sqrt ( (Lab[1]*Lab[1]) + (Lab[2]*Lab[2]) );
        double H = atan2 ( Lab[2], Lab[1] ) / acos(-1.0) * 180.0;

        if ( H < 0.0 )
        {
            H += 360.0;
        }

        (*this)[0] = L;
        (*this)[1] = C;
        (*this)[2] = H;
    }
}

ColorLCH::ColorLCH(double l, double c, double h) :
    ColorTriplet(l, c, h)
{
}

/////////////////////////////////////////////////////////////////////
// ColorMeasure.cpp: implementation of the CColor class.
//
//////////////////////////////////////////////////////////////////////

CColor::CColor():m_XYZValues(1.0,3,1)
{
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(const CColor &aColor):m_XYZValues(aColor.m_XYZValues)
{
	m_pSpectrum = NULL;
	m_pLuxValue = NULL;

	if ( aColor.m_pSpectrum )
		m_pSpectrum = new CSpectrum ( *aColor.m_pSpectrum );

	if ( aColor.m_pLuxValue )
		m_pLuxValue = new double ( *aColor.m_pLuxValue );
}

CColor::CColor(const ColorXYZ& aMatrix):m_XYZValues(aMatrix)
{
	assert(aMatrix.GetColumns() == 1);
	assert(aMatrix.GetRows() == 3);

	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(double ax,double ay):m_XYZValues(0.0,3,1)
{
    ColorxyY tempColor(ax,ay,1);
    SetxyYValue(tempColor);

	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(double aX,double aY, double aZ):m_XYZValues(0.0,3,1)
{
	SetX(aX);
	SetY(aY);
	SetZ(aZ);

	m_pSpectrum = NULL;
	m_pLuxValue = NULL;
}

CColor::CColor(ifstream &theFile):m_XYZValues(0.0,3,1)
{
    uint32_t  version = 0;

    m_pSpectrum = NULL;
    m_pLuxValue = NULL;

    theFile.read((char*)&version, 4);
    version = littleEndianUint32ToHost(version);

    // les données XYZ (on utilise le constructeur de Matrix)
    m_XYZValues = Matrix(theFile);

    Matrix XYZtoSensorMatrix = Matrix(theFile);
    Matrix SensorToXYZMatrix = Matrix(theFile);
    if ( version == 2 || version == 4 || version == 5 || version == 6 )
        m_pSpectrum = new CSpectrum (theFile, (version == 2 || version == 4));
    if ( version == 3 || version == 4 || version == 6 )
    {
        double readValue;
        theFile.read((char*)&readValue, 8);
        readValue = littleEndianDoubleToHost(readValue);
        m_pLuxValue = new double(readValue);
    }
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

	m_XYZValues = aColor.m_XYZValues;

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

const double & CColor::operator[](const int nRow) const
{ 
	assert(nRow < m_XYZValues.GetRows());
	return m_XYZValues[nRow];
}

double & CColor::operator[](const int nRow)
{ 
	assert(nRow < m_XYZValues.GetRows());
	return m_XYZValues[nRow];
}

double CColor::GetLuminance() const
{
	return GetY();
}

double CColor::GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, bool useOldDeltaEFormula) const
{
	double dE;
	double x=GetxyYValue()[0];
	double y=GetxyYValue()[1];
	double u = 4.0*x / (-2.0*x + 12.0*y + 3.0); 
	double v = 9.0*y / (-2.0*x + 12.0*y + 3.0); 
	double xRef=refColor.GetxyYValue()[0];
	double yRef=refColor.GetxyYValue()[1];
	double uRef = 4.0*xRef / (-2.0*xRef + 12.0*yRef + 3.0); 
	double vRef = 9.0*yRef / (-2.0*xRef + 12.0*yRef + 3.0); 
	
	if ( useOldDeltaEFormula )
	{
		dE = 1300.0 * sqrt ( pow((u - uRef),2) + pow((v - vRef),2) );
	}
	else
	{
		double u_white = colorReference.GetWhite_uValue();
		double v_white = colorReference.GetWhite_vValue();
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
	double x=GetxyYValue()[0];
	double y=GetxyYValue()[1];
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
	double x=GetxyYValue()[0];
	double y=GetxyYValue()[1];
	double xRef=refColor.GetxyYValue()[0];
	double yRef=refColor.GetxyYValue()[1];
	double dxy = sqrt ( pow((x - xRef),2) + pow((y - yRef),2) );

	return dxy;
}


int CColor::GetColorTemp(CColorReference colorReference) const
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
		if(GetRGBValue(colorReference)[0] >  GetRGBValue(colorReference)[2]) // if red > blue then TC < 1500 else TC > 12000
			return 1499;
		else
			return 12001;
	}
}

ColorXYZ CColor::GetXYZValue() const 
{
	return m_XYZValues;
}

ColorRGB CColor::GetRGBValue(CColorReference colorReference) const 
{
	if(isValid())
	{
        CLockWhileInScope dummy(m_matrixSection);
		return colorReference.XYZtoRGBMatrix*(m_XYZValues);
	}
	else
    {
        return ColorRGB();
    }
}

ColorxyY CColor::GetxyYValue() const
{
    return ColorxyY(m_XYZValues);
}

Colorxyz CColor::GetxyzValue() const
{
    return Colorxyz(m_XYZValues);
}

double CColor::GetLValue(double YWhiteRef) const 
{
    return ColorLab(m_XYZValues)[0];
}

ColorLab CColor::GetLabValue(double YWhiteRef, CColorReference colorReference) const 
{
    return ColorLab(m_XYZValues, YWhiteRef, colorReference);
}

ColorLCH CColor::GetLCHValue(double YWhiteRef, CColorReference colorReference) const 
{
    return ColorLCH(m_XYZValues, YWhiteRef, colorReference);
}

void CColor::SetXYZValue(const ColorXYZ& aColor) 
{
    m_XYZValues = aColor;
}

void CColor::SetRGBValue(const ColorRGB& aColor, CColorReference colorReference) 
{
    if(!aColor.isValid())
    {
        m_XYZValues = ColorXYZ();
        return;
    }

    CLockWhileInScope dummy(m_matrixSection);
    m_XYZValues = ColorXYZ(colorReference.RGBtoXYZMatrix*aColor);
}

void CColor::SetxyYValue(double x, double y, double Y) 
{
    ColorxyY xyY(x, y, Y);
    m_XYZValues = ColorXYZ(xyY);
}

void CColor::SetxyYValue(const ColorxyY& xyY)
{
    m_XYZValues = ColorXYZ(xyY);
}

bool CColor::isValid() const
{
    return m_XYZValues.isValid();
}

void CColor::SetLabValue(const ColorLab& aColor, CColorReference colorReference) 
{
    if(!aColor.isValid())
    {
        m_XYZValues = ColorXYZ();
        return;
    }

    double var_Y = ( aColor[0] + 16.0 ) / 116.0;
    double var_X = aColor[1] / 500.0 + var_Y;
    double var_Z = var_Y - aColor[2] / 200.0;

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

    m_XYZValues = ColorXYZ(colorReference.GetWhite().GetX() * var_X, 
        colorReference.GetWhite().GetY() * var_Y,
        colorReference.GetWhite().GetZ() * var_Z);
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

bool CColor::HasSpectrum () const
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

bool CColor::HasLuxValue () const
{
	return ( m_pLuxValue != NULL );
}

double CColor::GetLuxValue () const
{
	return ( m_pLuxValue ? (*m_pLuxValue) : 0.0 );
}

double CColor::GetLuxOrLumaValue (const int luminanceCurveMode) const
{
	// Return Luxmeter value when option authorize it and luxmeter value exists.
	return ( luminanceCurveMode == 1 && m_pLuxValue ? (*m_pLuxValue) : GetY() );
}

double CColor::GetPreferedLuxValue (bool preferLuxmeter) const
{
	// Return Luxmeter value when option authorize it and luxmeter value exists.
	return ( preferLuxmeter && m_pLuxValue ? (*m_pLuxValue) : GetY() );
}

void CColor::Output(ostream& ostr) const
{
    m_XYZValues.Output(ostr);
}

ostream& operator <<(ostream& ostr, const CColor& obj)
{
    obj.Output(ostr);
    return ostr;
}

#ifdef LIBHCFR_HAS_MFC
void CColor::Serialize(CArchive& archive)
{
	if (archive.IsStoring())
	{
		int version=1;

		if ( m_pLuxValue )
		{
			if ( m_pSpectrum )
				version = 6;
			else
				version = 3;
		}
		else if ( m_pSpectrum )
		{
			version = 5;
		}

		archive << version;
		m_XYZValues.Serialize(archive) ;

        Matrix ignore(0.0, 3, 3);
		ignore.Serialize(archive);
		ignore.Serialize(archive);

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
		
		if ( version > 6 )
			AfxThrowArchiveException ( CArchiveException::badSchema );
		
		m_XYZValues.Serialize(archive) ;
		Matrix ignore(0.0, 3, 3);
		ignore.Serialize(archive);
		ignore.Serialize(archive);

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

		if ( version == 2 || version == 4 || version == 5 || version == 6 )
		{
			int NbBands, WaveLengthMin, WaveLengthMax;
			double dBandWidth;
			archive >> NbBands;
			archive >> WaveLengthMin;
			archive >> WaveLengthMax;
			if ( version == 2 || version == 4 )
			{
				int nBandWidth;
				archive >> nBandWidth;
				dBandWidth = nBandWidth;
			}
			else
			{
				archive >> dBandWidth;
			}
			m_pSpectrum = new CSpectrum ( NbBands, WaveLengthMin, WaveLengthMax, dBandWidth );
			m_pSpectrum -> Serialize(archive);
		}

		if ( version == 3 || version == 4 || version == 6 )
		{
			m_pLuxValue = new double;
			archive >> (* m_pLuxValue);
		}
	}
}
#endif

/////////////////////////////////////////////////////////////////////
// Implementation of the CSpectrum class.

CSpectrum::CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, double BandWidth) : Matrix(0.0,NbBands,1)
{
	m_WaveLengthMin	= WaveLengthMin;
	m_WaveLengthMax = WaveLengthMax;
	m_BandWidth = BandWidth;    
}

CSpectrum::CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, double BandWidth, double * pValues) : Matrix(0.0,NbBands,1)
{
	m_WaveLengthMin	= WaveLengthMin;
	m_WaveLengthMax = WaveLengthMax;
	m_BandWidth = BandWidth;    

	for ( int i = 0; i < NbBands ; i ++ )
		Matrix::operator()(i,0) = pValues [ i ];
}

CSpectrum::CSpectrum (ifstream &theFile, bool oldFileFormat):Matrix()
{
  uint32_t NbBands, WaveLengthMin, WaveLengthMax;
  theFile.read((char*)&NbBands, 4);
  NbBands = littleEndianUint32ToHost(NbBands);
  theFile.read((char*)&WaveLengthMin, 4);
  m_WaveLengthMin = littleEndianUint32ToHost(WaveLengthMin);
  theFile.read((char*)&WaveLengthMax, 4);
  m_WaveLengthMax = littleEndianUint32ToHost(WaveLengthMax);
  if(oldFileFormat)
  {
      uint32_t nBandwidth;
      theFile.read((char*)&nBandwidth, 4);
      m_BandWidth = littleEndianUint32ToHost(nBandwidth);
  }
  else
  {
      double value;
      theFile.read((char*)&value, 8);
      m_BandWidth = littleEndianDoubleToHost(value);
  }
  
  // on saute la version (toujours a 1)
  theFile.seekg(4, ios::cur);
  
  Matrix::readFromFile(theFile);
  
}

const double & CSpectrum::operator[](const int nRow) const
{ 
	assert(nRow < GetRows());
	return Matrix::operator ()(nRow,0); 
}

CColor CSpectrum::GetXYZValue() const
{
	// Not implemented yet
	return noDataColor;
}

#ifdef LIBHCFR_HAS_MFC
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
#endif

#ifdef LIBHCFR_HAS_WIN32_API
void GenerateSaturationColors (const CColorReference& colorReference, COLORREF * GenColors, int nSteps, BOOL bRed, BOOL bGreen, BOOL bBlue, BOOL b16_235 )
{
	// Retrieve color luma coefficients matching actual reference
	const double KR = colorReference.GetRedReferenceLuma ();  
	const double KG = colorReference.GetGreenReferenceLuma ();
	const double KB = colorReference.GetBlueReferenceLuma (); 

	double K = ( bRed ? KR : 0.0 ) + ( bGreen ? KG : 0.0 ) + ( bBlue ? KB : 0.0 );
	double luma = K * 255.0;	// Luma for pure color

	// Compute vector between neutral gray and saturated color in CIExy space
	ColorRGB Clr1;
	double	xstart, ystart, xend, yend;

	// Retrieve gray xy coordinates
	xstart = colorReference.GetWhite().GetxyYValue()[0];
	ystart = colorReference.GetWhite().GetxyYValue()[1];

	// Define target color in RGB mode
	Clr1[0] = ( bRed ? 255.0 : 0.0 );
	Clr1[1] = ( bGreen ? 255.0 : 0.0 );
	Clr1[2] = ( bBlue ? 255.0 : 0.0 );

	// Compute xy coordinates of 100% saturated color
	ColorXYZ Clr2(Clr1, colorReference);
    ColorxyY Clr3(Clr2);
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

			ColorxyY UnsatClr_xyY(x,y,luma);

			ColorXYZ UnsatClr(UnsatClr_xyY);

			ColorRGB UnsatClr_rgb(UnsatClr, colorReference);

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
#endif

Matrix ComputeConversionMatrix(Matrix & measures, Matrix & references, ColorXYZ & WhiteTest, ColorXYZ & WhiteRef, bool	bUseOnlyPrimaries)
{
	Matrix	m;

	if (!WhiteTest.isValid() || !WhiteRef.isValid())
	{
		// TODO: add a messagebox
		bUseOnlyPrimaries = true;
	}

	if ( ! bUseOnlyPrimaries )
	{
		if(references.Determinant() == 0) // check that reference matrix is inversible
		{
			// TODO: we can also exit with an error message... should check what is the best solution...
			bUseOnlyPrimaries = true;
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

double ArrayIndexToGrayLevel ( int nCol, int nSize, bool bIRE )
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

double GrayLevelToGrayProp ( double Level, bool bIRE )
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
