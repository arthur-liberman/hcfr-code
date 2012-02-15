//  ColorHCFR
//  HCFRScreensManager+alerts.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/05/08.
//
//  $Rev: 60 $
//  $Date: 2008-07-07 14:08:13 +0100 (Mon, 07 Jul 2008) $
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

#import "HCFRScreensManager+alerts.h"
@interface HCFRScreensManager (AlertsPrivate)
-(NSWindow*) findMostInterestingWindow;
@end

@implementation HCFRScreensManager (Alerts)
-(void) displayAlert:(NSAlert*)alert modalDelegate:(id)delegate didEndSelector:(SEL)didEndSelector contextInfo:(void *)contextInfo
{
  // si l'état n'est pas "darkened"
  if (!screensDarkened)
  {
    // on parcours les fenêtres pour trouver la plus judicieuse.
    [alert beginSheetModalForWindow:[self findMostInterestingWindow]
                      modalDelegate:delegate
                     didEndSelector:didEndSelector
                        contextInfo:contextInfo];
  }
  else // on est en darkened
  {
    // Si on n'a pas de fenêtre affichée, c'est qu'on n'a que un écran, et qu'il est utilisé pour la calibration.
    // Dans ce cas, on doit prendre la fenêtre de calibration.
    if ([displayedWindows count] == 0)
    {
      [alert beginSheetModalForWindow:[self findMostInterestingWindow]
                        modalDelegate:delegate
                       didEndSelector:didEndSelector
                          contextInfo:contextInfo];
    }
    // Sinon, on a au moins un écran dispo pour afficher l'alerte.
    // On utilise le premier, qui est forcement celui de la vue de contrôle si on en à une.
    else
    {
      if (controlView != nil)
        [alert beginSheetModalForWindow:[controlView window]
                          modalDelegate:delegate
                         didEndSelector:didEndSelector
                            contextInfo:contextInfo];
      else
        [alert beginSheetModalForWindow:[displayedWindows objectAtIndex:0]
                                         modalDelegate:delegate
                                        didEndSelector:didEndSelector
                                           contextInfo:contextInfo];
    }
  }
}

-(NSWindow*) findMostInterestingWindow
{
  NSArray *screens = [NSScreen screens];
  NSArray *windows = [NSApp windows];
  int     i;
  // Si on n'a que un écran
  if ([screens count] == 1)
  {
    // on recherche une fenêtre plein écran
    for (i = 0; i<[windows count]; i++)
    {
      if (NSEqualSizes([[windows objectAtIndex:i] frame].size, [[screens objectAtIndex:0] frame].size))
      {
        if ([[windows objectAtIndex:i] isVisible])
          return [windows objectAtIndex:i];
      }
    }

    // si on en trouve pas, on prend la fenêtre principale
    return [NSApp mainWindow];
  }
  
  // Si on a plusieurs écrans, est-ce que la fenêtre principale est cachée par une fenêtre full-screen ?
  // pour le moment, on retourne la mainWindow, parceque on sait que on a prévu de rendre la fenetre
  // principale visible .... mais c'est pas propre.
  return [NSApp mainWindow];
}
@end
