//  ColorHCFR
//  HCFRKiGenerator.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/01/08.
//
//  $Rev: 133 $
//  $Date: 2010-12-26 17:22:04 +0000 (Sun, 26 Dec 2010) $
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

#import "HCFRKiGenerator.h"
#import "HCFRKiGeneratorIRProfilesRepository.h"

#define kHCFRCancelException @"Cancel"
@interface HCFRKiGenerator (Private)
-(bool) selectIRProfileWithName:(NSString*)profileName;
-(void) refresh;
@end

// pour ce générateur, on réordonne les étapes, pour coller au DVD
//                         0 1 2 3 4 5  6 7 8 9      <- étape actuelle
static int nextStep[10] = {6,2,3,4,5,-1,7,8,9,1};//  <- étape suivante

@implementation HCFRKiGenerator
-(HCFRKiGenerator*) init
{
  [super init];
  
  abortRequested = NO;
  profileToUse = nil;
  
  [self refresh];

  return self;
}

- (void) dealloc {
  [super dealloc];
}

#pragma mark Identification du générateur et Gestion du panneau de configuration
+(NSString*) generatorName
{
  return HCFRLocalizedString(@"HCFR generator", @"HCFR generator");
}
+(NSString*) generatorId
{
  return @"homecinema-fr.generators.KiGenerator";
}
-(BOOL) hasASetupView
{
  return YES;
}
-(NSView*) setupView
{
  if (setupView == nil)
  {
    // On charge le nib en lazy load
    [NSBundle loadNibNamed:@"KiGenerator" owner:self];
    [self irProfilesListChanged];
  }
  
  [self irProfileChanged:self];
  
  return setupView;
}

#pragma mark sauvegarde/chargement des préférences du capteur
-(NSDictionary*) getPreferencesDictionary
{
  NSString  *profileName = [[irProfilesPopup selectedItem] title];
  return [NSDictionary dictionaryWithObject:profileName forKey:@"KiGeneratorIrProfileName"];
}
-(void) setPreferencesWithDictionary:(NSDictionary*)preferences
{
  if (![self selectIRProfileWithName:(NSString*)[preferences objectForKey:@"KiGeneratorIrProfileName"]])
  {
    NSString *reason = [NSString stringWithFormat:HCFRLocalizedString(@"The IR profile %@ could not be found. A default profile was selected instead.",
                                                                    @"The IR profile %@ could not be found. A default profile was selected instead."),
                                                 [preferences objectForKey:@"KiGeneratorIrProfileName"]];

    [[NSException exceptionWithName:kHCFRGeneratorInvalidePreferenceException
                             reason:reason
                           userInfo:nil] raise];
  }
}

#pragma mark Les fonctions de gestion internes
/*!
    @function 
    @abstract   Selectionne le profile répondant au nom fournis.
    @discussion  Si aucun profile n'est trouvé, le premier de la liste est
 selectionné, et la fonction retourne false.
    @param      profileName Le nom du profile à selectionner
    @result     YES si le profile a été selectionné, NO si il n'a pas été trouvé.
*/
-(bool) selectIRProfileWithName:(NSString*)profileName
{
  // on charge le nib, histoire d'avoir accès au popup
  [self setupView];
  
  NSMenuItem*  menuItem = [irProfilesPopup itemWithTitle:profileName];
  
  if (menuItem != nil)
  {
    [irProfilesPopup selectItem:menuItem];
    return YES;
  }
  else
  {
    [irProfilesPopup selectItem:[irProfilesPopup itemAtIndex:0]];
    return NO;
  }
}

#pragma mark Les actions IB
-(IBAction) irProfileChanged:(id)sender
{
  HCFRKiGeneratorIRProfile  *selectedProfile = [[irProfilesPopup selectedItem] representedObject];
  
  [authorTextField setStringValue:[selectedProfile author]];
  [descriptionTextView setString:[selectedProfile description]];
}

#pragma mark Control du générateur
-(void) activate
{
  kiDevice = [[HCFRKiDevice sharedDevice] retain];

  // on s'inscrit comme listener sur les changements de la liste des profiles
  // charge la liste des profiles
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] addListener:self];
  [self irProfilesListChanged];
}
-(void) deactivate
{
  [kiDevice release];
  [[HCFRKiGeneratorIRProfilesRepository sharedRepository] removeListener:self];
}
-(BOOL) isGeneratorAvailable
{
  return ( [kiDevice isReady] && ([irProfilesPopup selectedItem] != nil) ) ;
}
-(void) startMeasures:(HCFRMeasuresType)measuresToPerforme
{
  // on ne peux pas lancer deux fois en parallele
  if ([self isRunning])
    return;
  
  measuresToPerformeBitmask = measuresToPerforme;
  stepFrameIndex = -1; // le no de frame de l'étape actuelle. On commence à -1, car on incrémente avant l'étape.
  stepDidEnd = false;
  
  // On selectionne la première étape à lancer
  currentStep = 0;
    
  while ( ((measuresToPerformeBitmask & (1<<currentStep)) == 0) && currentStep != -1 )
    currentStep = nextStep[currentStep];
  
  // si aucune étape n'a été séléctionnée, on laisse tomber
  if (currentStep == -1)
  {
    NSLog (@"HCFR Generator : no step to run.");
    return;
  }
  
  // check IR profile
  if (profileToUse != nil)
    [profileToUse release];
  profileToUse = [[[irProfilesPopup selectedItem] representedObject] retain];
  //TODO : vérifier que le profile est utilisable

  xPosition = yPosition = 0;
  currentMenu = DVDMenuUnknown;
  
  [self setRunning:YES];
}
-(BOOL) nextFrame
{
  NSAssert ([self isRunning], @"HCFRKiGenerator->nextFrame : the generator is not started. Start it before calling nextFrame");
  
  bool result = NO; // sera à false si il faut passer à l'étape suivante
  
  // on incrémente le compteur avant l'étape car la valeur ne doit pas etre modifiée tant que
  // la frame est affichée pour pouvoir connaitre la valeur de référence associée.
  stepFrameIndex ++;

  @try
  {
    // si stepDidEnd vaut true, on passe à l'étape suivante.
    if (stepDidEnd)
    {
      [self finishCurrentStep];
      
      // si on est en calibration, il n'y a pas d'étape suivante. On arrête la.
      if (measuresToPerformeBitmask == kSensorCalibrationMeasures)
        currentStep = -1;
      else
      {
        // on sélectionne la prochaine étape à effectuer
        // pour le generateur Ki, on réordonne les étapes, pour coller au DVD
        currentStep = nextStep[currentStep];
        while ( ((measuresToPerformeBitmask & (1<<currentStep)) == 0) && currentStep != -1)
          currentStep = nextStep[currentStep];
      }
      stepFrameIndex = 0;
      stepDidEnd = NO;
    }  
    
    // Si une anulation est demandée, on arrete là
    // On ne lève pas d'exception : ça provoquerai l'affichage d'un message d'alerte.
    // L'utilisateur à demandé une interruption, ça ne sert à rien de lui rappeler
    // "vous avez demandé une annulation" : il le sait déjà.
    if (abortRequested)
    {
      // c'est traité, on reset la demande
      abortRequested = NO;
      return NO;
    }
    
    // si l'étape actuelle n'existe pas, on a fini
    if (currentStep == -1)
    {
      return NO;
    }
    
    // cas particulier : la calibration
    // Il y a un écran spécifique pour cette étape,
    // on ne peux pas la considérer comme une série de deux étapes
    // (composantes et contrasts plein écran).
    // Donc, on la traite à part.
    if (measuresToPerformeBitmask == kSensorCalibrationMeasures)
    {
      result = [self sensorCalibrationStep];
    }
    else
    {
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
          NSAssert1 (NO, @"Unimplemented step %d in KiGenerator", currentStep);
          break;
      }
    }
    
    // si necessaire, on indique qu'il faudra passer à l'étape suivante au prochain tour
    if (!result)
      stepDidEnd = YES;
  }
  @catch (NSException *exception)
  {
    // si on a annulé, on retourne NO
    if ([[exception name] isEqualToString:kHCFRCancelException])
      return NO;
    else
      [exception raise];
  }
  
  return YES;
}
-(void) close
{
  [profileToUse release];
  profileToUse = nil;
  [self setRunning:NO];
}

#pragma mark gestion de la position dans les menus du DVD
-(BOOL) gotoPositionX:(int)targetX Y:(int)targetY inMenu:(DVDMenus)menu
{
  unsigned short i;
  
  // on ne sais pas changer de menu, on n'a pas de flêche gauche ou haut
  // -> il y a des cas qu'on ne peut pas gérer.
  // dans ce cas, on retourne NO
  if ((targetX < xPosition) || (targetY < yPosition) || (menu != currentMenu))
    return NO;
      
  for (i = 0; i < targetY - yPosition; i ++)
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileDownArrowCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  for (i = 0; i < targetX - xPosition; i ++)
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileRightArrowCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  xPosition = targetX;
  yPosition = targetY;
  return YES;
}

#pragma mark Les étapes du générateur
-(bool) sensorCalibrationStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                    @"DVD player setup")
                                                    defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                                  alternateButton:nil
                                                      otherButton:nil
                                        informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Adjustement' menu, and place the selection on the primaries item.",
                                                                                    @"Enter the 'Adjustement' menu, and place the selection on the primaries item.")]];
    if (result == NSAlertAlternateReturn)
      [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];
    
    currentMenu = DVDMenuSettings;
    xPosition = yPosition = 0;
    
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) grayscaleStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                    @"DVD player setup")
                                                    defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                                  alternateButton:nil
                                                      otherButton:nil
                                        informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Adjustments' menu, and place the selection on the 0 IRE item.",
                                                                                    @"Enter the 'Adjustments' menu, and place the selection on the 0 IRE item.")]];
    if (result == NSAlertAlternateReturn)
      [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];
 
    currentMenu = DVDMenuSettings;
    xPosition = yPosition = 0;
      
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 10);
}
-(bool) nearBlackStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:0 Y:1 inMenu:DVDMenuAdvancedSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                         defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                       alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                           otherButton:nil
                             informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Advanced adjustments' menu, and place the selection on the black 'Aditional grayscale' item.",
                                                                         @"Select the 'Advanced adjustments' menu, and place the selection on the black 'Aditional grayscale' item.")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuAdvancedSettings;
      xPosition = 0;
      yPosition = 0;
    }
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }

  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) nearWhiteStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:1 Y:1 inMenu:DVDMenuAdvancedSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Advanced adjustments' menu, and place the selection on the white 'Aditional grayscale' item.",
                                                                                      @"Enter the 'Advanced adjustments' menu, and place the selection on the white 'Aditional grayscale' item.")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuAdvancedSettings;
      xPosition = 0;
      yPosition = 1;
    }
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) redSaturationStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:0 Y:2 inMenu:DVDMenuAdvancedSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Advanced adjustments', and place the selection on the red saturation item.",
                                                                                      @"Enter the 'Advanced adjustments', and place the selection on the red saturation item.")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuAdvancedSettings;
      xPosition = 0;
      yPosition = 0;
    }
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) greenSaturationStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:0 Y:3 inMenu:DVDMenuAdvancedSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Advanced adjustments', and place the selection on the green saturation item.",
                                                                                      @"Enter the 'Advanced adjustments', and place the selection on the green saturation item.")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuAdvancedSettings;
      xPosition = 0;
      yPosition = 1;
    }
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) blueSaturationStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:0 Y:4 inMenu:DVDMenuAdvancedSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Advanced adjustments', and place the selection on the blue saturation item.",
                                                                                      @"Enter the 'Advanced adjustments', and place the selection on the blue saturation item.")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuAdvancedSettings;
      xPosition = 0;
      yPosition = 2;
    }
    else
    {
      [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
      usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
    }
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 4);
}
-(bool) fullscreenContrastStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:0 Y:3 inMenu:DVDMenuSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Adjustments' menu, and place the selection on the first contrast item (fullscreen black).",
                                                                                      @"Enter the 'Adjustments' menu, and place the selection on the first contrast item (fullscreen black).")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuSettings;
      xPosition = 0;
      yPosition = 2;
    }
    // puis on rentre dans le menu
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 1);
}

-(bool) ansiContrastStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:2 Y:3 inMenu:DVDMenuSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Adjustments' menu, and place the selection on the third contrast item (first ANSI).",
                                                                                      @"Enter the 'Adjustments' menu, and place the selection on the third contrast item (first ANSI).")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuSettings;
      xPosition = 2;
      yPosition = 3;
    }
    else
    {
      [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
      [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor placement",
                                                                         @"Sensor placement")
                                         defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                       alternateButton:nil
                                           otherButton:nil
                             informativeTextWithFormat:HCFRLocalizedString(@"Place the sensor in a black area",
                                                                         @"Place the sensor in a black area")]];
    }
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 1);
}

-(bool) primaryComponentsStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:0 Y:1 inMenu:DVDMenuSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Adjustement' menu, and place the selection on the primaries item.",
                                                                                      @"Enter the 'Adjustement' menu, and place the selection on the primaries item.")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuSettings;
      xPosition = 0;
      yPosition = 1;
    }
    // puis on rentre dans le chapitre
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 2);
}
-(bool) secondaryComponentsStep
{
  // si c'est la première frame, on lance la lecture des mires.
  if (stepFrameIndex == 0)
  {
    // si le positionnement auto est pas possible, on demande à l'utilisateur de le faire
    if (![self gotoPositionX:3 Y:2 inMenu:DVDMenuSettings])
    {
      int result = [self displayAlert:[NSAlert alertWithMessageText:HCFRLocalizedString(@"DVD player setup",
                                                                                      @"DVD player setup")
                                                      defaultButton:HCFRLocalizedString(@"Ok", @"Ok")
                                                    alternateButton:HCFRLocalizedString(@"Cancel", @"Cancel")
                                                        otherButton:nil
                                          informativeTextWithFormat:HCFRLocalizedString(@"Enter the 'Adjustement' menu, and place the selection on the yellow secondary item.",
                                                                                      @"Enter the 'Adjustement' menu, and place the selection on the yellow secondary item.")]];
      if (result == NSAlertAlternateReturn)
        [[NSException exceptionWithName:kHCFRCancelException reason:@"User cancel" userInfo:nil] raise];

      currentMenu = DVDMenuSettings;
      xPosition = 3;
      yPosition = 1;
    }
    // puis on rentre dans le menu
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileOkCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000);
  }
  // Sinon, on avance d'un chapitre
  else
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuNavigationDelay]*1000);
  }
  
  // si on est au max, c'est la dernière étape.
  return (stepFrameIndex != 2);
}

-(void) finishCurrentStep
{
  int i;

  // pas de fin spécifique en mode calibration
  if (measuresToPerformeBitmask == kSensorCalibrationMeasures)
  {
    [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
    usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000); // on utilise la valeur "menu validation" vu qu'on retourne sur un menu
  }
  else
  {
    switch (1<<currentStep)
    {
      case kGrayscaleMeasure :
      case kGrayscaleNearWhiteMeasure :
      case kRedSaturationMeasure :
      case kGreenSaturationMeasure :
      case kBlueSaturationMeasure :
      case kANSIContrastMeasure :
      case kGrayscaleNearBlackMeasure :
        // on avance d'un chapitre pour retourner sur le menu
        [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
        usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000); // on utilise la valeur "menu validation" vu qu'on retourne sur un menu
        break;
      case kPrimaryComponentsMeasure :
      case kSecondaryComponentsMeasure :
      case kFullScreenContrastMeasure :
        // on avance de 3 chapitres pour retourner sur le menu
        for (i=0; i<3; i++)
        {
          [[HCFRKiDevice sharedDevice] sendIRCode:[profileToUse codeBufferForType:IRProfileNextChapterCode]];
          usleep ([profileToUse delayForType:IRProfileChapterNavigationDelay]*1000);
        }
          usleep ([profileToUse delayForType:IRProfileMenuValidationDelay]*1000); // on utilise la valeur "menu validation" vu qu'on retourne sur un menu
        break;
    }
  }
  xPosition = 0; // le DVD retourne en 0,0 à la fin des chapitres
  yPosition = 0;
}

#pragma mark Gestion du type de données
-(HCFRDataType) currentDataType
{
  // cas particulier de la calibration
  if (measuresToPerformeBitmask == kSensorCalibrationMeasures)
  {
    // composantes
    if (stepFrameIndex <= 2)
      return kComponentsDataType;
    // fullscreen contrast
    else
      return kFullScreenContrastDataType;
  }
  else
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
  }
  return kUndefinedDataType;
}

-(double) currentReferenceValue
{
  double composantValue = 0;
  
  // cas particulier de la calibration
  if (measuresToPerformeBitmask == kSensorCalibrationMeasures)
  {
    // composantes
    if (stepFrameIndex <= 2)
      composantValue = stepFrameIndex;
    // fullscreen contrast
    else if (stepFrameIndex == 3)
      composantValue = 1; // blanc
    else
      composantValue = 0; // noir
  }
  else
  {
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
      case kSecondaryComponentsMeasure : 
        if (stepFrameIndex == 0)
          composantValue = kYellowComponent;
        else if (stepFrameIndex == 1)
          composantValue = kCyanComponent;
        else if (stepFrameIndex == 2)
          composantValue = kMagentaComponent;
        break;
      case kFullScreenContrastMeasure : // contrast fullscreen : étape n¬∞0 -> noir, étape n¬∞1 -> blanc
        composantValue = stepFrameIndex;
        break;
      case kANSIContrastMeasure : // contrast ansi : étape n¬∞0 -> noir, étape n¬∞1 -> blanc
        composantValue = stepFrameIndex;
        break;
    }
  }
  
  return composantValue;
}

#pragma mark fonctions spécifiques à la sonde HCFR
-(void) refresh
{
}

#pragma mark implémentation du protocol HCFRIrProfilesRepositoryListener
-(void) irProfilesListChanged
{
  BOOL menuShouldBeEnabled = NO;
  // on complète le menu
  [irProfilesPopup removeAllItems];
  NSEnumerator              *profilesEnumerator = [[[HCFRKiGeneratorIRProfilesRepository sharedRepository] factoryProfilesArray] objectEnumerator];
  HCFRKiGeneratorIRProfile  *currentProfile;
  while (currentProfile = [profilesEnumerator nextObject])
  {
    if (![currentProfile isUsable])
      continue;
    
    NSMenuItem  *newItem = [[NSMenuItem alloc] init];
    [newItem setTitle:[currentProfile name]];
    [newItem setRepresentedObject:currentProfile];
    
    [[irProfilesPopup menu] addItem:newItem];
    [newItem release];
    menuShouldBeEnabled = YES;
  }

  BOOL firstCustomProfile = YES; // pour mettre le séparateur si necessaire
  profilesEnumerator = [[[HCFRKiGeneratorIRProfilesRepository sharedRepository] customProfilesArray] objectEnumerator];
  while (currentProfile = [profilesEnumerator nextObject])
  {
    if (![currentProfile isUsable])
      continue;
    
    if (firstCustomProfile && menuShouldBeEnabled) // si on a des profiles custom et qu'il y a des profiles factory
    {
      [[irProfilesPopup menu] addItem:[NSMenuItem separatorItem]];
      firstCustomProfile = NO;
    }
    
    NSMenuItem  *newItem = [[NSMenuItem alloc] init];
    [newItem setTitle:[currentProfile name]];
    [newItem setRepresentedObject:currentProfile];
    
    [[irProfilesPopup menu] addItem:newItem];
    [newItem release];
    menuShouldBeEnabled = YES;
  }
  
  // on n'active le menu que si il contient des éléments (en plus du séparateur entre les profiles perso et factory
  [irProfilesPopup setEnabled:menuShouldBeEnabled];
}
@end