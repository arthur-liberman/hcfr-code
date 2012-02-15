//  ColorHCFR
//  HCFRGenerator.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/08/07.
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

#import "HCFRGenerator.h"
#import "HCFRScreensManager+alerts.h"

@interface HCFRGenerator (Private)
// gestion des alertes
-(void) displayAlertInMainThread:(NSAlert*)alert;
-(void) alertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;
@end

@implementation HCFRGenerator
#pragma mark Constructeur & destructeur
-(HCFRGenerator*) init
{
  [super init];
  
  return self;
}

-(void) dealloc
{
  [super dealloc];
}

#pragma mark Identification du générateur et Gestion du panneau de configuration
+(NSString*) generatorName
{
  return @"Not implemented generator !";
}
+(NSString*) generatorId
{
  NSAssert1(NO, @"%@ : mandatory function generatorID not defined", [self className]);
  return nil; // juste pour éviter un warning
}
-(BOOL) hasASetupView
{
  // par defaut, on n'a pas de panneau de config
  return NO;
}
-(NSView*) setupView
{
  // on ne fait rien par défaut.
  return nil;
}
-(BOOL) hasAControlView
{
  return NO;
}
-(NSView*) controlView
{
  return nil;
}

#pragma mark sauvegarde/chargement des préférences du capteur
-(NSDictionary*) getPreferencesDictionary
{
  return [NSDictionary dictionary];
}
-(void) setPreferencesWithDictionary:(NSDictionary*)preferences
{
  // par defaut, on ne fait rien
}

#pragma mark Control du générateur
-(void) setColorReference:(HCFRColorReference*)newReference
{
  // par defaut, on s'en fout.
}
-(BOOL) isGeneratorAvailable
{
  // par défaut, le génarateur n'est pas utiliable.
  return NO;
}
-(void) activate
{
}
-(void) deactivate
{
}
-(void) startMeasures:(HCFRMeasuresType)measuresToPerforme
{
  // il faut absuluement surcharger cette fonction
  // Donc, on lève une exception si ça n'a pas été fait.
  NSAssert (NO, @"HCFRGenerator : startMeasures not implemented");
}
-(BOOL) nextFrame
{
  // il faut absuluement surcharger cette fonction
  // Donc, on lève une exception si ça n'a pas été fait.
  NSAssert (NO, @"HCFRGenerator : nextFrame not implemented");
  
  return NO;
}
-(void) close
{
  // il faut absuluement surcharger cette fonction
  // Donc, on lève une exception si ça n'a pas été fait.
  NSAssert (NO, @"HCFRGenerator : close not implemented");
}

#pragma mark Les fonctions de gestion des options
-(void) setMessageBeforeContrastsMeasures:(BOOL)newValue
{
  messageBeforeContrastsMeasure = newValue;
}
-(BOOL) getMessageBeforeContrastMeasures
{
  return messageBeforeContrastsMeasure;
}
-(void) setRunning:(BOOL)newRunning
{
  running = newRunning;
}
-(BOOL) isRunning
{
  return running;
}

-(NSScreen*) screenUsed
{
  return nil;
}

#pragma mark Gestion du type de données
-(HCFRDataType) currentDataType
{
  // il faut absuluement surcharger cette fonction
  // Donc, on lève une exception si ça n'a pas été fait.
  NSAssert (NO, @"HCFRGenerator : currentDataType not implemented");
  
  return kUndefinedDataType;
}

-(double) currentReferenceValue
{
  // il faut absuluement surcharger cette fonction
  // Donc, on lève une exception si ça n'a pas été fait.
  NSAssert (NO, @"HCFRGenerator : currentReferenceValue not implemented");
  
  return 0.0;
}

#pragma mark Surcharges divers
-(NSString*) description
{
  return [[self class] generatorName];
}

#pragma mark les fonction d'alerte
-(int) displayAlert:(NSAlert*)alert
{
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

-(void) displayAlertInMainThread:(NSAlert*)alert
{
  [[HCFRScreensManager sharedManager] displayAlert:alert
                                     modalDelegate:self
                                    didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                                       contextInfo:nil];
 
//beginSheetModalForWindow:[NSApp mainWindow] modalDelegate:self
//                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

#pragma mark Selecteur de fin d'alerte
- (void)alertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  lastAlertReturnCode = returnCode;
  
  // plus besoin d'attendre, on peut continuer.
  waitForInteraction = NO;
}
@end
