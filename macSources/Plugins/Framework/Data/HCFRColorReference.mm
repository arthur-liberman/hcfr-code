//  ColorHCFR
//  HCFRColorReference.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 07/09/07.
//
//  $Rev: 50 $
//  $Date: 2008-05-22 10:00:37 +0100 (Thu, 22 May 2008) $
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

#import "HCFRColorReference.h"


@implementation HCFRColorReference
-(HCFRColorReference*) init
{
  [super init];
  
  colorReference = new CColorReference(SDTV,D65,2.20);
  
  return self;
}

-(HCFRColorReference*) initWithColorStandard:(ColorStandard)standard whiteTarget:(WhiteTarget)white
{
  [super init];
  
  colorReference = new CColorReference(standard, white, -1);
  
  return self;
}

-(id) initWithCoder:(NSCoder*)coder
{  
  ColorStandard standard = (ColorStandard) [coder decodeIntForKey:@"standard"];
  WhiteTarget   white = (WhiteTarget) [coder decodeIntForKey:@"white"];
  
  return [self initWithColorStandard:standard whiteTarget:white];
}

-(void) encodeWithCoder:(NSCoder*)coder
{
  int standard = (int)colorReference->m_standard;
  int white = (int)colorReference->m_white;

  [coder encodeInt:standard forKey:@"standard"];
  [coder encodeInt:white forKey:@"white"];
}

-(void) dealloc
{
  delete(colorReference);
  
  [super dealloc];
}

-(CColorReference) cColorReference
{
  return *colorReference;
}
-(ColorStandard) colorStandard
{
  return colorReference->m_standard;
}
-(WhiteTarget) whiteTarget
{
  return colorReference->m_white;
}

-(CColor) white
{
  return colorReference->GetWhite();
}
-(CColor) redPrimary
{
  return colorReference->redPrimary;
}
-(CColor) greenPrimary
{
  return colorReference->greenPrimary;
}
-(CColor) bluePrimary
{
  return colorReference->bluePrimary;
}
-(double) redReferenceLuma
{
  return colorReference->GetRedReferenceLuma();
}
-(double) greenReferenceLuma
{
  return colorReference->GetGreenReferenceLuma();
}
-(double) blueReferenceLuma
{
  return colorReference->GetBlueReferenceLuma();
}
@end
