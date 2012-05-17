//  ColorHCFR
//  HCFRCalibrationEngine.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
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

#import "HCFRCalibrationEngine.h"
#import "HCFRConstants.h"

@interface HCFRCalibrationEngine (Private)
/*!
 @method     
 @abstract   thread worker for sensor calibration
 @discussion This function is designed to be the main function of a separate thread that will
 perform sensor calibration. It sets up the thread's envrionment and executes performSensorCalibration
 */
-(void) startThreadedSensorCalibration;
/*!
 @method     
 @abstract   Perform the measures
 @discussion This function is the "payload" of the calibratoin part of the engin : it is the one
 that acquires the needed measures and compute the calibration data.
 */
-(void) performSensorCalibration;
/*!
 @method     
 @abstract   thread worker for measures
 @discussion This function is designed to be the main function of a separate thread that will
 perform measures. It will set up the thread's environment and execute performSensorCalibration
 if necessary and performMeasures:.
 */
-(void) startThreadedMeasures:(NSNumber*)measuresToDo;
/*!
 @method     
 @abstract   Perform the measures
 @discussion This function is the "payload" of the engin : it is the one
 that really perform the measures.
 */
-(void) performMeasures:(NSNumber*)measuresToDo;
-(void) setStatus:(NSString*)newStatus;
@end

@implementation HCFRCalibrationEngine

-(HCFRCalibrationEngine*) init
{
  [super init];
  
  if (self != nil) {
    measureLock = [[NSLock alloc] init];
    delegate = nil;
    engineStatus = [HCFRLocalizedString(@"idle",@"idle") retain];
    
    // Chargement de la vue de contrôle depuis le nib
    [NSBundle loadNibNamed:@"CalibrationEngine" owner:self];
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
#pragma mark Accesseurs
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
-(void) setConfiguration:(HCFRConfiguration *)configuration
{
  [self setGenerator:[configuration generator]];
  [self setSensor:[configuration sensor]];
}
-(HCFRGenerator*) generator
{
  return generator;
}
-(void) setGenerator:(HCFRGenerator*)newGenerator
{
  if (generator != nil)
  {
//    [generator deactivate];
    [generator release];
  }
  generator = [newGenerator retain];
//  [newGenerator activate];
}
-(HCFRSensor*) sensor
{
  return sensor;
}
-(void) setSensor:(HCFRSensor*)newSensor
{
  if (sensor != nil)
  {
//    [sensor deactivate];
    [sensor release];
  }
  sensor = [newSensor retain];
//  [sensor activate];
}
-(HCFRDataSerie*) dataSerie
{
  return dataSerie;
}
-(void) setDataSerie:(HCFRDataSerie*)newDataSerie
{
  if (dataSerie != nil)
    [dataSerie release];
  dataSerie = [newDataSerie retain];
}

#pragma mark control de la procédure de calibration
-(BOOL) checkEnvironment:(bool)requiresASerie
{
  // on commence par vérifier la cohérence de nos données
  NSAssert(generator!=nil, @"HCFRCalibrationEngine->startCalibration : generateur null !");
  NSAssert(sensor!=nil, @"HCFRCalibrationEngine->startCalibration : capteur null !");
  
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
  
  // pareil pour le générateur
  if (![generator isGeneratorAvailable])
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Generator unavailable",@"Generator unavailable")
                                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                   alternateButton:nil
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"The selected generator (%@) is not usable.\nCheck the connections.",
                                                                     @"The selected generator (%@) is not usable.\nCheck the connections."), [[generator class] generatorName]];
    [alert beginSheetModalForWindow:[NSApp mainWindow] modalDelegate:nil
                     didEndSelector:nil contextInfo:nil];
    return NO;
  }
  
  return YES;
}

-(bool) startSensorCalibration
{
  if (![self checkEnvironment:NO])
    return false;
  
  abortRequested = NO;
  
  [NSThread detachNewThreadSelector:@selector(startThreadedSensorCalibration) toTarget:self withObject:nil];
  
  // fini pour nous : les mesures se feront dans la tâche dédiée.
  return true;
}

-(void) startThreadedSensorCalibration
{
  // on verrouille pour éviter les mesures concurentes.
  [measureLock lock];
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  
  // on prévient le délégué sur la thread principale
  [self notifyEngineStart];

  [self performSensorCalibration];
  
  [pool release];
  [measureLock unlock];

  // on prévient le délégué sur la thread principale
  [self notifyEngineEnd];
}

// cette fonction n'est pas déclarée dans le fichier header : elle n'est utilisée que pour lancher une thread séparée
// et ne doit pas être utilisée depuis l'exterieur.
// Pour lancer une calibration, utilisez la fonction startCalibration.
-(void) performSensorCalibration
{
  // les données de calibration
  HCFRColor *redComponent , *greenComponent, *blueComponent, *whiteComponent, *blackComponent;
  
  redComponent = greenComponent = blueComponent = whiteComponent = blackComponent = nil;
  

  // on ne souhaite pas de message avant le contrast
  // on stock néanmoins la valeur courrante, pour
  // pouvoir la remettre en place après.
  BOOL initialMessageBeforeContrastMeasuresValue = [generator getMessageBeforeContrastMeasures];
  [generator setMessageBeforeContrastsMeasures:NO];

  // La sonde ou le générateur peuvent lever une exception en cas de soucis.
  // On prévoie donc de les traiter
  @try
  {
    // puis on lance la calibration dans une thread séparée.
    //  [generator startMeasures:kFullScreenContrastMeasure];
    [generator startMeasures:(HCFRMeasuresType)kSensorCalibrationMeasures];

    [self setStatus:HCFRLocalizedString(@"Waiting for the generator", @"Waiting for the generator")];
    [self willChangeValueForKey:@"currentMeasureName"];
    while (!abortRequested && [generator nextFrame]) { // une boucle infinie dont on sort lorsque les mesures soient finies.
      double    reference;

      [self didChangeValueForKey:@"currentMeasureName"];
  
      // on fait une petite pause, le temps d'être sur que l'image est OK (iris auto,
      // lampes à puissance variable et autres blagues)
      [self setStatus:HCFRLocalizedString(@"Waiting for the image source to stabilize",
                                        @"Waiting for the image source to stabilize")];
      float   delay = [[NSUserDefaults standardUserDefaults] floatForKey:kHCFRMeasureDelayValue];
      [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:delay]];
      
      // on récupère les valeur théoriques
      reference = [generator currentReferenceValue];
      
      // on averti le capteur de la valeur qu'on vas demander, afin que les capteurs virtuels
      // sachent quoi retourner.
      [sensor nextMeasureWillBeType:[generator currentDataType] referenceValue:[NSNumber numberWithDouble:reference]];

      // on reset le compteur d'innactivité de la machine pour éviter que l'économiseur ne se déclanche
      // ou que la luminausité de l'écran ne soit baissée
      UpdateSystemActivity(UsrActivity);

      // et la valeur mesurée
      [self setStatus:HCFRLocalizedString(@"Performing measure", @"Performing measure")];
      HCFRColor *value = [sensor currentMeasure];
      // la valeur ne peut en aucun cas être nulle
      NSAssert(value != nil, @"HCFRCalibrationEngine->StartThreadedSensorCalibration : Nil value received from sensor !");

      HCFRDataType  dataType = [generator currentDataType];
//      double        referenceValue = [generator currentReferenceValue];

      // On conserve les données localement pour le calcul de la matrice de calibration.
      switch (dataType)
      {
        case kComponentsDataType :
          switch ((int)reference)
          {
            case kRedComponent :
              redComponent = value;
              break;
            case kGreenComponent :
              greenComponent = value;
              break;
            case kBlueComponent :
              blueComponent = value;
              break;
            default :
              NSAssert(NO, @"HCFRCalibrationEngine:startThreadedSensorCalibration : invalide component !");
              break;
          }
          break;
        case kFullScreenContrastDataType :
          switch ((int)reference)
          {
            case 0 :
              blackComponent = value;
              break;
            case 1 :
              whiteComponent = value;
              break;
          }
          break;
        default :
          NSAssert(NO, @"HCFRCalibrationEngine:startThreadedSensorCalibration : Invalide data type !");
          break;
      }
      [self setStatus:HCFRLocalizedString(@"Waiting for the generator", @"Waiting for the generator")];

      [self willChangeValueForKey:@"currentMeasureName"];

    } // de while
    [self didChangeValueForKey:@"currentMeasureName"];
    
    // si on a toutes les données, on calcule la matrice.
    // on calcule la matrice de calibration
    if (redComponent != nil && greenComponent != nil &&
        blueComponent != nil && whiteComponent != nil && blackComponent != nil)
    {
      Matrix calibrationMatrix = [self computeCalibrationMatrixWithRedMeasure:redComponent
                                                                 greenMeasure:greenComponent
                                                                  blueMeasure:blueComponent
                                                                 blackMeasure:blackComponent
                                                                 whiteMeasure:whiteComponent];
      // Si la matrice obtenue est l'identité, c'est que la calibration à écouhé
      // TODO : lever une exception dans la fonction computeCalibrationMatrix... serait plus propre !
      if (!calibrationMatrix.IsIdentity())
      {
        [sensor setSensorToXYZMatrix:calibrationMatrix];
      }
    }
  }
  @catch (NSException *e)
  {
    if ([[e name] isEqualToString:kHCFRSensorFailureException])
    {
      [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Measures interrupted",@"Measures interrupted")
                       defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                     alternateButton:nil
                         otherButton:nil
           informativeTextWithFormat:HCFRLocalizedString(@"%@.\nThe calibration could not be performed.",
                                                       @"%@.\nThe calibration could not be performed."), [e reason]]];
    }
    else if ([[e name] isEqualToString:kHCFRGeneratorDidNotStartException])
    {
      [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"The generator did not start",@"The generator did not start")
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
    [generator setMessageBeforeContrastsMeasures:initialMessageBeforeContrastMeasuresValue];
    [generator close];
    [self setStatus:HCFRLocalizedString(@"idle",@"idle")];
  }
}

-(bool) startMeasures:(HCFRMeasuresType)measuresToDo
{
  if (![self checkEnvironment:YES])
    return false;
  
  // OK, tout est bon, on peut commencer
  // On nettoie le data store si c'est necessaire
    
  [self willChangeValueForKey:@"currentMeasureName"];
  [self didChangeValueForKey:@"currentMeasureName"];
  
  // on initialise les variables d'état
  abortRequested = NO;
  sensorFacingSource = NO;
  
  // Si une calibration est necessaire, on la fait
  // on affiche un message pour le signaler à l'utilisateur.
  // La calibration sera effectuée par la thread dédiée, avant les mesures.
  if ([sensor needsCalibration] && ![sensor calibrationPerformed])
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor calibration",@"Sensor calibration")
                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                   alternateButton:nil
                       otherButton:nil
         informativeTextWithFormat:HCFRLocalizedString(@"The sensor must be calibrated before performing measures.\nThe calibration will now start.",
                                                     @"The sensor must be calibrated before performing measures.\nThe calibration will now start.")];
    [alert runModal];
  }
  
  // et on lance la tâche de mesure.
  [NSThread detachNewThreadSelector:@selector(startThreadedMeasures:) toTarget:self withObject:[NSNumber numberWithInt:measuresToDo]];
  
  // Fini pour nous, maintenant, tout se passera dans la thread dédiée
  return true;
}

// cette fonction n'est pa déclarée dans le fichier header : elle n'est utilisée que pour lancher une thread séparée
// et ne doit pas être utilisée depuis l'exterieur.
// Pour lancer une calibration, utilisez la fonction startCalibration.
-(void) startThreadedMeasures:(NSNumber*)measuresToDo
{
  // on verrouille pour éviter les mesures concurentes.
  [measureLock lock];

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // on prévient le délégué sur la thread principale
  [self notifyEngineStart];

  // Si la calibration est nécessaire, on la fait.
  if ([sensor needsCalibration] && ![sensor calibrationPerformed])
  {
    [self performSensorCalibration];
    // Si la calibration est annulée, ce sera indiqué par la variable
    // abortRequested, il n'y aura pas de mesure effectuées.
  }
  [self performMeasures:measuresToDo];

  [measureLock unlock];
  [pool release];
  [self notifyEngineEnd];
}

-(void) performMeasures:(NSNumber*)measuresToDo
{
  // La sonde ou le générateur peuvent lever une exception en cas de soucis.
  // On prévoie donc de la traiter
  @try
  {
      // puis on démarre le générateur
    [generator startMeasures:(HCFRMeasuresType)[measuresToDo intValue]];
  
    // On boucle tant que il n'y a pas d'annulation demandée et que il y des frames a afficher.
    [self setStatus:HCFRLocalizedString(@"Waiting for the generator", @"Waiting for the generator")];
    [self willChangeValueForKey:@"currentMeasureName"];
    while (!abortRequested && [generator nextFrame])
    { 
      HCFRColor *value;
      double    reference;
          
      [self didChangeValueForKey:@"currentMeasureName"];
      
      // si on effecture la première mesure de contrast et que l'utilisateur l'a demandé, on affiche une alerte
      if ((([generator currentDataType] == kFullScreenContrastDataType) || ([generator currentDataType] == kANSIContrastDataType)) &&
          ([[NSUserDefaults standardUserDefaults] boolForKey:kHCFRPauseBeforeContrastSteps]) && (!sensorFacingSource))
      {
        NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor placement",@"Sensor placement")
                                         defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                       alternateButton:nil
                                           otherButton:nil
                             informativeTextWithFormat:HCFRLocalizedString(@"For contrast measures, the sensor must face the source.\n Place the sensor, then press Ok.",
                                                                         @"For contrast measures, the sensor must face the source.\n Place the sensor, then press Ok.")];
        int result = [self displayAlert:alert];
        // et si l'utilisateur le demande, on annule.
        if (result==NSAlertAlternateReturn)
          break;
        
        sensorFacingSource = YES;
      }
      
      // on fait une petite pause, le temps d'être sur que l'image est OK (iris auto,
      // lampes à puissance variable et autres blagues)
      [self setStatus:HCFRLocalizedString(@"Waiting for the image source to stabilize",
                                        @"Waiting for the image source to stabilize")];
      float   delay = [[NSUserDefaults standardUserDefaults] floatForKey:kHCFRMeasureDelayValue];
      [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:delay]];
      
      // on met un point d'arrêt à ce moment là, car la mesure peut durer un peu
      if (abortRequested)
        break;
      
      // on récupère les valeur théoriques
      reference = [generator currentReferenceValue];
      
      // on averti le capteur de la valeur qu'on vas demander, afin que les capteurs virtuels
      // sachent quoi retourner.
      [sensor nextMeasureWillBeType:[generator currentDataType] referenceValue:[NSNumber numberWithDouble:reference]];
      
      // on reset le compteur d'innactivité de la machine pour éviter que l'économiseur ne se déclanche
      // ou que la luminausité de l'écran ne soit baissée
      UpdateSystemActivity(UsrActivity);

      // et la valeur mesurée
      [self setStatus:HCFRLocalizedString(@"Performing measure", @"Performing measure")];
      value = [sensor currentMeasure];
      // la valeur ne peut en aucun cas être nulle
      NSAssert(value != nil, @"HCFRCalibrationEngine->startThreadedMeasures : Nil value received from sensor !");

      // on les stockes dans le dataStore
      [dataSerie addMeasure:value withReferenceNumber:[NSNumber numberWithDouble:reference] forType:[generator currentDataType]];  

      [self setStatus:HCFRLocalizedString(@"Waiting for the generator", @"Waiting for the generator")];
      [self willChangeValueForKey:@"currentMeasureName"];
      
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
    else if ([[e name] isEqualToString:kHCFRGeneratorDidNotStartException])
    {
      [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"The generator did not start",@"The generator did not start")
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
    [generator close];
    [self setStatus:HCFRLocalizedString(@"idle",@"idle")];
  }

}

-(IBAction) abortMeasures:(id)sender
{
  [self abortMeasures];
}

-(void) abortMeasures
{
  // On se contente de signaler qu'une interruption à été demandée.
  // Lors de la prochaine mesure, la procédure sera interrompue.
  abortRequested = YES;
  [self setStatus:HCFRLocalizedString(@"Aborting ...", @"Aborting ...")];
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
-(NSString*) currentMeasureName
{
  // Si on n'est pas en cours de mesure, on retourne une chaine vide
  if ([measureLock tryLock])
  {
    [measureLock unlock];
    return @"not running";
  }
  
  HCFRDataType  dataType = [generator currentDataType];
  double        referenceValue = [generator currentReferenceValue];
  switch (dataType)
  {
    case kLuminanceDataType :
      return [HCFRLocalizedString(@"Luminance", @"Luminance") stringByAppendingFormat:@" (%d%%)", (int)referenceValue];
    case kFullScreenContrastDataType :
      if (referenceValue == 0)
        return HCFRLocalizedString(@"Full screen contrast (black)", @"Full screen contrast (black)");
      else
        return HCFRLocalizedString(@"Full screen contrast (white)", @"Full screen contrast (white)");
    case kANSIContrastDataType :
      if (referenceValue == 0)
        return HCFRLocalizedString(@"ANSI contrast (black)", @"ANSI contrast (black)");
      else
        return HCFRLocalizedString(@"ANSI contrast (white)", @"ANSI contrast (white)");
    case kRedSaturationDataType :
      return [HCFRLocalizedString(@"Red saturation", @"Red saturation") stringByAppendingFormat:@" (%d%%)", (int)referenceValue];
    case kGreenSaturationDataType :
      return [HCFRLocalizedString(@"Green saturation", @"Green saturation") stringByAppendingFormat:@" (%d%%)", (int)referenceValue];
    case kBlueSaturationDataType :
      return [HCFRLocalizedString(@"Blue saturation", @"Blue saturation") stringByAppendingFormat:@" (%d%%)", (int)referenceValue];
    case kComponentsDataType :
      switch ((int)referenceValue)
      {
        case kRedComponent :
          return HCFRLocalizedString(@"Red component", @"Red component");
        case kGreenComponent :
          return HCFRLocalizedString(@"Green component", @"Green component");
        case kBlueComponent :
          return HCFRLocalizedString(@"Blue component", @"Blue component");
        case kCyanComponent :
          return HCFRLocalizedString(@"Cyan component", @"Cyan component");
        case kMagentaComponent :
          return HCFRLocalizedString(@"Magenta component", @"Magenta component");
        case kYellowComponent :
          return HCFRLocalizedString(@"Yellow component", @"Yellow component");
      }
      break;
    case kContinuousDataType : // ne devrait jamais apparaitre dans le cas du calibration engine/
      return HCFRLocalizedString(@"Continuous measure", @"Continuous measure");
    case kUndefinedDataType :
      return HCFRLocalizedString(@"Unknown measure", @"Unknown measure");
  }
  
  return @"Unknown measure name";
}

#pragma mark Calcul de la matrice de calibration
-(Matrix) computeCalibrationMatrixWithRedMeasure:(HCFRColor*)redMeasure
                                    greenMeasure:(HCFRColor*)greenMeasure
                                     blueMeasure:(HCFRColor*)blueMeasure
                                    blackMeasure:(HCFRColor*)blackMeasure
                                    whiteMeasure:(HCFRColor*)whiteMeasure
{
  // on se protège des exceptions c++ lancées par la classe matrix
  // faute de quoi on risque un plantage violent du logiciel en cas d'erreur
  try
  {
    // on récupère les différentes mesures sous forme de matrice
    CColor measuredRedMatrix = [redMeasure XYZColor];
    CColor measuredGreenMatrix = [greenMeasure XYZColor];
    CColor measuredBlueMatrix = [blueMeasure XYZColor];
    CColor measuredBlackMatrix = [blackMeasure XYZColor];
    CColor measuredWhiteMatrix = [whiteMeasure XYZColor];
            
    if (YES)//compensateBlack) // TODO gérer ça via les prefs
    {
      int i;
      for (i = 0; i<3; i++)
      {
        measuredRedMatrix[i] = measuredRedMatrix[i] - 2.0/3.0 * measuredBlackMatrix[i];
        measuredGreenMatrix[i] = measuredGreenMatrix[i] - 2.0/3.0 * measuredBlackMatrix[i];
        measuredBlueMatrix[i] = measuredBlueMatrix[i] - 2.0/3.0 * measuredBlackMatrix[i];
      }
    }
    
    // on se crée une matrice carrée avec les mesures
    Matrix measures(0.0, 3, 3);
    measures(0,0)=measuredRedMatrix[0];
    measures(1,0)=measuredRedMatrix[1];
    measures(2,0)=measuredRedMatrix[2];
    measures(0,1)=measuredGreenMatrix[0];
    measures(1,1)=measuredGreenMatrix[1];
    measures(2,1)=measuredGreenMatrix[2];
    measures(0,2)=measuredBlueMatrix[0];
    measures(1,2)=measuredBlueMatrix[1];
    measures(2,2)=measuredBlueMatrix[2];
    
    // on vérifie l'additivité
    double sum[3];
    double differences[3];
    for(int i=0;i<3;i++)
    {
      sum[i]=measuredRedMatrix[i]+measuredGreenMatrix[i]+measuredBlueMatrix[i];
      differences[i] = ((sum[i] - measuredWhiteMatrix[i])/measuredWhiteMatrix[i])*100.0;
      if(fabs(differences[i]) > 15)// m_maxAdditivityErrorPercent) TODO : gérer ça via les prefs
      {
        [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Calibration failure",@"Calibration failure")
                         defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                       alternateButton:nil
                           otherButton:nil
             informativeTextWithFormat:HCFRLocalizedString(@"The error margin on additivity is too high to use those calibration values (%f %%).",
                                                         @"The error margin on additivity is too high to use those calibration values (%f %%)."), fabs(differences[i])]];
        
        return IdentityMatrix(3); // on retourne une matrice de taille nulle.
      }
    }
    
    // On affiche les résultats
    NSMutableString  *message = [NSMutableString stringWithString:HCFRLocalizedString(@"Additivity :\n",@"Additivity :\n")];
    [message appendFormat:HCFRLocalizedString(@"Red : %.0f instead of %.0f (%.1f %%)\n",
                                            @"Red : %.0f instead of %.0f (%.1f %%)\n"), sum[0], measuredWhiteMatrix[0], differences[0]];
    [message appendFormat:HCFRLocalizedString(@"Green : %.0f instead of %.0f (%.1f %%)\n",
                                            @"Green : %.0f instead of %.0f (%.1f %%)\n"), sum[1], measuredWhiteMatrix[1], differences[1]];
    [message appendFormat:HCFRLocalizedString(@"Blue : %.0f instead of %.0f (%.1f %%)\n",
                                            @"Blue : %.0f instead of %.0f (%.1f %%)\n"), sum[2], measuredWhiteMatrix[2], differences[2]];
    
    [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Calibration results",@"Calibration results")
                                            defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                          alternateButton:nil
                                              otherButton:nil
                                informativeTextWithFormat:message]];
    
    
    // on vérifie que le discriminant n'est pas null.
    // si c'est le cas, pas besoin d'aller plus loin : on ne peut pas l'utiliser.
    if (measures.Determinant() == 0)
      return IdentityMatrix(3); // on retourne une matrice de taille nulle.
    
    // TODO : pour le moment, on utilise des matrice codées en dur.
    // Il serait bien d'avoir une fenêtre de prefs pour la calibration qui permette de gérer ça.
    // Default chromacities are the Rec709 ones
    Matrix chromacities(0.0,3,3);
    chromacities(0,0)=0.640;	// Red chromacities
    chromacities(1,0)=0.330;
    chromacities(2,0)=1.0-chromacities(1,0)-chromacities(0,0);
    
    chromacities(0,1)=0.300;	// Green chromacities
    chromacities(1,1)=0.600;
    chromacities(2,1)=1.0-chromacities(0,1)-chromacities(1,1);
    
    chromacities(0,2)=0.150;	// Blue chromacities
    chromacities(1,2)=0.060;
    chromacities(2,2)=1.0-chromacities(0,2)-chromacities(1,2);
    
    // Default white is D65
    Matrix white(0.0,3,1);
    white(0,0)=0.3127;
    white(1,0)=0.3290;
    white(2,0)=1.0-white(0,0)-white(1,0);  
    
    Matrix result;
    // A partir des résultats de mesures, on crée la matrice capteur -> XYZ
    if (YES)// !useOnlyPrimaries) //TODO : gérer par les prefs. Voir OneDeviceSensor.cpp:674. 
    {
      // component gain for reference white
      Matrix invertedReference = chromacities.GetInverse();
      Matrix referenceWhiteGain=invertedReference*white;
      
      // component gain for measured white
      Matrix invertedMeasures = measures.GetInverse();
      Matrix measuredWhiteGain=invertedMeasures*measuredWhiteMatrix;
      
      // transforme component gain matrix into a diagonal matrix
      Matrix diagonalReferenceWhite(0.0,3,3);
      diagonalReferenceWhite(0,0)=referenceWhiteGain(0,0);
      diagonalReferenceWhite(1,1)=referenceWhiteGain(1,0);
      diagonalReferenceWhite(2,2)=referenceWhiteGain(2,0);
      
      // transforme measured gain matrix into a diagonal matrix
      Matrix diagonalMeasuredWhite(0.0,3,3);
      diagonalMeasuredWhite(0,0)=measuredWhiteGain(0,0);
      diagonalMeasuredWhite(1,1)=measuredWhiteGain(1,0);
      diagonalMeasuredWhite(2,2)=measuredWhiteGain(2,0);
      
      // Compute component distribution for reference white
      Matrix referenceWhiteComponentMatrix=chromacities*diagonalReferenceWhite;
      
      // Compute component distribution for measured white
      Matrix measuredWhiteComponentMatrix=measures*diagonalMeasuredWhite;
      
      // Compute XYZ transformation matrix
      Matrix	invT=measuredWhiteComponentMatrix.GetInverse();
      result=referenceWhiteComponentMatrix*invT;
    }
    
    // on vérifie que le determinant n'est pas null, et si c'est OK, on retourne la matrice
    if (result.Determinant() == 0)
      return IdentityMatrix(3);
    else
      return result;
  }
  catch (MatrixException e)
  {
    // on la converti en exception obj-c qui sera gérée par le calibration Engine.
    [[NSException exceptionWithName:@"MatrixException" reason:[NSString stringWithUTF8String:e.getMessage()] userInfo:nil] raise];
  }
  return IdentityMatrix(3);
}
@end
