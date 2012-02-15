//  ColorHCFR
//  HCFRContinuousAcquisitionEngine.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 28/12/10.
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

#import <HCFRPlugins/HCFRScreensManager+alerts.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import "HCFRContinuousAcquisitionEngine.h"
#import "HCFRGraphController.h"


@interface HCFRContinuousAcquisitionEngine (Private)
-(BOOL) checkEnvironment:(bool)requiresASerie;
-(void) startThreadedMeasures;
-(void) setStatus:(NSString*)newStatus;
-(NSString*) status;
-(void) abortMeasures;
-(void) setPause:(BOOL)newValue;
@end

@implementation HCFRContinuousAcquisitionEngine
@synthesize dataSerie;

- (HCFRContinuousAcquisitionEngine*) initWithPluginController:(HCFRPluginsController*)pluginsController
{
  self = [super init];
  if (self != nil) {
    measureLock = [[NSLock alloc] init];
    delegate = Nil;
    engineStatus = [HCFRLocalizedString(@"idle",@"idle") retain];
    generator = Nil;
    pauseBetweenMeasures = 1.0;

    // Chargement de la vue de contrôle depuis le nib
    if (![NSBundle loadNibNamed:@"ContinuousAcquisitionEngine" owner:self])
    {
      NSLog(@"HCFRContinuousAcquisitionEngine : failed to load xib file.");
      return self;
    }
    
    // add graphs to control view
    NSEnumerator *pluginsEnumerator;
    pluginsEnumerator = [[pluginsController graphicPlugins] objectEnumerator];
    HCFRGraphController *currentGraph;
    
    while (currentGraph = [pluginsEnumerator nextObject])
    {
      // on saute tous les graphiques qui ne sont pas des graphiques de calibration :
      // seul les graphs de calibration sont ajoutés à la fenêtre principale.
      if ([currentGraph graphicType] != kContinuousMeasuresGraph)
        continue;
      
      [controlView addGraph:currentGraph];
    }
    
  }
  return self;
}

-(void) dealloc
{
  // on s'assure que aucune mesure ne tourne
  [measureLock lock];
  
  if (generator != nil)
    [generator release];
  if (sensor != nil)
    [sensor release];
  if (dataSerie != nil)
    [dataSerie release];
  if (delegate != nil)
    [delegate release];

  [measureLock unlock];
  [measureLock release];
  
  [super dealloc];
}

-(void) setConfiguration:(HCFRConfiguration *)configuration
{
  // on charge uniquement le capteur de la configuration.
  // Le générateur est forcement un HCFRManualFullscreenGenerator, et il
  // ne sera utilisé que si plusieurs écrans sont disponibles.
  [self setSensor:[configuration sensor]];
}
-(HCFRGenerator*) generator
{
  return generator;
}
-(HCFRSensor*) sensor
{
  return sensor;
}
-(void) setSensor:(HCFRSensor*)newSensor
{
  if (sensor != nil)
  {
    [sensor release];
  }
  sensor = [newSensor retain];
}

-(BOOL) checkEnvironment:(bool)requiresASerie
{
  // on commence par vérifier la cohérence de nos données
  NSAssert(sensor!=nil, @"HCFRContinuousAcquisitionEngine->startCalibration : capteur null !");

  // on regarde si on a une dataSerie
  if (dataSerie == nil && requiresASerie)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"No data serie available",@"No data serie available")
                                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                   alternateButton:nil
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"No selected data serie. Please select the data serie in which the measures will be stored.",
                                                                       @"No selected data serie. Please select the data serie in which the measures will be stored.")];
    [alert beginSheetModalForWindow:[NSApp mainWindow] modalDelegate:nil
                     didEndSelector:nil contextInfo:nil];
    return NO;
  }
  
  // on vérifie que le capteur est bien dispo
  if (![sensor isAvailable])
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor unavailable",@"Sensor unavailable")
                                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                   alternateButton:nil
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"The selected sensor (%@) is not usable.\nCheck the connections.",
                                                                       @"The selected sensor (%@) is not usable.\nCheck the connections."), [[sensor class] sensorName]];
    
    [alert beginSheetModalForWindow:[NSApp mainWindow] modalDelegate:nil
                     didEndSelector:nil contextInfo:nil];
    return NO;
  }
  
  return YES;
}

-(bool) startMeasuresWithColorReference:(HCFRColorReference*)colorReference;
{
  if (![self checkEnvironment:YES])
    return false;
  
  abortRequested = NO;

  if ([sensor needsCalibration] && ![sensor calibrationPerformed])
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor calibration",@"Sensor calibration")
                                     defaultButton:HCFRLocalizedString(@"Yes",@"Yes")
                                   alternateButton:HCFRLocalizedString(@"No, cancel measures",@"No, cancel measures")
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"The sensor is not calibrated.\nDo you really want to start continuous measures without calibration ?",
                                                                       @"The sensor is not calibrated.\nDo you really want to start continuous measures without calibration ?")];
    NSInteger result = [alert runModal];
    if (result == NSAlertAlternateReturn)
      return false;
  }
  
  // on regarde si on plusieurs écrans
  // si oui, on utilise le générateur interne, si non, on considère qu'on utilise le générateur externe.
  // le générateur est crée avant les mesures, et détruit à la fin.
  if ([[NSScreen screens] count] > 1)
  {
    // TODO : gérer avec une fenêtre et un popup plutot qu'une alerte pour gérer machine possédant plus de 2 écrans.
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Which screen will be measured ?",@"Which screen will be measured ?")
                                     defaultButton:HCFRLocalizedString(@"Primaray screen",@"Primaray screen")
                                   alternateButton:HCFRLocalizedString(@"Secondary screen",@"Secondary screen")
                                       otherButton:HCFRLocalizedString(@"Another screen",@"Another screen")
                         informativeTextWithFormat:HCFRLocalizedString(@"Indicate on which screen the color should be displayed. If the generator is external (DVD or Blueray pplayer for exemple), select Another screen.",
                                                                       @"Indicate on which screen the color should be displayed. If the generator is external (DVD or Blueray pplayer for exemple), select Another screen.")];
    NSInteger result = [alert runModal];
    if (result == NSAlertDefaultReturn) {
      generator = [[HCFRManualFullscreenGenerator alloc] init];
      [generator setColorReference:colorReference];
      [generator setScreenUsed:[NSScreen mainScreen]];
      [controlView setGeneratorControlView:[generator controlView]];
    }
    else if (result == NSAlertAlternateReturn) {
      generator = [[HCFRManualFullscreenGenerator alloc] init];
      [generator setColorReference:colorReference];
      [generator setScreenUsed:[[NSScreen screens] objectAtIndex:1]];
      [controlView setGeneratorControlView:[generator controlView]];
    }
     else if (result == NSAlertOtherReturn) {
       // Pas de générateur.
     }      
  }
  else {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Prepare the external generator",@"Prepare the external generator")
                                     defaultButton:HCFRLocalizedString(@"Start",@"Start")
                                   alternateButton:nil
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"Only one screen is connected to the machine. It will be used to display the control screen.\nDisplay the requested image on the external screen and press Start to begin measures.",
                                                                       @"Only one screen is connected to the machine. It will be used to display the control screen.\nDisplay the requested image on the external screen and press Start to begin measures.")];
    [alert runModal];

    // DEBUG
    generator = [[HCFRManualFullscreenGenerator alloc] init];
    [generator setColorReference:colorReference];
    [controlView setGeneratorControlView:[generator controlView]];    
  }

  
  [NSThread detachNewThreadSelector:@selector(startThreadedMeasures) toTarget:self withObject:nil];
  
  // fini pour nous, tout se fera dans la thread dédiée à partir de la.
  return true;
}

// cette fonction n'est pa déclarée dans le fichier header : elle n'est utilisée que pour lancher une thread séparée
// et ne doit pas être utilisée depuis l'exterieur.
// Pour lancer les mesures, utilisez la fonction startContinuousMeasures.
-(void) startThreadedMeasures
{
  // on verrouille pour éviter les mesures concurentes.
  [measureLock lock];
  
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  // on prévient le délégué sur la thread principale
  [self notifyEngineStart];
  
  // La sonde peut lever une exception en cas de soucis.
  // On prévoie donc de la traiter
  @try
  {
    int measureIndex = 1;
    // puis on démarre le générateur si on en a un.
    // Si il n'y a pas de générateur, c'est que l'utilisateur utilise une source exterieure.
    if (generator != Nil)
      [generator startMeasures:kContinuousMeasures];
    
    // On boucle tant que il n'y a pas d'annulation demandée et que il y des frames a afficher.
    [self setStatus:HCFRLocalizedString(@"Waiting for the generator", @"Waiting for the generator")];
    while (!abortRequested)
    { 
      HCFRColor *value;
      
      // on fait une pause si l'utilisateur l'a demandé
      if (pauseBetweenMeasures > 0.0) {
        [self setStatus:HCFRLocalizedString(@"Waiting ...",
                                            @"Waiting ...")];
        [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:pauseBetweenMeasures]];
      }
      
      // on met un point d'arrêt à ce moment là, car la mesure peut durer un peu
      if (abortRequested)
        break;
      
      // on averti le capteur de la valeur qu'on vas demander, afin que les capteurs virtuels
      // sachent quoi retourner.
      [sensor nextMeasureWillBeType:kContinuousDataType referenceValue:[NSNumber numberWithInt:measureIndex]];

      // on reset le compteur d'innactivité de la machine pour éviter que l'économiseur ne se déclanche
      // ou que la luminausité de l'écran ne soit baissée
      UpdateSystemActivity(UsrActivity);
      
      // et la valeur mesurée
      [self setStatus:HCFRLocalizedString(@"Performing measure", @"Performing measure")];
      value = [sensor currentMeasure];
      // la valeur ne peut en aucun cas être nulle
      NSAssert(value != nil, @"HCFRContinuousAcquisitionEngine->startThreadedMeasures : Nil value received from sensor !");

      // on les stockes dans le dataStore
      [dataSerie appendContinuousMeasure:value];
      
      if (pauseRequested && !abortRequested)
        [self setStatus:HCFRLocalizedString(@"Paused", @"Paused")];
      while (pauseRequested && !abortRequested)
      {
        [NSThread sleepForTimeInterval:0.2]; // On vérifie si on doit rester en pause 5 fois par seconde.
      }
      
      measureIndex ++;
    }
  }
  @catch (NSException *e)
  {
    if ([[e name] isEqualToString:kHCFRSensorFailureException])
    {
      [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Capture interrupted",@"Capture interrupted")
                                         defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                       alternateButton:nil
                                           otherButton:nil
                             informativeTextWithFormat:HCFRLocalizedString(@"%@.\nThe measure sequence was aborted.",
                                                                           @"%@.\nThe measure sequence was aborted."), [e reason]]];
    }
    else
    {
      [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Internal programm error",@"Internal programm error")
                                         defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                       alternateButton:nil
                                           otherButton:nil
                             informativeTextWithFormat:HCFRLocalizedString(@"An unexpected event occured :\n%@",
                                                                           @"An unexpected event occured :\n%@"), [e reason]]];
    }
  }
  @finally
  {
    if (delegate != nil)
      [self notifyEngineEnd];
    if (generator != Nil)
    {
      [controlView setGeneratorControlView:nil];
      [generator close];
      [generator release];
      generator = nil;
    }
    [self setStatus:HCFRLocalizedString(@"idle",@"idle")];
    [measureLock unlock];
    [pool release];
  }
  
}

-(IBAction) abortMeasures:(id)sender
{
  [self abortMeasures];
}
-(void) abortMeasures;
{
  // On se contente de signaler qu'une interruption à été demandée.
  // Lors de la prochaine mesure, la procédure sera interrompue.
  abortRequested = YES;
  [self setStatus:HCFRLocalizedString(@"Aborting ...", @"Aborting ...")];
}
-(IBAction) toggleMeasuresPause:(NSButton*)sender;
{
  [self setPause:([sender state] != NSOffState)];
}
-(void) setPause:(BOOL)newValue;
{
  // On se contente de signaler qu'une interruption à été demandée.
  // Lors de la prochaine mesure, la procédure sera interrompue.
  pauseRequested = newValue;
}

-(bool) isRunning
{
  // Comment ça elle est bourrain ma méthode ?
  // Un peu, c'est vrai, mais je n'ai pas trouvé d'autre manière de tester un NSLock ...
  if ([measureLock tryLock])
  {
    [measureLock unlock];
    return NO;
  }
  return YES;
}

-(NSScreen*) getCalibratedScreen
{
  if (generator == nil)
    return nil;
  
  return [generator screenUsed];
}
-(NSView*) getFullScreenControlView
{
  return controlView;
}
//-(NSView*) getControlWindow
//{
//  return controlView;
//}

-(void) setDataSerie:(HCFRDataSerie *)newDataSerie
{
  if (dataSerie != Nil)
    [dataSerie release];

  if (newDataSerie != Nil)
    [newDataSerie retain];
  
  dataSerie = newDataSerie;
  
  [controlView setDataSerie:dataSerie];
}

#pragma mark Fonctions de feeback
-(void) setStatus:(NSString*)newStatus
{
  NSString  *oldStatus =  engineStatus;
  [self willChangeValueForKey:@"status"];
  engineStatus = [newStatus retain];
  [oldStatus release];
  [self didChangeValueForKey:@"status"];
}
-(NSString*) status
{
  return engineStatus;
}
@end
