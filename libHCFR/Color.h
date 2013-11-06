/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005-2012 Association Homecinema Francophone.  All rights reserved.
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
//  John Adcock
/////////////////////////////////////////////////////////////////////////////

// Color.h: interface for the CColor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(COLOR_H_INCLUDED_)
#define COLOR_H_INCLUDED_

#ifndef PI
#define PI 3.141592653589793f
#endif

#define FX_NODATA -99999.99

#include "libHCFR_Config.h"
#include "Matrix.h"

typedef enum 
{
	PALSECAM = 0,
	SDTV = 1,
	HDTV = 2,
	HDTVa = 3,
	CC6 = 4,
	CC6a = 5,
	sRGB = 6
} ColorStandard;

typedef enum 
{
	GCD = 0,
	MCD = 1,
	AXIS = 2,
	SKIN = 3,
} CCPatterns;

typedef enum 
{
	Default = -1,
	D65 = 0,
	D55 = 1,
	D50 = 2,
	D75 = 3,
	A=4,
	B=5,
	C=6,
	E=7,
    DCUST,
} WhiteTarget;

class CSpectrum;
class CColorReference;

class ColorTriplet : public Matrix
{
protected:
    ColorTriplet();
    ColorTriplet(const Matrix& matrix);
    ColorTriplet(double a, double b, double c);
public:
    const double& operator[](const int nRow) const;
    double& operator[](const int nRow);
    bool isValid() const;
};

class ColorRGB;
class ColorxyY;

class ColorXYZ : public ColorTriplet
{
public:
    ColorXYZ();
    explicit ColorXYZ(const Matrix& matrix);
    ColorXYZ(const ColorRGB& RGB, CColorReference colorReference);
    explicit ColorXYZ(const ColorxyY& xyY);
    ColorXYZ(double X, double Y, double Z);
    int GetColorTemp(const CColorReference& colorReference) const;
    double GetDeltaE(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS ) const;
    double GetOldDeltaE(const ColorXYZ& refColor) const;
	double GetDeltaxy(const ColorXYZ& refColor, const CColorReference& colorReference) const;
};

class ColorxyY: public ColorTriplet
{
public:
    ColorxyY();
    explicit ColorxyY(const Matrix& matrix);
    explicit ColorxyY(const ColorXYZ& XYZ);
    ColorxyY(double x, double y, double YY = 1.0);
};

class Colorxyz: public ColorTriplet
{
public:
    Colorxyz();
    explicit Colorxyz(const Matrix& matrix);
    explicit Colorxyz(const ColorXYZ& XYZ);
    Colorxyz(double x, double y, double z);
};

class ColorRGB: public ColorTriplet
{
public:
    ColorRGB();
    explicit ColorRGB(const Matrix& matrix);
    ColorRGB(const ColorXYZ& XYZ, CColorReference colorReference);
    ColorRGB(double r, double g, double b);
};

class ColorLab: public ColorTriplet
{
public:
    ColorLab();
    explicit ColorLab(const Matrix& matrix);
    ColorLab(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorLab(double L, double a, double b);
};

class ColorLuv: public ColorTriplet
{
public:
    ColorLuv();
    explicit ColorLuv(const Matrix& matrix);
    ColorLuv(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorLuv(double L, double u, double v);
};

class ColorLCH: public ColorTriplet
{
public:
    ColorLCH();
    explicit ColorLCH(const Matrix& matrix);
    ColorLCH(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorLCH(double L, double C, double H);
};

class ColorRGBDisplay : public ColorTriplet
{
public:
    ColorRGBDisplay();
    explicit ColorRGBDisplay(double aGreyPercent);
    ColorRGBDisplay(double aRedPercent,double aGreenPercent,double aBluePercent);

#ifdef LIBHCFR_HAS_WIN32_API
    explicit ColorRGBDisplay(COLORREF aColor);
    COLORREF GetColorRef(bool is16_235) const;
    static BYTE ConvertPercentToBYTE(double percent, bool is16_235);
#endif
};

class CColor
{
public:
	CColor();
	CColor(const CColor& aColor);
	CColor(const ColorXYZ& aMatrix);
	CColor(double aX,double aY, double aZ);	// XYZ initialisation
	CColor(double ax,double ay);			// xy initialisation
    CColor(ifstream &theFile);
	~CColor();

	CColor& operator =(const CColor& obj);

	const double& operator[](const int nRow) const;
	double& operator[](const int nRow);

    bool isValid() const;

	double GetLuminance() const;
    double GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS ) const;
    double GetDeltaE(const CColor & refColor) const;
	double GetDeltaxy(const CColor & refColor, const CColorReference& colorReference) const;
	ColorXYZ GetXYZValue() const;
	ColorRGB GetRGBValue(CColorReference colorReference) const;
	ColorxyY GetxyYValue() const;
	Colorxyz GetxyzValue() const;
	ColorLab GetLabValue(double YWhiteRef, CColorReference colorReference) const;
	ColorLCH GetLCHValue(double YWhiteRef, CColorReference colorReference) const;

	void SetXYZValue(const ColorXYZ& aColor);
	void SetRGBValue(const ColorRGB& aColor, CColorReference colorReference);
	void SetxyYValue(double x, double y, double Y);
    void SetxyYValue(const ColorxyY& aColor);

	void SetX(double aX) { m_XYZValues[0]=aX; }
	void SetY(double aY) { m_XYZValues[1]=aY; }
	void SetZ(double aZ) { m_XYZValues[2]=aZ; }

	double GetX() const { return m_XYZValues[0]; } 
	double GetY() const { return m_XYZValues[1]; }
	double GetZ() const { return m_XYZValues[2]; }

	void SetSpectrum ( CSpectrum & aSpectrum );
	void ResetSpectrum ();
	bool HasSpectrum () const;
	CSpectrum GetSpectrum () const;

	void SetLuxValue ( double LuxValue );
	void ResetLuxValue ();
	bool HasLuxValue () const;
	double GetLuxValue () const;
	double GetLuxOrLumaValue (const int luminanceCurveMode) const;
	double GetPreferedLuxValue (bool preferLuxmeter) const;
    void applyAdjustmentMatrix(const Matrix& adjustment);

    void Output(ostream& ostr) const;

#ifdef LIBHCFR_HAS_MFC
    void Serialize(CArchive& archive);
#endif

protected:
    ColorXYZ m_XYZValues;
	CSpectrum *	m_pSpectrum;
	double *	m_pLuxValue;
};

class CSpectrum: public Matrix
{
public:
	CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, double BandWidth );
	CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, double BandWidth, double * pValues );
    CSpectrum (ifstream &theFile, bool oldFileFormat = false);

	const double& operator[](const int nRow) const;

	CColor GetXYZValue() const;

#ifdef LIBHCFR_HAS_MFC
    void Serialize(CArchive& archive);
#endif

public:
	int		m_WaveLengthMin;
	int		m_WaveLengthMax;
	double	m_BandWidth;
};

class CColorReference
{
public:
	ColorStandard m_standard;
	WhiteTarget m_white;

	ColorXYZ whiteColor;
	string standardName;
	const char *whiteName;
	ColorXYZ redPrimary;
	ColorXYZ greenPrimary;
	ColorXYZ bluePrimary;
	ColorXYZ yellowSecondary;
	ColorXYZ cyanSecondary;
	ColorXYZ magentaSecondary;
	double gamma;
	Matrix RGBtoXYZMatrix;
	Matrix XYZtoRGBMatrix;

    void	UpdateSecondary ( ColorXYZ& secondary, const ColorXYZ& primary1, const ColorXYZ& primary2, const ColorXYZ& primaryOpposite );

public:
	CColorReference(ColorStandard aStandard, WhiteTarget aWhiteTarget=Default, double aGamma=-1.0, string strModified=" modified", ColorXYZ c_whitecolor=ColorXYZ() );
	~CColorReference();
	ColorXYZ GetWhite() const { return whiteColor; } 
	string GetName() const {return standardName;}
	const char *GetWhiteName() const {return whiteName; }
	ColorXYZ GetRed() const { return redPrimary; }
	ColorXYZ GetGreen() const { return greenPrimary; }
	ColorXYZ GetBlue() const { return bluePrimary; }
	ColorXYZ GetYellow() const { return yellowSecondary; }
	ColorXYZ GetCyan() const { return cyanSecondary; }
	ColorXYZ GetMagenta() const { return magentaSecondary; }
	
	// Primary colors relative-to-white luminance, depending on color standard. White luminance reference value is 1.
	//standard 3 added 75% of rec709 colorspace to be used with 75% saturation patterns so ref luminance is the same as full colorspace
	//standard 5 added a set of 6 color checker patterns

	double GetRedReferenceLuma () const
	{ 
		double luma;
		switch(m_standard)
		{
			case 3:
				luma = 0.214;
				break;
			case 4:
				luma = 0.3553;
				break;
			case 5:
				luma = 0.3480;
				break;
			default:
				luma = RGBtoXYZMatrix(1,0); 
				break;
		}
		return luma;
	}
	double GetGreenReferenceLuma () const
	{ 
		double luma;
		switch(m_standard)
		{
			case 3:
				luma = 0.709;
				break;
			case 4:
				luma = 0.1903;
				break;
			case 5:
				luma = 0.1871;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1); 
				break;
		}
		return luma;
	}
	double GetBlueReferenceLuma () const
	{ 
		double luma;
		switch(m_standard)
		{
			case 3:
				luma = 0.075;
				break;
			case 4:
				luma = 0.1312;
				break;
			case 5:
				luma = 0.1309;
				break;
			default:
				luma = RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
	}
	double GetYellowReferenceLuma () const
	{ 
		double luma;
		switch(m_standard)
		{
			case 3:
				luma = 0.939;
				break;
			case 4:
				luma = 0.1171;
				break;
			case 5:
				luma = 0.1128;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1)+RGBtoXYZMatrix(1,0); 
				break;
		}
		return luma;
	}
	double GetCyanReferenceLuma () const
	{ 
		double luma;
		switch(m_standard)
		{
			case 3:
				luma = 0.787;
				break;
			case 4:
				luma = 0.4365;
				break;
			case 5:
				luma = 0.4333;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1)+RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
	}
	double GetMagentaReferenceLuma () const
	{ 
		double luma;
		switch(m_standard)
		{
			case 3:
				luma = 0.289;
				break;
			case 4:
				luma = 0.4290;
				break;
			case 5:
				luma = 0.4305;
				break;
			default:
				luma = RGBtoXYZMatrix(1,0)+RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
	}
	double GetCC24ReferenceLuma (int nCol, int aCCMode)
	{ 
			double	YLumaGCD[24]={	0.0,
					0.3473,
					0.4982,
					0.6470,
					0.7905,
					1.0,
					0.0970,
					0.3521,
					0.1875,
					0.1288,
					0.2347,
					0.4211,
					0.2832,
					0.1149,
					0.1839,
					0.0633,
					0.4335,
					0.4262,
					0.0590,
					0.2300,
					0.1142,
					0.5953,
					0.1872,
					0.1946
					};
			double YLumaMCD[24]={	
					0.0302,
					0.0846,
					0.1911,
					0.3531,
					0.5844,
					0.8817,
					0.0944,
					0.3480,
					0.1871,
					0.1309,
					0.2333,
					0.4218,
					0.2845,
					0.1128,
					0.1860,
					0.0633,
					0.4333,
					0.4305,
					0.0591,
					0.2305,
					0.1142,
					0.5953,
					0.1843,
					0.1895
				}; 
			double YLumaAXIS[24]={	
					0.0019,
					0.0089,
					0.0220,
					0.0417,
					0.0684,
					0.1026,
					0.1444,
					0.1942,
					0.0065,
					0.0301,
					0.0740,
					0.1402,
					0.2301,
					0.3449,
					0.4856,
					0.6532,
					0.0007,
					0.0030,
					0.0075,
					0.0141,
					0.0232,
					0.0348,
					0.0490,
					0.0659
				}; 
			double YLumaSKIN[24]={	
					0.7839,
					0.7031,
					0.6607,
					0.5309,
					0.5802,
					0.7605,
					0.5340,
					0.4339,
					0.4288,
					0.3620,
					0.3672,
					0.2619,
					0.2162,
					0.2073,
					0.6455,
					0.4636,
					0.2595,
					0.2218,
					0.1852,
					0.0491,
					0.2976,
					0.2320,
					0.0712,
					0.2604
				}; 
        switch(aCCMode)
        {
        case GCD:
            return YLumaGCD[nCol];
        case MCD:
            return YLumaMCD[nCol];
        case AXIS:
            return YLumaAXIS[nCol];
        case SKIN:
            return YLumaSKIN[nCol];
        }
            return YLumaGCD[nCol];
	}



#ifdef LIBHCFR_HAS_MFC
    void Serialize(CArchive& archive);
#endif
};

ostream& operator <<(ostream& ostr, const CColor& obj);

extern CColorReference GetStandardColorReference(ColorStandard aColorStandard);

extern CColor theMeasure;
extern CColor noDataColor;

// Tool functions
extern void GenerateSaturationColors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int nSteps, bool bRed, bool bGreen, bool bBlue, double gamma);
extern void GenerateCC24Colors (ColorRGBDisplay* GenColors, int aCCMode);
extern Matrix ComputeConversionMatrix(const ColorXYZ measures[3], const ColorXYZ references[3], const ColorXYZ & WhiteTest, const ColorXYZ & WhiteRef, bool	bUseOnlyPrimaries);
double ArrayIndexToGrayLevel ( int nCol, int nSize, bool m_bUseRoundDown);
double GrayLevelToGrayProp ( double Level, bool m_bUseRoundDown );

#endif // !defined(COLOR_H_INCLUDED_)
