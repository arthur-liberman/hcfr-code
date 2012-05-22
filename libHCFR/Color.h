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
	sRGB = 3,
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
};

class ColorxyY: public ColorTriplet
{
public:
    ColorxyY();
    explicit ColorxyY(const Matrix& matrix);
    explicit ColorxyY(const ColorXYZ& XYZ);
    ColorxyY(double x, double y, double YY);
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

class ColorLCH: public ColorTriplet
{
public:
    ColorLCH();
    explicit ColorLCH(const Matrix& matrix);
    ColorLCH(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorLCH(double L, double C, double H);
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
	int GetColorTemp(CColorReference colorReference) const;
    double GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, bool useOldDeltaEFormula) const;
    double GetDeltaE(const CColor & refColor) const;
	double GetDeltaxy(const CColor & refColor) const;
	double GetLValue(double YWhiteRef) const;	// L for Lab or LCH
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
	void SetLabValue(const ColorLab& aColor, CColorReference colorReference);

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

class CIRELevel
{
public:
	double m_redIRELevel;
	double m_greenIRELevel;
	double m_blueIRELevel;
	bool m_b16_235;

public:
	CIRELevel(double aIRELevel,bool bIRE,bool b16_235=false);
	CIRELevel(double aRedIRELevel,double aGreenIRELevel,double aBlueIRELevel,bool bIRE,bool b16_235=false);

#ifdef WIN32
    operator COLORREF();
#endif
};

class CColorReference
{
public:
	ColorStandard m_standard;
	WhiteTarget m_white;

	CColor whiteColor;
	double u_white;	// CIE u' v' for reference white (useful for Delta E)
	double v_white;
	string standardName;
	const char *whiteName;
	CColor redPrimary;
	CColor greenPrimary;
	CColor bluePrimary;
	CColor yellowSecondary;
	CColor cyanSecondary;
	CColor magentaSecondary;
	double gamma;
	Matrix RGBtoXYZMatrix;
	Matrix XYZtoRGBMatrix;

    void	UpdateSecondary ( CColor & secondary, const CColor & primary1, const CColor & primary2, const CColor & primaryOpposite );

public:
	CColorReference(ColorStandard aStandard, WhiteTarget aWhiteTarget=Default, double aGamma=-1.0, string strModified=" modified");
	~CColorReference();
	CColor GetWhite() const { return whiteColor; } 
	double GetWhite_uValue() const { return u_white; }
	double GetWhite_vValue() const { return v_white; }
	string GetName() const {return standardName;}
	const char *GetWhiteName() const {return whiteName; }
	CColor GetRed() const { return redPrimary; }
	CColor GetGreen() const { return greenPrimary; }
	CColor GetBlue() const { return bluePrimary; }
	CColor GetYellow() const { return yellowSecondary; }
	CColor GetCyan() const { return cyanSecondary; }
	CColor GetMagenta() const { return magentaSecondary; }
	
	// Primary colors relative-to-white luminance, depending on color standard. White luma reference value is 1.
	double GetRedReferenceLuma () const { return RGBtoXYZMatrix(1,0); /*0.212671 in Rec709*/ }
	double GetGreenReferenceLuma () const { return RGBtoXYZMatrix(1,1); /*0.715160 in Rec709*/ }
	double GetBlueReferenceLuma () const { return RGBtoXYZMatrix(1,2); /*0.072169 in Rec709*/ }

	
#ifdef LIBHCFR_HAS_MFC
    void Serialize(CArchive& archive);
#endif
};

ostream& operator <<(ostream& ostr, const CColor& obj);

extern CColorReference GetStandardColorReference(ColorStandard aColorStandard);

extern CColor theMeasure;
extern CColor noDataColor;

// Tool functions
#ifdef LIBHCFR_HAS_WIN32_API
extern void GenerateSaturationColors (const CColorReference& colorReference, COLORREF * GenColors, int nSteps, BOOL bRed, BOOL bGreen, BOOL bBlue, BOOL b16_235 );
#endif
extern Matrix ComputeConversionMatrix(const ColorXYZ measures[3], const ColorXYZ references[3], const ColorXYZ & WhiteTest, const ColorXYZ & WhiteRef, bool	bUseOnlyPrimaries);
double ArrayIndexToGrayLevel ( int nCol, int nSize, bool bIRE );
double GrayLevelToGrayProp ( double Level, bool bIRE );


#endif // !defined(COLOR_H_INCLUDED_)
