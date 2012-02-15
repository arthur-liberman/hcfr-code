//  ColorHCFR
//  HCFRFullscreenImageView.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 28/08/08.
//
//  $Rev: 105 $
//  $Date: 2008-08-28 22:53:12 +0100 (Thu, 28 Aug 2008) $
//  $Author$
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

#import "HCFRFullscreenImageView.h"


@implementation HCFRFullscreenImageView
- (id) init {
  self = [super init];
  if (self != nil) {
    [self setAutoresizesSubviews:YES];

    imagePath = nil;
      
    imageView = [[NSImageView alloc] init];
    [imageView setFrame:[self frame]];
    [imageView setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [self addSubview:imageView];
  }
  return self;
}
- (void) dealloc {
  [imageView release];
  [super dealloc];
}

-(void) setImagePath:(NSString*)newPath
{
  if (imagePath != nil)
    [imagePath release];
  imagePath = [newPath retain];
}

-(void) startAnimation
{
  if (imagePath == nil)
    return;
  
  NSImage   *image = [[NSImage alloc] initWithContentsOfFile:imagePath];
  
  [imageView setImage:image];
  
  [image release];
}
-(void) stopAnimation
{
  [imageView setImage:nil];
}

- (void)drawRect:(NSRect)rect
{
  // on met un fond noir
  [[NSColor blackColor] set];
  [NSBezierPath fillRect:rect];
  [super drawRect:rect];
}
@end
