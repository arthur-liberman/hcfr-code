//  ColorHCFR
//  HCFRManualDVDGenerator.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 22/11/07.
//
//  $Rev: 103 $
//  $Date: 2008-08-28 18:39:32 +0100 (Thu, 28 Aug 2008) $
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

#import "HCFRManualDVDGenerator.h"

@implementation HCFRManualDVDGenerator
-(HCFRManualDVDGenerator*) init
{
  [super init];

  abortRequested = NO;

  return self;
}
- (void) dealloc {
  [super dealloc];
}

#pragma mark Identification du générateur et Gestion du panneau de configuration
+(NSString*) generatorName
{
  return HCFRLocalizedString(@"Manual external generator", @"Manual external generator");
}
+(NSString*) generatorId
{
  return @"homecinema-fr.generators.ManualExternalGenerator";
}
-(BOOL) hasASetupView
{
  return NO;
}

#pragma mark Control du générateur
-(BOOL) isGeneratorAvailable
{
  // Le générateur est toujours utilisable : pas de periphériques externe ni quoi que
  // ce soit qui puisse poser problème.
  return YES;
}
-(void) activate
{
  NSLog (@"dvdGen : activate");
}
-(void) deactivate
{
  NSLog (@"dvdGen : deactivate");
}
-(void) startMeasures:(HCFRMeasuresType)measuresToPerforme
{
  // on ne peux pas lancer deux fois en parallele
  if ([self isRunning])
    return;
  
  measuresToPerformeBitmask = measuresToPerforme;
  stepFrameIndex = -1; // le n° de frame de l'étape actuelle. On commence à -1, car on incrémente avant l'étape.
  stepDidEnd = false;
  
//  NSLog (@"mask : measuresToPerformeBitmask %x", measuresToPerformeBitmask);
  if ((measuresToPerformeBitmask & kAllMeasures) == 0)
  {
    NSLog (@"ManualDVDGenerator : no step to run");
    
    [[NSException exceptionWithName:kHCFRGeneratorDidNotStartException
                             reason:HCFRLocalizedString(@"No step to run.",@"No step to run.")
                           userInfo:nil] raise];
  }
  
  // On selectionne la première étape à lancer
  currentStep = 0;
  
  while ( ((measuresToPerformeBitmask & (1<<currentStep)) == 0) && currentStep < 9 )
    currentStep ++;
    
  [self setRunning:YES];
}

-(BOOL) nextFrame
{
  NSAssert ([self isRunning], @"HCFRManualDVDGenerator->nextFrame : the generator is not started. Start it before calling nextFrame");
  
  bool result = NO; // sera à false si il faut passer à l'étape suivante
  
  // on incrémente le compteur avant l'étape car la valeur ne doit pas être modifiée tant qu'on
  // reste à l'étape pour pouvoir connaitre la valeur de référence.
  stepFrameIndex ++;

  // si stepDidEnd vaut true, on passe à l'étape suivante.
  if (stepDidEnd)
  {
    // on sélectionne la prochaine étape à effectuer
    currentStep ++;
    while ( ((measuresToPerformeBitmask & (1<<currentStep)) == 0) && (1<<currentStep) < kAllMeasures)
      currentStep ++;
    
    stepFrameIndex = 0;
    stepDidEnd = NO;
  }  
  
  // si l'étape actuelle n'existe pas, on a fini
  if ((1<<currentStep) > kAllMeasures)
    return NO;
  
  switch (1<<currentStep)
  {
    case kGrayscaleMeasure :
      result = [self grayscaleStep];
      break;
    case kGrayscaleNearBlackMeasure :
      result = [self nearBlackStep];
      break;
    case kGrayscaleNearWhiteMeasure :
      result = [self nearWhiteStep];
      break;
    case kRedSaturationMeasure :
      result = [self redSaturationStep];
      break;
    case kGreenSaturationMeasure :
      result = [self greenSaturationStep];
      break;
    case kBlueSaturationMeasure :
      result = [self blueSaturationStep];
      break;
    case kPrimaryComponentsMeasure :
      result = [self primaryComponentsStep];
      break;
    case kSecondaryComponentsMeasure :
      result = [self secondaryComponentsStep];
      break;
    case kFullScreenContrastMeasure :
      result = [self fullscreenContrastStep];
      break;
    case kANSIContrastMeasure :
      result = [self ansiContrastStep];
      break;
    default :
      NSAssert1 (NO, @"Unimplemented step %d in manualDVDGenerator", currentStep);
      break;
  }
  
  // Si une anulation est demandée, on arrête là
  // On ne lève pas d'exception : ça provoquerai l'affichage d'un message d'alerte.
  // L'utilisateur à demandé une interruption, ça ne sert à rien de lui rappeler
  // "vous avez demandé une annulation" : il le sait déjà.
  if (abortRequested)
  {
    // c'est traité, on reset la demande
    abortRequested = NO;
    return NO;
  }
  
  // si necessaire, on indique qu'il faudra passer à l'étape suivante au prochain tour
  if (!result)
    stepDidEnd = YES;
  
  return YES;
}
-(void) close
{
  [self setRunning:NO];
}

#pragma mark Les étapes du générateur
-(bool) grayscaleStep
{
  int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                  defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                    otherButton:nil
                                      informativeTextWithFormat:HCFRLocalizedString(@"Select chapter IRE %i",
                                                                                  @"Select chapter IRE %i"), 10*stepFrameIndex]];
  abortRequested = (result == NSAlertAlternateReturn);
    
    
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 10);
}
-(bool) nearBlackStep
{
  int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                  defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                    otherButton:nil
                                      informativeTextWithFormat:HCFRLocalizedString(@"Select chapter IRE %i",
                                                                                  @"Select chapter IRE %i"), stepFrameIndex]];
  abortRequested = (result == NSAlertAlternateReturn);

  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) nearWhiteStep
{
  int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                  defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                    otherButton:nil
                                      informativeTextWithFormat:HCFRLocalizedString(@"Select chapter IRE %i",
                                                                                  @"Select chapter IRE %i"), 96+stepFrameIndex]];
  
  abortRequested = (result == NSAlertAlternateReturn);

  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) redSaturationStep
{
  int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                  defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                    otherButton:nil
                                      informativeTextWithFormat:HCFRLocalizedString(@"Select red saturation %i%%",
                                                                                  @"Select red saturation %i%%"), stepFrameIndex*25]];
    
  abortRequested = (result == NSAlertAlternateReturn);

  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) greenSaturationStep
{
  int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                  defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                    otherButton:nil
                                      informativeTextWithFormat:HCFRLocalizedString(@"Select green saturation %i%%",
                                                                                  @"Select green saturation %i%%"), stepFrameIndex*25]];
  
  abortRequested = (result == NSAlertAlternateReturn);

  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) blueSaturationStep
{
  int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                  defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                    otherButton:nil
                                      informativeTextWithFormat:HCFRLocalizedString(@"Select blue saturation %i%%",
                                                                                  @"Select blue saturation %i%%"), stepFrameIndex*25]];
  
  abortRequested = (result == NSAlertAlternateReturn);

  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) fullscreenContrastStep
{
  // Si c'est la première frame du lot, on demande à l'utilisateur de placer le capteur face au projecteur
  if (stepFrameIndex == 0)
  {
    if (messageBeforeContrastsMeasure)
    {
      NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor placement",@"Sensor placement")
                                       defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                     alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                         otherButton:nil
                           informativeTextWithFormat:HCFRLocalizedString(@"For contrast measures, the sensor must face the source.\n Please place the sensor face to the source.",
                                                                       @"For contrast measures, the sensor must face the source.\n Please place the sensor face to the source.")];
      int result = [self displayAlert:alert];
      abortRequested = (result == NSAlertAlternateReturn);
      if (abortRequested)
        return NO;
    }
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor placement",@"Sensor placement")
                                     defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                   alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"Select the fullscreen black frame.",
                                                                     @"Select the fullscreen black frame.")];
    int result = [self displayAlert:alert];
    abortRequested = (result == NSAlertAlternateReturn);
    return YES;
  }
  else if (stepFrameIndex == 1)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                     defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                   alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"Select the fullscreen white frame.",
                                                                     @"Select the fullscreen white frame.")];
    [self displayAlert:alert];
    int result = [self displayAlert:alert];
    abortRequested = (result == NSAlertAlternateReturn);
    
    return NO;
  }
  
  return NO; // on ne passe jamais par la, c'est juste pour éviter un warning
}

-(bool) ansiContrastStep
{
  
  // Si c'est la première frame du lot, on demande à l'utilisateur de placer le capteur face au projecteur
  if (stepFrameIndex == 0)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor placement",@"Sensor placement")
                                     defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                   alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"Select the first ANSI contrast frame, and place the sensor in a black area.",
                                                                     @"Select the first ANSI contrast frame, and place the sensor in a black area.")];
    // on affiche l'alerte
    [self displayAlert:alert];
    int result = [self displayAlert:alert];
    abortRequested = (result == NSAlertAlternateReturn);
    
    return YES;
  }
  else if (stepFrameIndex == 1)
  {
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor placement",@"Sensor placement")
                                     defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                   alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"Select the second ANSI contrast frame.",
                                                                     @"Select the second ANSI contrast frame.")];
    // on affiche l'alerte
    int result = [self displayAlert:alert];
    abortRequested = (result == NSAlertAlternateReturn);
    
    return NO;
  }
  
  return NO; // on ne passe jamais par la, c'est juste pour éviter un warning
}

-(bool) primaryComponentsStep
{
  int result;
  switch (stepFrameIndex)
  {
    case 0 :
      result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Select the red primary chapter",
                                                                                      @"Select the red primary chapter")]];
      abortRequested = (result == NSAlertAlternateReturn);
      return YES;
      break;
    case 1 :
      result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok") alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel") otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Select the green primary chapter",
                                                                                      @"Select the green primary chapter")]];
      abortRequested = (result == NSAlertAlternateReturn);
      return YES;
      break;
    case 2 :
      result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok") alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel") otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Select the blue primary chapter",
                                                                                      @"Select the blue primary chapter")]];
      abortRequested = (result == NSAlertAlternateReturn);
      return NO;
      break;
  }
  
  return NO; // on ne passe jamais par la, c'est juste pour éviter un warning
}
-(bool) secondaryComponentsStep
{
  int result;
  
  switch (stepFrameIndex)
  {
    case 0 :
      result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Select the cyan secondary chapter",
                                                                                      @"Select the cyan secondary chapter")]];
      abortRequested = (result == NSAlertAlternateReturn);
      return YES;
      break;
    case 1 :
      result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Select the magenta secondary chapter",
                                                                                      @"Select the magenta secondary chapter")]];
      abortRequested = (result == NSAlertAlternateReturn);
      return YES;
      break;
    case 2 :
      result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Next step", @"Next step")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Select the yellow secondary chapter",
                                                                                      @"Select the yellow secondary chapter")]];
      abortRequested = (result == NSAlertAlternateReturn);
      return NO;
      break;
  }
  
  return NO; // on ne passe jamais par la, c'est juste pour éviter un warning
}


#pragma mark Gestion du type de données
-(HCFRDataType) currentDataType
{
  switch (1<<currentStep)
  {
    case kGrayscaleMeasure :
    case kGrayscaleNearBlackMeasure :
    case kGrayscaleNearWhiteMeasure :
      return kLuminanceDataType;
      break;
    case kRedSaturationMeasure :
      return kRedSaturationDataType;
      break;
    case kGreenSaturationMeasure :
      return kGreenSaturationDataType;
      break;
    case kBlueSaturationMeasure :
      return kBlueSaturationDataType;
      break;
    case kPrimaryComponentsMeasure :
    case kSecondaryComponentsMeasure :
      return kComponentsDataType;
      break;
    case kFullScreenContrastMeasure :
      return kFullScreenContrastDataType;
      break;
    case kANSIContrastMeasure :
      return kANSIContrastDataType;
      break;
  }
  return kUndefinedDataType;
}

-(double) currentReferenceValue
{
  double composantValue = 0;
  
  switch (1<<currentStep)
  {
    case kGrayscaleMeasure : // luminosité
      composantValue = stepFrameIndex*10;
      break;
    case kGrayscaleNearBlackMeasure :
      composantValue = stepFrameIndex*1;
      break;
    case kGrayscaleNearWhiteMeasure :
      composantValue = stepFrameIndex*1+96;
      break;
    case kRedSaturationMeasure : // saturation du rouge
      composantValue = stepFrameIndex*25;
      break;
    case kGreenSaturationMeasure : // saturation du vert
      composantValue = stepFrameIndex*25;
      break;
    case kBlueSaturationMeasure : // saturation du bleu
      composantValue = stepFrameIndex*25;
      break;
    case kPrimaryComponentsMeasure : // composantes primaires : on considère les valeurs du type enuméré comme stable -> on mappe brutalement.
      composantValue = stepFrameIndex;
      break;
    case kSecondaryComponentsMeasure : // composantes secondaires : on considère les valeurs du type enuméré comme stable -> on mappe brutalement.
      composantValue = 3+stepFrameIndex;
      break;
    case kFullScreenContrastMeasure : // contrast fullscreen : étape n°0 -> noir, étape n°1 -> blanc
      composantValue = stepFrameIndex;
      break;
    case kANSIContrastMeasure : // contrast ansi : étape n°0 -> noir, étape n°1 -> blanc
      composantValue = stepFrameIndex;
      break;
  }
  
  return composantValue;
}

/*#pragma mark Selecteur de fin d'alerte
- (void)alertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
  if (returnCode == NSAlertAlternateReturn)
    abortRequested = YES;
    
  // plus besoin d'attendre, on peut continuer.
  waitForInteraction = NO;
}

#pragma mark les fonction utilitaires
-(void) displayAlert:(NSAlert*) alert
{
  [self performSelectorOnMainThread:@selector(displayAlertInMainThread:) withObject:alert waitUntilDone:NO];
  waitForInteraction = YES;
  while (waitForInteraction)
  {
    [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
  }
}

-(void) displayAlertInMainThread:(NSAlert*)alert
{
  [alert beginSheetModalForWindow:[NSApp mainWindow] modalDelegate:self
                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:) contextInfo:nil];
}*/
@end
