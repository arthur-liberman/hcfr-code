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
//	Georges GALLERAND
/////////////////////////////////////////////////////////////////////////////

// CIEChartImage.cpp : CIE chart drawing
//

#include "stdafx.h"
#include "color.h"
#include <math.h>


/* The following tables gives the  spectral  chromaticity  co-ordinates
   x(\lambda) and y(\lambda) for wavelengths in 5 nanometre increments
   from 380 nm through  780  nm.   These  co-ordinates  represent  the
   position in the CIE x-y space of pure spectral colours of the given
   wavelength, and  thus  define  the  outline  of  the  CIE  "tongue"
   diagram. 
*/

// The following table gives coordinates for the left side of the "tongue",
// in top to bottom order (wavelengths in reverse order)

static double spectral_chromaticity_left[][2] = {
    { 0.0743, 0.8338 },
    { 0.0566, 0.8280 },	// Added for smoothing
    { 0.0389, 0.8120 },
    { 0.0250, 0.7830 },	// Added for smoothing
    { 0.0139, 0.7502 },
    { 0.0039, 0.6548 },
    { 0.0082, 0.5384 },
    { 0.0235, 0.4127 },
    { 0.0454, 0.2950 },
    { 0.0687, 0.2007 },
    { 0.0913, 0.1327 },
    { 0.1096, 0.0868 },
    { 0.1241, 0.0578 },
    { 0.1355, 0.0399 },
    { 0.1440, 0.0297 },
    { 0.1510, 0.0227 },
    { 0.1566, 0.0177 },
    { 0.1611, 0.0138 },
    { 0.1644, 0.0109 },
    { 0.1669, 0.0086 },
    { 0.1689, 0.0069 },
    { 0.1703, 0.0058 },
    { 0.1714, 0.0051 },
    { 0.1721, 0.0048 },
    { 0.1726, 0.0048 },
    { 0.1730, 0.0048 },
    { 0.1733, 0.0048 },
    { 0.1736, 0.0049 },
    { 0.1738, 0.0049 },
    { 0.1740, 0.0050 },
    { 0.1741, 0.0050 }			/* 380 nm */
};

// The following table gives coordinates for the right side of the "tongue",
// in top to bottom order (wavelengths in ascending order) 
// The ending chromaticity is 380 nm to add the closing segment

static double spectral_chromaticity_right[][2] = {
    { 0.0743, 0.8338 },
    { 0.0943, 0.8310 },
    { 0.1142, 0.8262 },
    { 0.1547, 0.8059 },
    { 0.1929, 0.7816 },
    { 0.2296, 0.7543 },
    { 0.2658, 0.7243 },
    { 0.3016, 0.6923 },
    { 0.3373, 0.6589 },
    { 0.3731, 0.6245 },
    { 0.4087, 0.5896 },
    { 0.4441, 0.5547 },
    { 0.4788, 0.5202 },
    { 0.5125, 0.4866 },
    { 0.5448, 0.4544 },
    { 0.5752, 0.4242 },
    { 0.6029, 0.3965 },
    { 0.6270, 0.3725 },
    { 0.6482, 0.3514 },
    { 0.6658, 0.3340 },
    { 0.6801, 0.3197 },
    { 0.6915, 0.3083 },
    { 0.7006, 0.2993 },
    { 0.7079, 0.2920 },
    { 0.7140, 0.2859 },
    { 0.7190, 0.2809 },
    { 0.7230, 0.2770 },
    { 0.7260, 0.2740 },
    { 0.7283, 0.2717 },
    { 0.7300, 0.2700 },
    { 0.7311, 0.2689 },
    { 0.7320, 0.2680 },
    { 0.7327, 0.2673 },
    { 0.7334, 0.2666 },
    { 0.7340, 0.2660 },
    { 0.7344, 0.2656 },
    { 0.7346, 0.2654 },
    { 0.7347, 0.2653 },         /* 730 nm */
    { 0.1741, 0.0050 }			/* 380 nm */
};

// The following table gives coordinates for the left side of the CIE u'v' diagram,
// in top to bottom order (wavelengths in reverse order)

static double spectral_chromaticity_uv_left[][2] = {
	{ 0.0501, 0.5868 },
	{ 0.0360, 0.5861 },
	{ 0.0231, 0.5837 },
	{ 0.0123, 0.5770 },
	{ 0.0046, 0.5638 },
	{ 0.0014, 0.5432 },
	{ 0.0035, 0.5131 },
	{ 0.0119, 0.4698 },
	{ 0.0282, 0.4117 },
	{ 0.0521, 0.3427 },
	{ 0.0828, 0.2708 },
	{ 0.1147, 0.2044 },
	{ 0.1441, 0.1510 },
	{ 0.1690, 0.1119 },
	{ 0.1877, 0.0871 },
	{ 0.2033, 0.0688 },
	{ 0.2161, 0.0549 },
	{ 0.2266, 0.0437 },
	{ 0.2347, 0.0350 },
	{ 0.2411, 0.0279 },
	{ 0.2461, 0.0226 },
	{ 0.2496, 0.0191 },
	{ 0.2522, 0.0169 },
	{ 0.2537, 0.0159 },
	{ 0.2545, 0.0159 },
	{ 0.2552, 0.0159 },
	{ 0.2557, 0.0159 },
	{ 0.2561, 0.0163 },
	{ 0.2564, 0.0163 },
	{ 0.2566, 0.0166 },
	{ 0.2568, 0.0166 }
};

// The following table gives coordinates for the right side of the CIE u'v' diagram,
// in top to bottom order (wavelengths in ascending order) 
// The ending chromaticity is 380 nm to add the closing segment

static double spectral_chromaticity_uv_right[][2] = {
	{ 0.0501, 0.5868 },
	{ 0.0643, 0.5865 },
	{ 0.0792, 0.5856 },
	{ 0.0953, 0.5841 },
	{ 0.1127, 0.5821 },
	{ 0.1319, 0.5796 },
	{ 0.1531, 0.5766 },
	{ 0.1766, 0.5732 },
	{ 0.2026, 0.5694 },
	{ 0.2312, 0.5651 },
	{ 0.2623, 0.5604 },
	{ 0.2960, 0.5554 },
	{ 0.3315, 0.5501 },
	{ 0.3681, 0.5446 },
	{ 0.4035, 0.5393 },
	{ 0.4379, 0.5342 },
	{ 0.4692, 0.5296 },
	{ 0.4968, 0.5254 },
	{ 0.5203, 0.5219 },
	{ 0.5399, 0.5190 },
	{ 0.5565, 0.5165 },
	{ 0.5709, 0.5143 },
	{ 0.5830, 0.5125 },
	{ 0.5929, 0.5111 },
	{ 0.6005, 0.5099 },
	{ 0.6064, 0.5090 },
	{ 0.6109, 0.5084 },
	{ 0.6138, 0.5079 },
	{ 0.6162, 0.5076 },
	{ 0.6180, 0.5073 },
	{ 0.6199, 0.5070 },
	{ 0.6215, 0.5068 },
	{ 0.6226, 0.5066 },
	{ 0.6231, 0.5065 },
	{ 0.6234, 0.5065 },
	{ 0.2568, 0.0166 }
};

static double fBlackBody_xy[][2] = {
	{ 0.6499, 0.3474 }, //  1000 K 
	{ 0.6361, 0.3594 }, //  1100 K 
	{ 0.6226, 0.3703 }, //  1200 K 
	{ 0.6095, 0.3801 }, //  1300 K 
	{ 0.5966, 0.3887 }, //  1400 K 
	{ 0.5841, 0.3962 }, //  1500 K 
	{ 0.5720, 0.4025 }, //  1600 K 
	{ 0.5601, 0.4076 }, //  1700 K 
	{ 0.5486, 0.4118 }, //  1800 K 
	{ 0.5375, 0.4150 }, //  1900 K 
	{ 0.5267, 0.4173 }, //  2000 K 
	{ 0.5162, 0.4188 }, //  2100 K 
	{ 0.5062, 0.4196 }, //  2200 K 
	{ 0.4965, 0.4198 }, //  2300 K 
	{ 0.4872, 0.4194 }, //  2400 K 
	{ 0.4782, 0.4186 }, //  2500 K 
	{ 0.4696, 0.4173 }, //  2600 K 
	{ 0.4614, 0.4158 }, //  2700 K 
	{ 0.4535, 0.4139 }, //  2800 K 
	{ 0.4460, 0.4118 }, //  2900 K 
	{ 0.4388, 0.4095 }, //  3000 K 
	{ 0.4254, 0.4044 }, //  3200 K 
	{ 0.4132, 0.3990 }, //  3400 K 
	{ 0.4021, 0.3934 }, //  3600 K 
	{ 0.3919, 0.3877 }, //  3800 K 
	{ 0.3827, 0.3820 }, //  4000 K 
	{ 0.3743, 0.3765 }, //  4200 K 
	{ 0.3666, 0.3711 }, //  4400 K 
	{ 0.3596, 0.3659 }, //  4600 K 
	{ 0.3532, 0.3609 }, //  4800 K 
	{ 0.3473, 0.3561 }, //  5000 K 
	{ 0.3419, 0.3516 }, //  5200 K 
	{ 0.3369, 0.3472 }, //  5400 K 
	{ 0.3323, 0.3431 }, //  5600 K 
	{ 0.3281, 0.3392 }, //  5800 K 
	{ 0.3242, 0.3355 }, //  6000 K 
	{ 0.3205, 0.3319 }, //  6200 K 
	{ 0.3171, 0.3286 }, //  6400 K 
	{ 0.3140, 0.3254 }, //  6600 K 
	{ 0.3083, 0.3195 }, //  7000 K 
	{ 0.3022, 0.3129 }, //  7500 K 
	{ 0.2970, 0.3071 }, //  8000 K 
	{ 0.2887, 0.2975 }, //  9000 K 
	{ 0.2824, 0.2898 }, // 10000 K 
	{ 0.2734, 0.2785 }, // 12000 K 
	{ 0.2675, 0.2707 }, // 14000 K 
	{ 0.2634, 0.2650 }, // 16000 K 
	{ 0.2580, 0.2574 }, // 20000 K 
	{ 0.2516, 0.2481 }, // 30000 K 
	{ 0.2487, 0.2438 }  // 40000 K 
};

// Draw Black Body curve on CIE 1931(xy) or 1976(u'v') charts, depending on flag bCIEuv

void DrawBlackBodyCurve ( CDC* pDC, int cxMax, int cyMax, BOOL doFullChart, BOOL bCIEuv )
{
	int		i;
	int		nbPoints = sizeof ( fBlackBody_xy ) / sizeof ( fBlackBody_xy[0] );
	POINT	pt [ sizeof ( fBlackBody_xy ) / sizeof ( fBlackBody_xy[0] ) ];
	double	xCie, yCie;
	double	uCie, vCie;

	for ( i = 0; i < nbPoints ; i ++ )
	{
		xCie = fBlackBody_xy [ i ] [ 0 ];
		yCie = fBlackBody_xy [ i ] [ 1 ];

		if ( bCIEuv )
		{
			uCie = ( 4.0 * xCie ) / ( (-2.0 * xCie ) + ( 12.0 * yCie ) + 3.0 );
			vCie = ( 9.0 * yCie ) / ( (-2.0 * xCie ) + ( 12.0 * yCie ) + 3.0 );

			xCie = uCie;
			yCie = vCie;
		}

		pt [ i ].x = (int) ( ( xCie / (bCIEuv ? 0.7 : 0.8) ) * (double) cxMax );
		pt [ i ].y = (int) ( ( 1.0 - ( yCie / (bCIEuv ? 0.7 : 0.9) ) ) * (double) cyMax );
	}

	CPen bbcPen ( PS_SOLID, 2, ( doFullChart ? RGB(0,0,0) : RGB(80,80,80) ) );
	CPen * pOldPen = pDC -> SelectObject ( & bbcPen );
	pDC -> Polyline ( pt, nbPoints );
	pDC -> SelectObject ( pOldPen );
}



#define MODE_SEGMENT_POINT 				0
#define MODE_SEGMENT_POINT_FLAG			1
#define MODE_SEGMENT_START_LINE			2
#define MODE_SEGMENT_START_LINE_FLAG	3
#define MODE_SEGMENT_END_LINE			4

/////////////////////////////////////////////////////////////////////////////
// This structure contains data describing each segment
// (bresenham algorythm + data used to compute points in texture)

struct Elem_Bres
{
	int  AbsDeltaX, IncX, S, S1, S2, X, First_X, Last_X, First_Y, Last_Y;
	WORD Config;
};

/////////////////////////////////////////////////////////////////////////////
// This function is a specialized polygon tracing function
// It assumes polygon has a simple shape, with one left side and one right side.
// This algorythm is efficient with this simple shape, even when number of vertex is
// very high. Pixel color is computed to match CIE diagram.

void DrawCIEChart(CDC* pDC, int cxMax, int cyMax, BOOL doFullChart, BOOL doShowBlack, BOOL bCIEuv ) 
{
	int			i, j, k, l;
	int			MinY, MaxY;
	int			Y_Cour;
	int			NbLeftPoints, NbRightPoints;
	BOOL		bInPolygon;
	BOOL		bOnLine;
	int			Limit [ 8 ];
	int			Mode [ 8 ];
	POINT		ptLimit [ 8 ];
	int			NbLimits;
	int			From [ 4 ];
	int			To [ 4 ];
	POINT		ptFrom [ 4 ];
	POINT		ptTo [ 4 ];
	int			NbSegments;
	COLORREF	Color;

	int			DeltaX;
	int			AbsDeltaY;
	int			delta;
	int			nMaxRgbVal = ( doFullChart ? 255 : 192 );
	double		d, squared_white_ray = 0.02;
	double		xCie, yCie, zCie, yCieLine;
	double		uCie, vCie;
	double		val_r, val_g, val_b;
	double		val_min, val_max;
	int			nCurrentPoint [ 2 ];	// Index 0 is left side of the tongue, Index 1 is right side
	int			nbptShape [ 2 ];		// Index 0 is left side of the tongue, Index 1 is right side
	POINT		ptShape [ 2 ] [ 128 ];	// Index 0 is left side of the tongue, Index 1 is right side
	Elem_Bres	TB [ 2 ];				// Index 0 is left side of the tongue, Index 1 is right side
	double		(*pLeft) [2];
	double		(*pRight) [2];

	// Variables for CIExy to rgb conversion
	const double gamma = 1.0 / 2.2;

    CColor	WhiteReference = GetColorReference().GetWhite ();
	CColor	RedReference = GetColorReference().GetRed();
	CColor	GreenReference = GetColorReference().GetGreen();
	CColor	BlueReference = GetColorReference().GetBlue();

	const double xr = RedReference.GetxyYValue()[0];
	const double yr = RedReference.GetxyYValue()[1];
	const double zr = 1 - (xr + yr);

    const double xg = GreenReference.GetxyYValue()[0];
	const double yg = GreenReference.GetxyYValue()[1];
	const double zg = 1 - (xg + yg);

    const double xb = BlueReference.GetxyYValue()[0];
	const double yb = BlueReference.GetxyYValue()[1];
	const double zb = 1 - (xb + yb);

    const double xw = WhiteReference.GetxyYValue()[0];
	const double yw = WhiteReference.GetxyYValue()[1];
	const double zw = 1 - (xw + yw);

    /* xyz -> rgb matrix, before scaling to white. */
    double rx = yg*zb - yb*zg;
	double ry = xb*zg - xg*zb;
	double rz = xg*yb - xb*yg;
    double gx = yb*zr - yr*zb;
	double gy = xr*zb - xb*zr;
	double gz = xb*yr - xr*yb;
    double bx = yr*zg - yg*zr;
	double by = xg*zr - xr*zg;
	double bz = xr*yg - xg*yr;

    /* White scaling factors.
       Dividing by yw scales the white luminance to unity, as conventional. */
    const double rw = (rx*xw + ry*yw + rz*zw) / yw;
    const double gw = (gx*xw + gy*yw + gz*zw) / yw;
    const double bw = (bx*xw + by*yw + bz*zw) / yw;

    /* xyz -> rgb matrix, correctly scaled to white. */
    rx = rx / rw;
	ry = ry / rw;
	rz = rz / rw;
    gx = gx / gw;
	gy = gy / gw;
	gz = gz / gw;
    bx = bx / bw;
	by = by / bw;
	bz = bz / bw;

	if ( bCIEuv )
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_uv_left ) / sizeof ( spectral_chromaticity_uv_left [ 0 ] );
		pLeft = spectral_chromaticity_uv_left;

		NbRightPoints = sizeof ( spectral_chromaticity_uv_right ) / sizeof ( spectral_chromaticity_uv_right [ 0 ] );
		pRight = spectral_chromaticity_uv_right;
	}
	else
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_left ) / sizeof ( spectral_chromaticity_left [ 0 ] );
		pLeft = spectral_chromaticity_left;

		NbRightPoints = sizeof ( spectral_chromaticity_right ) / sizeof ( spectral_chromaticity_right [ 0 ] );
		pRight = spectral_chromaticity_right;
	}

	// compute parameter points 
	nbptShape [ 0 ] = 0;
	for ( i = 0; i < NbLeftPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9), max CIEuv are (0.7, 0.7)
		xCie = pLeft [ i ] [ 0 ];
		yCie = pLeft [ i ] [ 1 ];

		ptShape [ 0 ] [ nbptShape [ 0 ] ].x = (int) ( ( xCie / (bCIEuv ? 0.7 : 0.8) ) * (double) cxMax );
		ptShape [ 0 ] [ nbptShape [ 0 ] ].y = (int) ( ( 1.0 - ( yCie / (bCIEuv ? 0.7 : 0.9) ) ) * (double) cyMax );

		if ( nbptShape [ 0 ] == 0 || ptShape [ 0 ] [ nbptShape [ 0 ] ].y > ptShape [ 0 ] [ nbptShape [ 0 ] - 1 ].y )
		{
			// Accept this point
			nbptShape [ 0 ] ++;
		}
	}

	nbptShape [ 1 ] = 0;
	for ( i = 0; i < NbRightPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9)
		xCie = pRight [ i ] [ 0 ];
		yCie = pRight [ i ] [ 1 ];

		ptShape [ 1 ] [ nbptShape [ 1 ] ].x = (int) ( ( xCie / (bCIEuv ? 0.7 : 0.8) ) * (double) cxMax );
		ptShape [ 1 ] [ nbptShape [ 1 ] ].y = (int) ( ( 1.0 - ( yCie / (bCIEuv ? 0.7 : 0.9) ) ) * (double) cyMax );
		
		if ( nbptShape [ 1 ] == 0 || ptShape [ 1 ] [ nbptShape [ 1 ] ].y > ptShape [ 1 ] [ nbptShape [ 1 ] - 1 ].y )
		{
			// Accept this point
			nbptShape [ 1 ] ++;
		}
	}

	// Retrieve minimum and maximum Y coordinate
	// We do not have to search them like in a classical polygon drawing scheme because they
	// are perfectly ordered in constant arrays
	MinY = ptShape [ 0 ] [ 0 ].y;
	MaxY = ptShape [ 0 ] [ nbptShape [ 0 ] - 1  ].y;

	// Initialize first left and right segments
	nCurrentPoint [ 0 ] = 0;
	nCurrentPoint [ 1 ] = 0;

	for ( i = 0; i < 2; i ++ )	
	{
		// Compute bresenham values
		TB [ i ].X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].First_X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].Last_X = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x;
		TB [ i ].First_Y = ptShape [ i ] [ nCurrentPoint [ i ] ].y;
		TB [ i ].Last_Y = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y;
		DeltaX = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x - ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].IncX = ( DeltaX > 0 ) - ( DeltaX < 0 );
		AbsDeltaY = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y - ptShape [ i ] [ nCurrentPoint [ i ] ].y;
		TB [ i ].AbsDeltaX = abs ( DeltaX );
		
		if ( TB [ i ].AbsDeltaX > AbsDeltaY )
			delta = TB [ i ].AbsDeltaX;
		else
			delta = AbsDeltaY;

		if ( ! TB [ i ].AbsDeltaX )
			TB [ i ].Config = 1;
		else if ( ! AbsDeltaY )
			TB [ i ].Config = 2;
		else if ( TB [ i ].AbsDeltaX > AbsDeltaY )
		{
			TB [ i ].Config = 3;
			TB [ i ].S1 = AbsDeltaY * 2;
			TB [ i ].S = TB [ i ].S1 - TB [ i ].AbsDeltaX;
			TB [ i ].S2 = TB [ i ].S - TB [ i ].AbsDeltaX;
		}
		else
		{
			TB [ i ].Config = 4;
			TB [ i ].S1 = TB [ i ].AbsDeltaX * 2;
			TB [ i ].S = TB [ i ].S1 - AbsDeltaY;
			TB [ i ].S2 = TB [ i ].S - AbsDeltaY;
		}
	}

	// Run along Y
	Y_Cour = MinY;
	while ( Y_Cour <= MaxY )
	{
		NbLimits = 0;
		// i is zéro for left side, 1 for right side
		for ( i = 0 ; i < 2 ; i ++ )
		{
			if ( Y_Cour <= TB [ i ].Last_Y )
			{
				// Run Bresenham algorythm and compute limits
				switch ( TB [ i ].Config )
				{
					case 1:
						 // Vertical segment
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;

						 NbLimits ++;
						 break;

					case 2:
						 // Horizontal segment
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + TB [ i ].AbsDeltaX;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X - TB [ i ].AbsDeltaX;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }

						 Mode [ NbLimits ++ ] = MODE_SEGMENT_START_LINE;
						 Mode [ NbLimits ++ ] = MODE_SEGMENT_END_LINE;
						 break;

					case 3:
						 // Quite horizontal (more than one point on a line)
						 // Runs Bresenham algorythm and count number of pixels on this line
						 j = 0;
						 while ( TB [ i ].AbsDeltaX > 0 )
						 {
							j ++;
							TB [ i ].X += TB [ i ].IncX;
							TB [ i ].AbsDeltaX  --;
					
							if ( TB [ i ].S < 0 )
								TB [ i ].S += TB [ i ].S1;
							else
							{
								TB [ i ].S += TB [ i ].S2;
								break;
							}
						 }
					
						 // Stores limit values
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X - j;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + j;
						 }

						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE_FLAG;
						 NbLimits ++;
				 
						 Mode [ NbLimits ] = MODE_SEGMENT_END_LINE;
						 NbLimits ++;
						 break;

					case 4:
						 // Quite vertical (only one point on a line)
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;
						 NbLimits ++;

						 // Runs Bresenham algorythm
						 if ( TB [ i ].S < 0 )
							TB [ i ].S += TB [ i ].S1;
						 else
						 {
							TB [ i ].S += TB [ i ].S2;
							TB [ i ].X += TB [ i ].IncX;
						 }
						 break;
				}
			}
			
			if ( Y_Cour == TB [ i ].Last_Y )
			{
				// We need to change to the next segment
				nCurrentPoint [ i ] ++;

				// Compute bresenham values
				TB [ i ].X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].First_X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].Last_X = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x;
				TB [ i ].First_Y = ptShape [ i ] [ nCurrentPoint [ i ] ].y;
				TB [ i ].Last_Y = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y;
				DeltaX = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x - ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].IncX = ( DeltaX > 0 ) - ( DeltaX < 0 );
				AbsDeltaY = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y - ptShape [ i ] [ nCurrentPoint [ i ] ].y;
				TB [ i ].AbsDeltaX = abs ( DeltaX );
				
				if ( TB [ i ].AbsDeltaX > AbsDeltaY )
					delta = TB [ i ].AbsDeltaX;
				else
					delta = AbsDeltaY;

				if ( ! TB [ i ].AbsDeltaX )
					TB [ i ].Config = 1;
				else if ( ! AbsDeltaY )
					TB [ i ].Config = 2;
				else if ( TB [ i ].AbsDeltaX > AbsDeltaY )
				{
					TB [ i ].Config = 3;
					TB [ i ].S1 = AbsDeltaY * 2;
					TB [ i ].S = TB [ i ].S1 - TB [ i ].AbsDeltaX;
					TB [ i ].S2 = TB [ i ].S - TB [ i ].AbsDeltaX;
				}
				else
				{
					TB [ i ].Config = 4;
					TB [ i ].S1 = TB [ i ].AbsDeltaX * 2;
					TB [ i ].S = TB [ i ].S1 - AbsDeltaY;
					TB [ i ].S2 = TB [ i ].S - AbsDeltaY;
				}

				// Run Bresenham algorythm and compute limits for the newly replaced segment
				// (this is simply duplicated code)
				switch ( TB [ i ].Config )
				{
					case 1:
						 // Vertical segment
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;

						 NbLimits ++;
						 break;

					case 2:
						 // Horizontal segment
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + TB [ i ].AbsDeltaX;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X - TB [ i ].AbsDeltaX;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }

						 Mode [ NbLimits ++ ] = MODE_SEGMENT_START_LINE;
						 Mode [ NbLimits ++ ] = MODE_SEGMENT_END_LINE;
						 break;

					case 3:
						 // Quite horizontal (more than one point on a line)
						 // Runs Bresenham algorythm and count number of pixels on this line
						 j = 0;
						 while ( TB [ i ].AbsDeltaX > 0 )
						 {
							j ++;
							TB [ i ].X += TB [ i ].IncX;
							TB [ i ].AbsDeltaX  --;
					
							if ( TB [ i ].S < 0 )
								TB [ i ].S += TB [ i ].S1;
							else
							{
								TB [ i ].S += TB [ i ].S2;
								break;
							}
						 }
					
						 // Stores limit values
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X - j;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + j;
						 }

						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE_FLAG;
						 NbLimits ++;
				 
						 Mode [ NbLimits ] = MODE_SEGMENT_END_LINE;
						 NbLimits ++;
						 break;

					case 4:
						 // Quite vertical (only one point on a line)
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;
						 NbLimits ++;

						 // Runs Bresenham algorythm
						 if ( TB [ i ].S < 0 )
							TB [ i ].S += TB [ i ].S1;
						 else
						 {
							TB [ i ].S += TB [ i ].S2;
							TB [ i ].X += TB [ i ].IncX;
						 }
						 break;
				}
			}
		}

		// Compute segments limits
		NbSegments = 0;
		bInPolygon = FALSE;
		bOnLine = FALSE;

		for ( i = 0 ; i < NbLimits ; i ++ )
		{
			switch ( Mode [ i ] )
			{
				case MODE_SEGMENT_POINT_FLAG:
					 bInPolygon = ! bInPolygon;

				case MODE_SEGMENT_POINT:
					 if ( bOnLine || bInPolygon )
					 {
						if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
						{
							To [ NbSegments - 1 ] = Limit [ i + 1 ];
							ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
						}
						else
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i + 1 ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i + 1 ];
							NbSegments ++;
						}
					 }
					 else
					 {
						if ( ! NbSegments || To [ NbSegments - 1 ] != Limit [ i ] )
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i ];
							NbSegments ++;
						}
					 }
					 break;

				case MODE_SEGMENT_START_LINE_FLAG:
					 bInPolygon = ! bInPolygon;
			
				case MODE_SEGMENT_START_LINE:
					 bOnLine ++;
					 if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
					 {
						To [ NbSegments - 1 ] = Limit [ i + 1 ];
						ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
					 }
					 else
					 {
						From [ NbSegments ] = Limit [ i ];
						To [ NbSegments ] = Limit [ i + 1 ];
						ptFrom [ NbSegments ] = ptLimit [ i ];
						ptTo [ NbSegments ] = ptLimit [ i + 1 ];
						NbSegments ++;
					 }
					 break;

				case MODE_SEGMENT_END_LINE:
					 bOnLine --;
					 if ( bOnLine || bInPolygon )
					 {
						if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
						{
							To [ NbSegments - 1 ] = Limit [ i + 1 ];
							ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
						}
						else
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i + 1 ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i + 1 ];
							NbSegments ++;
						}
					 }
					 break;
			}
		}
		
		for ( i = 0 ; i < NbSegments ; i ++ )
		{
			k = max ( 0, From [ i ] );
			l = min ( cxMax - 1, To [ i ] ) - k;
			if ( l >= 0 )
			{
				// Compute color of each pixel
				// Transform pixel coordinates (k, Y_Cour) in CIExy coordinates to determine its color
				yCieLine = (double) ( cyMax - Y_Cour ) * (bCIEuv ? 0.7 : 0.9) / (double) cyMax;

				for ( j = 0 ; j <= l ; j ++ )
				{
					if ( bCIEuv )
					{
						// Use CIE u'v' coordinates: convert them into CIE xy coordinates to define pixel color
						uCie = (double) k * 0.7 / (double) cxMax;
						vCie = yCieLine;

						xCie = ( 9.0 * uCie ) / ( ( 6.0 * uCie ) - ( 16.0 * vCie ) + 12.0 );
						yCie = ( 4.0 * vCie ) / ( ( 6.0 * uCie ) - ( 16.0 * vCie ) + 12.0 );
					}
					else
					{
						xCie = (double) k * 0.8 / (double) cxMax;
						yCie = yCieLine;
					}

					// Test if we are inside white circle to enlight
					d = (xCie-xw)*(xCie-xw) + (yCie-yw)*(yCie-yw);
					if ( d < squared_white_ray )
					{
						// Enlarge smoothly white zone
						d = pow ( d / squared_white_ray, 0.5 );
						xCie = xw + (xCie - xw ) * d;
						yCie = yw + (yCie - yw ) * d;
					}

					zCie = 1.0 - ( xCie + yCie );

					/* rgb of the desired point */
					val_r = rx*xCie + ry*yCie + rz*zCie;
					val_g = gx*xCie + gy*yCie + gz*zCie;
					val_b = bx*xCie + by*yCie + bz*zCie;
					
					val_min = min ( val_r, val_g );
					if ( val_b < val_min )
						val_min = val_b;
					
					if (val_min < 0) 
					{
						val_r -= val_min;
						val_g -= val_min;
						val_b -= val_min;
					}
						
					// Scale to max(rgb) = 1. 
					val_max = max ( val_r, val_g );
					if ( val_b > val_max )
						val_max = val_b;
					
					if ( val_max > 0 ) 
					{
						val_r /= val_max;
						val_g /= val_max;
						val_b /= val_max;
					}
					
					Color = RGB ( (int)(pow(val_r,gamma) * nMaxRgbVal), (int)(pow(val_g,gamma) * nMaxRgbVal), (int)(pow(val_b,gamma) * nMaxRgbVal) );

					pDC -> SetPixel ( k ++, Y_Cour, Color );
				}
			}
		}

		Y_Cour ++;
	}

	if ( doShowBlack )
	{
		// Draw black body curve on CIE diagram
		DrawBlackBodyCurve ( pDC, cxMax, cyMax, doFullChart, bCIEuv );
	}
}

/////////////////////////////////////////////////////////////////////////////
// This function is a specialized "outside-polygon" tracing function
// It draws a white surrounding around CIE tongue.
// The algorythm is very similar to DrawCIEChart, to draw the outside of the same figure

void DrawCIEChartWhiteSurrounding(CDC* pDC, int cxMax, int cyMax, BOOL bCIEuv ) 
{
	int			i, j;
	int			MinY, MaxY;
	int			Y_Cour;
	int			NbLeftPoints, NbRightPoints;
	BOOL		bInPolygon;
	BOOL		bOnLine;
	int			Limit [ 8 ];
	int			Mode [ 8 ];
	POINT		ptLimit [ 8 ];
	int			NbLimits;
	int			From [ 4 ];
	int			To [ 4 ];
	POINT		ptFrom [ 4 ];
	POINT		ptTo [ 4 ];
	int			NbSegments;

	int			DeltaX;
	int			AbsDeltaY;
	int			delta;
	int			nCurrentPoint [ 2 ];	// Index 0 is left side of the tongue, Index 1 is right side
	int			nbptShape [ 2 ];		// Index 0 is left side of the tongue, Index 1 is right side
	POINT		ptShape [ 2 ] [ 128 ];	// Index 0 is left side of the tongue, Index 1 is right side
	Elem_Bres	TB [ 2 ];				// Index 0 is left side of the tongue, Index 1 is right side
	double		xCie, yCie;
	double		(*pLeft) [2];
	double		(*pRight) [2];

	CPen		WhitePen ( PS_SOLID,1,RGB(255,255,255 ) );
	
	if ( bCIEuv )
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_uv_left ) / sizeof ( spectral_chromaticity_uv_left [ 0 ] );
		pLeft = spectral_chromaticity_uv_left;

		NbRightPoints = sizeof ( spectral_chromaticity_uv_right ) / sizeof ( spectral_chromaticity_uv_right [ 0 ] );
		pRight = spectral_chromaticity_uv_right;
	}
	else
	{
		NbLeftPoints = sizeof ( spectral_chromaticity_left ) / sizeof ( spectral_chromaticity_left [ 0 ] );
		pLeft = spectral_chromaticity_left;

		NbRightPoints = sizeof ( spectral_chromaticity_right ) / sizeof ( spectral_chromaticity_right [ 0 ] );
		pRight = spectral_chromaticity_right;
	}

	// compute parameter points 
	nbptShape [ 0 ] = 0;
	for ( i = 0; i < NbLeftPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9), max CIEuv are (0.7, 0.7)
		xCie = pLeft [ i ] [ 0 ];
		yCie = pLeft [ i ] [ 1 ];

		ptShape [ 0 ] [ nbptShape [ 0 ] ].x = (int) ( ( xCie / (bCIEuv ? 0.7 : 0.8) ) * (double) cxMax );
		ptShape [ 0 ] [ nbptShape [ 0 ] ].y = (int) ( ( 1.0 - ( yCie / (bCIEuv ? 0.7 : 0.9) ) ) * (double) cyMax );

		if ( nbptShape [ 0 ] == 0 || ptShape [ 0 ] [ nbptShape [ 0 ] ].y > ptShape [ 0 ] [ nbptShape [ 0 ] - 1 ].y )
		{
			// Accept this point
			nbptShape [ 0 ] ++;
		}
	}

	nbptShape [ 1 ] = 0;
	for ( i = 0; i < NbRightPoints ; i ++ )
	{
		// Get CIExy or CIE u'v' coordinates and convert it in pixel coordinates in (0,0) (cxMax,cyMax)
		// Note: max CIExy coordinates are (0.8, 0.9)
		xCie = pRight [ i ] [ 0 ];
		yCie = pRight [ i ] [ 1 ];

		ptShape [ 1 ] [ nbptShape [ 1 ] ].x = (int) ( ( xCie / (bCIEuv ? 0.7 : 0.8) ) * (double) cxMax );
		ptShape [ 1 ] [ nbptShape [ 1 ] ].y = (int) ( ( 1.0 - ( yCie / (bCIEuv ? 0.7 : 0.9) ) ) * (double) cyMax );
		
		if ( nbptShape [ 1 ] == 0 || ptShape [ 1 ] [ nbptShape [ 1 ] ].y > ptShape [ 1 ] [ nbptShape [ 1 ] - 1 ].y )
		{
			// Accept this point
			nbptShape [ 1 ] ++;
		}
	}

	// Retrieve minimum and maximum Y coordinate
	// We do not have to search them like in a classical polygon drawing scheme because they
	// are perfectly ordered in constant arrays
	MinY = ptShape [ 0 ] [ 0 ].y;
	MaxY = ptShape [ 0 ] [ nbptShape [ 0 ] - 1  ].y;

	// Initialize first left and right segments
	nCurrentPoint [ 0 ] = 0;
	nCurrentPoint [ 1 ] = 0;

	for ( i = 0; i < 2; i ++ )	
	{
		// Compute bresenham values
		TB [ i ].X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].First_X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].Last_X = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x;
		TB [ i ].First_Y = ptShape [ i ] [ nCurrentPoint [ i ] ].y;
		TB [ i ].Last_Y = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y;
		DeltaX = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x - ptShape [ i ] [ nCurrentPoint [ i ] ].x;
		TB [ i ].IncX = ( DeltaX > 0 ) - ( DeltaX < 0 );
		AbsDeltaY = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y - ptShape [ i ] [ nCurrentPoint [ i ] ].y;
		TB [ i ].AbsDeltaX = abs ( DeltaX );
		
		if ( TB [ i ].AbsDeltaX > AbsDeltaY )
			delta = TB [ i ].AbsDeltaX;
		else
			delta = AbsDeltaY;

		if ( ! TB [ i ].AbsDeltaX )
			TB [ i ].Config = 1;
		else if ( ! AbsDeltaY )
			TB [ i ].Config = 2;
		else if ( TB [ i ].AbsDeltaX > AbsDeltaY )
		{
			TB [ i ].Config = 3;
			TB [ i ].S1 = AbsDeltaY * 2;
			TB [ i ].S = TB [ i ].S1 - TB [ i ].AbsDeltaX;
			TB [ i ].S2 = TB [ i ].S - TB [ i ].AbsDeltaX;
		}
		else
		{
			TB [ i ].Config = 4;
			TB [ i ].S1 = TB [ i ].AbsDeltaX * 2;
			TB [ i ].S = TB [ i ].S1 - AbsDeltaY;
			TB [ i ].S2 = TB [ i ].S - AbsDeltaY;
		}
	}

	pDC -> FillSolidRect ( 0, 0, cxMax, MinY, RGB(255,255,255) );
	pDC -> FillSolidRect ( 0, MaxY, cxMax, cyMax, RGB(255,255,255) );

	CPen * pOldPen = pDC -> SelectObject ( & WhitePen );

	// Run along Y
	Y_Cour = MinY;
	while ( Y_Cour <= MaxY )
	{
		NbLimits = 0;
		// i is zéro for left side, 1 for right side
		for ( i = 0 ; i < 2 ; i ++ )
		{
			if ( Y_Cour <= TB [ i ].Last_Y )
			{
				// Run Bresenham algorythm and compute limits
				switch ( TB [ i ].Config )
				{
					case 1:
						 // Vertical segment
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;

						 NbLimits ++;
						 break;

					case 2:
						 // Horizontal segment
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + TB [ i ].AbsDeltaX;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X - TB [ i ].AbsDeltaX;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }

						 Mode [ NbLimits ++ ] = MODE_SEGMENT_START_LINE;
						 Mode [ NbLimits ++ ] = MODE_SEGMENT_END_LINE;
						 break;

					case 3:
						 // Quite horizontal (more than one point on a line)
						 // Runs Bresenham algorythm and count number of pixels on this line
						 j = 0;
						 while ( TB [ i ].AbsDeltaX > 0 )
						 {
							j ++;
							TB [ i ].X += TB [ i ].IncX;
							TB [ i ].AbsDeltaX  --;
					
							if ( TB [ i ].S < 0 )
								TB [ i ].S += TB [ i ].S1;
							else
							{
								TB [ i ].S += TB [ i ].S2;
								break;
							}
						 }
					
						 // Stores limit values
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X - j;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + j;
						 }

						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE_FLAG;
						 NbLimits ++;
				 
						 Mode [ NbLimits ] = MODE_SEGMENT_END_LINE;
						 NbLimits ++;
						 break;

					case 4:
						 // Quite vertical (only one point on a line)
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;
						 NbLimits ++;

						 // Runs Bresenham algorythm
						 if ( TB [ i ].S < 0 )
							TB [ i ].S += TB [ i ].S1;
						 else
						 {
							TB [ i ].S += TB [ i ].S2;
							TB [ i ].X += TB [ i ].IncX;
						 }
						 break;
				}
			}
			
			if ( Y_Cour == TB [ i ].Last_Y )
			{
				// We need to change to the next segment
				nCurrentPoint [ i ] ++;

				// Compute bresenham values
				TB [ i ].X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].First_X = ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].Last_X = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x;
				TB [ i ].First_Y = ptShape [ i ] [ nCurrentPoint [ i ] ].y;
				TB [ i ].Last_Y = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y;
				DeltaX = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].x - ptShape [ i ] [ nCurrentPoint [ i ] ].x;
				TB [ i ].IncX = ( DeltaX > 0 ) - ( DeltaX < 0 );
				AbsDeltaY = ptShape [ i ] [ nCurrentPoint [ i ] + 1 ].y - ptShape [ i ] [ nCurrentPoint [ i ] ].y;
				TB [ i ].AbsDeltaX = abs ( DeltaX );
				
				if ( TB [ i ].AbsDeltaX > AbsDeltaY )
					delta = TB [ i ].AbsDeltaX;
				else
					delta = AbsDeltaY;

				if ( ! TB [ i ].AbsDeltaX )
					TB [ i ].Config = 1;
				else if ( ! AbsDeltaY )
					TB [ i ].Config = 2;
				else if ( TB [ i ].AbsDeltaX > AbsDeltaY )
				{
					TB [ i ].Config = 3;
					TB [ i ].S1 = AbsDeltaY * 2;
					TB [ i ].S = TB [ i ].S1 - TB [ i ].AbsDeltaX;
					TB [ i ].S2 = TB [ i ].S - TB [ i ].AbsDeltaX;
				}
				else
				{
					TB [ i ].Config = 4;
					TB [ i ].S1 = TB [ i ].AbsDeltaX * 2;
					TB [ i ].S = TB [ i ].S1 - AbsDeltaY;
					TB [ i ].S2 = TB [ i ].S - AbsDeltaY;
				}

				// Run Bresenham algorythm and compute limits for the newly replaced segment
				// (this is simply duplicated code)
				switch ( TB [ i ].Config )
				{
					case 1:
						 // Vertical segment
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;

						 NbLimits ++;
						 break;

					case 2:
						 // Horizontal segment
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + TB [ i ].AbsDeltaX;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X - TB [ i ].AbsDeltaX;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }

						 Mode [ NbLimits ++ ] = MODE_SEGMENT_START_LINE;
						 Mode [ NbLimits ++ ] = MODE_SEGMENT_END_LINE;
						 break;

					case 3:
						 // Quite horizontal (more than one point on a line)
						 // Runs Bresenham algorythm and count number of pixels on this line
						 j = 0;
						 while ( TB [ i ].AbsDeltaX > 0 )
						 {
							j ++;
							TB [ i ].X += TB [ i ].IncX;
							TB [ i ].AbsDeltaX  --;
					
							if ( TB [ i ].S < 0 )
								TB [ i ].S += TB [ i ].S1;
							else
							{
								TB [ i ].S += TB [ i ].S2;
								break;
							}
						 }
					
						 // Stores limit values
						 if ( TB [ i ].IncX > 0 )
						 {
							Limit [ NbLimits ] = TB [ i ].X - j;
							Limit [ NbLimits + 1 ] = TB [ i ].X;
						 }
						 else
						 {
							Limit [ NbLimits ] = TB [ i ].X;
							Limit [ NbLimits + 1 ] = TB [ i ].X + j;
						 }

						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_START_LINE_FLAG;
						 NbLimits ++;
				 
						 Mode [ NbLimits ] = MODE_SEGMENT_END_LINE;
						 NbLimits ++;
						 break;

					case 4:
						 // Quite vertical (only one point on a line)
						 Limit [ NbLimits ] = TB [ i ].X;
						 if ( TB [ i ].Last_Y == Y_Cour )
							Mode [ NbLimits ] = MODE_SEGMENT_POINT;
						 else
							Mode [ NbLimits ] = MODE_SEGMENT_POINT_FLAG;
						 NbLimits ++;

						 // Runs Bresenham algorythm
						 if ( TB [ i ].S < 0 )
							TB [ i ].S += TB [ i ].S1;
						 else
						 {
							TB [ i ].S += TB [ i ].S2;
							TB [ i ].X += TB [ i ].IncX;
						 }
						 break;
				}
			}
		}

		// Compute segments limits
		NbSegments = 0;
		bInPolygon = FALSE;
		bOnLine = FALSE;

		for ( i = 0 ; i < NbLimits ; i ++ )
		{
			switch ( Mode [ i ] )
			{
				case MODE_SEGMENT_POINT_FLAG:
					 bInPolygon = ! bInPolygon;

				case MODE_SEGMENT_POINT:
					 if ( bOnLine || bInPolygon )
					 {
						if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
						{
							To [ NbSegments - 1 ] = Limit [ i + 1 ];
							ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
						}
						else
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i + 1 ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i + 1 ];
							NbSegments ++;
						}
					 }
					 else
					 {
						if ( ! NbSegments || To [ NbSegments - 1 ] != Limit [ i ] )
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i ];
							NbSegments ++;
						}
					 }
					 break;

				case MODE_SEGMENT_START_LINE_FLAG:
					 bInPolygon = ! bInPolygon;
			
				case MODE_SEGMENT_START_LINE:
					 bOnLine ++;
					 if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
					 {
						To [ NbSegments - 1 ] = Limit [ i + 1 ];
						ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
					 }
					 else
					 {
						From [ NbSegments ] = Limit [ i ];
						To [ NbSegments ] = Limit [ i + 1 ];
						ptFrom [ NbSegments ] = ptLimit [ i ];
						ptTo [ NbSegments ] = ptLimit [ i + 1 ];
						NbSegments ++;
					 }
					 break;

				case MODE_SEGMENT_END_LINE:
					 bOnLine --;
					 if ( bOnLine || bInPolygon )
					 {
						if ( NbSegments && To [ NbSegments - 1 ] == Limit [ i ] )
						{
							To [ NbSegments - 1 ] = Limit [ i + 1 ];
							ptTo [ NbSegments - 1 ] = ptLimit [ i + 1 ];
						}
						else
						{
							From [ NbSegments ] = Limit [ i ];
							To [ NbSegments ] = Limit [ i + 1 ];
							ptFrom [ NbSegments ] = ptLimit [ i ];
							ptTo [ NbSegments ] = ptLimit [ i + 1 ];
							NbSegments ++;
						}
					 }
					 break;
			}
		}
		
		// Get left white segment xmax
		i = max ( 0, From [ 0 ] );
		if ( i > 0 )
		{
			pDC -> MoveTo ( 0, Y_Cour );
			pDC -> LineTo ( i, Y_Cour );
		}

		// Get right white segment xmin
		i = To [ NbSegments - 1 ] + 1;

		if ( i < cxMax )
		{
			pDC -> MoveTo ( i, Y_Cour );
			pDC -> LineTo ( cxMax, Y_Cour );
		}

		Y_Cour ++;
	}

	pDC -> SelectObject ( pOldPen );
}

// Draw a polygon matching DeltaE value around white reference point
// Delta E is computed in CIE u'v' space, and need conversion for CIE xy

void DrawDeltaECurve(CDC* pDC, int cxMax, int cyMax, double DeltaE, BOOL bCIEuv ) 
{
	int			i;
	const int	nbPoints = 24;
	POINT		pt [ nbPoints + 1 ];
	double		xCie, yCie;
	double		uCie, vCie;
	double		uvRay = DeltaE / 1300.0;
	double		ang;
	CColor		WhiteReference = GetColorReference().GetWhite();
	double		xr=WhiteReference.GetxyYValue()[0];
	double		yr=WhiteReference.GetxyYValue()[1];
	double		ur = 4.0*xr / (-2.0*xr + 12.0*yr + 3.0); 
	double		vr = 9.0*yr / (-2.0*xr + 12.0*yr + 3.0); 

	const double twopi = acos (-1.0) * 2.0;

	for ( i = 0; i < nbPoints ; i ++ )
	{
		ang = twopi * ( (double) i ) / ( (double) nbPoints );

		uCie = ur + ( uvRay * cos ( ang ) );
		vCie = vr + ( uvRay * sin ( ang ) );

		if ( bCIEuv )
		{
			xCie = uCie;
			yCie = vCie;
		}
		else
		{
			xCie = ( 9.0 * uCie ) / ( ( 6.0 * uCie ) - ( 16.0 * vCie ) + 12.0 );
			yCie = ( 4.0 * vCie ) / ( ( 6.0 * uCie ) - ( 16.0 * vCie ) + 12.0 );
		}

		pt [ i ].x = (int) ( ( xCie / (bCIEuv ? 0.7 : 0.8) ) * (double) cxMax );
		pt [ i ].y = (int) ( ( 1.0 - ( yCie / (bCIEuv ? 0.7 : 0.9) ) ) * (double) cyMax );
	}
	
	// Close figure
	pt [ nbPoints ] = pt [ 0 ];

	CPen dEPen ( PS_SOLID, 2, RGB(0,0,255) );
	CPen * pOldPen = pDC -> SelectObject ( & dEPen );
	pDC -> Polyline ( pt, nbPoints + 1 );
	pDC -> SelectObject ( pOldPen );
}
