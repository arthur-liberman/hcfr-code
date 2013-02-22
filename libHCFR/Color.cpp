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
#include <stdexcept>

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

ColorXYZ illuminantA(ColorxyY(0.447593,0.407539));
ColorXYZ illuminantB(ColorxyY(0.348483,0.351747));
ColorXYZ illuminantC(ColorxyY(0.310115,0.316311));
ColorXYZ illuminantE(ColorxyY(0.333334,0.333333));
ColorXYZ illuminantD50(ColorxyY(0.345669,0.358496));
ColorXYZ illuminantD55(ColorxyY(0.332406,0.347551));
ColorXYZ illuminantD65(ColorxyY(0.312712,0.329008));
ColorXYZ illuminantD75(ColorxyY(0.299023,0.314963));

ColorxyY primariesPAL[3] =	{	ColorxyY(0.6400, 0.3300),
								ColorxyY(0.2900, 0.6000),
								ColorxyY(0.1500, 0.0600) };

ColorxyY primariesRec601[3] ={	ColorxyY(0.6300, 0.3400),
								ColorxyY(0.3100, 0.5950),
								ColorxyY(0.1550, 0.0700) };

ColorxyY primariesRec709[3] ={	ColorxyY(0.6400, 0.3300),
								ColorxyY(0.3000, 0.6000),
								ColorxyY(0.1500, 0.0600) };

ColorxyY primariesRec709a[3] ={	ColorxyY(0.5575, 0.3298), //75% sat/lum Rec709 w/2.22 gamma
								ColorxyY(0.3032, 0.5313),
								ColorxyY(0.1911, 0.1279) };

ColorxyY primariesCC6[3] ={	ColorxyY(0.3877, 0.3528), //some color check references, secondardies will add 3 more
								ColorxyY(0.2472, 0.2663),
								ColorxyY(0.3415, 0.4315)};
/* The 75% saturation 75% amplitude and color checker xy locations are calculated assuming gamma=2.22 and starting with the follow triplets
and then used as pseudo-primaries/secondaries as follows

75% (16-235)
	R	G	B	Y	C	M
R'	165	77	58	180	95	156
G'	60	176	58	180	176	80
B'	60	77	126	88	176	156

CC6 (16-235)
	R	G	B	Y	C	M
R'	187	95	94	77	152	213
G'	142	121	109	95	178	154
B'	127	149	74	161	100	54

*/


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

CColorReference::CColorReference(ColorStandard aColorStandard, WhiteTarget aWhiteTarget, double aGamma, string	strModified)
{
	m_standard = aColorStandard;
    ColorxyY* primaries = primariesRec601;

	switch(aColorStandard)
	{
		case PALSECAM:
		{
			standardName="PAL/SECAM";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesPAL;
			break;
		}
		case SDTV:
		{
			standardName="SDTV Rec601";
			whiteColor=illuminantD65;
			m_white=D65;
			whiteName="D65";
            primaries = primariesRec601;
			break;
		}
		case HDTV:
		{
			standardName="HDTV Rec709";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesRec709;
			break;
		}
		case HDTVa:
		{
			standardName="75% HDTV Rec709";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesRec709a;
			break;
		}
		case CC6:
		{
			standardName="Color Checker 6";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesCC6;
			break;
		}
		case sRGB:
		{
			standardName="sRGB";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesRec709;
			break;
		}
		default:
		{
            standardName="Unknown";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
		}
	}

    redPrimary = ColorXYZ(primaries[0]);
    greenPrimary = ColorXYZ(primaries[1]);
    bluePrimary = ColorXYZ(primaries[2]);

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

    // Compute transformation matrices
	Matrix primariesMatrix(redPrimary);
	primariesMatrix.CMAC(greenPrimary);
	primariesMatrix.CMAC(bluePrimary);	
	
	RGBtoXYZMatrix=ComputeRGBtoXYZMatrix(primariesMatrix, whiteColor);

	XYZtoRGBMatrix=RGBtoXYZMatrix.GetInverse();

	// Adjust reference primary colors Y values relatively to white Y
	ColorxyY aColor(redPrimary);
	aColor[2] = GetRedReferenceLuma ();
	redPrimary = ColorXYZ(aColor);

	aColor = ColorxyY(greenPrimary);
	aColor[2] = GetGreenReferenceLuma ();
    greenPrimary = ColorXYZ(aColor);

	aColor = ColorxyY(bluePrimary);
	aColor[2] = GetBlueReferenceLuma ();
	bluePrimary = ColorXYZ(aColor);

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

void CColorReference::UpdateSecondary ( ColorXYZ & secondary, const ColorXYZ& primary1, const ColorXYZ& primary2, const ColorXYZ& primaryOpposite )
{
	// Compute intersection between line (primary1-primary2) and line (primaryOpposite-white)
	ColorxyY	prim1(primary1);
	ColorxyY	prim2(primary2);
	ColorxyY	primOppo(primaryOpposite);
	ColorxyY	white(GetWhite());

	double x1 = prim1[0];
	double y1 = prim1[1];
	double x2 = white[0];
	double y2 = white[1];
	double dx1 = prim2[0] - x1;
	double dy1 = prim2[1] - y1;
	double dx2 = primOppo[0] - x2;
	double dy2 = primOppo[1] - y2;
	
	double k = ( ( ( x2 - x1 ) / dx1 ) + ( dx2 / ( dx1 * dy2 ) ) * ( y1 - y2 ) ) / ( 1.0 - ( ( dx2 * dy1 ) / ( dx1 * dy2 ) ) );

	ColorxyY aColor;
	if (CColorReference::m_standard != 4)
	{
    	aColor = ColorxyY ( x1 + k * dx1, y1 + k * dy1, prim1[2] + prim2[2] );
	}
	else
	{
		if (x1 > 0.38) aColor =  ColorxyY(0.2076,	0.1799,	.116);
		if (x1 > 0.2 && x1 < 0.3) aColor = ColorxyY(0.3755,	0.4970,	.444); 
		if (x1 > 0.3 && x1 < 0.38) aColor = ColorxyY(0.4763, 0.4431, .426); 
	}
	secondary = ColorXYZ(aColor);
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
// ColorRGBDisplay Class
//
//////////////////////////////////////////////////////////////////////

ColorRGBDisplay::ColorRGBDisplay()
{
}

ColorRGBDisplay::ColorRGBDisplay(double aGreyPercent)
{
    (*this)[0] = aGreyPercent;
    (*this)[1] = aGreyPercent;
    (*this)[2] = aGreyPercent;
}

ColorRGBDisplay::ColorRGBDisplay(double aRedPercent,double aGreenPercent,double aBluePercent)
{
    (*this)[0] = aRedPercent;
    (*this)[1] = aGreenPercent;
    (*this)[2] = aBluePercent;
}

#ifdef LIBHCFR_HAS_WIN32_API

ColorRGBDisplay::ColorRGBDisplay(COLORREF aColor)
{
    (*this)[0] = double(GetRValue(aColor)) / 2.55;
    (*this)[1] = double(GetGValue(aColor)) / 2.55;
    (*this)[2] = double(GetBValue(aColor)) / 2.55;
}

COLORREF ColorRGBDisplay::GetColorRef(bool is16_235) const
{
    BYTE r(ConvertPercentToBYTE((*this)[0], is16_235));
    BYTE g(ConvertPercentToBYTE((*this)[1], is16_235));
    BYTE b(ConvertPercentToBYTE((*this)[2], is16_235));
    return RGB(r, g, b);
}

BYTE ColorRGBDisplay::ConvertPercentToBYTE(double percent, bool is16_235)
{
    double coef;
    double offset;

    if (is16_235)
    {
        coef = (235.0 - 16.0) / 100.0;
        offset = 16.0;
    }
    else
    {
        coef = 255.0 / 100.0;
        offset = 0.0;
    }

    int result((int)floor(offset + percent * coef + 0.5));
    if(result < 0)
    {
        return 0;
    }
    else if(result > 255)
    {
        return 255;
    }
    else
    {
        return (BYTE)result;
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
    // allow small negative values but reject anything based off FX_NODATA
    return ((*this)[0] > -1.0) && ((*this)[1] > -1.0) && ((*this)[2] > -1.0);
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
        *this = ColorXYZ(colorReference.RGBtoXYZMatrix*RGB);
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

int ColorXYZ::GetColorTemp(const CColorReference& colorReference) const
{
    double aXYZ[3];
    double aColorTemp;

    aXYZ[0]=(*this)[0];
    aXYZ[1]=(*this)[1];
    aXYZ[2]=(*this)[2];

    if(XYZtoCorColorTemp(aXYZ,&aColorTemp) == 0)
    {
        return (int)aColorTemp;
    }
    else
    {
        ColorRGB rgb(*this, colorReference);
        if(rgb[0] >  rgb[2]) // if red > blue then TC < 1500 else TC > 12000
        {
            return 1499;
        }
        else
        {
            return 12001;
        }
    }
}

double ColorXYZ::GetDeltaE(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, bool useOldDeltaEFormula) const
{
    if ( useOldDeltaEFormula )
    {
		//LUV
        ColorLuv LuvRef(refColor, YWhiteRef, colorReference);
        ColorLuv Luv(*this, YWhite, colorReference);
        double dE = sqrt ( pow ((Luv[0] - LuvRef[0]),2) + pow((Luv[1] - LuvRef[1]),2) + pow((Luv[2] - LuvRef[2]),2) );
        return dE;
//        return GetOldDeltaE(refColor);
    }
    else
    {
		//CIE94
        ColorLab LabRef(refColor, YWhiteRef, colorReference);
        ColorLab Lab(*this, YWhite, colorReference);
		double dL2 = pow ((LabRef[0] - Lab[0]),2.0);
		double C1 = sqrt ( pow (Lab[1],2.0) + pow (Lab[2],2.0));
		double C2 = sqrt ( pow (LabRef[1],2.0) + pow (LabRef[2],2.0));
		double dC2 = pow ((C1-C2),2.0);
		double da2 = pow (LabRef[1] - Lab[1],2.0);
		double db2 = pow (LabRef[2] - Lab[2],2.0);
		double dH2 = (da2 + db2 - dC2);
		//kl=kc=kh=1
		double dE = sqrt ( dL2 + dC2/pow((1+0.045*C1),2.0) + dH2/pow((1+0.015*C1),2.0) );
        return dE;
    }
}

double ColorXYZ::GetOldDeltaE(const ColorXYZ& refColor) const
{
    ColorxyY xyY(*this);
    ColorxyY xyYRef(refColor);
    double x=xyY[0];
    double y=xyY[1];
    double u = 4.0*x / (-2.0*x + 12.0*y + 3.0); 
    double v = 9.0*y / (-2.0*x + 12.0*y + 3.0); 
    double xRef=xyYRef[0];
    double yRef=xyYRef[1];
    double uRef = 4.0*xRef / (-2.0*xRef + 12.0*yRef + 3.0); 
    double vRef = 9.0*yRef / (-2.0*xRef + 12.0*yRef + 3.0); 

    double dE = 1300.0 * sqrt ( pow((u - uRef),2) + pow((v - vRef),2) );

    return dE;
}

double ColorXYZ::GetDeltaxy(const ColorXYZ& refColor, const CColorReference& colorReference) const
{
    ColorxyY xyY(*this);
    ColorxyY xyYRef(refColor);
    if(!xyYRef.isValid())
    {
        xyYRef = ColorxyY(colorReference.GetWhite());
    }
    double x=xyY[0];
    double y=xyY[1];
    double xRef=xyYRef[0];
    double yRef=xyYRef[1];
    double dxy = sqrt ( pow((x - xRef),2) + pow((y - yRef),2) );
    return dxy;
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
        *this = ColorRGB(colorReference.XYZtoRGBMatrix*XYZ);
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
        double scaling = YWhiteRef/colorReference.GetWhite()[1];

        double var_X = XYZ[0]/(colorReference.GetWhite()[0]*scaling);
        double var_Y = XYZ[1]/(colorReference.GetWhite()[1]*scaling);
        double var_Z = XYZ[2]/(colorReference.GetWhite()[2]*scaling);
        const double epsilon(216.0 / 24389.0);
        const double kappa(24389.0 / 27.0);

        if ( var_X > epsilon )
        {
            var_X = pow(var_X,1.0/3.0);
        }
        else
        {
            var_X = (kappa*var_X + 16.0) / 116.0;
        }

        if (var_Y > epsilon)
        {
            var_Y = pow(var_Y,1.0/3.0);
        }
        else
        {
            var_Y = (kappa*var_Y + 16.0) / 116.0;
        }

        if (var_Z > epsilon)
        {
            var_Z = pow(var_Z,1.0/3.0);
        }
        else
        {
            var_Z = (kappa*var_Z + 16.0) / 116.0;
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
// implementation of the ColorLuv class.
//////////////////////////////////////////////////////////////////////
ColorLuv::ColorLuv()
{
}

ColorLuv::ColorLuv(const Matrix& matrix) :
    ColorTriplet(matrix)
{
}

ColorLuv::ColorLuv(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference)
{
    if(XYZ.isValid())
    {
        ColorxyY white(colorReference.GetWhite());
        double scaling = YWhiteRef/white[2];
        const double epsilon(216.0 / 24389.0);
        const double kappa(24389.0 / 27.0);

        double var_Y = XYZ[1]/(white[2]*scaling);

        if(var_Y <= 0.0)
        {
            (*this)[0] = 0.0;
            (*this)[1] = 0.0;
            (*this)[2] = 0.0;
            return;
        }

        if (var_Y > epsilon)
        {
            var_Y = pow(var_Y,1.0/3.0);
        }
        else
        {
            var_Y = (kappa * var_Y + 16.0)/116.0;
        }
        ColorxyY xyY(XYZ);
        double u = 4.0*xyY[0] / (-2.0*xyY[0] + 12.0*xyY[0] + 3.0); 
        double v = 9.0*xyY[1] / (-2.0*xyY[0] + 12.0*xyY[1] + 3.0);
        double u_white = 4.0*white[0] / (-2.0*white[0] + 12.0*white[0] + 3.0); 
        double v_white = 9.0*white[1] / (-2.0*white[0] + 12.0*white[1] + 3.0); 
        (*this)[0] = 116.0*var_Y-16.0;	 // CIE-L*
        (*this)[1] = 13.0 * (*this)[0] * (u - u_white);
        (*this)[2] = 13.0 * (*this)[0] * (v - v_white);
    }
}

ColorLuv::ColorLuv(double l, double a, double b) :
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
    m_XYZValues = ColorXYZ(Matrix(theFile));

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
    return m_XYZValues.GetDeltaE(YWhite, refColor.m_XYZValues, YWhiteRef, colorReference, useOldDeltaEFormula);
}

double CColor::GetDeltaE(const CColor & refColor) const
{
    return m_XYZValues.GetOldDeltaE(refColor.m_XYZValues);
}


double CColor::GetDeltaxy(const CColor & refColor, const CColorReference& colorReference) const
{
    return m_XYZValues.GetDeltaxy(refColor.m_XYZValues, colorReference);
}

ColorXYZ CColor::GetXYZValue() const 
{
	return m_XYZValues;
}

ColorRGB CColor::GetRGBValue(CColorReference colorReference) const 
{
	if(isValid())
	{
		CColorReference aColorRef=GetStandardColorReference(HDTV);
        CLockWhileInScope dummy(m_matrixSection);
		return ColorRGB((colorReference.m_standard==3||colorReference.m_standard==4)?aColorRef.XYZtoRGBMatrix*(m_XYZValues):colorReference.XYZtoRGBMatrix*(m_XYZValues));
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

void CColor::applyAdjustmentMatrix(const Matrix& adjustment)
{
    if(m_XYZValues.isValid())
    {
        ColorXYZ newValue(adjustment * m_XYZValues);
        m_XYZValues = newValue;
    }
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

void GenerateSaturationColors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int nSteps, bool bRed, bool bGreen, bool bBlue)
{
    // Retrieve color luma coefficients matching actual reference
    const double KR = colorReference.GetRedReferenceLuma ();  
    const double KG = colorReference.GetGreenReferenceLuma ();
    const double KB = colorReference.GetBlueReferenceLuma (); 

    double K = ( bRed ? KR : 0.0 ) + ( bGreen ? KG : 0.0 ) + ( bBlue ? KB : 0.0 );

    // Compute vector between neutral gray and saturated color in CIExy space
    ColorRGB Clr1;
    double	xstart, ystart, xend, yend;

    // Retrieve gray xy coordinates
    ColorxyY whitexyY(colorReference.GetWhite());
    xstart = whitexyY[0];
    ystart = whitexyY[1];

    // Define target color in RGB mode
    Clr1[0] = ( bRed ? 1.0 : 0.0 );
    Clr1[1] = ( bGreen ? 1.0 : 0.0 );
    Clr1[2] = ( bBlue ? 1.0 : 0.0 );

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
            clr = K;
            comp = K;
        }
        else if ( i == nSteps - 1 )
        {
            clr = 1.0;
            comp = 0.0;
        }
        else
        {
            double x, y;

            x = xstart + ( (xend - xstart) * (double) i / (double) (nSteps - 1) );
            y = ystart + ( (yend - ystart) * (double) i / (double)(nSteps - 1) );

            ColorxyY UnsatClr_xyY(x,y,K);
            ColorXYZ UnsatClr(UnsatClr_xyY);

            ColorRGB UnsatClr_rgb(UnsatClr, colorReference);

            // Both components are theoretically equal, get medium value
            clr = ( ( ( bRed ? UnsatClr_rgb[0] : 0.0 ) + ( bGreen ? UnsatClr_rgb[1] : 0.0 ) + ( bBlue ? UnsatClr_rgb[2] : 0.0 ) ) / (double) ( bRed + bGreen + bBlue ) );
            comp = ( ( K - ( K * (double) clr ) ) / ( 1.0 - K ) );

            if ( clr < 0.0 )
            {
                clr = 0.0;
            }
            else if ( clr > 1.0 )
            {
                clr = 1.0;
            }
            if ( comp < 0.0 )
            {
                comp = 0.0;
            }
            else if ( comp > 1.0 )
            {
                comp = 1.0;
            }
        }
        // adjust "color gamma"
        double clr2 = ( 100.0 * pow ( clr , 0.45 ) );
        double comp2 = ( 100.0 * pow ( comp , 0.45 ) );
        GenColors [ i ] = ColorRGBDisplay( ( bRed ? clr2 : comp2 ), ( bGreen ? clr2 : comp2 ), ( bBlue ? clr2 : comp2 ) );
    }
}

Matrix ComputeConversionMatrix3Colour(const ColorXYZ measures[3], const ColorXYZ references[3])
{
    Matrix measuresXYZ(measures[0]);
    measuresXYZ.CMAC(measures[1]);
    measuresXYZ.CMAC(measures[2]);

    Matrix referencesXYZ(references[0]);
    referencesXYZ.CMAC(references[1]);
    referencesXYZ.CMAC(references[2]);

    if(measuresXYZ.Determinant() == 0) // check that reference matrix is inversible
    {
        throw std::logic_error("Can't invert measures matrix");
    }

    // Use only primary colors to compute transformation matrix
    Matrix result = referencesXYZ*measuresXYZ.GetInverse();
    return result;
}

Matrix ComputeConversionMatrix(const ColorXYZ measures[3], const ColorXYZ references[3], const ColorXYZ & WhiteTest, const ColorXYZ & WhiteRef, bool	bUseOnlyPrimaries)
{
    if (!WhiteTest.isValid() || !WhiteRef.isValid() || bUseOnlyPrimaries)
    {
        return ComputeConversionMatrix3Colour(measures, references);
    }

    // implement algorithm from :
    // http://www.avsforum.com/avs-vb/attachment.php?attachmentid=246852&d=1337250619

    // get the inputs as xyz
    Matrix measuresxyz = Colorxyz(measures[0]);
    measuresxyz.CMAC(Colorxyz(measures[1]));
    measuresxyz.CMAC(Colorxyz(measures[2]));
    Matrix referencesxyz = Colorxyz(references[0]);
    referencesxyz.CMAC(Colorxyz(references[1]));
    referencesxyz.CMAC(Colorxyz(references[2]));
    Colorxyz whiteMeasurexyz(WhiteTest);
    Colorxyz whiteReferencexyz(WhiteRef);

    Matrix RefWhiteGain(referencesxyz.GetInverse() * whiteReferencexyz);
    Matrix MeasWhiteGain(measuresxyz.GetInverse() * whiteMeasurexyz);

    // Transform component gain matrix into a diagonal matrix
    Matrix kr(0.0,3,3);
    kr(0,0) = RefWhiteGain(0,0);
    kr(1,1) = RefWhiteGain(1,0);
    kr(2,2) = RefWhiteGain(2,0);

    // Transform component gain matrix into a diagonal matrix
    Matrix km(0.0,3,3);
    km(0,0) = MeasWhiteGain(0,0);
    km(1,1) = MeasWhiteGain(1,0);
    km(2,2) = MeasWhiteGain(2,0);

    // Compute component distribution for reference white
    Matrix N(referencesxyz * kr);

    // Compute component distribution for measured white
    Matrix M(measuresxyz * km);

    // Compute transformation matrix
    Matrix transform(N * M.GetInverse());

    // find out error adjustment in white value with this matrix
    ColorXYZ testResult(transform * WhiteTest);
    double errorAdjustment(WhiteRef[1] / testResult[1]);

    // and scale the matrix with this value
    transform *= errorAdjustment;

    return transform;
}

double ArrayIndexToGrayLevel ( int nCol, int nSize, bool m_bUseRoundDown)
{
    // Gray percent: return a value between 0 and 100
	// normal rounding (GCD disk), round down (AVSHD disk)

//    return ( (double)nCol*100.0/(double)(nSize-1) );
	if (m_bUseRoundDown)
		return ( floor((double)nCol / (double)(nSize-1) * 219.0) / 219.0 * 100.0 );
	else
		return ( floor((double)nCol / (double)(nSize-1) * 219.0 + 0.5) / 219.0 * 100.0 );


}

double GrayLevelToGrayProp ( double Level, bool m_bUseRoundDown)
{
    // Gray Level: return a value between 0 and 1
//    normal rounding (GCD disk), round down (AVSHD disk)
	if (m_bUseRoundDown)
    	return Level = (floor(Level / 100.0 * 219.0 + 16.0) - 16.0) / 219.0;
	else
		return Level = (floor(Level / 100.0 * 219.0 + 16.5) - 16.0) / 219.0;
}

