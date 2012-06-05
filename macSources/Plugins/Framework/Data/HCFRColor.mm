//  ColorHCFR
//  HCFRMeasure.m
// -----------------------------------------------------------------------------
//  Created by JŽr™me Duquennoy on 30/08/07.
//
//  $Rev: 134 $
//  $Date: 2011-02-23 22:44:34 +0000 (Wed, 23 Feb 2011) $
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

#import "HCFRColor.h"

@interface HCFRColor(Private)
+(NSColor*) nscolor:(int)color withSaturation:(float)saturation forColorReference:(HCFRColorReference*)colorReference;
@end


@implementation HCFRColor

-(HCFRColor*) initWithMatrix:(Matrix)newMatrix
{
  [super init];
  
  color = new CColor(ColorXYZ(newMatrix));

  return self;
}

-(HCFRColor*) initWithRGBMatrix:(Matrix)newMatrix colorReference:(HCFRColorReference*)reference
{
  [super init];
  
  ColorRGB newColor (newMatrix);
  
  color = new CColor();
  color->SetRGBValue(newColor, [reference cColorReference]);

  return self;
}

-(HCFRColor*) initWithColor:(CColor)newColor
{
    [super init];
    
    color = new CColor(newColor);
    
    return self;
}

-(HCFRColor*) initWithColorXYZ:(ColorXYZ)newColor
{
  [super init];
  
  color = new CColor(newColor);

  return self;
}

-(id) initWithCoder:(NSCoder*)coder
{
  [super init];

  color = new CColor();

  color->SetX([coder decodeDoubleForKey:@"X"]);
  color->SetY([coder decodeDoubleForKey:@"Y"]);
  color->SetZ([coder decodeDoubleForKey:@"Z"]);
       
  return self;
}

-(void) dealloc
{
  if (color != nil)
    delete (color);
  
  [super dealloc];
}

-(void) encodeWithCoder:(NSCoder*)coder
{
  [coder encodeDouble:[self X] forKey:@"X"];
  [coder encodeDouble:[self Y] forKey:@"Y"];
  [coder encodeDouble:[self Z] forKey:@"Z"];

}
#pragma mark Fonctions reprises de CColor
-(double) X
{
  return color->GetX();
}
-(double) Y
{
  return color->GetY();
}
-(double) Z
{
  return color->GetZ();
}
-(double) luminance:(bool)preferLuxmeterMeasureIfAvailable
{
  return color->GetPreferedLuxValue(preferLuxmeterMeasureIfAvailable);
}
-(ColorXYZ) XYZColor
{
  return color->GetXYZValue();
}
-(ColorRGB) RGBColorWithColorReference:(HCFRColorReference*)reference
{
  NSAssert (reference!=nil, @"HCFRColor : cannot compute RGB color with nil color reference.");
  return color->GetRGBValue([reference cColorReference]);
}
-(ColorxyY) xyYColor
{
  return color->GetxyYValue();
}
-(double) luminance
{
  return color->GetLuminance();
}
-(double) temperatureWithColorReference:(HCFRColorReference*)reference
{
  return color->GetXYZValue().GetColorTemp([reference cColorReference]);
}

-(void) display
{
  cout << [self X] << ", " << [self Y] << ", " << [self Z] << endl;
}

#pragma mark Fonctions utilitaires de gŽnŽration de couleurs
+(NSColor*) greyNSColorWithIRE:(float)IRE
{
  return [NSColor colorWithCalibratedWhite:IRE/100.0 alpha:1.0];
}

+(NSColor*) redNSColorWithSaturation:(float)saturation forColorReference:(HCFRColorReference*)colorReference
{
  return [HCFRColor nscolor:1 withSaturation:saturation forColorReference:colorReference];
}


+(NSColor*) greenNSColorWithSaturation:(float)saturation forColorReference:(HCFRColorReference*)colorReference
{
  return [HCFRColor nscolor:2 withSaturation:saturation forColorReference:colorReference];
}


+(NSColor*) blueNSColorWithSaturation:(float)saturation forColorReference:(HCFRColorReference*)colorReference
{
  return [HCFRColor nscolor:3 withSaturation:saturation forColorReference:colorReference];
}

+(NSColor*) nscolor:(int)colorIndex withSaturation:(float)saturation forColorReference:(HCFRColorReference*)colorReference
{
  // Retrieve color luma coefficients matching actual reference
  const double KR = [colorReference redReferenceLuma];  
  const double KG = [colorReference greenReferenceLuma];  
  const double KB = [colorReference blueReferenceLuma];  
  
  // TODO : l'index de couleur en int, c'est mal. Mais comment faire pour ne pas crŽer une dŽpendance trop importante
  // ni avoir une 15me constante
  bool  redSaturation = (colorIndex == 1);
  bool  greenSaturation = (colorIndex == 2);
  bool  blueSaturation = (colorIndex == 3);
  
  double K = ( redSaturation ? KR : 0.0 ) + ( greenSaturation ? KG : 0.0 ) + ( blueSaturation ? KB : 0.0 );
  double luma = K * 255.0;	// Luma for pure color
  
  // Compute vector between neutral gray and saturated color in CIExy space
  ColorRGB Clr1;
  CColor Clr2;
  double	xstart, ystart, xend, yend;
  
  // Retrieve gray xy coordinates
  ColorxyY whitexyYValues = [colorReference white].GetxyYValue();
  xstart = whitexyYValues[0];
  ystart = whitexyYValues[1];
  
  // Define target color in RGB mode
  Clr1[0] = ( redSaturation ? 255.0 : 0.0 );
  Clr1[1] = ( greenSaturation ? 255.0 : 0.0 );
  Clr1[2] = ( blueSaturation ? 255.0 : 0.0 );
  
  // Compute xy coordinates of 100% saturated color
  Clr2.SetRGBValue(Clr1, [colorReference cColorReference]);
  ColorxyY Clr3=Clr2.GetxyYValue();
  xend=Clr3[0];
  yend=Clr3[1];
  
  double clr, comp;
  
  if ( saturation == 0 )
  {
    clr = luma;
    comp = luma;
  }
  else if ( saturation == 100 )
  {
    clr = 255.0;
    comp = 0.0;
  }
  else
  {
    double x, y;
    
    x = xstart + ( (xend - xstart) * (saturation/100.0) );
    y = ystart + ( (yend - ystart) * (saturation/100.0) );
    
    ColorxyY UnsatClr_xyY(x,y,luma);
    
    CColor UnsatClr;
    UnsatClr.SetxyYValue (UnsatClr_xyY);
    
    ColorRGB UnsatClr_rgb = UnsatClr.GetRGBValue ([colorReference cColorReference]);
    
    // Both components are theoretically equal, get medium value
    clr = ( ( redSaturation ? UnsatClr_rgb[0] : 0.0 ) + ( greenSaturation ? UnsatClr_rgb[1] : 0.0 ) + ( blueSaturation ? UnsatClr_rgb[2] : 0.0 ) ) /
    ( ( redSaturation ? 1 : 0 ) + ( greenSaturation ? 1 : 0 ) + ( blueSaturation ? 1 : 0 ) );
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
  
  clr = pow ( clr / 255.0, 0.45 );
  comp = pow ( comp / 255.0, 0.45 );
  
  NSNumber *redValue = [NSNumber numberWithDouble:( redSaturation ? clr : comp )];
  NSNumber *greenValue = [NSNumber numberWithDouble:( greenSaturation ? clr : comp )];
  NSNumber *blueValue = [NSNumber numberWithDouble:( blueSaturation ? clr : comp )];
  
  return [NSColor colorWithDeviceRed:[redValue doubleValue]
                               green:[greenValue doubleValue]
                                blue:[blueValue doubleValue]
                               alpha:1.0];
  
}

@end
