//  ColorHCFR
//  HCFRMainController+appDelegate.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 21/05/08.
//
//  $Rev: 67 $
//  $Date: 2008-07-20 20:02:36 +0100 (Sun, 20 Jul 2008) $
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

#import "HCFRMainController+appDelegate.h"


@implementation HCFRMainController (appDelegate)
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
  if ([calibrationEngine isRunning])
    [calibrationEngine abortMeasures];
  
  if ([dataStore dirty])
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Do you want to save before quitting ?",
                                                                     @"Do you want to save before quitting ?")
                                     defaultButton:HCFRLocalizedString(@"Yes", @"Yes")
                                   alternateButton:HCFRLocalizedString(@"No", @"No")
                                       otherButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                         informativeTextWithFormat:@""];
    int result = [alert runModal];
    
    if (result == NSAlertDefaultReturn)
    {
      [self save:self];
      // si le data store est encore dirty, c'est que l'utilisateur à annulé
      if ([dataStore dirty])
        return NSTerminateCancel;
    }
    else if (result == NSAlertOtherReturn)
      return NSTerminateCancel;
  }
  return NSTerminateNow;
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
  return [self loadFileAtPath:filename];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
  [self loadFileAtPath:[filenames objectAtIndex:0]];
}
@end
