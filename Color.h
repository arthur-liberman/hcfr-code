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

// Color.h: interface for the CColor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_COLOR_H__BF14995B_5241_4AE0_89EF_21D3DD240DAD__INCLUDED_)
#define AFX_COLOR_H__BF14995B_5241_4AE0_89EF_21D3DD240DAD__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define FX_NODATA -99999.99

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

class CColor: public Matrix
{
public:
	CColor();
	CColor(const CColor &aColor);
	CColor(Matrix aMatrix);
	CColor(double aX,double aY, double aZ);	// XYZ initialisation
	CColor(double ax,double ay);			// xy initialisation
	~CColor();

	CColor& operator =(const CColor& obj);

	double& operator[](const int nRow) const;
	void Serialize(CArchive& archive);

	double GetLuminance() const;
	int GetColorTemp() const;
	double GetDeltaE(double YWhite) const;
	double GetDeltaE(double YWhite, const CColor & refColor, double YWhiteRef) const;
	double GetDeltaE(const CColor & refColor) const;
	double GetDeltaxy(const CColor & refColor) const;
	double GetLValue(double YWhiteRef) const;	// L for Lab or LCH
	CColor GetXYZValue() const;
	CColor GetRGBValue() const;
	CColor GetxyYValue() const;
	CColor GetLabValue(double YWhiteRef) const;
	CColor GetLCHValue(double YWhiteRef) const;
	CColor GetSensorValue() const;

	Matrix GetSensorToXYZMatrix() const;
	void SetSensorToXYZMatrix(const Matrix & aMatrix);

	void SetXYZValue(const CColor & aColor);
	void SetRGBValue(const CColor & aColor);
	void SetxyYValue(const CColor & aColor);
	void SetLabValue(const CColor & aColor);
	void SetSensorValue(const CColor & aColor);

	void SetX(double aX) { Matrix::operator ()(0,0)=aX; }
	void SetY(double aY) { Matrix::operator ()(1,0)=aY; }
	void SetZ(double aZ) { Matrix::operator ()(2,0)=aZ; }

	double GetX() const { return Matrix::operator ()(0,0); } 
	double GetY() const { return Matrix::operator ()(1,0); }
	double GetZ() const { return Matrix::operator ()(2,0); }

	void SetSpectrum ( CSpectrum & aSpectrum );
	void ResetSpectrum ();
	BOOL HasSpectrum () const;
	CSpectrum GetSpectrum () const;

	void SetLuxValue ( double LuxValue );
	void ResetLuxValue ();
	BOOL HasLuxValue () const;
	double GetLuxValue () const;
	double GetLuxOrLumaValue () const;
	double GetPreferedLuxValue () const;

protected:
    Matrix m_XYZtoSensorMatrix;
    Matrix m_SensorToXYZMatrix;

	CSpectrum *	m_pSpectrum;
	double *	m_pLuxValue;
};

class CSpectrum: public Matrix
{
public:
	CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, int BandWidth );
	CSpectrum(int NbBands, int WaveLengthMin, int WaveLengthMax, int BandWidth, double * pValues );
	CSpectrum(CSpectrum &aSpectrum);
	~CSpectrum();

	double& operator[](const int nRow) const;
	void Serialize(CArchive& archive);

	CColor GetXYZValue() const;

public:
	int		m_WaveLengthMin;
	int		m_WaveLengthMax;
	int		m_BandWidth;
};

class CIRELevel
{
public:
	double m_redIRELevel;
	double m_greenIRELevel;
	double m_blueIRELevel;
	BOOL m_b16_235;

public:
	CIRELevel(double aIRELevel,BOOL bIRE,BOOL b16_235=FALSE);
	CIRELevel(double aRedIRELevel,double aGreenIRELevel,double aBlueIRELevel,BOOL bIRE,BOOL b16_235=FALSE);

	operator COLORREF();
};

class CColorReference
{
public:
	ColorStandard m_standard;
	WhiteTarget m_white;

private:
	CColor whiteColor;
	double u_white;	// CIE u' v' for reference white (useful for Delta E)
	double v_white;
	CString standardName;
	char *whiteName;
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
	CColorReference(ColorStandard aStandard, WhiteTarget aWhiteTarget=Default, double aGamma=-1);
	~CColorReference();
	CColor GetWhite() { return whiteColor; }
	double GetWhite_uValue() { return u_white; }
	double GetWhite_vValue() { return v_white; }
	CString GetName() {return standardName;}
	char *GetWhiteName() {return whiteName; }
	CColor GetRed() { return redPrimary; }
	CColor GetGreen() { return greenPrimary; }
	CColor GetBlue() { return bluePrimary; }
	CColor GetYellow() { return yellowSecondary; }
	CColor GetCyan() { return cyanSecondary; }
	CColor GetMagenta() { return magentaSecondary; }
	
	// Primary colors relative-to-white luminance, depending on color standard. White luma reference value is 1.
	double GetRedReferenceLuma ()	{ return RGBtoXYZMatrix(1,0); /*0.212671 in Rec709*/ }
	double GetGreenReferenceLuma ()	{ return RGBtoXYZMatrix(1,1); /*0.715160 in Rec709*/ }
	double GetBlueReferenceLuma ()	{ return RGBtoXYZMatrix(1,2); /*0.072169 in Rec709*/ }

	const Matrix & GetRGBtoXYZMatrix ()	{ return RGBtoXYZMatrix; }
	const Matrix & GetXYZtoRGBMatrix ()	{ return XYZtoRGBMatrix; }
};

extern void SetColorReference(ColorStandard aColorStandard, WhiteTarget aWhiteTarget=Default,double aGamma=-1);
extern CColorReference & GetColorReference();
extern CColorReference GetStandardColorReference(ColorStandard aColorStandard);

extern CColorReference theColorReference;
extern CColor theMeasure;
extern CColor noDataColor;

// Tool functions
extern void GenerateSaturationColors ( COLORREF * GenColors, int nSteps, BOOL bRed, BOOL bGreen, BOOL bBlue, BOOL b16_235 );
extern Matrix ComputeConversionMatrix(Matrix & measures, Matrix & references, CColor & WhiteTest, CColor & WhiteRef );
double ArrayIndexToGrayLevel ( int nCol, int nSize, BOOL bIRE );
double GrayLevelToGrayProp ( double Level, BOOL bIRE );


#endif // !defined(AFX_COLOR_H__BF14995B_5241_4AE0_89EF_21D3DD240DAD__INCLUDED_)
