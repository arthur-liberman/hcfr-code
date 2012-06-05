//  ColorHCFR
//  HCFRIntegratedGenerator.m
// -----------------------------------------------------------------------------
//  Created by Jerome Duquennoy on 31/08/07.
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

#import "HCFRIntegratedGenerator.h"

@interface HCFRIntegratedGenerator (Private)
/*!
    @function 
    @abstract   Genere les composantes RVB pour les étapes de saturation
    @discussion Cette fonction à été reprise de la version PC.
    @param      percentage le pourcentage de saturation à obtenir (entre 0 et 100)
    @param      measure Le type de mesure qui indiquera si on cherche une saturation bleu, rouge ou verte
    @result     Un NSArray contenant des NSNumber dans cet ordre : rouge, vert, bleu.
*/
-(NSArray*) RVBValuesForColorSaturation:(double)precentage measureType:(HCFRMeasuresType)measure;
@end

@implementation HCFRIntegratedGenerator
-(HCFRIntegratedGenerator*) init
{
  [super init];

  // on crée la fenetre plein écran
  fullScreenWindow = [[HCFRFullScreenWindow alloc] init];
  [fullScreenWindow setReleasedWhenClosed:NO];
  [fullScreenWindow setNextResponder:self];
  [fullScreenWindow setLevel:NSScreenSaverWindowLevel]; // on couvre tout et tout le monde !
  
  stepForScaleMeasures = 10; // pas des mesures
  stepForDetailedScaleMeasures = 1; // pas des mesures détaillées

  // et les différentes vues
  unicolorView = [[HCFRUnicolorView alloc] init];
  ansiContrastView = [[HCFRANSIContrastView alloc] init];

  // On charge le nib contenant l'interface de configuration.
  [NSBundle loadNibNamed:@"IntegratedGenerator" owner:self];
    
  // pour le moment, pas d'interruption demandée
  abortRequested = NO;
  
  // la référence de couleur (pour le moment, on n'en a pas)
  colorReference = nil;
  
  // le vérrou pour attendre les interractions utilisateur
  waitForInteraction = NO;

  // on charge la liste des ecran dans le popup associé
  [screensPopupButton removeAllItems];
  NSEnumerator  *enumerator = [[NSScreen screens] objectEnumerator];
  NSScreen      *currentScreen;
  int           screenNumber = 1;
  while (currentScreen = (NSScreen*)[enumerator nextObject])
  {
    NSMenu      *popupMenu = [screensPopupButton menu];
    NSMenuItem  *menuItem = [[NSMenuItem alloc] init];
    
    [menuItem setRepresentedObject:currentScreen];
    [menuItem setTitle:[NSString stringWithFormat:HCFRLocalizedString(@"Screen %d", @"Screen %d"), screenNumber]];
    
    [popupMenu addItem:menuItem];
    [menuItem release];
    screenNumber ++;
  }
  
  [self setRunning:NO];
  
  return self;
}

-(void) setColorReference:(HCFRColorReference*)newReference
{
  if (colorReference != nil)
    [colorReference release];
  
  colorReference = [newReference retain];
}
-(void) dealloc
{
  if (colorReference != nil)
    [colorReference release];
  
  [fullScreenWindow release];
  [unicolorView release];
  [ansiContrastView release];
  
  [super dealloc]; 
}

#pragma mark Chargement/sauvegarde des prefs
-(NSDictionary*) getPreferencesDictionary
{
  int screenIndex = [screensPopupButton indexOfSelectedItem];

  return [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithInt:screenIndex],@"selectedScreenIndex",
                                                    [NSNumber numberWithInt:[self screenCoverage]],@"screenCoverage", nil];
}
-(void) setPreferencesWithDictionary:(NSDictionary*)preferences
{
  NSNumber *screenIndexNumber = [preferences objectForKey:@"selectedScreenIndex"];
  int index = [screenIndexNumber intValue];
  int nbScreens = [[screensPopupButton menu] numberOfItems];
  
  if (index < [[screensPopupButton menu] numberOfItems])
    [screensPopupButton selectItemAtIndex:index];
  else
    [screensPopupButton selectItemAtIndex:nbScreens-1];
  
  [self selectedScreenChanged:screensPopupButton];
  
  if ([preferences objectForKey:@"screenCoverage"] != nil)
    [self setScreenCoverage:[[preferences objectForKey:@"screenCoverage"] intValue]];
}

#pragma mark IBActions
-(IBAction) selectedScreenChanged:(NSPopUpButton*)sender
{
  [fullScreenWindow setScreen:[[screensPopupButton selectedItem] representedObject]];
}

#pragma mark KVC
-(int) screenCoverage
{
  return [unicolorView screenCoverage];
}
-(void) setScreenCoverage:(int)newCoverage
{
  [unicolorView setScreenCoverage:newCoverage];
}

#pragma mark Identification du générateur et Gestion du panneau de configuration
+(NSString*) generatorName
{
  return HCFRLocalizedString(@"Integrated generator",@"Integrated generator");
}
+(NSString*) generatorId
{
  return @"homecinema-fr.generators.IntegratedGenerator";
}
-(BOOL) hasASetupView
{
  return YES;
}
-(NSView*) setupView
{
  return configurationView;
}
-(NSScreen*) screenUsed
{
  return [[screensPopupButton selectedItem] representedObject];
}

#pragma mark Control du générateur
-(BOOL) isGeneratorAvailable
{
  // Le générateur est toujours utilisable : pas de periphériques externe ni quoi que
  // ce soit qui puisse poser problème.
  return YES;
}
-(void) startMeasures:(HCFRMeasuresType)measuresToPerforme;
{
  NSAssert (colorReference!=nil, @"HCFRIntegratedGenerator->startMeasures : no color reference available.\nUse setColorReference before starting measures");

  // on ne peux pas lancer deux fois en parallele
  if ([self isRunning])
    return;
  
  measuresToPerformeBitmask = measuresToPerforme;
  stepFrameIndex = -1; // le n° de frame de l'étape actuelle. On commence à -1, car on incrémente avant l'étape.
  stepDidEnd = false;

  // on vérifie qu'on a au monins une étape à effectuer
  // si aucuen étape n'a été séléctionnée, on laisse tomber
  if ((measuresToPerformeBitmask & kAllMeasures) == 0)
  {
    NSLog (@"HCFRIntegretedGenerator : no step to run");
    
    [[NSException exceptionWithName:kHCFRGeneratorDidNotStartException
                             reason:HCFRLocalizedString(@"No step to run.",@"No step to run.")
                           userInfo:nil] raise];
  }
  
  // On selectionne la première étape à lancer
  currentStep = 0;
  
  while ( ((measuresToPerformeBitmask & (1<<currentStep)) == 0) && currentStep < 9)
    currentStep ++;
    
  [unicolorView setDescription:@""];
  [unicolorView setColor:[NSColor blackColor]];
  [fullScreenWindow setContentView:unicolorView];

  [fullScreenWindow makeKeyAndOrderFront:self];
  [self setRunning:YES];
}

-(BOOL) nextFrame
{
  NSAssert ([self isRunning], @"HCFRIntegratedGenerator->nextFrame : the generator is not started. Start it before calling nextFrame");
  
  // Si un annulation à été demandée, on arrète là.
  // On ne lève pas d'exception : ça provoquerai l'affichage d'un message d'alerte.
  // L'utilisateur à demandé une interruption, ça ne sert à rien de lui rappeler
  // "vous avez demandé une annulation" : il le sait déjà.
  if (abortRequested)
  {
    abortRequested = NO; // La demande est traitée, on l'enlève.
    return NO;
  }
  
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

    // si l'étape actuelle n'existe pas, on a fini
    if ((1<<currentStep) > kAllMeasures)
      return NO;
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
  
  switch ((HCFRMeasuresType) 1<<currentStep) // on cast pour que le compilo puisse vérifier qu'on traite tous les cas
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
      NSAssert1 (NO, @"Unimplemented step %d in HCFRIntegratedGenerator", currentStep);
      break;
  }
  
  // si necessaire, on indique qu'il faudra passer à l'étape suivante au prochain tour
  if (!result)
    stepDidEnd = YES;
    
  return YES;
}
-(void) close
{
  [fullScreenWindow close];
  [self setRunning:NO];
}

#pragma mark Les étapes du générateur
-(bool) grayscaleStep
{
  // Si c'est la première frame du lot, on met la vue unicolor en place.
  if (stepFrameIndex == 0)
    [fullScreenWindow setContentView:unicolorView];
  
  float value = stepFrameIndex*stepForScaleMeasures;
  value = fmin (100.0, value);
  
  [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Grayscale : %d IRE", @"Grayscale : %d IRE"), (int)value]];
  [unicolorView setColor:[NSColor colorWithCalibratedWhite:value/100.0 alpha:1.0]];

  // si on est au max, c'est la dernière étape.
  return (value != 100);
}
-(bool) nearBlackStep
{
  // Si c'est la première frame du lot, on met la vue unicolor en place.
  if (stepFrameIndex == 0)
    [fullScreenWindow setContentView:unicolorView];
  
  float value = stepFrameIndex*stepForDetailedScaleMeasures;
  
  [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Grayscale : %d IRE", @"Grayscale : %d IRE"), (int)value]];
  [unicolorView setColor:[NSColor colorWithCalibratedWhite:value/100.0 alpha:1.0]];
  
  // si on est au max, c'est la dernière étape.
  return (value < 10);
}
-(bool) nearWhiteStep
{
  // Si c'est la première frame du lot, on met la vue unicolor en place.
  if (stepFrameIndex == 0)
    [fullScreenWindow setContentView:unicolorView];
  
  float value = 90+stepFrameIndex*stepForDetailedScaleMeasures;
  value = fmin (100.0, value);
  
  [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Grayscale : %d IRE", @"Grayscale : %d IRE"), (int)value]];

  [unicolorView setColor:[HCFRColor greyNSColorWithIRE:value]];

  // si on est au max, c'est la dernière étape.
  return (value != 100);
}
-(bool) redSaturationStep
{
  // Si c'est la première frame du lot, on met la vue unicolor en place.
  if (stepFrameIndex == 0)
    [fullScreenWindow setContentView:unicolorView];
  
  float value = stepFrameIndex*stepForScaleMeasures;
  value = fmin (100.0, value);
  
  [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Red saturation : %d%%", @"Red saturation : %d%%"), (int)value]];

  [unicolorView setColor:[HCFRColor redNSColorWithSaturation:value
                                             forColorReference:colorReference]];
  
  // si on est au max, c'est la dernière étape.
  return (value != 100);
}
-(bool) greenSaturationStep
{
  // Si c'est la première frame du lot, on met la vue unicolor en place.
  if (stepFrameIndex == 0)
    [fullScreenWindow setContentView:unicolorView];
  
  float value = stepFrameIndex*stepForScaleMeasures;
  value = fmin (100.0, value);
  
  [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Green saturation : %d%%",@"Green saturation : %d%%"), (int)value]];
  
  [unicolorView setColor:[HCFRColor greenNSColorWithSaturation:value
                                            forColorReference:colorReference]];

  // si on est au max, c'est la dernière étape.
  return (value != 100);
}
-(bool) blueSaturationStep
{
  // Si c'est la première frame du lot, on met la vue unicolor en place.
  if (stepFrameIndex == 0)
    [fullScreenWindow setContentView:unicolorView];
  
  float value = stepFrameIndex*stepForScaleMeasures;
  value = fmin (100.0, value);
  
  [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Blue saturation : %d%%",@"Blue saturation : %d%%"), (int)value]];
  
  [unicolorView setColor:[HCFRColor blueNSColorWithSaturation:value
                                            forColorReference:colorReference]];
  
  // si on est au max, c'est la dernière étape.
  return (value != 100);
}
-(bool) fullscreenContrastStep
{
    // Si c'est la première frame du lot, on demande à l'utilisateur de placer le capteur face au projecteur
  if (stepFrameIndex == 0)
  {
    [fullScreenWindow setContentView:unicolorView];
    [unicolorView setDescription:HCFRLocalizedString(@"Full screen contrast", @"Full screen contrast")];
    
    // Si on est en cours de calibration, (bitmask "calibration"), on applique le screenCoverage
    // Sinon, c'est une mesure de contrast plein écran, on se met en plein écran.
    if (measuresToPerformeBitmask == kSensorCalibrationMeasures)
      [unicolorView setColor:[NSColor colorWithCalibratedWhite:0.0 alpha:1.0]];
    else
      [unicolorView setFullscreenColor:[NSColor colorWithCalibratedWhite:0.0 alpha:1.0]];
            
    return YES;
  }
  else if (stepFrameIndex == 1)
  {
    // Si on est en cours de calibration, (bitmask "calibration"), on applique le screenCoverage
    // Sinon, c'est une mesure de contrast plein écran, on se met en plein écran.
    if (measuresToPerformeBitmask == kSensorCalibrationMeasures)
      [unicolorView setColor:[NSColor colorWithCalibratedWhite:1.0 alpha:1.0]];
    else
      [unicolorView setFullscreenColor:[NSColor colorWithCalibratedWhite:1.0 alpha:1.0]];

    return NO;
  }

  return NO; // on ne passe jamais par la, c'est juste pour éviter un warning
}

-(bool) ansiContrastStep
{
  
  // Si c'est la première frame du lot, on demande à l'utilisateur de placer le capteur face au projecteur
  if (stepFrameIndex == 0)
  {
    [fullScreenWindow setContentView:ansiContrastView];
//    [ansiContrastView setCrossStyle:kCrossOnBlackRegions];
    
    NSAlert *alert = [NSAlert alertWithMessageText:HCFRLocalizedString(@"Sensor placement",@"Sensor placement")
                                     defaultButton:HCFRLocalizedString(@"Ok",@"Ok")
                                   alternateButton:nil
                                       otherButton:nil
                         informativeTextWithFormat:HCFRLocalizedString(@"Place the sensor in the black area, then press Ok.",
                                                                     @"Place the sensor in the black area, then press Ok.")];
    // on affiche l'alerte
    int result = [self displayAlert:alert];
    abortRequested = (result==NSAlertAlternateReturn);
    
    return YES;
  }
  else if (stepFrameIndex == 1)
  {
    [ansiContrastView invertBlackAndWhite];
    
    return NO;
  }

  return NO; // on ne passe jamais par la, c'est juste pour éviter un warning
}

-(bool) primaryComponentsStep
{
  // Si c'est la première frame du lot, on met la vue unicolor en place.
  if (stepFrameIndex == 0)
    [fullScreenWindow setContentView:unicolorView];
  
  switch (stepFrameIndex)
  {
    case 0 :
      [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Red componnent",@"Red componnent")]];
      [unicolorView setColor:[NSColor redColor]];
      return YES;
      break;
    case 1 :
      [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Green componnent",@"Green componnent")]];
      [unicolorView setColor:[NSColor greenColor]];
      return YES;
      break;
    case 2 :
      [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Blue componnent",@"Blue componnent")]];
      [unicolorView setColor:[NSColor blueColor]];
      return NO;
      break;
  }
  
  return NO; // on ne passe jamais par la, c'est juste pour éviter un warning
}
-(bool) secondaryComponentsStep
{
  // Si c'est la première frame du lot, on met la vue unicolor en place.
  if (stepFrameIndex == 0)
    [fullScreenWindow setContentView:unicolorView];
  
  switch (stepFrameIndex)
  {
    case 0 :
      [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Cyan componnent",@"Cyan componnent")]];
      [unicolorView setColor:[NSColor cyanColor]];
      return YES;
      break;
    case 1 :
      [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Magenta componnent",@"Magenta componnent")]];
      [unicolorView setColor:[NSColor magentaColor]];
      return YES;
      break;
    case 2 :
      [unicolorView setDescription:[NSString stringWithFormat:HCFRLocalizedString(@"Yellow componnent",@"Yellow componnent")]];
      [unicolorView setColor:[NSColor yellowColor]];
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
      composantValue = stepFrameIndex*stepForScaleMeasures;
      break;
    case kGrayscaleNearBlackMeasure :
      composantValue = stepFrameIndex*stepForDetailedScaleMeasures;
      break;
    case kGrayscaleNearWhiteMeasure :
      composantValue = stepFrameIndex*stepForDetailedScaleMeasures+90;
      break;
    case kRedSaturationMeasure : // saturation du rouge
      composantValue = stepFrameIndex*stepForScaleMeasures;
      break;
    case kGreenSaturationMeasure : // saturation du vert
      composantValue = stepFrameIndex*stepForScaleMeasures;
      break;
    case kBlueSaturationMeasure : // saturation du bleux
      composantValue = stepFrameIndex*stepForScaleMeasures;
      break;
    case kPrimaryComponentsMeasure : // composantes primaires : on considère les valeurs du type enuméré comme stable -> on mappe brutalement.
      composantValue = stepFrameIndex;
      break;
    case kSecondaryComponentsMeasure : // composantes primaires : on considère les valeurs du type enuméré comme stable -> on mappe brutalement.
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

-(NSArray*) RVBValuesForColorSaturation:(double)percentage measureType:(HCFRMeasuresType)measure
{
  if (measure & kSaturationsMeasure == 0)
  {
    NSLog (@"HCFRIntegratedSaturation->RVBValuesForColorSaturation : invalide measure type : %d", measure);
    return [NSArray arrayWithObjects:[NSNumber numberWithDouble:0.0],[NSNumber numberWithDouble:0.0],[NSNumber numberWithDouble:0.0],nil];
  }
  
  // Retrieve color luma coefficients matching actual reference
  const double KR = [colorReference redReferenceLuma];  
  const double KG = [colorReference greenReferenceLuma];  
  const double KB = [colorReference blueReferenceLuma];  
  
  bool  redSaturation = (measure == kRedSaturationMeasure);
  bool  greenSaturation = (measure == kGreenSaturationMeasure);
  bool  blueSaturation = (measure == kBlueSaturationMeasure);
  
  
  double K = ( redSaturation ? KR : 0.0 ) + ( greenSaturation ? KG : 0.0 ) + ( blueSaturation ? KB : 0.0 );
  double luma = K * 255.0;	// Luma for pure color
  
  // Compute vector between neutral gray and saturated color in CIExy space
  ColorRGB Clr1;
  CColor Clr2;
  double	xstart, ystart, xend, yend;
  
  // Retrieve gray xy coordinates
  ColorxyY whitexyYValues = [colorReference white].GetxyYValue();
  xstart = whitexyYValues[0];
  ystart = whitexyYValues[1];
  
  // Define target color in RGB mode
  Clr1[0] = ( redSaturation ? 255.0 : 0.0 );
  Clr1[1] = ( greenSaturation ? 255.0 : 0.0 );
  Clr1[2] = ( blueSaturation ? 255.0 : 0.0 );
  
  // Compute xy coordinates of 100% saturated color
  Clr2.SetRGBValue(Clr1, [colorReference cColorReference]);
  ColorxyY Clr3=Clr2.GetxyYValue();
  xend=Clr3[0];
  yend=Clr3[1];
  
  double clr, comp;
  
  if ( percentage == 0 )
  {
    clr = luma;
    comp = luma;
  }
  else if ( percentage == 100 )
  {
    clr = 255.0;
    comp = 0.0;
  }
  else
  {
    double x, y;
    
    x = xstart + ( (xend - xstart) * (percentage/100.0) );
    y = ystart + ( (yend - ystart) * (percentage/100.0) );
    
    ColorxyY UnsatClr_xyY(x,y,luma);
    
    CColor UnsatClr;
    UnsatClr.SetxyYValue (UnsatClr_xyY);
    
    ColorRGB UnsatClr_rgb = UnsatClr.GetRGBValue ([colorReference cColorReference]);
    
    // Both components are theoretically equal, get medium value
    clr = ( ( redSaturation ? UnsatClr_rgb[0] : 0.0 ) + ( greenSaturation ? UnsatClr_rgb[1] : 0.0 ) + ( blueSaturation ? UnsatClr_rgb[2] : 0.0 ) ) /
          ( ( redSaturation ? 1 : 0 ) + ( greenSaturation ? 1 : 0 ) + ( blueSaturation ? 1 : 0 ) );
    comp = ( ( luma - ( K * (double) clr ) ) / ( 1.0 - K ) );
    
    if ( clr < 0.0 )
      clr = 0.0;
    else if ( clr > 255.0 )
      clr = 255.0;
    
    if ( comp < 0.0 )
      comp = 0.0;
    else if ( comp > 255.0 )
      comp = 255.0;
  }
  
  clr = pow ( clr / 255.0, 0.45 );
  comp = pow ( comp / 255.0, 0.45 );
  
  NSNumber *redValue = [NSNumber numberWithDouble:( redSaturation ? clr : comp )];
  NSNumber *greenValue = [NSNumber numberWithDouble:( greenSaturation ? clr : comp )];
  NSNumber *blueValue = [NSNumber numberWithDouble:( blueSaturation ? clr : comp )];
  
  return [NSArray arrayWithObjects:redValue, greenValue, blueValue, nil];
}

#pragma mark Gestion des evenements
- (void)cancelOperation:(id)sender
{
  abortRequested = YES;
}
@end
