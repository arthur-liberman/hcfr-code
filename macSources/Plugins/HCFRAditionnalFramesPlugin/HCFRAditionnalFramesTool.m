//  ColorHCFR
//  HCFRAditionnalFramesTool.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 27/08/08.
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

#import "HCFRAditionnalFramesTool.h"
#import <HCFRplugins/HCFRUtilities.h>

// les vues
#import "HCFRWhiteAnimatedView.h"
#import "HCFRFullscreenImageView.h"


@implementation AditionalFrameWindow
- (void)mouseUp:(NSEvent *)theEvent
{
  [self close];
}
- (void)cancelOperation:(id)sender
{
  [self close];
}
@end

@implementation HCFRAditionnalFramesTool
+(NSString*) toolName {
  return HCFRLocalizedString(@"Aditionnal frames", @"Aditionnal frames");
}
- (id) init {
  self = [super init];
  if (self != nil) {
    [NSBundle loadNibNamed:@"AditionnalFrames" owner:self];
    
    [framesPopup removeAllItems];
    NSMenuItem              *newItem;
    HCFRWhiteAnimatedView   *newWhiteView;
    HCFRFullscreenImageView *newFullView;
      
    // la vue animée blanche
    newItem = [[NSMenuItem alloc] init];
    newWhiteView = [[HCFRWhiteAnimatedView alloc] initWithWhiteLevel:100 step:-1];
    [(HCFRWhiteAnimatedView*)newWhiteView setFrameDescription:HCFRLocalizedString(@"This frame displays a white background and two slightly darker vertical bars, moving from left to right.\nIt is used to visualy tune the contrast setting for hight lights accuracy.\nModify the contrast setting until you can distinguish the two vertical bars.",
                                                                        @"White animated frame description")];
    [newItem setTitle:HCFRLocalizedString(@"White animated frame",@"White animated frame")];
    [newItem setRepresentedObject:newWhiteView];
    [newWhiteView release];
    [[framesPopup menu] addItem:newItem];
    [newItem release];

    // la vue animée noire
    newItem = [[NSMenuItem alloc] init];
    newWhiteView = [[HCFRWhiteAnimatedView alloc] initWithWhiteLevel:0 step:1];
    [(HCFRWhiteAnimatedView*)newWhiteView setFrameDescription:HCFRLocalizedString(@"This frame displays a black background and two slightly lighter vertical bars, moving from left to right.\nIt is used to visualy tune the luminosity setting for low lights accuracy.\nModify the luminosity setting until you can distinguish the two vertical bars.",
                                                                        @"black animated frame description")];
    [newItem setTitle:HCFRLocalizedString(@"Black animated frame",@"Black animated frame")];
    [newItem setRepresentedObject:newWhiteView];
    [newWhiteView release];
    [[framesPopup menu] addItem:newItem];
    [newItem release];

    [[framesPopup menu] addItem:[NSMenuItem separatorItem]];
    
    // l'image SMPTE - 4/3
    newItem = [[NSMenuItem alloc] init];
    newFullView = [[HCFRFullscreenImageView alloc] init];
    [newFullView setFrameDescription:HCFRLocalizedString(@"The SMPTE pattern for diffuser with a 4/3 ratio.",
                                                     @"SMPTE 4/3 description")];
    [(HCFRFullscreenImageView*)newFullView setImagePath:[[NSBundle bundleForClass:[self class]] pathForResource:@"smpte" ofType:@"png"]];
    [newItem setTitle:HCFRLocalizedString(@"SMPTE - 4/3",@"SMPTE - 4/3")];
    [newItem setRepresentedObject:newFullView];
    [newFullView release];
    [[framesPopup menu] addItem:newItem];
    [newItem release];
    
    // l'image SMPTE - 16/9
    newItem = [[NSMenuItem alloc] init];
    newFullView = [[HCFRFullscreenImageView alloc] init];
    [newFullView setFrameDescription:HCFRLocalizedString(@"The SMPTE pattern for diffuser with a 16/9 ratio.",
                                                     @"SMPTE 16/9 description")];
    [(HCFRFullscreenImageView*)newFullView setImagePath:[[NSBundle bundleForClass:[self class]] pathForResource:@"smpte-hd" ofType:@"png"]];
    [newItem setTitle:HCFRLocalizedString(@"SMPTE - 16/9",@"SMPTE - 16/9")];
    [newItem setRepresentedObject:newFullView];
    [newFullView release];
    [[framesPopup menu] addItem:newItem];
    [newItem release];

    // l'image de test
    newItem = [[NSMenuItem alloc] init];
    newFullView = [[HCFRFullscreenImageView alloc] init];
    [newFullView setFrameDescription:HCFRLocalizedString(@"Simply an image to admire the result of your work ;-)",
                                                     @"test image description")];
    [(HCFRFullscreenImageView*)newFullView setImagePath:[[NSBundle bundleForClass:[self class]] pathForResource:@"testimg" ofType:@"jpg"]];
    [newItem setTitle:HCFRLocalizedString(@"Test image",@"Test image")];
    [newItem setRepresentedObject:newFullView];
    [newFullView release];
    [[framesPopup menu] addItem:newItem];
    [newItem release];
    
    [self frameChanged:self];
  }
  return self;
}
- (void) dealloc {
  if (fullscreenWindow != nil)
    [fullscreenWindow release];
  
  [super dealloc];
}

-(void) startTool
{
  // on recharge la liste des écrans
  [screensPopup removeAllItems];
  NSEnumerator  *enumerator = [[NSScreen screens] objectEnumerator];
  NSScreen      *currentScreen;
  int           screenNumber = 1;
  while (currentScreen = (NSScreen*)[enumerator nextObject])
  {
    NSMenu      *popupMenu = [screensPopup menu];
    NSMenuItem  *menuItem = [[NSMenuItem alloc] init];
    
    [menuItem setRepresentedObject:currentScreen];
    [menuItem setTitle:[NSString stringWithFormat:HCFRLocalizedString(@"Screen %d", @"Screen %d"), screenNumber]];
    
    [popupMenu addItem:menuItem];
    [menuItem release];
    screenNumber ++;
  }
  
  // on affiche la fenêtre.
  [mainWindow center];
  [mainWindow makeKeyAndOrderFront:self];
}

#pragma mark IBActions
-(IBAction) displayFrame:(id)sender
{
  [self setupFrame:[[framesPopup selectedItem] representedObject]];
  [fullscreenWindow makeKeyAndOrderFront:self];
}
-(IBAction) screenChanged:(id)sender
{
  if (fullscreenWindow != nil)
  {
    [fullscreenWindow setScreen:[[screensPopup selectedItem] representedObject]];
  }
}
-(IBAction) frameChanged:(id)sender
{
  [descriptionTextView setString:[[[framesPopup selectedItem] representedObject] frameDescription]];
}

#pragma mark Fonctions utilitaires
-(void) setupFrame:(HCFRAditionnalFrameView*)newFrame
{
  if (fullscreenWindow == nil)
  {
    fullscreenWindow = (AditionalFrameWindow*)[[AditionalFrameWindow alloc] initWithScreen:[[screensPopup selectedItem] representedObject]];
    [fullscreenWindow setDelegate:nil];
    [fullscreenWindow setReleasedWhenClosed:NO];
    [fullscreenWindow setLevel:NSPopUpMenuWindowLevel];
  }

  if (currentView != nil)
  {
    [currentView stopAnimation];
    [currentView release];
    currentView = nil;
  }

  if (newFrame != nil)
  {
    currentView = [newFrame retain];
    [fullscreenWindow setContentView:currentView];
    [currentView startAnimation];
  }
  else
  {
    NSView *view = [[NSView alloc] init];
    [fullscreenWindow setContentView:view];
    [view release];
  }
}

#pragma mark Delegué de la fenêtre
- (void)windowWillClose:(NSNotification *)notification
{
  if ([notification object] == fullscreenWindow)
  {
    [self setupFrame:nil];
  }
  else if ([notification object] == mainWindow)
  {
    [fullscreenWindow close];
  }
}
@end
