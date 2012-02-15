//  ColorHCFR
//  HCFRSensor.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
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

#import <HCFRPlugins/HCFRSensor.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import "libHCFR/CalibrationFile.h"
#import "libHCFR/Exceptions.h"

#import "HCFRScreensManager+alerts.h"

@implementation HCFRSensor
#pragma mark constructeur et desturcteur
-(id) init
{
  [super init];

  // par défaut, on prend une matrice identité.
  sensorToXYZMatrix = IdentityMatrix(3);
  calibrationPerformed = NO;
  currentCalibrationFile = Nil;
  
  return self;
}

-(void) dealloc
{
  if (currentCalibrationFile != nil)
  {
    [currentCalibrationFile release];
    currentCalibrationFile = nil;
  } 

  [super dealloc];
}

#pragma mark fonction divers
+(NSString*) sensorName
{
  return @"Not implemented ...";
}
+(NSString*) sensorId
{
  NSAssert1(NO, @"%@ : mandatory function sensorID not defined", [self className]);
  return nil; // juste pour éviter un warning
}
-(void) refresh
{
  // par défaut, on ne fait rien.
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

#pragma mark Gestion du capteur
-(BOOL) isAvailable
{
  return NO;
}
-(BOOL) needsCalibration
{
  return NO;
}
-(void) setSensorToXYZMatrix:(Matrix)newSensorToXYZMatrix
{
  sensorToXYZMatrix = newSensorToXYZMatrix;
  calibrationPerformed = YES;
}
-(Matrix) getSensorToXYZMatrix
{
  return sensorToXYZMatrix;
}
-(BOOL) calibrationPerformed
{
  return calibrationPerformed;
}
-(NSString*) currentCalibrationFile
{
  return currentCalibrationFile;
}
-(void) activate
{
}
-(void) deactivate
{
}
#pragma mark Gestion du fichier de calibration
-(void) clearCalibrationData
{
  calibrationPerformed = NO;
  if (currentCalibrationFile != nil)
  {
    [currentCalibrationFile release];
    currentCalibrationFile = nil;
  } 
}
-(BOOL) readCalibrationFile:(NSString *)filePath
{
  HCFRCalibrationData calibrationData;
  int                 pathLength = [filePath length];
  char                *cFilePath = new char[pathLength+1];

  NSLog (@"HCFR : reading calibration file : %@", filePath);
  
  [filePath getCString:cFilePath maxLength:pathLength+1 encoding:NSUTF8StringEncoding];
  
  try
  {
    calibrationData = readCalibrationFile(cFilePath);
  }
  catch (Exception e)
  {
    NSLog (HCFRLocalizedString(@"Exception in readCalibrationFile %s : %s",
                               @"Exception in readCalibrationFile %s : %s"), cFilePath, e.message);
    // TODO: LEVER UNE EXCEPTION COCOA pour signaler le problème
    delete cFilePath;
    return NO;
  }
  
  [self setSensorToXYZMatrix:calibrationData.sensorToXYZMatrix];

  delete cFilePath;
  
  currentCalibrationFile = [[filePath lastPathComponent] retain];
    
  return YES;
}

#pragma mark Gestion du panneau de configuration
-(BOOL) hasASetupView
{
  // par défaut, on n'a pas de panneau de config
  return NO;
}
-(NSView*) setupView
{
  // on ne fait rien par défaut.
  return nil;
}

#pragma mark implementation du protocol DataSource
-(void) nextMeasureWillBeType:(HCFRDataType)type referenceValue:(NSNumber*)referenceValue
{
  // rien ...
}
-(HCFRColor*) currentMeasure
{
  // par defaut, on lève une exception :
  // cette fonction doit absoluement être redéfinie
  NSAssert (NO, @"HCFRSensor:currentMeasure must be defined by subclasses.");
  
  return nil;
}

#pragma mark Selecteur de fin d'alerte
- (void)alertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  lastAlertReturnCode = returnCode;
  
  // plus besoin d'attendre, on peut continuer.
  waitForInteraction = NO;
}

#pragma mark les fonction d'alerte
-(int) displayAlert:(NSAlert*) alert
{
  [self performSelectorOnMainThread:@selector(displayAlertInMainThread:) withObject:alert waitUntilDone:NO];
  waitForInteraction = YES;
  
  // TODO : si on est dans la thread principale, on bloque tout.
  // Il faudrait pouvoir tester, mais ce n'est possible que en 10.5, alors
  // que ce logiciel tourne également en 10.4.
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
}
@end
