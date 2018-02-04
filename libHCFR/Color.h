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
	HDTVa = 3, //75%
	HDTVb = 4, //Plasma
	sRGB = 5,
	UHDTV = 6,  //P3
	UHDTV2 = 7, //BT.2020
	UHDTV3 = 8, //P3 in BT.2020
	UHDTV4 = 9, //R709 in BT.2020
    CUSTOM = 10,
	CC6 = 11 //unused
} ColorStandard;

typedef enum 
{
	GCD = 0,
	MCD = 1,
	CMC = 3,
	CMS = 4,
	CPS = 5,
	SKIN = 2,
	CCSG = 6,
	AXIS = 7,
	CM4LUM = 8,
	CM5LUM = 9,
	CM10LUM = 10,
	CM4SAT = 11,
	CM4SAT75 = 12,
	CM5SAT = 13,
	CM5SAT75 = 14,
	CM10SAT = 15,
	CM10SAT75 = 16,
	CM6NB = 17,
	CMDNR = 18,
	MASCIOR50 = 19,
	LG54016 = 20,
	LG54017 = 21,
	LG100017 = 22,
	LG400017 = 23,
	RANDOM250 = 24,
	RANDOM500 = 25,
    USER = 26
} CCPatterns;

typedef enum 
{
	Default = -1,
	D65 = 0,
	D55 = 1,
	D50 = 2,
	D75 = 3,
	D93 = 4,
	A=5,
	B=6,
	C=7,
	E=8,
	DCI=9,
    DCUST=10
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
    double GetDeltaLCH(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight, double &dC, double &dH ) const;
    double GetDeltaE(double YWhite, const ColorXYZ& refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight ) const;
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

class ColorICtCp: public ColorTriplet
{
public:
    ColorICtCp();
    explicit ColorICtCp(const Matrix& matrix);
    ColorICtCp(const ColorXYZ& XYZ, double YWhiteRef, CColorReference colorReference);
    ColorICtCp(double I, double Ct, double Cp);
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
	double GetDeltaLCH(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight, double &dC, double &dH ) const;
	double GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef, const CColorReference & colorReference, int dE_form, bool isGS, int gw_Weight ) const;
	double GetDeltaxy(const CColor & refColor, const CColorReference& colorReference) const;
    double GetDeltaE(const CColor & refColor) const;
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
	CColorReference(ColorStandard aStandard, WhiteTarget aWhiteTarget=Default, double aGamma=-1.0, string strModified=" ", ColorXYZ c_whitecolor=ColorXYZ(), ColorxyY c_redcolor=ColorxyY(), ColorxyY c_greencolor=ColorxyY(), ColorxyY c_bluecolor=ColorxyY() );
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

	double GetRedReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.214;
				break;
			case HDTVb:
				luma = sats?0.214:0.1346;
				break;
			default:
				luma = RGBtoXYZMatrix(1,0); 
				break;
		}
		return luma;
	}
	double GetGreenReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.709;
				break;
			case HDTVb:
				luma = sats?0.709:0.4545;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1); 
				break;
		}
		return luma;
	}
	double GetBlueReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.075;
				break;
			case HDTVb:
				luma = sats?0.075:0.2450;
				break;
			default:
				luma = RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
	}
	double GetYellowReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.939;
				break;
			case HDTVb:
				luma = sats?0.939:0.564;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1)+RGBtoXYZMatrix(1,0); 
				break;
		}
		return luma;
	}
	double GetCyanReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.787;
				break;
			case HDTVb:
				luma = sats?0.787:0.4798;
				break;
			default:
				luma = RGBtoXYZMatrix(1,1)+RGBtoXYZMatrix(1,2); 
				break;
		}
		return luma;
	}
	double GetMagentaReferenceLuma (bool sats) const
	{ 
		double luma;
		switch(m_standard)
		{
			case HDTVa:
				luma = 0.289;
				break;
			case HDTVb:
				luma = sats?0.289:0.1775;
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
			double YLumaCMC[24]={	
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
			double YLumaCMS[19]={	
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
					0.0075
				}; 
			double YLumaCPS[19]={	
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
					0.0075
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
			double YLumaCCSG[96]={	
                1.0000001	,
                0.738088067	,
                0.562500085	,
                0.48443202	,
                0.412722924	,
                0.336036738	,
                0.277845557	,
                0.179393853	,
                0.145822012	,
                0.109913453	,
                0.084623747	,
                0.065152885	,
                0.031299435	,
                0.019302872	,
                0.0	,
                0.072632706	,
                0.033599124	,
                0.649729299	,
                0.068448725	,
                0.320011162	,
                0.151796246	,
                0.101612302	,
                0.195190895	,
                0.383074167	,
                0.648655441	,
                0.032527814	,
                0.122922601	,
                0.288220499	,
                0.121063543	,
                0.654810398	,
                0.286559727	,
                0.082476567	,
                0.160002235	,
                0.033351688	,
                0.41715407	,
                0.396663407	,
                0.75617621	,
                0.129543333	,
                0.028820265	,
                0.056027244	,
                0.02947327	,
                0.653485212	,
                0.027030095	,
                0.200048555	,
                0.096366392	,
                0.574202851	,
                0.165108056	,
                0.174045419	,
                0.644588514	,
                0.286302395	,
                0.11299344	,
                0.176524689	,
                0.277228756	,
                0.655379263	,
                0.63678518	,
                0.294800906	,
                0.207018188	,
                0.289558283	,
                0.030861033	,
                0.642701225	,
                0.307158163	,
                0.469206417	,
                0.028982311	,
                0.283102257	,
                0.295624136	,
                0.507752548	,
                0.318401466	,
                0.135497878	,
                0.365812171	,
                0.141352885	,
                0.319847087	,
                0.445366757	,
                0.58900034	,
                0.276587018	,
                0.201050398	,
                0.328841056	,
                0.445066932	,
                0.332020667	,
                0.32463258	,
                0.337987282	,
                0.089262765	,
                0.344029939	,
                0.302082933	,
                0.42851571	,
                0.033222132	,
                0.284512795	,
                0.186593377	,
                0.036389845	,
                0.275583029	,
                0.284388122	,
                0.187683865	,
                0.30140631	,
                0.295256131	,
                0.295157132	,
                0.437537547	,
                0.029079704					
                };
        
        switch(aCCMode)
        {
        case GCD:
            return YLumaGCD[nCol];
        case MCD:
            return YLumaMCD[nCol];
        case CMC:
            return YLumaCMC[nCol];
        case CMS:
            return YLumaCMS[nCol];
        case CPS:
            return YLumaCPS[nCol];
        case SKIN:
            return YLumaSKIN[nCol];
        case CCSG:
            return YLumaCCSG[nCol];
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
extern void GenerateSaturationColors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int nSteps, bool bRed, bool bGreen, bool bBlue, int mode = 0, double m_DiffuseL = 94.37844, double m_MasterMinL = 0.0, double m_MasterMaxL = 4000.0, double m_TargetMinL = 0.00, double m_TargetMaxL = 700.0, bool ToneMap = FALSE, bool cBT2390 = FALSE );
extern bool GenerateCC24Colors (const CColorReference& colorReference, ColorRGBDisplay* GenColors, int aCCMode, int mode, double m_DiffuseL = 94.37844, double m_MasterMinL = 0.0, double m_MasterMaxL = 4000.0, double m_TargetMinL = 0.00, double m_TargetMaxL = 700.0, bool ToneMap = FALSE, bool cBT2390 = FALSE );
extern Matrix ComputeConversionMatrix(const ColorXYZ measures[3], const ColorXYZ references[3], const ColorXYZ & WhiteTest, const ColorXYZ & WhiteRef, bool	bUseOnlyPrimaries);
double ArrayIndexToGrayLevel ( int nCol, int nSize, bool m_bUseRoundDown, bool m_b10bit = FALSE );
double GrayLevelToGrayProp ( double Level, bool m_bUseRoundDown, bool m_b10bit = FALSE );
double getL_EOTF ( double x, CColor White, CColor Black, double g_rel, double split, int mode, double m_DiffuseL = 94.37844, double m_MasterMinL = 0.0, double m_MasterMaxL = 4000.0, double m_TargetMinL = 0.00, double m_TargetMaxL = 700.0, bool ToneMap = FALSE, bool cBT2390 = FALSE, double m_TargetSysGamma = 1.2 );

#endif // !defined(COLOR_H_INCLUDED_)
