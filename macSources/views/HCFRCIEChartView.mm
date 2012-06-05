//  ColorHCFR
//  HCFRCIEChartView.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 04/09/07.
//
//  $Rev: 135 $
//  $Date: 2011-02-27 10:48:51 +0000 (Sun, 27 Feb 2011) $
//  $Author: jerome $
// -----------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation; either version 2 of the License,
// or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
// the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
// Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program; if not,
// write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// -----------------------------------------------------------------------------

#import "HCFRCIEChartView.h"

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

@interface HCFRCIEChartView (Private)
#pragma mark Fonctions de dessin utilitaires
-(void) drawCircleAtPoint:(NSPoint)center radius:(int)radius borderColor:(NSColor*)borderColor fillColor:(NSColor*)fillColor;
-(void) drawGrid;
-(void) drawBlackBodyCurve;
- (void)createBackgroundReps;
@end

@implementation HCFRCIEChartView

- (HCFRCIEChartView*)initWithFrame:(NSRect)frame colorReference:(HCFRColorReference*)newReference useUVCoordinates:(bool)useUV
{
  self = [super initWithFrame:frame];
    if (self) {
      reference = [newReference retain];
      useUVCoordinates = useUV;
      
      redComponent = nil;
      greenComponent = nil;
      blueComponent = nil;
      
      xyBackgroundImageRep = nil;
      uvBackgroundImageRep = nil;
      
      [self setAutoresizingMask:NSViewMinXMargin|NSViewMaxXMargin|NSViewMinYMargin|NSViewMaxYMargin];
      [self setAutoresizesSubviews:YES];

      progressIndicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, 12)];
      [progressIndicator setHidden:YES];
      [progressIndicator setIndeterminate:NO];
      [progressIndicator setUsesThreadedAnimation:YES];
      [self addSubview:progressIndicator];
      [progressIndicator setMinValue:0.0];
      [progressIndicator setMaxValue:200.0]; // on a deux graphs -> 200

      backgroundCreationLock = [[NSLock alloc] init];
      abortBackgroundCalculation = NO;
      
      [NSThread detachNewThreadSelector:@selector(createBackgroundReps) toTarget:self withObject:nil];
      
      showBlackBodyCurve = NO;
      showGrid = NO;
    }
    return self;
}

-(void) dealloc
{
  // on s'assure qu'on n'a pas une tache en arriere plan
  abortBackgroundCalculation = YES;
  [backgroundCreationLock lock];
  [backgroundCreationLock unlock];
  
  [progressIndicator release];
  
  if (xyBackgroundImageRep != nil)
  {
    [xyBackgroundImageRep release];
    xyBackgroundImageRep = nil;
  }
  if (uvBackgroundImageRep != nil)
  {
    [uvBackgroundImageRep release];
    uvBackgroundImageRep = nil;
  }
  
  if (reference != nil)
  {
    [reference release];
    reference = nil;
  }
  [backgroundCreationLock release];

  [super dealloc];
}

// Appelée lors de l'initialisation dans une thread séparée.
- (void)createBackgroundReps
{
  // cette tâche n'est pas franchement pas prioritaire.
  [NSThread setThreadPriority:0.2];

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  // si une tache existe déja, on lui demande d'abandonner
  abortBackgroundCalculation = YES;
  [backgroundCreationLock lock];
  // on reset le flag d'annulation
  abortBackgroundCalculation = NO;
  
  // on supprime les images calculées et on met le progress monitor
  if (uvBackgroundImageRep != nil)
  {
    [uvBackgroundImageRep release];
    uvBackgroundImageRep = nil;
  }
  if (xyBackgroundImageRep != nil)
  {
    [xyBackgroundImageRep release];
    xyBackgroundImageRep = nil;
  }
  
  // on met le progressIndicator
  [progressIndicator setDoubleValue:0.0];
  [progressIndicator setHidden:NO];
  [progressIndicator startAnimation:self];
  
  [self setNeedsDisplay:YES];

  // et au boulot !
  uvBackgroundImageRep = [self createBackgroundRepForCoordinates:YES];
  // si uvBackgroundImageRep est nil, c'est qu'on a annulé, on 
  // ne calcule pas l'image suivante.
  if (uvBackgroundImageRep != nil)
  {
    // on raffraichi sans attendre, pour gagner quelques secondes.
    if (useUVCoordinates)
      [self setNeedsDisplay:YES];
    
    [uvBackgroundImageRep retain];
    xyBackgroundImageRep = [self createBackgroundRepForCoordinates:NO];
    if (xyBackgroundImageRep != nil)
      [xyBackgroundImageRep retain];
  }
  
  // on retire le progress indicator
  [progressIndicator setHidden:YES];
  [self setNeedsDisplay:YES];

  [backgroundCreationLock unlock];

  [pool release];
}

- (NSBitmapImageRep*)createBackgroundRepForCoordinates:(BOOL)drawForUVCoordinates
{
  NSRect frame = NSMakeRect(0,0,640,480);

  NSBitmapImageRep *backgroundImageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                               pixelsWide:frame.size.width
                                                               pixelsHigh:frame.size.height
                                                            bitsPerSample:8
                                                          samplesPerPixel:4
                                                                 hasAlpha:YES
                                                                 isPlanar:NO
                                                           colorSpaceName:NSCalibratedRGBColorSpace
                                                              bytesPerRow:frame.size.width*4
                                                             bitsPerPixel:NULL];

  // on se fait un graphic contexte pour pouvoir utiliser NSBezierPath, en sauvant le contexte actuel
	[NSGraphicsContext saveGraphicsState];
  NSGraphicsContext *myContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:backgroundImageRep];
	[NSGraphicsContext setCurrentContext:myContext];

  //----------------------------
  // Génération de l'image CIE
  //----------------------------  
  double		(*leftPointsArray) [2];   // les points du contour gauche. Pointera sur un des tableaux pré-calculés, selon les coordonnées choisies
	double		(*rightPointsArray) [2];  // les points du contour droit. Pointera sur un des tableaux pré-calculés, selon les coordonnées choisies
  int       nbLeftPoints = 0;         // le nombre de points du contour gauche
  int       nbRightPoints = 0;        // le nombre de points du contour droit
  
  // on remplie l'image de noir.
  [[NSColor blackColor] set];
  [NSBezierPath fillRect:frame];
  
  // On choisie les tableaux de points pour le contour en fonction des coordonnées choisies (UV ou XY)
  if (drawForUVCoordinates)
  {
		nbLeftPoints = sizeof ( spectral_chromaticity_uv_left ) / sizeof ( spectral_chromaticity_uv_left [ 0 ] );
		leftPointsArray = spectral_chromaticity_uv_left;
    
		nbRightPoints = sizeof ( spectral_chromaticity_uv_right ) / sizeof ( spectral_chromaticity_uv_right [ 0 ] );
		rightPointsArray = spectral_chromaticity_uv_right;
  }
  else
  {
		nbLeftPoints = sizeof ( spectral_chromaticity_left ) / sizeof ( spectral_chromaticity_left [ 0 ] );
		leftPointsArray = spectral_chromaticity_left;
    
		nbRightPoints = sizeof ( spectral_chromaticity_right ) / sizeof ( spectral_chromaticity_right [ 0 ] );
		rightPointsArray = spectral_chromaticity_right;
  }
  
  //----------------------------
  // On trace les bords.
  //----------------------------
  NSBezierPath  *CIEBezierPath = [[NSBezierPath alloc] init];
  // on démarre le path au premier du tableau de gauche
  // on doit inverser les coordonnées en y, vue que la vue est retournée ...
  NSPoint currentCIEPoint = NSMakePoint(leftPointsArray[0][0],leftPointsArray[0][1]);
  NSPoint pixelPoint = [self CIEToPixel:currentCIEPoint forFrame:frame forCoordinates:drawForUVCoordinates];
  pixelPoint.y = frame.size.height - pixelPoint.y;
  [CIEBezierPath moveToPoint:pixelPoint];
  // et on boucle à partir du point suivant (le 1)
  int leftPointsIndex;
  for (leftPointsIndex = 1; leftPointsIndex < nbLeftPoints; leftPointsIndex++)
  {
    currentCIEPoint = NSMakePoint(leftPointsArray[leftPointsIndex][0],leftPointsArray[leftPointsIndex][1]);
    pixelPoint = [self CIEToPixel:currentCIEPoint forFrame:frame forCoordinates:drawForUVCoordinates]; 
    // on doit inverser les coordonnées en y, vue que la vue est retournée ...
    pixelPoint.y = frame.size.height - pixelPoint.y;
    [CIEBezierPath lineToPoint:pixelPoint];
  }
  // on parcours le tableau de droite en ordre inverse
  // et on ommet le dernier point, qui est equivalent au premier du tableau précedent.
  int rightPointsIndex;
  for (rightPointsIndex = nbRightPoints-1; rightPointsIndex >= 0; rightPointsIndex--)
  {
    currentCIEPoint = NSMakePoint(rightPointsArray[rightPointsIndex][0],rightPointsArray[rightPointsIndex][1]);
    pixelPoint = [self CIEToPixel:currentCIEPoint forFrame:frame forCoordinates:drawForUVCoordinates]; 
    // on doit inverser les coordonnées en y, vue que la vue est retournée ...
    pixelPoint.y = frame.size.height - pixelPoint.y;
    [CIEBezierPath lineToPoint:pixelPoint];
  }
  
  // Pour finir, on clos le path
  [CIEBezierPath closePath];  
  
  //----------------------------
  // La partie ammusante : on met la couleur.
  //----------------------------
  CColor	WhiteReference = [reference white];
	CColor	RedReference = [reference redPrimary];
	CColor	GreenReference = [reference greenPrimary];
	CColor	BlueReference = [reference bluePrimary];
  
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
  
  // on parcourt les points du rectangle, et si ils sont contenus dans le path, on calcule la couleur
  int xPosition, yPosition;
  for (yPosition = 0; yPosition < frame.size.height; yPosition ++)
  {
    [progressIndicator incrementBy:100/frame.size.height];
    for (xPosition = 0; xPosition < frame.size.width; xPosition ++)
    {
      // si on a une demande d'annulation, on arrête
      if (abortBackgroundCalculation)
      {
        [backgroundImageRep release];
        [CIEBezierPath release];
        [NSGraphicsContext restoreGraphicsState];
        
        // on retourne nil pour indiquer que on n'a pas fait le travail.
        return nil;
      }
      
      NSPoint pixelPoint = NSMakePoint(xPosition,yPosition);
      // si on n'est pas dans le path, on passe directement au point suivant.
      if (![CIEBezierPath containsPoint:pixelPoint])
        continue;
      
      NSPoint xyCIEPoint = [self pixelToCIE:pixelPoint forFrame:frame forCoordinates:drawForUVCoordinates];
      
      // on renomme les coordonnées du point pour être cohérents avec la valeur en z.
      float CIE_x = xyCIEPoint.x;
      float CIE_y = xyCIEPoint.y;
      float CIE_z = 1 - (CIE_x + CIE_y);
      
      // on récupère les valeurs RGB en appliquant la matrice optenue précédement.
      float redValue = rx*CIE_x + ry*CIE_y + rz*CIE_z;
      float greenValue = gx*CIE_x + gy*CIE_y + gz*CIE_z;
      float blueValue = bx*CIE_x + by*CIE_y + bz*CIE_z;
      
      // on ne veux pas de valeur inferieur à zéro
      float minValue = fmin(redValue, fmin(greenValue, blueValue));
      if (minValue < 0)
      {
        redValue -= minValue;
        greenValue -= minValue;
        blueValue -= minValue;
      }
      
      // et on souhaite que la valeur la plus importante soit 1
      float maxValue = fmax(redValue, fmax(greenValue, blueValue));
      if (maxValue > 0)
      {
        redValue /= maxValue;
        greenValue /= maxValue;
        blueValue /= maxValue;
      }
      
      // on applique le gamma et on met à l'echelle
      const double gamma = 1.0 / 2.2;
      redValue = pow(redValue, gamma);
      greenValue = pow(greenValue, gamma);
      blueValue = pow(blueValue, gamma);
      
      // il ne reste pllus qu'a colorier le pixel
      [backgroundImageRep setColor:[NSColor colorWithCalibratedRed:redValue
                                                             green:greenValue
                                                              blue:blueValue
                                                             alpha:1.0] atX:xPosition y:yPosition];
    }
  }
  
  [CIEBezierPath release];
  
  // on restore le contexte graphique original
	[NSGraphicsContext restoreGraphicsState];
  
  return [backgroundImageRep autorelease];
}
-(void) drawRect:(NSRect)rect {
  //-----------------------------
  // l'image de fond
  //-----------------------------
  if (useUVCoordinates && (uvBackgroundImageRep != nil))
  {
    NSImage      *backgroundImage;
    backgroundImage = [[NSImage alloc] init];
    [backgroundImage addRepresentation:uvBackgroundImageRep];
    [backgroundImage drawInRect:[self bounds] fromRect:NSZeroRect operation:NSCompositeCopy fraction:1.0];
    [backgroundImage release];
  }
  else if (!useUVCoordinates && (xyBackgroundImageRep != nil))
  {
    NSImage      *backgroundImage;
    backgroundImage = [[NSImage alloc] init];
    [backgroundImage addRepresentation:xyBackgroundImageRep];
    [backgroundImage drawInRect:[self bounds] fromRect:NSZeroRect operation:NSCompositeCopy fraction:1.0];
    [backgroundImage release];
  }
  else // l'image n'est pas prète, le progressIndicator est en place normalement
  {
    [[NSColor blackColor] set];
    [NSBezierPath fillRect:[self bounds]];
    [super drawRect:rect];
  }
  
  //-----------------------------
  // la grille, si necessaire
  //-----------------------------
  if (showGrid)
    [self drawGrid];
  
  //-----------------------------
  // la courbe de température du corps noir
  //-----------------------------
  if (showBlackBodyCurve)
    [self drawBlackBodyCurve];
  
  //-----------------------------
  // Le gamut de reference (si la reference est dispo)
  //-----------------------------
  if (reference != nil)
  {
    NSPoint refRedPoint = [self colorToCoordinates:[reference redPrimary]];
    NSPoint refGreenPoint = [self colorToCoordinates:[reference greenPrimary]];
    NSPoint refBluePoint = [self colorToCoordinates:[reference bluePrimary]];
    
    NSBezierPath *gamutPath = [[NSBezierPath alloc] init];
    [gamutPath moveToPoint:refRedPoint];
    [gamutPath lineToPoint:refGreenPoint];
    [gamutPath lineToPoint:refBluePoint];
    [gamutPath closePath];
    
    [gamutPath setLineWidth:2.0];
    [[NSColor grayColor] set];
    [gamutPath stroke];
    [gamutPath release];
    
    [self drawCircleAtPoint:refRedPoint radius:5 borderColor:[NSColor grayColor] fillColor:[NSColor redColor]];
    [self drawCircleAtPoint:refGreenPoint radius:5 borderColor:[NSColor grayColor] fillColor:[NSColor greenColor]];
    [self drawCircleAtPoint:refBluePoint radius:5 borderColor:[NSColor grayColor] fillColor:[NSColor blueColor]];
  }
  
  //-----------------------------
  // les composantes et le gamut, si on a les mesures
  //-----------------------------
  NSBezierPath *gamutPath = [[NSBezierPath alloc] init];
  BOOL         inPosition = NO;
  if (redComponent != nil)
  {
    NSPoint graphPoint = [self colorToCoordinates:[redComponent XYZColor]];
    [gamutPath moveToPoint:graphPoint];
    inPosition = YES;
    [self drawCircleAtPoint:graphPoint radius:4 borderColor:[NSColor grayColor] fillColor:[NSColor redColor]];
  }
  if (greenComponent != nil)
  {
    NSPoint graphPoint = [self colorToCoordinates:[greenComponent XYZColor]];
    if (inPosition)
      [gamutPath lineToPoint:graphPoint];
    else
      [gamutPath moveToPoint:graphPoint];
    inPosition = YES;
    [self drawCircleAtPoint:graphPoint radius:4 borderColor:[NSColor grayColor] fillColor:[NSColor greenColor]];
  }
  if (blueComponent != nil && inPosition) // si on n'a que un point, on ne s'embête pas, on ne trace rien.
  {
    NSPoint graphPoint = [self colorToCoordinates:[blueComponent XYZColor]];
    [gamutPath lineToPoint:graphPoint];
    [self drawCircleAtPoint:graphPoint radius:4 borderColor:[NSColor grayColor] fillColor:[NSColor blueColor]];
  }
  if (inPosition)
  {
    [[NSColor whiteColor] set];
    [gamutPath closePath];
    [gamutPath stroke];
    [[[NSColor whiteColor] colorWithAlphaComponent:0.2] set];
    [gamutPath fill];
  }
  [gamutPath release];
  if (cyanComponent != nil)
  {
    NSPoint graphPoint = [self colorToCoordinates:[cyanComponent XYZColor]];
    [self drawCircleAtPoint:graphPoint radius:4 borderColor:[NSColor grayColor] fillColor:[NSColor cyanColor]];
  }
  if (magentaComponent != nil)
  {
    NSPoint graphPoint = [self colorToCoordinates:[magentaComponent XYZColor]];
    [self drawCircleAtPoint:graphPoint radius:4 borderColor:[NSColor grayColor] fillColor:[NSColor magentaColor]];
  }
  if (yellowComponent != nil)
  {
    NSPoint graphPoint = [self colorToCoordinates:[yellowComponent XYZColor]];
    [self drawCircleAtPoint:graphPoint radius:4 borderColor:[NSColor grayColor] fillColor:[NSColor yellowColor]];
  }
  
  //-----------------------------
  // les droites entre les primaires et leur secondaire opposée
  //-----------------------------
  NSBezierPath  *linesPath = [[NSBezierPath alloc] init];
  float dashStyle[2] = {5.0, 5.0};
  [linesPath setLineDash:dashStyle count:2 phase:0.0];
  [[[NSColor grayColor] colorWithAlphaComponent:0.5] set];
  if (blueComponent != nil && yellowComponent != nil)
  {
    NSPoint primaryPoint = [self colorToCoordinates:[blueComponent XYZColor]];
    NSPoint secondaryPoint = [self colorToCoordinates:[yellowComponent XYZColor]];
    
    [linesPath moveToPoint:primaryPoint];
    [linesPath lineToPoint:secondaryPoint];
  }
  if (greenComponent != nil && magentaComponent != nil)
  {
    NSPoint primaryPoint = [self colorToCoordinates:[greenComponent XYZColor]];
    NSPoint secondaryPoint = [self colorToCoordinates:[magentaComponent XYZColor]];
    
    [linesPath moveToPoint:primaryPoint];
    [linesPath lineToPoint:secondaryPoint];
  }
  if (redComponent != nil && cyanComponent != nil)
  {
    NSPoint primaryPoint = [self colorToCoordinates:[redComponent XYZColor]];
    NSPoint secondaryPoint = [self colorToCoordinates:[cyanComponent XYZColor]];
    
    [linesPath moveToPoint:primaryPoint];
    [linesPath lineToPoint:secondaryPoint];
  }
  [linesPath stroke];
  [linesPath release];
}

-(BOOL) isFlipped
{
  return NO; 
}

-(void) setReference:(HCFRColorReference*)newReference
{
  NSAssert (newReference != nil, @"HCFRCIEChartView->setReference : Invalide color reference (nil)");
  if (reference != newReference)
  {
    if (reference != nil)
      [reference release];
    reference = [newReference retain];
    
    // on recrée les diagrammes CIE
    [NSThread detachNewThreadSelector:@selector(createBackgroundReps) toTarget:self withObject:nil];
  }

}

#pragma mark accesseurs
-(void) setShowGrid:(bool)newValue
{
  if (showGrid != newValue)
  {
    showGrid = newValue;
    [self setNeedsDisplay:YES];
  }
}
-(void) setShowBlackBodyCurve:(bool)newValue
{
  if (showBlackBodyCurve != newValue)
  {
    showBlackBodyCurve = newValue;
    [self setNeedsDisplay:YES];
  }
}
-(void) setCoordinates:(bool)useUV
{
  if (useUVCoordinates != useUV)
  {
    useUVCoordinates = useUV;
    
    [self setNeedsDisplay:YES];
  }
}
-(void) setRedComponent:(HCFRColor*)newRedComponent
{
  if (redComponent != nil)
    [redComponent release];
  if (newRedComponent != nil)
    redComponent = [newRedComponent retain];
  else
    redComponent = nil;
  
  [self setNeedsDisplay:YES];
}
-(void) setGreenComponent:(HCFRColor*)newGreenComponent
{
  if (greenComponent != nil)
    [greenComponent release];
  if (newGreenComponent != nil)
    greenComponent = [newGreenComponent retain];
  else
    greenComponent = nil;

  [self setNeedsDisplay:YES];
}
-(void) setBlueComponent:(HCFRColor*)newBlueComponent
{
  if (blueComponent != nil)
    [blueComponent release];
  if (newBlueComponent != nil)
    blueComponent = [newBlueComponent retain];
  else
    blueComponent = nil;

  [self setNeedsDisplay:YES];
}
-(void) setCyanComponent:(HCFRColor*)newCyanComponent
{
  if (cyanComponent != nil)
    [cyanComponent release];
  if (newCyanComponent != nil)
    cyanComponent = [newCyanComponent retain];
  else
    cyanComponent = nil;
  
  [self setNeedsDisplay:YES];
}
-(void) setMagentaComponent:(HCFRColor*)newMagentaComponent
{
  if (magentaComponent != nil)
    [magentaComponent release];
  if (newMagentaComponent != nil)
    magentaComponent = [newMagentaComponent retain];
  else
    magentaComponent = nil;
  
  [self setNeedsDisplay:YES];
}
-(void) setYellowComponent:(HCFRColor*)newYellowComponent
{
  if (yellowComponent != nil)
    [yellowComponent release];
  if (newYellowComponent != nil)
    yellowComponent = [newYellowComponent retain];
  else
    yellowComponent = nil;
  
  [self setNeedsDisplay:YES];
}

#pragma mark Les fonctions de traduction couleur <-> coordonnées
-(NSPoint) colorToCoordinates:(CColor)color
{
  ColorxyY xyyValue = color.GetxyYValue();
  NSPoint xyPoint = NSMakePoint(xyyValue[0],xyyValue[1]);

  return [self colorPointToCIECoordienates:xyPoint];
}
-(NSPoint) colorPointToCIECoordienates:(NSPoint)xyPoint
{
  if (useUVCoordinates)
  {
		xyPoint.x = ( 4.0 * xyPoint.x ) / ( (-2.0 * xyPoint.x ) + ( 12.0 * xyPoint.y ) + 3.0 );
		xyPoint.y = ( 9.0 * xyPoint.y ) / ( (-2.0 * xyPoint.x ) + ( 12.0 * xyPoint.y ) + 3.0 );
  }
  
  // puis on converti le point en coordonnées XY dans le referentiel du graph
  return [self CIEToPixel:xyPoint forFrame:[self frame] forCoordinates:useUVCoordinates];
}

-(NSPoint) CIEToPixel:(NSPoint)inputPoint forFrame:(NSRect)frame forCoordinates:(BOOL)UVCoordinates
{
  NSPoint result;
  
  if (UVCoordinates)
  {
    result.x = inputPoint.x*frame.size.width/0.7;
    result.y = inputPoint.y*frame.size.height/0.7;
  }
  else
  {
    result.x = inputPoint.x*frame.size.width/0.8;
    result.y = inputPoint.y*frame.size.height/0.9;
  }
  return result;
}

-(NSPoint) pixelToCIE:(NSPoint)inputPoint forFrame:(NSRect)frame forCoordinates:(BOOL)UVCoordinates
{
  NSPoint result;
  
  if (UVCoordinates)
  {
    double u, v;
    
    // on calcule les coordonnées 
    u = inputPoint.x*0.7/frame.size.width;
    v = ( frame.size.height - inputPoint.y ) * 0.7 / frame.size.height;
    
    result.x = (9.0 * u) / ((6.0 * u) - (16.0 * v) + 12.0 );
    result.y = (4.0 * v) / ((6.0 * u) - (16.0 * v) + 12.0 );
  }
  else
  {
    result.x = inputPoint.x*0.8/frame.size.width;
    result.y = ( frame.size.height - inputPoint.y )*0.9/frame.size.height;
  }
  return result;
}

#pragma mark Fonctions de dessin utilitaires
-(void) drawCircleAtPoint:(NSPoint)center radius:(int)radius borderColor:(NSColor*)borderColor fillColor:(NSColor*)fillColor
{
  NSRect rect = NSMakeRect(center.x-radius,center.y-radius,2*radius,2*radius);
  NSBezierPath *circlePath = [NSBezierPath bezierPathWithOvalInRect:rect];
  
  [fillColor set];
  [circlePath fill];
  [circlePath setLineWidth:1.5];
  [borderColor set];
  [circlePath stroke];
  [circlePath setLineWidth:1];
  [NSBezierPath strokeLineFromPoint:NSMakePoint(center.x,center.y+radius) toPoint:NSMakePoint(center.x,center.y-radius)];
  [NSBezierPath strokeLineFromPoint:NSMakePoint(center.x+radius,center.y) toPoint:NSMakePoint(center.x-radius,center.y)];
}
-(void) drawGrid
{
  float         position;
  float         xMax, yMax, xStep, yStep;
  NSBezierPath  *path = [[NSBezierPath alloc] init];

  [[[NSColor darkGrayColor] colorWithAlphaComponent:0.5] set];
  [path setLineWidth:1.0];
  
  if (useUVCoordinates)
  {
    xStep = [self frame].size.width/7;
    yStep = [self frame].size.height/7;
  }
  else
  {
    xStep = [self frame].size.width/8;
    yStep = [self frame].size.height/9;
  }
  xMax = [self frame].size.width;
  yMax = [self frame].size.height;
  
  // les verticales
  for (position = 0.0; position < xMax; position = position + xStep)
  {
    [path moveToPoint:NSMakePoint(position,0.0)];
    [path lineToPoint:NSMakePoint(position,yMax)];
  }
  // les horizontales
  for (position = 0.0; position < yMax; position = position + yStep)
  {
    [path moveToPoint:NSMakePoint(0.0,position)];
    [path lineToPoint:NSMakePoint(xMax,position)];
  }
  
  [path stroke];
  [path release];
}
-(void) drawBlackBodyCurve
{
  NSBezierPath  *path = [[NSBezierPath alloc] init];
  int           i;
  NSRect        frame = [self frame];
	unsigned int  nbPoints = sizeof ( fBlackBody_xy ) / sizeof ( fBlackBody_xy [ 0 ] );
  
  [[NSColor darkGrayColor] set];
  [path setLineWidth:1.5];
  
  NSPoint startPoint = [self colorPointToCIECoordienates:NSMakePoint(fBlackBody_xy[0][0],fBlackBody_xy[0][1])];
  [path moveToPoint:startPoint];
  
  for (i = 1; i < nbPoints; i++)
  {
    NSPoint point = [self colorPointToCIECoordienates:NSMakePoint(fBlackBody_xy[i][0],fBlackBody_xy[i][1])];
    [path lineToPoint:point];
  }
  
  [path stroke];
  [path release];
}
@end
