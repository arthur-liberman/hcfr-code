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
#include <sstream>

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
ColorXYZ illuminantD93(ColorxyY(0.284863,0.293221));
//ColorXYZ illuminantDCI(ColorxyY(0.333,0.334));//check this
ColorXYZ illuminantDCI(ColorxyY(0.314,0.351));

//initialize manual colors
ColorxyY primariesPAL[3] =	{	ColorxyY(0.6400, 0.3300),
								ColorxyY(0.2900, 0.6000),
								ColorxyY(0.1500, 0.0600) };

ColorxyY primariesRec601[3] ={	ColorxyY(0.6300, 0.3400),
								ColorxyY(0.3100, 0.5950),
								ColorxyY(0.1550, 0.0700) };

ColorxyY primariesRec709[3] ={	ColorxyY(0.6400, 0.3300),
								ColorxyY(0.3000, 0.6000),
								ColorxyY(0.1500, 0.0600) };

ColorxyY primariesDCI[3] ={	ColorxyY(0.6800, 0.3200),
								ColorxyY(0.265, 0.6900),
								ColorxyY(0.1500, 0.0600) };

ColorxyY primariesRec709a[3] ={	ColorxyY(0.5575, 0.3298), //75% sat/lum Rec709 w/2.2 gamma
								ColorxyY(0.3032, 0.5313),
								ColorxyY(0.1911, 0.1279) };

ColorxyY primariesCC6[3] ={	ColorxyY(0.3787, 0.3564), //some color check references, secondardies will add 3 more, GCD values
								ColorxyY(0.2484, 0.2647),
								ColorxyY(0.3418, 0.4327)};

//ColorxyY primariesCC6a[3] ={	ColorxyY(0.3804, 0.3565), //some color check references, secondardies will add 3 more, MCD values
//								ColorxyY(0.2493, 0.2667),
//								ColorxyY(0.3379, 0.4327)};

//optimized for plasma
ColorxyY primariesCC6a[3] ={	ColorxyY(0.625, 0.330), 
								ColorxyY(0.303, 0.533),
								ColorxyY(0.245, 0.217)};

/* The 75% saturation 75% amplitude and color checker xy locations are calculated 
assuming gamma=2.2, starting with the follow triplets from the GCD disk, and then used as pseudo-primaries/secondaries

75% (16-235)
	R	G	B	Y	C	M
R'	165	77	58	180	95	156
G'	60	176	58	180	176	80
B'	60	77	126	88	176	156

CC6 (16-235)
	R	G	B	Y	C	M
R'	182	97	93	80	152	213
G'	145	121	108	95	176	154
B'	128	150	73	156	71	55

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

CColorReference::CColorReference(ColorStandard aColorStandard, WhiteTarget aWhiteTarget, double aGamma, string	strModified, ColorXYZ c_whiteColor, ColorxyY c_redColor, ColorxyY c_greenColor, ColorxyY c_blueColor)
{
	m_standard = aColorStandard;
    ColorxyY* primaries = primariesRec601;

	switch(aColorStandard)
	{
		case CUSTOM:
		{
			standardName="CUSTOM";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
			primaries[0] = c_redColor.isValid()?c_redColor:primariesRec709[0];
			primaries[1] = c_greenColor.isValid()?c_greenColor:primariesRec709[1];
			primaries[2] = c_blueColor.isValid()?c_blueColor:primariesRec709[2];
			break;
		}
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
		case UHDTV:
		{
			standardName="UHDTV DCI-P3";
			whiteColor=illuminantDCI;
			whiteName="DCI-P3";
			m_white=DCI;
            primaries = primariesDCI;
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
		case HDTVb:
		{
			standardName="Color Checker HDTV OPT-Plasma";
			whiteColor=illuminantD65;
			whiteName="D65";
			m_white=D65;
            primaries = primariesCC6a;
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
		case DCUST:
			standardName+=strModified;
			whiteColor=c_whiteColor;
			whiteName="CUSTOM";
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
		case D93:
			standardName+=strModified;
			whiteColor=illuminantD93;
			whiteName="D93";
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
	aColor[2] = GetRedReferenceLuma (false);
	redPrimary = ColorXYZ(aColor);

	aColor = ColorxyY(greenPrimary);
	aColor[2] = GetGreenReferenceLuma (false);
    greenPrimary = ColorXYZ(aColor);

	aColor = ColorxyY(bluePrimary);
	aColor[2] = GetBlueReferenceLuma (false);
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
	if (CColorReference::m_standard != HDTVb )
	{
    	aColor = ColorxyY ( x1 + k * dx1, y1 + k * dy1, prim1[2] + prim2[2] );
	}
	else //optimized
	{
		if (x1 <= 0.626 && x1 >= 0.624) aColor = ColorxyY(0.418,0.502,0.5644);
		if (x1 <= 0.304 && x1 >= 0.302) aColor = ColorxyY(0.226,0.329,0.4798);
		if (x1 <= 0.246 && x1 >= 0.244) aColor = ColorxyY(0.321,0.157,0.1775);
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
	// by default use virtual DisplayRGBColor function
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

double ColorXYZ::GetDeltaE(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight ) const
{
	CColorReference cRef=CColorReference(HDTV, D65, 2.2); //special modes assume rec.709
	double dE;
	if (YWhite <= 0) YWhite = 120.;
	if (YWhiteRef <= 0) YWhiteRef = 1.0;
    //gray world weighted white reference
    switch (gw_Weight)
    {
    case 1:
    {
        YWhiteRef = 0.15 * YWhiteRef;
        YWhite = 0.15 * YWhite;
        break;
    }
    case 2:
    {
        YWhiteRef = 0.05 * YWhiteRef;
        YWhite = 0.05 * YWhite;
    }
    }
	if (!(colorReference.m_standard == HDTVb || colorReference.m_standard == CC6 || colorReference.m_standard == HDTVa))
		cRef=colorReference;
	switch (dE_form)
	{
		case 0:
		{
		//CIE76uv
        ColorLuv LuvRef(refColor, YWhiteRef, cRef);
        ColorLuv Luv(*this, YWhite, cRef);
        dE = sqrt ( pow ((Luv[0] - LuvRef[0]),2) + pow((Luv[1] - LuvRef[1]),2) + pow((Luv[2] - LuvRef[2]),2) );
		break;
		}
		case 1:
		{
		//CIE76ab
        ColorLab LabRef(refColor, YWhiteRef, cRef);
        ColorLab Lab(*this, YWhite, cRef);
        dE = sqrt ( pow ((Lab[0] - LabRef[0]),2) + pow((Lab[1] - LabRef[1]),2) + pow((Lab[2] - LabRef[2]),2) );
		break;
		}
		//CIE94
		case 2:
		{
        ColorLab LabRef(refColor, YWhiteRef, cRef);
        ColorLab Lab(*this, YWhite, cRef);
		double dL2 = pow ((LabRef[0] - Lab[0]),2.0);
		double C1 = sqrt ( pow (LabRef[1],2.0) + pow (LabRef[2],2.0));
		double C2 = sqrt ( pow (Lab[1],2.0) + pow (Lab[2],2.0));
		double dC2 = pow ((C1-C2),2.0);
		double da2 = pow (LabRef[1] - Lab[1],2.0);
		double db2 = pow (LabRef[2] - Lab[2],2.0);
		double dH2 =  (da2 + db2 - dC2);
		//kl=kc=kh=1
		dE = sqrt ( dL2 + dC2/pow((1+0.045*C1),2.0) + dH2/pow((1+0.015*C1),2.0) );
		break;
		}
		case 3:
		{
		//CIE2000
        ColorLab LabRef(refColor, YWhiteRef, cRef);
        ColorLab Lab(*this, YWhite, cRef);
		double L1 = LabRef[0];
		double L2 = Lab[0];
		double Lp = (L1 + L2) / 2.0;
		double a1 = LabRef[1];
		double a2 = Lab[1];
		double b1 = LabRef[2];
		double b2 = Lab[2];
		double C1 = sqrt( pow(a1, 2.0) + pow(b1, 2.0) );
		double C2 = sqrt( pow(a2, 2.0) + pow(b2, 2.0) );
		double C = (C1 + C2) / 2.0;
		double G = (1 - sqrt ( pow(C, 7.0) / (pow(C, 7.0) + pow(25, 7.0)) ) ) / 2;
		double a1p = a1 * (1 + G);
		double a2p = a2 * (1 + G);
		double C1p = sqrt ( pow(a1p, 2.0) + pow(b1, 2.0) );
		double C2p = sqrt ( pow(a2p, 2.0) + pow(b2, 2.0) );
		double Cp = (C1p + C2p) / 2.0;
		double h1p = (atan2(b1, a1p) >= 0?atan2(b1, a1p):atan2(b1, a1p) + PI * 2.0);
		double h2p = (atan2(b2, a2p) >= 0?atan2(b2, a2p):atan2(b2, a2p) + PI * 2.0);
		double Hp = ( abs(h1p - h2p) > PI ?(h1p + h2p + PI * 2) / 2.0:(h1p + h2p) / 2.0);
		double T = 1 - 0.17 * cos(Hp - 30. / 180. * PI) + 0.24 * cos(2 * Hp) + 0.32 * cos(3 * Hp + 6.0 / 180. * PI) - 0.20 * cos(4 * Hp - 63.0 / 180. * PI);
		double dhp = ( abs(h2p - h1p) <= PI?(h2p - h1p):((h2p <= h1p)?(h2p - h1p + 2 * PI):(h2p - h1p - 2 * PI)));
		double dLp = L2 - L1;
		double dCp = C2p - C1p;
		double dHp = 2 * sqrt( C1p * C2p ) * sin( dhp / 2);
		double SL = 1 + (0.015 * pow( (Lp - 50), 2.0 ) / sqrt( 20 + pow( Lp - 50, 2.0) ) );
		double SC = 1 + 0.045 * Cp;
		double SH = 1 + 0.015 * Cp * T;
		double dtheta = 30 * exp( -1.0 * pow(( (Hp * 180. / PI - 275.0) / 25.0), 2.0) );
		double RC = 2 * sqrt ( pow(Cp, 7.0) / (pow(Cp, 7.0) + pow(25.0, 7.0)) );
		double RT = -1.0 * RC * sin(2 * dtheta / 180. * PI);
		dE = sqrt ( pow( dLp / SL, 2.0) + pow( dCp / SC, 2.0) + pow( dHp / SH, 2.0) + RT * (dCp / SC) * (dHp / SH));
		break;
		}
		case 4:
			//CMC(1:1)
		{
	        ColorLab LabRef(refColor, YWhiteRef, cRef);
		    ColorLab Lab(*this, YWhite, cRef);
			double L1 = LabRef[0];
			double L2 = Lab[0];
			double a1 = LabRef[1];
			double a2 = Lab[1];
			double b1 = LabRef[2];
			double b2 = Lab[2];
			double C1 = sqrt (pow(a1, 2.0) + pow(b1,2.0));
			double C2 = sqrt (pow(a2, 2.0) + pow(b2,2.0));
			double dC = C1 - C2;
			double dL = L1 - L2;
			double da = a1 - a2;
			double db = b1 - b2;
			double dH = sqrt( abs(pow(da, 2.0) + pow(db,2.0) - pow(dC,2.0)) );
			double SL = (L1 < 16?0.511:0.040975*L1/(1+0.01765*L1));
			double SC = 0.0638 * C1/(1 + 0.0131*C1) + 0.638;
			double F = sqrt ( pow(C1,4.0)/(pow(C1,4.0) + 1900) );
			double H1 = atan2(b1,a1) / PI * 180.; 
			H1 = (H1 < 0?H1 + 360:( (H1 >= 360) ? H1-360:H1 ) );
			double T = (H1 <= 345 && H1 >= 164 ? 0.56 + abs(0.2 * cos ((H1 + 168)/180.*PI)) : 0.36 + abs(0.4 * cos((H1+35)/180.*PI)) ); 
			double SH = SC * (F * T + 1 - F);
			dE = sqrt( pow(dL/SL,2.0)  + pow(dC/SC,2.0) + pow(dH/SH,2.0) );
			break;
		}
		case 5:
		{
            if (isGS)
            {
    		//CIE76uv
                ColorLuv LuvRef(refColor, YWhiteRef, cRef);
                ColorLuv Luv(*this, YWhite, cRef);
                dE = sqrt ( pow ((Luv[0] - LuvRef[0]),2) + pow((Luv[1] - LuvRef[1]),2) + pow((Luv[2] - LuvRef[2]),2) );
            }
            else
            {
        		//CIE2000
                ColorLab LabRef(refColor, YWhiteRef, cRef);
                ColorLab Lab(*this, YWhite, cRef);
		        double L1 = LabRef[0];
		        double L2 = Lab[0];
		        double Lp = (L1 + L2) / 2.0;
		        double a1 = LabRef[1];
		        double a2 = Lab[1];
		        double b1 = LabRef[2];
		        double b2 = Lab[2];
		        double C1 = sqrt( pow(a1, 2.0) + pow(b1, 2.0) );
		        double C2 = sqrt( pow(a2, 2.0) + pow(b2, 2.0) );
		        double C = (C1 + C2) / 2.0;
		        double G = (1 - sqrt ( pow(C, 7.0) / (pow(C, 7.0) + pow(25, 7.0)) ) ) / 2;
		        double a1p = a1 * (1 + G);
		        double a2p = a2 * (1 + G);
		        double C1p = sqrt ( pow(a1p, 2.0) + pow(b1, 2.0) );
		        double C2p = sqrt ( pow(a2p, 2.0) + pow(b2, 2.0) );
		        double Cp = (C1p + C2p) / 2.0;
		        double h1p = (atan2(b1, a1p) >= 0?atan2(b1, a1p):atan2(b1, a1p) + PI * 2.0);
		        double h2p = (atan2(b2, a2p) >= 0?atan2(b2, a2p):atan2(b2, a2p) + PI * 2.0);
		        double Hp = ( abs(h1p - h2p) > PI ?(h1p + h2p + PI * 2) / 2.0:(h1p + h2p) / 2.0);
		        double T = 1 - 0.17 * cos(Hp - 30. / 180. * PI) + 0.24 * cos(2 * Hp) + 0.32 * cos(3 * Hp + 6.0 / 180. * PI) - 0.20 * cos(4 * Hp - 63.0 / 180. * PI);
		        double dhp = ( abs(h2p - h1p) <= PI?(h2p - h1p):((h2p <= h1p)?(h2p - h1p + 2 * PI):(h2p - h1p - 2 * PI)));
		        double dLp = L2 - L1;
		        double dCp = C2p - C1p;
		        double dHp = 2 * sqrt( C1p * C2p ) * sin( dhp / 2);
		        double SL = 1 + (0.015 * pow( (Lp - 50), 2.0 ) / sqrt( 20 + pow( Lp - 50, 2.0) ) );
		        double SC = 1 + 0.045 * Cp;
		        double SH = 1 + 0.015 * Cp * T;
		        double dtheta = 30 * exp( -1.0 * pow(( (Hp * 180. / PI - 275.0) / 25.0), 2.0) );
		        double RC = 2 * sqrt ( pow(Cp, 7.0) / (pow(Cp, 7.0) + pow(25.0, 7.0)) );
		        double RT = -1.0 * RC * sin(2 * dtheta / 180. * PI);
		        dE = sqrt ( pow( dLp / SL, 2.0) + pow( dCp / SC, 2.0) + pow( dHp / SH, 2.0) + RT * (dCp / SC) * (dHp / SH));
            }
		break;
		}
	}
	return dE;
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

double CColor::GetDeltaE(const CColor & refColor) const
{
    return m_XYZValues.GetOldDeltaE(refColor.m_XYZValues);
}

double CColor::GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight ) const
{
		return m_XYZValues.GetDeltaE(YWhite, refColor.m_XYZValues, YWhiteRef, colorReference, dE_form, isGS, gw_Weight );
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
		CColorReference aColorRef=CColorReference(HDTV, D65);
        CLockWhileInScope dummy(m_matrixSection);
		return ColorRGB((colorReference.m_standard==HDTVb||colorReference.m_standard==CC6||colorReference.m_standard==HDTVa)?aColorRef.XYZtoRGBMatrix*(m_XYZValues):colorReference.XYZtoRGBMatrix*(m_XYZValues));
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
	CColorReference aColorRef=CColorReference(HDTV);
	m_XYZValues = ColorXYZ( (colorReference.m_standard==HDTVb||colorReference.m_standard==CC6||colorReference.m_standard==HDTVa)?aColorRef.RGBtoXYZMatrix*aColor:colorReference.RGBtoXYZMatrix*aColor);
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

bool GenerateCC24Colors (ColorRGBDisplay* GenColors, int aCCMode)
{
	//six cases, one for GCD sequence, one for Mascior's disk (Chromapure based), and four different generator only cases
	//GCD
	//MCD
    //CCGS 96 CalMAN ColorChecker SG patterns
    //USER user defined
	char * path;
	char appPath[255];
	path = getenv("APPDATA");
	strcpy(appPath, path);
	strcat(appPath, "\\color");
	bool bOk = true;
	switch (aCCMode)
	{
	case GCD:
		{
//GCD
            GenColors [ 0 ] = ColorRGBDisplay( 0, 0, 0 );
            GenColors [ 1 ] = ColorRGBDisplay( 62, 62, 62 );
            GenColors [ 2 ] = ColorRGBDisplay( 73, 73, 73 );
            GenColors [ 3 ] = ColorRGBDisplay( 82, 82, 82 );
            GenColors [ 4 ] = ColorRGBDisplay( 90, 90, 90 );
            GenColors [ 5 ] = ColorRGBDisplay( 100, 100, 100 );
            GenColors [ 6 ] = ColorRGBDisplay( 45.2, 31.96, 26.03 );
            GenColors [ 7 ] = ColorRGBDisplay( 75.8, 58.9, 51.14 );
            GenColors [ 8 ] = ColorRGBDisplay( 36.99, 47.95, 61.19 );
            GenColors [ 9 ] = ColorRGBDisplay( 35.16,42.01,26.03);
            GenColors [ 10 ] = ColorRGBDisplay( 51.14,50.23,68.95);
            GenColors [ 11 ] = ColorRGBDisplay( 38.81,73.97,66.21 );
            GenColors [ 12 ] = ColorRGBDisplay( 84.93,47.03,15.98);
            GenColors [ 13 ] = ColorRGBDisplay( 28.22,36.07,63.93);
            GenColors [ 14 ] = ColorRGBDisplay( 75.80, 32.88,37.90 );
            GenColors [ 15 ] = ColorRGBDisplay( 36.07, 24.20, 42.01);
            GenColors [ 16 ] = ColorRGBDisplay( 62.10, 73.06, 25.11 );
            GenColors [ 17 ] = ColorRGBDisplay( 89.95, 63.01, 17.81 );
            GenColors [ 18 ] = ColorRGBDisplay( 20.09, 24.20, 58.90);
            GenColors [ 19 ] = ColorRGBDisplay( 27.85, 57.99, 27.85);
            GenColors [ 20 ] = ColorRGBDisplay( 68.95, 19.18, 22.83);
            GenColors [ 21 ] = ColorRGBDisplay( 93.15, 78.08, 12.79 );
            GenColors [ 22 ] = ColorRGBDisplay( 73.06, 32.88, 57.08);
            GenColors [ 23 ] = ColorRGBDisplay( 0, 52.05, 63.93);
        break;
		}
	case MCD:
		{
	 	    GenColors [ 23 ] = ColorRGBDisplay( 21, 20.5, 21 );
            GenColors [ 22 ] = ColorRGBDisplay( 32.88, 32.88, 32.88 );
            GenColors [ 21 ] = ColorRGBDisplay( 47.49, 47.49, 47.03 );
            GenColors [ 20 ] = ColorRGBDisplay( 62.56, 62.56, 62.56 );
            GenColors [ 19 ] = ColorRGBDisplay( 78.54, 78.54, 78.08 );
            GenColors [ 18 ] = ColorRGBDisplay( 94.98, 94.52, 92.69 );
            GenColors [ 0 ] = ColorRGBDisplay( 44.74, 31.51, 26.03 );
            GenColors [ 1 ] = ColorRGBDisplay( 75.80, 58.45, 50.68 );
            GenColors [ 2 ] = ColorRGBDisplay( 36.99, 47.95, 60.73 );
            GenColors [ 3 ] = ColorRGBDisplay( 34.70,42.47,26.48);
            GenColors [ 4 ] = ColorRGBDisplay( 50.68,50.22,68.49);
            GenColors [ 5 ] = ColorRGBDisplay( 39.27,73.97,66.21 );
            GenColors [ 6 ] = ColorRGBDisplay( 84.47,47.49,16.44);
            GenColors [ 7 ] = ColorRGBDisplay( 28.77,35.62,64.38);
            GenColors [ 8 ] = ColorRGBDisplay( 75.80, 33.33,38.36 );
            GenColors [ 9 ] = ColorRGBDisplay( 36.07, 24.20, 42.01);
            GenColors [ 10 ] = ColorRGBDisplay( 62.10, 73.06, 24.66 );
            GenColors [ 11 ] = ColorRGBDisplay( 89.95, 63.47, 18.26 );
            GenColors [ 12 ] = ColorRGBDisplay( 19.63, 24.20, 59.36);
            GenColors [ 13 ] = ColorRGBDisplay( 28.31, 57.99, 27.85);
            GenColors [ 14 ] = ColorRGBDisplay( 68.95, 19.18, 22.83);
            GenColors [ 15 ] = ColorRGBDisplay( 93.15, 78.08, 12.79 );
            GenColors [ 16 ] = ColorRGBDisplay( 72.61, 32.42, 57.53);
            GenColors [ 17 ] = ColorRGBDisplay( 11.87, 51.60, 59.82);
		break;
		}
	case CMC:
		{
            GenColors [ 0 ] = ColorRGBDisplay( 100,	100,	100);
            GenColors [ 1 ] = ColorRGBDisplay( 89.9543379,	89.9543379,	89.9543379);
            GenColors [ 2 ] = ColorRGBDisplay( 82.19178082,	82.19178082,	82.19178082);
            GenColors [ 3 ] = ColorRGBDisplay( 73.05936073,	73.05936073,	73.05936073);
            GenColors [ 4 ] = ColorRGBDisplay( 62.10045662,	62.10045662,	62.10045662);
            GenColors [ 5 ] = ColorRGBDisplay( 0,	0,	0);
            GenColors [ 6 ] = ColorRGBDisplay( 45.20547945,	31.96347032,	26.02739726);
            GenColors [ 7 ] = ColorRGBDisplay( 75.79908676,	58.90410959,	51.14155251);
            GenColors [ 8 ] = ColorRGBDisplay( 36.98630137,	47.94520548,	61.18721461);
            GenColors [ 9 ] = ColorRGBDisplay( 35.15981735,	42.00913242,	26.02739726);
            GenColors [ 10 ] = ColorRGBDisplay( 51.14155251,	50.2283105,	68.94977169);
            GenColors [ 11 ] = ColorRGBDisplay( 38.81278539,	73.97260274,	66.21004566);
            GenColors [ 12 ] = ColorRGBDisplay( 84.93150685,	47.03196347,	15.98173516);
            GenColors [ 13 ] = ColorRGBDisplay( 29.22374429,	36.07305936,	63.92694064);
            GenColors [ 14 ] = ColorRGBDisplay( 75.79908676,	32.87671233,	37.89954338);
            GenColors [ 15 ] = ColorRGBDisplay( 36.07305936,	24.20091324,	42.00913242);
            GenColors [ 16 ] = ColorRGBDisplay( 62.10045662,	73.05936073,	25.11415525);
            GenColors [ 17 ] = ColorRGBDisplay( 89.9543379,	63.01369863,	17.80821918);
            GenColors [ 18 ] = ColorRGBDisplay( 20.0913242,	24.20091324,	58.90410959);
            GenColors [ 19 ] = ColorRGBDisplay( 27.85388128,	57.99086758,	27.85388128);
            GenColors [ 20 ] = ColorRGBDisplay( 68.94977169,	19.17808219,	22.83105023);
            GenColors [ 21 ] = ColorRGBDisplay( 93.15068493,	78.08219178,	12.78538813);
            GenColors [ 22 ] = ColorRGBDisplay( 73.05936073,	32.87671233,	57.07762557);
            GenColors [ 23 ] = ColorRGBDisplay( 0,	52.05479452,	63.92694064);		
        break;
		}
		//CalMan classic
	case CMS:
		{
            GenColors [ 0 ] = ColorRGBDisplay( 100, 100, 100 );
            GenColors [ 1 ] = ColorRGBDisplay( 0, 0, 0 );
            GenColors [ 2 ] = ColorRGBDisplay( 43.83561644,	25.11415525,	15.06849315 );
            GenColors [ 3 ] = ColorRGBDisplay( 79.9086758,	53.88127854,	40.1826484);
            GenColors [ 4 ] = ColorRGBDisplay( 100,	78.08219178,	59.8173516);
            GenColors [ 5 ] = ColorRGBDisplay( 100,	78.08219178,	67.12328767);
            GenColors [ 6 ] = ColorRGBDisplay( 96.80365297,	67.12328767,	48.85844749);
            GenColors [ 7 ] = ColorRGBDisplay( 78.08219178,	54.79452055,	36.07305936);
            GenColors [ 8 ] = ColorRGBDisplay( 56.16438356,	36.07305936,	20.0913242);
			GenColors [ 9 ] = ColorRGBDisplay( 80.82191781,	58.90410959,	45.20547945);
            GenColors [ 10 ] = ColorRGBDisplay( 63.01369863,	33.78995434,	12.78538813);
            GenColors [ 11 ] = ColorRGBDisplay( 84.01826484,	52.05479452,	36.07305936);
            GenColors [ 12 ] = ColorRGBDisplay( 82.19178082,	53.88127854,	41.09589041);
            GenColors [ 13 ] = ColorRGBDisplay( 98.17351598,	59.8173516,	45.20547945);
            GenColors [ 14 ] = ColorRGBDisplay( 78.08219178,	56.16438356,	42.00913242);
            GenColors [ 15 ] = ColorRGBDisplay( 78.99543379,	54.79452055,	42.00913242);
            GenColors [ 16 ] = ColorRGBDisplay( 79.9086758,	56.16438356,	41.09589041);
            GenColors [ 17 ] = ColorRGBDisplay( 47.94520548,	29.22374429,	15.06849315);
            GenColors [ 18 ] = ColorRGBDisplay( 84.93150685,	54.79452055,	36.98630137);
        break;
		}
		//CalMAN skin
	case CPS:
		{
            GenColors [ 0 ] = ColorRGBDisplay( 100, 100, 100 );
            GenColors [ 1 ] = ColorRGBDisplay( 84.47488584,	49.31506849,	34.24657534);
            GenColors [ 2 ] = ColorRGBDisplay( 78.99543379,	55.25114155,	48.40182648);
            GenColors [ 3 ] = ColorRGBDisplay( 93.60730594,	67.57990868,	57.99086758);
            GenColors [ 4 ] = ColorRGBDisplay( 94.52054795,	61.18721461,	52.96803653);
            GenColors [ 5 ] = ColorRGBDisplay( 75.34246575,	56.16438356,	42.92237443);
            GenColors [ 6 ] = ColorRGBDisplay( 74.88584475,	57.07762557,	49.31506849);
            GenColors [ 7 ] = ColorRGBDisplay( 55.25114155,	36.52968037,	24.65753425);
            GenColors [ 8 ] = ColorRGBDisplay( 76.25570776,	56.62100457,	49.7716895);
			GenColors [ 9 ] = ColorRGBDisplay( 75.79908676,	58.90410959,	51.14155251);
            GenColors [ 10 ] = ColorRGBDisplay( 77.16894977,	56.62100457,	48.85844749);
            GenColors [ 11 ] = ColorRGBDisplay( 62.10045662,	34.24657534,	17.80821918);
            GenColors [ 12 ] = ColorRGBDisplay( 47.03196347,	29.6803653,	18.72146119);
            GenColors [ 13 ] = ColorRGBDisplay( 81.73515982,	52.96803653,	42.46575342);
            GenColors [ 14 ] = ColorRGBDisplay( 82.64840183,	56.16438356,	43.83561644);
            GenColors [ 15 ] = ColorRGBDisplay( 93.15068493,	68.49315068,	48.40182648);
            GenColors [ 16 ] = ColorRGBDisplay( 81.73515982,	60.2739726,	42.46575342);
            GenColors [ 17 ] = ColorRGBDisplay( 45.20547945,	31.96347032,	26.48401826);
            GenColors [ 18 ] = ColorRGBDisplay( 76.25570776,	58.90410959,	51.14155251);
        break;
		}
		//Chromapure skin
	case SKIN:
		{
            GenColors [ 0 ] = ColorRGBDisplay( 100, 87.45, 76.86 );
            GenColors [ 1 ] = ColorRGBDisplay( 94.12, 83.53, 74.51 );
            GenColors [ 2 ] = ColorRGBDisplay( 93.33, 80.78, 70.20 );
            GenColors [ 3 ] = ColorRGBDisplay( 88.24, 72.15, 60.00 );
            GenColors [ 4 ] = ColorRGBDisplay( 89.80, 76.08, 59.61 );
            GenColors [ 5 ] = ColorRGBDisplay( 100, 86.27, 69.80  );
            GenColors [ 6 ] = ColorRGBDisplay( 89.80, 72.16, 56.08 );
            GenColors [ 7 ] = ColorRGBDisplay( 89.80, 62.75, 45.10 );
            GenColors [ 8 ] = ColorRGBDisplay( 90.59, 61.96, 42.75 );
            GenColors [ 9 ] = ColorRGBDisplay( 85.88, 56.47, 39.61 );
            GenColors [ 10 ] = ColorRGBDisplay( 80.78, 58.82, 48.63 );
            GenColors [ 11 ] = ColorRGBDisplay( 77.65, 47.06, 33.73 );
            GenColors [ 12 ] = ColorRGBDisplay( 72.94, 42.35, 28.63 );
            GenColors [ 13 ] = ColorRGBDisplay( 64.71, 44.71, 34.12 );
            GenColors [ 14 ] = ColorRGBDisplay( 94.12, 78.43, 78.82 );
            GenColors [ 15 ] = ColorRGBDisplay( 86.67, 65.88, 62.75 );
            GenColors [ 16 ] = ColorRGBDisplay( 72.55, 48.63, 42.75 );
            GenColors [ 17 ] = ColorRGBDisplay( 65.88, 45.88, 42.35 );
            GenColors [ 18 ] = ColorRGBDisplay( 67.84, 39.22, 32.16 );
            GenColors [ 19 ] = ColorRGBDisplay( 36.08, 21.96, 21.18 );
            GenColors [ 20 ] = ColorRGBDisplay( 79.61, 51.76, 25.88 );
            GenColors [ 21 ] = ColorRGBDisplay( 74.12, 44.71, 23.53 );
            GenColors [ 22 ] = ColorRGBDisplay( 43.92, 25.49, 22.35 );
            GenColors [ 23 ] = ColorRGBDisplay( 63.92, 52.55, 41.57 );
        break;
		}
	case AXIS:
		{
			GenColors [ 0 ] = ColorRGBDisplay(0,0,0);
			for (int i=0;i<10;i++) {GenColors [ i + 1 ] = ColorRGBDisplay( (i+1) * 10,	(i+1) * 10,	(i+1) * 10);}
			for (int i=0;i<10;i++) {GenColors [ i + 11 ] = ColorRGBDisplay( (i+1) * 10,	0,	0);}
			for (int i=0;i<10;i++) {GenColors [ i + 21 ] = ColorRGBDisplay( 0, (i+1) * 10,	0);}
			for (int i=0;i<10;i++) {GenColors [ i + 31] = ColorRGBDisplay( 0, 0, (i+1) * 10);}
			for (int i=0;i<10;i++) {GenColors [ i + 61 ] = ColorRGBDisplay( (i+1) * 10,	(i+1) * 10,	0);}
			for (int i=0;i<10;i++) {GenColors [ i + 41 ] = ColorRGBDisplay( 0, (i+1) * 10,	(i+1) * 10);}
			for (int i=0;i<10;i++) {GenColors [ i + 51] = ColorRGBDisplay( (i+1) * 10, 0, (i+1) * 10);}
        break;
		}
		//ColorCheckerSG 96 colors
    case CCSG:
        {
            GenColors [ 0 ] = ColorRGBDisplay(	100	,	100	,	100	);
            GenColors [ 1 ] = ColorRGBDisplay(	87.2146119	,	87.2146119	,	87.2146119	);
            GenColors [ 2 ] = ColorRGBDisplay(	77.1689498	,	77.1689498	,	77.1689498	);
            GenColors [ 3 ] = ColorRGBDisplay(	72.1461187	,	72.1461187	,	72.1461187	);
            GenColors [ 4 ] = ColorRGBDisplay(	67.1232877	,	67.1232877	,	67.1232877	);
            GenColors [ 5 ] = ColorRGBDisplay(	61.1872146	,	61.1872146	,	61.1872146	);
            GenColors [ 6 ] = ColorRGBDisplay(	56.1643836	,	56.1643836	,	56.1643836	);
            GenColors [ 7 ] = ColorRGBDisplay(	46.1187215	,	46.1187215	,	46.1187215	);
            GenColors [ 8 ] = ColorRGBDisplay(	42.0091324	,	42.0091324	,	42.0091324	);
            GenColors [ 9 ] = ColorRGBDisplay(	36.9863014	,	36.9863014	,	36.9863014	);
            GenColors [ 10 ] = ColorRGBDisplay(	32.8767123	,	32.8767123	,	32.8767123	);
            GenColors [ 11 ] = ColorRGBDisplay(	29.2237443	,	29.2237443	,	29.2237443	);
            GenColors [ 12 ] = ColorRGBDisplay(	21.0045662	,	21.0045662	,	21.0045662	);
            GenColors [ 13 ] = ColorRGBDisplay(	16.8949772	,	16.8949772	,	16.8949772	);
            GenColors [ 14 ] = ColorRGBDisplay(	0	,	0	,	0	);
            GenColors [ 15 ] = ColorRGBDisplay(	57.0776256	,	10.9589041	,	32.8767123	);
            GenColors [ 16 ] = ColorRGBDisplay(	29.2237443	,	17.8082192	,	27.8538813	);
            GenColors [ 17 ] = ColorRGBDisplay(	84.9315068	,	82.1917808	,	75.7990868	);
            GenColors [ 18 ] = ColorRGBDisplay(	43.8356164	,	25.1141553	,	15.0684932	);
            GenColors [ 19 ] = ColorRGBDisplay(	79.9086758	,	53.8812785	,	40.1826484	);
            GenColors [ 20 ] = ColorRGBDisplay(	35.1598174	,	43.8356164	,	51.1415525	);
            GenColors [ 21 ] = ColorRGBDisplay(	32.8767123	,	37.8995434	,	11.8721461	);
            GenColors [ 22 ] = ColorRGBDisplay(	50.2283105	,	46.1187215	,	57.0776256	);
            GenColors [ 23 ] = ColorRGBDisplay(	42.0091324	,	70.7762557	,	56.1643836	);
            GenColors [ 24 ] = ColorRGBDisplay(	100	,	78.0821918	,	59.8173516	);
            GenColors [ 25 ] = ColorRGBDisplay(	38.8127854	,	10.9589041	,	15.9817352	);
            GenColors [ 26 ] = ColorRGBDisplay(	74.8858447	,	11.8721461	,	29.2237443	);
            GenColors [ 27 ] = ColorRGBDisplay(	73.9726027	,	50.2283105	,	61.1872146	);
            GenColors [ 28 ] = ColorRGBDisplay(	42.9223744	,	35.1598174	,	53.8812785	);
            GenColors [ 29 ] = ColorRGBDisplay(	99.086758	,	78.0821918	,	70.7762557	);
            GenColors [ 30 ] = ColorRGBDisplay(	90.8675799	,	43.8356164	,	0	);
            GenColors [ 31 ] = ColorRGBDisplay(	24.2009132	,	31.0502283	,	56.1643836	);
            GenColors [ 32 ] = ColorRGBDisplay(	78.0821918	,	25.1141553	,	26.9406393	);
            GenColors [ 33 ] = ColorRGBDisplay(	31.9634703	,	15.0684932	,	31.9634703	);
            GenColors [ 34 ] = ColorRGBDisplay(	66.2100457	,	70.7762557	,	0	);
            GenColors [ 35 ] = ColorRGBDisplay(	91.7808219	,	58.9041096	,	0	);
            GenColors [ 36 ] = ColorRGBDisplay(	84.0182648	,	90.8675799	,	70.7762557	);
            GenColors [ 37 ] = ColorRGBDisplay(	79.9086758	,	0	,	8.2191781	);
            GenColors [ 38 ] = ColorRGBDisplay(	33.7899543	,	12.7853881	,	21.0045662	);
            GenColors [ 39 ] = ColorRGBDisplay(	46.1187215	,	11.8721461	,	43.8356164	);
            GenColors [ 40 ] = ColorRGBDisplay(	0	,	21.0045662	,	35.1598174	);
            GenColors [ 41 ] = ColorRGBDisplay(	73.9726027	,	85.8447489	,	72.1461187	);
            GenColors [ 42 ] = ColorRGBDisplay(	5.0228311	,	16.8949772	,	46.1187215	);
            GenColors [ 43 ] = ColorRGBDisplay(	26.0273973	,	54.7945205	,	15.9817352	);
            GenColors [ 44 ] = ColorRGBDisplay(	69.8630137	,	0	,	10.0456621	);
            GenColors [ 45 ] = ColorRGBDisplay(	96.803653	,	74.8858447	,	0	);
            GenColors [ 46 ] = ColorRGBDisplay(	75.7990868	,	26.0273973	,	47.9452055	);
            GenColors [ 47 ] = ColorRGBDisplay(	0	,	50.2283105	,	54.7945205	);
            GenColors [ 48 ] = ColorRGBDisplay(	90.8675799	,	79.9086758	,	74.8858447	);
            GenColors [ 49 ] = ColorRGBDisplay(	84.0182648	,	46.1187215	,	47.0319635	);
            GenColors [ 50 ] = ColorRGBDisplay(	74.8858447	,	0	,	15.0684932	);
            GenColors [ 51 ] = ColorRGBDisplay(	0	,	48.8584475	,	68.0365297	);
            GenColors [ 52 ] = ColorRGBDisplay(	32.8767123	,	59.8173516	,	68.0365297	);
            GenColors [ 53 ] = ColorRGBDisplay(	100	,	78.0821918	,	67.1232877	);
            GenColors [ 54 ] = ColorRGBDisplay(	74.8858447	,	84.0182648	,	75.7990868	);
            GenColors [ 55 ] = ColorRGBDisplay(	87.2146119	,	46.1187215	,	40.1826484	);
            GenColors [ 56 ] = ColorRGBDisplay(	94.0639269	,	20.0913242	,	15.0684932	);
            GenColors [ 57 ] = ColorRGBDisplay(	19.1780822	,	63.0136986	,	64.8401826	);
            GenColors [ 58 ] = ColorRGBDisplay(	0	,	22.8310502	,	26.9406393	);
            GenColors [ 59 ] = ColorRGBDisplay(	85.8447489	,	83.1050228	,	52.0547945	);
            GenColors [ 60 ] = ColorRGBDisplay(	100	,	40.1826484	,	0	);
            GenColors [ 61 ] = ColorRGBDisplay(	100	,	63.0136986	,	0	);
            GenColors [ 62 ] = ColorRGBDisplay(	0	,	22.8310502	,	20.0913242	);
            GenColors [ 63 ] = ColorRGBDisplay(	46.1187215	,	57.9908676	,	68.9497717	);
            GenColors [ 64 ] = ColorRGBDisplay(	85.8447489	,	47.9452055	,	27.8538813	);
            GenColors [ 65 ] = ColorRGBDisplay(	96.803653	,	67.1232877	,	48.8584475	);
            GenColors [ 66 ] = ColorRGBDisplay(	78.0821918	,	54.7945205	,	36.0730594	);
            GenColors [ 67 ] = ColorRGBDisplay(	56.1643836	,	36.0730594	,	20.0913242	);
            GenColors [ 68 ] = ColorRGBDisplay(	80.8219178	,	58.9041096	,	45.2054795	);
            GenColors [ 69 ] = ColorRGBDisplay(	63.0136986	,	33.7899543	,	12.7853881	);
            GenColors [ 70 ] = ColorRGBDisplay(	84.0182648	,	52.0547945	,	36.0730594	);
            GenColors [ 71 ] = ColorRGBDisplay(	78.0821918	,	69.8630137	,	0	);
            GenColors [ 72 ] = ColorRGBDisplay(	100	,	74.8858447	,	0	);
            GenColors [ 73 ] = ColorRGBDisplay(	0	,	63.0136986	,	56.1643836	);
            GenColors [ 74 ] = ColorRGBDisplay(	0	,	54.7945205	,	46.1187215	);
            GenColors [ 75 ] = ColorRGBDisplay(	82.1917808	,	53.8812785	,	41.0958904	);
            GenColors [ 76 ] = ColorRGBDisplay(	98.173516	,	59.8173516	,	45.2054795	);
            GenColors [ 77 ] = ColorRGBDisplay(	78.0821918	,	56.1643836	,	42.0091324	);
            GenColors [ 78 ] = ColorRGBDisplay(	78.9954338	,	54.7945205	,	42.0091324	);
            GenColors [ 79 ] = ColorRGBDisplay(	79.9086758	,	56.1643836	,	41.0958904	);
            GenColors [ 80 ] = ColorRGBDisplay(	47.9452055	,	29.2237443	,	15.0684932	);
            GenColors [ 81 ] = ColorRGBDisplay(	84.9315068	,	54.7945205	,	36.9863014	);
            GenColors [ 82 ] = ColorRGBDisplay(	72.1461187	,	56.1643836	,	9.1324201	);
            GenColors [ 83 ] = ColorRGBDisplay(	73.0593607	,	69.8630137	,	0	);
            GenColors [ 84 ] = ColorRGBDisplay(	25.1141553	,	21.0045662	,	14.1552511	);
            GenColors [ 85 ] = ColorRGBDisplay(	35.1598174	,	63.0136986	,	35.1598174	);
            GenColors [ 86 ] = ColorRGBDisplay(	0	,	53.8812785	,	31.0502283	);
            GenColors [ 87 ] = ColorRGBDisplay(	11.8721461	,	25.1141553	,	15.9817352	);
            GenColors [ 88 ] = ColorRGBDisplay(	22.8310502	,	63.0136986	,	42.9223744	);
            GenColors [ 89 ] = ColorRGBDisplay(	47.9452055	,	61.1872146	,	21.9178082	);
            GenColors [ 90 ] = ColorRGBDisplay(	20.0913242	,	53.8812785	,	10.0456621	);
            GenColors [ 91 ] = ColorRGBDisplay(	29.2237443	,	66.2100457	,	15.9817352	);
            GenColors [ 92 ] = ColorRGBDisplay(	78.9954338	,	52.0547945	,	16.8949772	);
            GenColors [ 93 ] = ColorRGBDisplay(	62.1004566	,	58.9041096	,	10.0456621	);
            GenColors [ 94 ] = ColorRGBDisplay(	64.8401826	,	73.0593607	,	0	);
            GenColors [ 95 ] = ColorRGBDisplay(	30.1369863	,	16.8949772	,	10.0456621	);
            break;
        }
    case USER:
        {//read in user defined colors
            char m_ApplicationPath [MAX_PATH];
			LPSTR lpStr;
            GetModuleFileName ( NULL, m_ApplicationPath, sizeof ( m_ApplicationPath ) );
			lpStr = strrchr ( m_ApplicationPath, (int) '\\' );
			lpStr [ 1 ] = '\0';
            CString strPath = m_ApplicationPath;
            ifstream colorFile(strPath+"usercolors.csv");
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }

	case CM10SAT:
        {//read in user defined colors
			strcat(appPath, "\\CM 10-Point Saturation (100AMP).csv");
			ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }

	case CM10SAT75:
        {//read in user defined colors
			strcat(appPath, "\\CM 10-Point Saturation (75AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	
	case CM4LUM:
        {//read in user defined colors
			strcat(appPath, "\\CM 4-Point Luminance.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }

	case CM5LUM:
        {//read in user defined colors
			strcat(appPath, "\\CM 5-Point Luminance.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	
	case CM10LUM:
        {//read in user defined colors
			strcat(appPath, "\\CM 10-Point Luminance.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	
	case CM4SAT:
        {//read in user defined colors
			strcat(appPath, "\\CM 4-Point Saturation (100AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	
	case CM4SAT75:
        {//read in user defined colors
			strcat(appPath, "\\CM 4-Point Saturation (75AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	
	case CM5SAT:
        {//read in user defined colors
			strcat(appPath, "\\CM 5-Point Saturation (100AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	
	case CM5SAT75:
        {//read in user defined colors
			strcat(appPath, "\\CM 5-Point Saturation (75AMP).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	
	case CM6NB:
        {//read in user defined colors
			strcat(appPath, "\\CM 6-Point Near Black.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }

	case CMDNR:
        {//read in user defined colors
			strcat(appPath, "\\CM Dynamic Range (Clipping).csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }

	case RANDOM250:
        {//read in user defined colors
			strcat(appPath, "\\Random_250.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	
	case RANDOM500:
        {//read in user defined colors
			strcat(appPath, "\\Random_500.csv");
            ifstream colorFile(appPath);
            std::string line;
            int cnt = 0;
            int n1,n2,n3;
            if (!colorFile) 
            {
				bOk = false;
	        }
			else
			{
            while(std::getline(colorFile, line) && cnt < 1000 ) //currently limited to 1000 colors
            {
                std::istringstream s(line);
                std::string field;
                s >> n1;
                getline(s, field,',');
                s >> n2;
                getline(s, field,',');
                s >> n3;
                GenColors [ cnt ] = ColorRGBDisplay(	( (n1 -16) / 219.) * 100	, (	(n2-16) / 219.) * 100	,	( (n3-16) /219. ) * 100.	);
                cnt++;
            }
			}
            break;
        }
	}
return bOk;
}

void GenerateSaturationColors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int nSteps, bool bRed, bool bGreen, bool bBlue )
{
	//use fully saturated space if user has special color space modes set
	int m_cRef=colorReference.m_standard;
	CColorReference cRef=((m_cRef==HDTVa  || m_cRef==HDTVb )?CColorReference(HDTV):colorReference);
    // Retrieve color luminance coefficients matching actual reference
    const double KR = cRef.GetRedReferenceLuma (true);  
    const double KG = cRef.GetGreenReferenceLuma (true);
    const double KB = cRef.GetBlueReferenceLuma (true); 
    double K = ( bRed ? KR : 0.0 ) + ( bGreen ? KG : 0.0 ) + ( bBlue ? KB : 0.0 );

    // Compute vector between neutral gray and saturated color in CIExy space
    ColorRGB Clr1;
    double	xstart, ystart, xend, yend;

    // Retrieve gray xy coordinates
    ColorxyY whitexyY(cRef.GetWhite());
    xstart = whitexyY[0];
    ystart = whitexyY[1];

    // Define target color in RGB mode
    Clr1[0] = ( bRed ? 1.0 : 0.0 );
    Clr1[1] = ( bGreen ? 1.0 : 0.0 );
    Clr1[2] = ( bBlue ? 1.0 : 0.0 );

    // Compute xy coordinates of 100% saturated color
    ColorXYZ Clr2(Clr1, cRef);

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

            ColorRGB UnsatClr_rgb(UnsatClr, cRef);

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
		// here we use 2.2 and targets get adjusted for user gamma: Targets assume all generated RGB triplets @2.22 gamma
        double clr2 = ( 100.0 * pow ( clr , 1.0/2.22 ) );
        double comp2 = ( 100.0 * pow ( comp , 1.0/2.22 ) );
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

double ArrayIndexToGrayLevel ( int nCol, int nSize, bool m_bUseRoundDown)//, bool m_b16_235)
{
    // Gray percent: return a value between 0 and 100 corresponding to whole number level based on
	// normal rounding (GCD disk), round down (AVSHD disk)

	//added handling of 0-255 targets
//	if (m_b16_235)
//	{
		if (m_bUseRoundDown)
		{
			return (  floor((double)nCol / (double)(nSize-1) * 219.0) / 219.0 * 100.0 );
		}
		else
		{
			return ( floor((double)nCol / (double)(nSize-1) * 219.0 + 0.5) / 219.0 * 100.0 );
		}
//	}
//	else
//	{
//		if (m_bUseRoundDown)
//			return ( floor((double)nCol / (double)(nSize-1) * 255.0) / 255.0 * 100.0 );
//		else
//			return ( floor((double)nCol / (double)(nSize-1) * 255.0 + 0.5) / 255.0 * 100.0 );
//	}
}

double GrayLevelToGrayProp ( double Level, bool m_bUseRoundDown)
{
	// round down test not needed because this was done in gray index to level call
    // Gray Level: return a value between 0 and 1 based on percentage level input
    //    normal rounding (GCD disk), round down (AVSHD disk)
//	if (m_bUseRoundDown)
//    	return Level = (floor(Level / 100.0 * 219.0 + 16.0) - 16.0) / 219.0;
		return Level = Level / 100.0;
//	else
//		return Level = (floor(Level / 100.0 * 219.0 + 16.5) - 16.0) / 219.0;
}

double getEOTF ( double valx, CColor White, CColor Black, double g_rel, double split, int mode)
{
//BT1886
	double maxL = White.isValid()?White.GetY():100.0;
	double minL = Black.isValid()?Black.GetY():0.0;
    double yh =  maxL * pow(0.5, g_rel);
    double exp0 = g_rel==0.0?2.4:g_rel;
	double outL, Lbt, offset;
//SMPTE2084
	double m1=0.1593017578125;
    double m2=78.84375;
    double c1=0.8359375;
    double c2=18.8515625;
	double c3=18.6875;
//sRGB
	double outL_sRGB;
    if( valx <= 0.04045 ) {
        outL_sRGB = valx / 12.92;
    }
    else
		outL_sRGB = pow( ( valx + 0.055 ) / 1.055, 2.4 );

	offset = split / 100.0 * minL;
    double a = pow ( ( pow (maxL,1.0/exp0 ) - pow ( offset,1.0/exp0 ) ),exp0 );
    double b = ( pow ( offset,1.0/exp0 ) ) / ( pow (maxL,1.0/exp0 ) - pow ( offset,1.0/exp0 ) );
    if (g_rel != 0.)
        exp0 = (log(yh)-log(a))/log(0.5+b);
	Lbt = ( a * pow ( (valx + b)<0?0:(valx+b), exp0 ) );

	switch (mode)
	{
		case 4:
		outL = (Lbt + minL * (1 - split / 100.))/(maxL + minL * (1 - split / 100.));
		break;
		case 5:
		outL = pow(max(pow(valx,1.0 / m2) - c1,0) / (c2 - c3 * pow(valx, 1.0 / m2)), 1.0 / m1);
		break;
		case 6:
		outL = outL_sRGB;
	}
    
	return outL;
}
