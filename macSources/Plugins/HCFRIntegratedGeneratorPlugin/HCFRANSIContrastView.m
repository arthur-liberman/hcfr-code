//  ColorHCFR
//  HCFRANSIContrastView.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/09/07.
//
//  $Rev: 103 $
//  $Date: 2008-08-28 18:39:32 +0100 (Thu, 28 Aug 2008) $
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

#import "HCFRANSIContrastView.h"

@implementation HCFRANSIContrastView
-(HCFRANSIContrastView*) init
{
  [super init];
  currentCrossStyle = kNoCross;
  blackRectFirst = YES;
  return self;
}

-(void) setCrossStyle:(ANSIContrastViewCrossStyle)newStyle
{
  currentCrossStyle = newStyle;
  [self display];
}

-(void) invertBlackAndWhite
{
  blackRectFirst = !blackRectFirst;
  [self display];
}

-(void) drawRect:(NSRect)rect
{
  // On fait un nombre pair de cases
  // afin d'avoir la même surface de blanc et de noir.
  NSRect  frame = [self frame];
  int     nbColumns = 4;
  int     nbRows = 4;
  NSRect  rectToDraw = NSMakeRect(0, 0, frame.size.width/nbColumns,frame.size.height/nbRows);
  int     column, row;
  bool    blackRect = blackRectFirst;
  
  for (column = 0; column < nbColumns; column++)
  {
    for (row = 0; row < nbRows; row++)
    {
      rectToDraw.origin.x = column*rectToDraw.size.width;
      rectToDraw.origin.y = row*rectToDraw.size.height;
      
      if (blackRect) // noir
      {
        [[NSColor blackColor] set];
        [NSBezierPath fillRect:rectToDraw];
        // si on doit faire une croix, on la fait
        if (currentCrossStyle == kCrossOnBlackRegions)
          [self drawCrossInRect:rectToDraw size:rectToDraw.size.width/4];
      }
      else // blanc
      {
        [[NSColor whiteColor] set];
        [NSBezierPath fillRect:rectToDraw];
        // si on doit faire une croix, on la fait
        if (currentCrossStyle == kCrossOnWhiteRegions)
          [self drawCrossInRect:rectToDraw size:rectToDraw.size.width/4];
      }
      blackRect = !blackRect;
    }
    blackRect = !blackRect;
  }
}

-(void) drawCrossInRect:(NSRect)rect size:(int)size
{
  [NSBezierPath setDefaultLineWidth:3.0];
  [[NSColor greenColor] set];
  
  NSPoint point1 = NSMakePoint(rect.origin.x+rect.size.width/2-size/2, rect.origin.y+rect.size.height/2-size/2);
  NSPoint point2 = NSMakePoint(rect.origin.x+rect.size.width/2+size/2, rect.origin.y+rect.size.height/2+size/2);
  
  [NSBezierPath strokeLineFromPoint:point1 toPoint:point2];
  
  point2 = NSMakePoint(rect.origin.x+rect.size.width/2+size/2, rect.origin.y+rect.size.height/2-size/2);
  point1 = NSMakePoint(rect.origin.x+rect.size.width/2-size/2, rect.origin.y+rect.size.height/2+size/2);

  [NSBezierPath strokeLineFromPoint:point1 toPoint:point2];
}
@end
