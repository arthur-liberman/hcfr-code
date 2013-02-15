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
	sRGB = 5
} ColorStandard;

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
    double GetDeltaE(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, bool useOldDeltaEFormula) const;
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
    double GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, bool useOldDeltaEFormula) const;
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
	CColorReference(ColorStandard aStandard, WhiteTarget aWhiteTarget=Default, double aGamma=-1.0, string strModified=" modified");
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
	
	// Primary colors relative-to-white luminance, depending on color standard. White luma reference value is 1.
	//standard 3 added 75% of rec709 colorspace to be used with 75% saturation patterns so ref luma is the same as full colorspace
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
				luma = 0.348;
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
				luma = 0.186;
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
				luma = 0.132;
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
				luma = 0.233;
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
				luma = 0.444;
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
				luma = 0.426;
				break;
			default:
				luma = RGBtoXYZMatrix(1,0)+RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
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
extern void GenerateSaturationColors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int nSteps, bool bRed, bool bGreen, bool bBlue);
extern Matrix ComputeConversionMatrix(const ColorXYZ measures[3], const ColorXYZ references[3], const ColorXYZ & WhiteTest, const ColorXYZ & WhiteRef, bool	bUseOnlyPrimaries);
double ArrayIndexToGrayLevel ( int nCol, int nSize);
double GrayLevelToGrayProp ( double Level );


#endif // !defined(COLOR_H_INCLUDED_)
