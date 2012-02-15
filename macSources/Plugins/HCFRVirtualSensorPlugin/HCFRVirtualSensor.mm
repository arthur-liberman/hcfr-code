//  ColorHCFR
//  HCFRVirtualSensor.m
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

#include <stdlib.h>
#import "HCFRVirtualSensor.h"

@interface HCFRVirtualSensor (Private)
-(void) nibDidLoad;
@end

@implementation HCFRVirtualSensor
-(HCFRVirtualSensor*)init
{
  [super init];
  
  simulatedDataSerie = [[HCFRDataSerie alloc] init];
  [simulatedDataSerie importFile:[[NSBundle bundleForClass:[self class]] pathForResource:@"defaultVirtualValues" ofType:@"csv"]];
  
  return self;
}

-(void) nibDidLoad
{
  [dataStoreStatus setStringValue:[NSString stringWithFormat:HCFRLocalizedString(@"Default values loaded (%d values)",
                                                                               @"Default values loaded (%d values)"),
                                                            [simulatedDataSerie count]]];
}
-(void) dealloc
{
  [simulatedDataSerie release];
  
  [super dealloc];
}

#pragma mark fonction divers
+(NSString*) sensorName
{
  return HCFRLocalizedString(@"Virtual sensor", @"Virtual sensor");
}
+(NSString*) sensorId;
{
  return @"homecinema-fr.sensors.virtualSensor";
}

#pragma mark Gestion du capteur
-(BOOL) isAvailable { return YES; } // toujours dispo
-(BOOL) needsCalibration { return NO; } // pas de calibration

#pragma mark Gestion du panneau de configuration
-(BOOL) hasASetupView { return YES; } // pour le moment, pas de setup
-(NSView*) setupView
{
  if (setupView == nil) // si on n'a pas chargé encore le nib, on le charge
  {
    if ([NSBundle loadNibNamed:@"VirtualSensor" owner:self])
      [self nibDidLoad];
  }
  return setupView; 
}

#pragma mark implementation du protocol DataSource
-(void) nextMeasureWillBeType:(HCFRDataType)type referenceValue:(NSNumber*)referenceValue;
{
  HCFRDataStoreEntry  *entry;
  
  // dans le cas général, on retrouve la valeur grace à sa valeur de référence.
  if (type != kContinuousDataType) {
    entry = [simulatedDataSerie entryWithReferenceValue:[referenceValue intValue] forType:type];
  }
  // on prévoie un cas particulier pour les mesures en continue :
  // la valeur de référence module 50 est l'index de la mesure à retourner
  // (la valeur de référence n'a pas de signification particulière dans le cas des mesures
  // en continue, l'affichage étant libre)
  else {
    entry = [simulatedDataSerie entryAtIndex:([referenceValue intValue]%50) forType:type];
  }


  // Si la mesure n'est pas disponible, on en crée une nulle.
  if (entry == nil)
  {
    NSLog (@"VirtualSensor : Entry not available for type %d, reference %d.", type, [referenceValue intValue]);
    nextMeasure = [[HCFRColor alloc] initWithMatrix:Matrix(0.0, 3, 1)];
    // TODO BUG : fuite de mémoire quand on retourne une valeur inconnue :
    // on ne fait jamais de release sur cette instance
  }
  else
  {
    nextMeasure = [entry value];
    // *(1+(rand()/(float)RAND_MAX)/10.0)
  } 
    
}
-(HCFRColor*) currentMeasure
{
  if ([alertBeforeValueButton state] == NSOnState)
    [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Presse Ok to continue.",@"Presse Ok to continue.")
                     defaultButton:@"Ok"
                   alternateButton:nil
                       otherButton:nil
         informativeTextWithFormat:HCFRLocalizedString(@"Virtual sensor pause", @"Virtual sensor pause")]];
  return nextMeasure;
}

#pragma mark Actions
-(IBAction)loadDataFromFile:(id)sender
{
  NSArray     *fileTypes = [HCFRDataSerie importableExtensions];
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  int         result;
  
  [openPanel setRequiredFileType:@"csv"];
  @try
  {
    result = [openPanel runModalForDirectory:NSHomeDirectory() file:nil types:fileTypes];
  
    if (result == NSOKButton)
    {
      [simulatedDataSerie clearStore];
      [simulatedDataSerie importFile:[openPanel filename]];
      [dataStoreStatus setStringValue:[NSString stringWithFormat:HCFRLocalizedString(@"%d values loaded from %@",
                                                                                   @"%d values loaded from %@"),
                                                                [simulatedDataSerie count],
                                                                [[openPanel filename] lastPathComponent]]];
    }
  }
  @catch (NSException *e)
  {
    [dataStoreStatus setStringValue:HCFRLocalizedString(@"Invalid file, no values loaded",
                                                        @"Invalid file, no values loaded")];
    
    [[NSAlert alertWithMessageText:HCFRLocalizedString(@"Could not read file",
                                                       @"Could not read file")
                     defaultButton:@"Ok" alternateButton:nil otherButton:nil
         informativeTextWithFormat:HCFRLocalizedString(@"The selected file could not be read :\n%@",
                                                       @"The selected file could not be read :\n%@"), [e reason]] runModal];
  }
}
@end
