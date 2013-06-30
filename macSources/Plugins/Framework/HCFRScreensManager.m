//  ColorHCFR
//  HCFRScreensManager.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/05/08.
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

#import "HCFRScreensManager.h"
#import <HCFRPlugins/HCFRFullScreenWindow.h>
#import <HCFRPlugins/HCFRUnicolorView.h>

static HCFRScreensManager *sharedManager = nil;

@implementation HCFRScreensManager
#pragma mark Fonctions de gestion du singleton
+ (HCFRScreensManager*)sharedManager
{
  @synchronized(self) {
    if (sharedManager == nil) {
      [[[self alloc] init] autorelease]; // On n'alloue pas sharedDevice ici : ce sera fait par allocWithZone
    }
  }
  return sharedManager;
}
+ (id)allocWithZone:(NSZone *)zone
{
  @synchronized(self) {
    if (sharedManager == nil) {
      sharedManager = [super allocWithZone:zone];
      return sharedManager;  // On alloue à l'instance partagée
    }
  }
  return nil; // et si d'autre allocations surviennent, on retourne nil.
}
- (id)copyWithZone:(NSZone *)zone
{
  return self;
}
- (id)retain
{
  return self;
}
- (unsigned)retainCount
{
  return UINT_MAX;  // Cet objet ne peut pas être releasé
}
- (oneway void)release
{
}
- (id)autorelease
{
  return self;
}
- (HCFRScreensManager*) init
{
  self = [super init];
  if (self != nil) {
    displayedWindows = [[NSMutableArray alloc] initWithCapacity:2];
    calibratedScreen = nil;
    controlView = nil;
    
    screensDarkened = NO;
  }
  return self;
}

#pragma mark Les fonctions de gestion des écrans
-(void) setCalibratedScreen:(NSScreen*)newCalibratedScreen
{
  if (calibratedScreen != nil)
    [calibratedScreen release];

  if (newCalibratedScreen == nil)
    calibratedScreen = nil;
  else
    calibratedScreen = [newCalibratedScreen retain];
}
-(void) setControlView:(NSView*)newControlView
{
  if (controlView != nil)
    [controlView release];
  controlView = [newControlView retain];
}
-(void) darken
{
  NSEnumerator  *screensEnumerator = [[NSScreen screens] objectEnumerator];
  NSScreen      *currentScreen;
  BOOL          firstScreen = YES;
  NSWindow      *controlWindow = nil; // on mémorise la fenêtre qui recevra la controlView pour lui donner le focus
  
  NSLog (@"screen manager : darken");
  
  // pour chaque écran, on met une fenêtre
  while (currentScreen = [screensEnumerator nextObject])
  {
    // on ne masque pas l'écran utilisé par le générateur, of course !
    if (calibratedScreen == currentScreen)
    {
      NSLog (@"screen manager : calibrated screen found -> skipping");
      continue;
    }
    
    HCFRFullScreenWindow  *newWindow = [[HCFRFullScreenWindow alloc] initWithScreen:currentScreen];
//    [newWindow setLevel:NSScreenSaverWindowLevel]; // commenté pour debug
    
    if ((controlView != nil) && firstScreen)
    {
      NSLog (@"screen manager : darkening first screen with control view");
      [newWindow setContentView:controlView];
      controlWindow = newWindow;
    }
    else
    {
      HCFRUnicolorView  *view = [[HCFRUnicolorView alloc] init];
      [view setColor:[NSColor blackColor]];
      [newWindow setContentView:view];
      [view release]; // la vue est retenue par la fenêtre, on ne la gère plus
    }
    
    // la fenêtre est transparente au début
    [newWindow setAlphaValue:0.0];
    [newWindow makeKeyAndOrderFront:self];
    
    [displayedWindows addObject:newWindow];
    [newWindow release];
    firstScreen = NO;
  }
  if (controlWindow != nil)
    [controlWindow makeKeyWindow];

  // puis on obscurci les fenêtes doucement. C'est rapide, pas besoin d'utiliser NSAnimation
  int nbWindows = [displayedWindows count];
  int animIndex;
  for (animIndex = 0; animIndex<11; animIndex++)
  {
    int windowIndex;
    for (windowIndex = 0; windowIndex<nbWindows; windowIndex++)
    {
      NSWindow  *window = [displayedWindows objectAtIndex:windowIndex];
      [window setAlphaValue:0.1*animIndex];
    }
    [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]]; // 20fps, 0.5 sec d'animation
  }
  
  screensDarkened = YES;
}
-(void) enlight
{
  // on eclaircie les fenêtes doucement. C'est rapide, pas besoin d'utiliser NSAnimation
  int nbWindows = [displayedWindows count];
  int animIndex, windowIndex;
  for (animIndex = 0; animIndex<11; animIndex++)
  {
    for (windowIndex = 0; windowIndex<nbWindows; windowIndex++)
    {
      NSWindow  *window = [displayedWindows objectAtIndex:windowIndex];
      [window setAlphaValue:(1-0.1*animIndex)];
    }
    [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]]; // 20fps, 0.5 sec d'animation
  }
  
  // on ne ferme pas les fenêtres, car cela provoquerait un releas automatique,
  // et donc un plantage lors du release provoqué par la suppression de la fenêtre du tableau.
  // Une fois enlevée du tableau, la fenêtre sera releasé, ce qui provoquera sa fermeture.
  [displayedWindows removeAllObjects];
  
  screensDarkened = NO;
}
-(void) ensureWindowVisibility:(NSWindow*)window
{
  // Si on à un écran calibré et qu'on dispose de plusieurs écrans,
  // on vérifie que la fenetre n'est pas sur l'écran de calibration
  // Si c'est le cas, on la déplace au centre du premer écran suivant.
  if ( ([[NSScreen screens] count] > 1) && (calibratedScreen != nil) )
  {
    NSRect windowFrame = [window frame];
    NSRect calibratedScreenFrame = [calibratedScreen frame];
    
    // si la fenêtre apparait sur l'écran qui sera calibré
    if (NSIntersectsRect(calibratedScreenFrame, windowFrame))
    {
      // on centre la fenêtre sur l'autre écran.
      NSArray *screens = [NSScreen screens];
      NSScreen *theUnusedScreen;
      if ([screens objectAtIndex:0] != calibratedScreen)
        theUnusedScreen = [screens objectAtIndex:0];
      else
        theUnusedScreen = [screens objectAtIndex:1];
      
      NSRect screen = [theUnusedScreen frame];
      NSPoint center = NSMakePoint(screen.origin.x + (screen.size.width / 2), screen.origin.y + (screen.size.height / 2));
      windowFrame.origin.x = center.x - (windowFrame.size.width / 2);
      windowFrame.origin.y = center.y - (windowFrame.size.height / 2);
      
      [window setFrame:windowFrame display:YES animate:YES];
    }
  }
}
@end
