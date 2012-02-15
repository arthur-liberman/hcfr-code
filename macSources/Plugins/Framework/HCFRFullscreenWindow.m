//  ColorHCFR
//  HCFRFullScreenWindow.m
// -----------------------------------------------------------------------------
//  Created by JŽr™me Duquennoy on 31/08/07.
//
//  $Rev: 42 $
//  $Date: 2008-05-11 01:09:36 +0100 (Sun, 11 May 2008) $
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

#import "HCFRFullScreenWindow.h"


@implementation HCFRFullScreenWindow
-(HCFRFullScreenWindow*) init
{
  // on se met sur l'Žcran principal par dŽfaut
  return [self initWithScreen:[[NSScreen screens] objectAtIndex:0]];
}
-(HCFRFullScreenWindow*) initWithScreen:(NSScreen*)screen
{
	[super initWithContentRect:NSMakeRect(0,0,400,400)
                   styleMask:NSBorderlessWindowMask
                     backing:NSBackingStoreBuffered
                       defer:NO];
	
  // Par defaut, on se met sur l'Žcran principal
  [self setFrame:[screen frame] display:NO];
  
  return self;
}

-(void) setScreen:(NSScreen*)newScreen
{
  NSAssert(newScreen!=nil, @"HCFRFullScreenWindow->setScreen : invalide nil screen");
  
  [self setFrame:[newScreen frame] display:NO];
}

- (BOOL)canBecomeKeyWindow
{
  return YES;
}
- (void)cancelOperation:(id)sender
{
  [[self nextResponder] cancelOperation:sender];
}
@end
