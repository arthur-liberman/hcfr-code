//  ColorHCFR
//  HCFRAbstractEngine.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
//
//  $Rev$
//  $Date$
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

#import "HCFRAbstractEngine.h"
#import <HCFRPlugins/HCFRScreensManager+alerts.h>

@interface HCFRAbstractEngine (Private)
- (void)alertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;
@end


@implementation HCFRAbstractEngine

-(NSScreen*) getCalibratedScreen
{
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

-(NSView*) getFullScreenControlView
{
  return Nil;
}
-(NSWindow*) getControlWindow
{
  return Nil;
}

-(void) setConfiguration:(HCFRConfiguration*)configuration
{
  [self doesNotRecognizeSelector:_cmd];
}
-(void) abortMeasures
{
  [self doesNotRecognizeSelector:_cmd];
}
-(bool) isRunning
{
  [self doesNotRecognizeSelector:_cmd];
  return false;
}

#pragma mark gestion du delegué
-(void) setDelegate:(NSObject<HCFREngineDelegate>*)newDelegate
{
  if (delegate != nil)
    [delegate release];
  
  delegate = [newDelegate retain];
}

-(void) notifyEngineStart
{
  if (delegate != nil)
    [delegate performSelectorOnMainThread:@selector(engineStartMeasures:)
                               withObject:self
                            waitUntilDone:NO];
}
-(void) notifyEngineEnd
{
  if (delegate != nil)
    [delegate performSelectorOnMainThread:@selector(engineMeasuresEnded:)
                               withObject:self
                            waitUntilDone:NO];
}

#pragma Gestion des alertes
-(int) displayAlert:(NSAlert*)alert
{
  // if we are in the main thread, the alert is modal.
  if ([NSThread isMainThread])
  {
    return [alert runModal];
  }
  // Else, the alert is displayed in the main thread, and the current thread
  // is locked until the alert is answered.
  else {
    [self performSelectorOnMainThread:@selector(displayAlertInMainThread:)
                           withObject:alert
                        waitUntilDone:NO];
    waitForInteraction = YES;
    while (waitForInteraction)
    {
      [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    return lastAlertReturnCode;
  }
}

-(void) displayAlertInMainThread:(NSAlert*)alert
{
  [[HCFRScreensManager sharedManager] displayAlert:alert
                                     modalDelegate:self
                                    didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                                       contextInfo:nil];
}
- (void)alertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  lastAlertReturnCode = returnCode;
  
  // plus besoin d'attendre, on peut continuer.
  waitForInteraction = NO;
}
@end
