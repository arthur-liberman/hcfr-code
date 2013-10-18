//  ColorHCFR
//  HCFRUnicolorView.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
//
//  $Rev: 85 $
//  $Date: 2008-07-28 09:37:49 +0100 (Mon, 28 Jul 2008) $
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

#import "HCFRUnicolorView.h"


@implementation HCFRUnicolorView
#pragma mark constructeur et destructeur
-(HCFRUnicolorView*) init
{
  [super init];
  
  color = [[NSColor blackColor] retain];
  screenCoverage = 100;
  forceFullscreen = NO;
  
  lightDescriptionAttributes = [[NSDictionary dictionaryWithObjectsAndKeys:[NSFont userFontOfSize:16], NSFontAttributeName,
    [[NSColor whiteColor] colorWithAlphaComponent:0.5], NSForegroundColorAttributeName, nil] retain];
  darkDescriptionAttributes = [[NSDictionary dictionaryWithObjectsAndKeys:[NSFont userFontOfSize:16], NSFontAttributeName,
    [[NSColor blackColor] colorWithAlphaComponent:0.5], NSForegroundColorAttributeName, nil] retain];
    
  return self;
}

-(void) dealloc
{
  [color release];
  [lightDescriptionAttributes release];
  [darkDescriptionAttributes release];
  if (description != nil)
    [description release];
  
  [super dealloc]; 
}

-(void) setFullscreenColor:(NSColor*)newColor
{
  [self setColor:newColor];
  
  forceFullscreen = YES;
}

-(void) setColor:(NSColor*)newColor
{
  NSAssert(newColor!=nil, @"HCFRUnicolorView->setColor : invalide nil color.");
  [color release];
  color = [newColor retain];
  
  forceFullscreen = NO;

  // on utilise display et non setNeedsDisplay
  // car on doit être sûr que le raffraichissement à bien
  // été fait lorsqu'on signale que la frame à changée.
  [self display];
}

-(int) screenCoverage
{
  return screenCoverage;
}
-(void) setScreenCoverage:(int)newCoverage
{
  screenCoverage = newCoverage;
}

-(void) setDescription:(NSString*)newDescription
{
  if (description != nil)
    [description release];

  if (newDescription == nil)
    description = nil;
  else
    description = [newDescription retain];
}

-(void) drawRect:(NSRect)rect
{
  NSRect    frame = [self frame];
  NSRect    destRect;
  
  if (!forceFullscreen)
  {
    destRect.size.width = frame.size.width / sqrt(1/(screenCoverage/100.0));
    destRect.size.height = frame.size.height / sqrt(1/(screenCoverage/100.0));
    destRect.origin.x = (frame.size.width - destRect.size.width)/2.0;
    destRect.origin.y = (frame.size.height - destRect.size.height)/2.0;
  }
  else
    destRect = frame;
  
  [[NSColor blackColor] set];
  [NSBezierPath fillRect:frame];

  [color set];
  [NSBezierPath fillRect:destRect];
  
  if (description != nil)
  {
    NSString  *colorSpaceName = [color colorSpaceName];
    double    brightness =0;
    
    if ([colorSpaceName isEqualToString:NSDeviceRGBColorSpace] || 
        [colorSpaceName isEqualToString:NSCalibratedRGBColorSpace])
      brightness = [color brightnessComponent];
    else if ([colorSpaceName isEqualToString:NSCalibratedWhiteColorSpace] ||
             [colorSpaceName isEqualToString:NSDeviceWhiteColorSpace])
      brightness = [color whiteComponent];
    
    if (brightness<0.5 || (screenCoverage < 80 && !forceFullscreen))
      [description drawAtPoint:NSMakePoint(10.0,10.0) withAttributes:lightDescriptionAttributes];
    else
      [description drawAtPoint:NSMakePoint(10.0,10.0) withAttributes:darkDescriptionAttributes];
  }
}
@end
