//  ColorHCFR
//  HCFREmptyView.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 27/11/07.
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

#import "HCFREmptyView.h"

@implementation HCFREmptyView
+(HCFREmptyView*) emptyViewWithString:(NSString*)initialString
{
  return [[[HCFREmptyView alloc] initWithString:initialString] autorelease];
}

-(HCFREmptyView*) init
{
  return [self initWithString:@"..."];
}
-(HCFREmptyView*) initWithString:(NSString*)theNewString
{
  [super initWithFrame:NSMakeRect(0,0,200,50)];
  
  theLabel = [[NSTextField alloc] initWithFrame:[self frame]];
  [theLabel setDrawsBackground:NO];
  [theLabel setTextColor:[NSColor grayColor]];
  [theLabel setBezeled:NO];
  [theLabel setEditable:NO];
  [theLabel setFont:[NSFont systemFontOfSize:24]];
  [theLabel setAutoresizingMask:NSViewMinYMargin|NSViewMaxYMargin|NSViewWidthSizable];
  [theLabel setAlignment:NSCenterTextAlignment];
  
  [self setAutoresizesSubviews:YES];
  [self addSubview:theLabel];

  [self setString:theNewString];

  return self;
}

-(void) setString:(NSString*)theNewString
{
  [theLabel setStringValue:theNewString];
}
@end
