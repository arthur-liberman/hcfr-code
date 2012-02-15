//  ColorHCFR
//  HCFRManualFullscreenGenerator.m
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 20/01/11.
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

#import "HCFRUtilities.h"
#import "HCFRManualFullscreenGenerator.h"


@implementation HCFRManualFullscreenGenerator

@synthesize selectedColorType;
@synthesize redValue;
@synthesize greenValue;
@synthesize blueValue;
@synthesize grayscaleLuminanceValue;
@synthesize selectedSaturationColor;
@synthesize saturationValue;

typedef enum {kRGBColorType = 0, kGrayscaleColorType = 1, kSaturationColorType = 2} ColorType;

#pragma mark Constructeur & destructeur
-(HCFRManualFullscreenGenerator*) init
{
  [super init];
  
  // quelque valeurs par defaut.
  selectedColorType = 1;
  redValue = 0.5;
  greenValue = 0.5;
  blueValue = 0.5;
  grayscaleLuminanceValue = 50;
  selectedSaturationColor = 1;
  saturationValue = 50;

  // On charge le nib contenant l'interface de configuration.
  [NSBundle loadNibNamed:@"ManualGenerator" owner:self];
  
  // on crée la fenetre plein écran
  fullScreenWindow = [[HCFRFullScreenWindow alloc] init];
  [fullScreenWindow setReleasedWhenClosed:NO];
  [fullScreenWindow setNextResponder:self];
//  [fullScreenWindow setLevel:NSScreenSaverWindowLevel]; // on couvre tout et tout le monde !
  // DEBUG, décommenter la ligne dessus pour prod

  screenUsed = nil;//[NSScreen mainScreen];
  unicolorView = [[HCFRUnicolorView alloc] init];
  
  // la référence de couleur (pour le moment, on n'en a pas)
  colorReference = nil;
    
  // On mets une vue de control par défault
  [colorControlBox setContentView:rgbColorSetupView];
  
  // et on met la couleur en place
  [self colorTypeChanged:self];
  
  return self;
}

-(void) dealloc
{
  if (colorReference != nil)
    [colorReference release];

  [unicolorView dealloc];
  [super dealloc];
}

#pragma mark Identification du générateur et Gestion du panneau de configuration
+(NSString*) generatorName
{
  return HCFRLocalizedString(@"Manual generator",@"Manual generator");
}
+(NSString*) generatorId
{
  return @"homecinema-fr.generators.InternalManualGenerator";
}
-(BOOL) hasAControlView
{
  return YES;
}
-(NSView*) controlView
{
  return controlView;
}
-(void) setColorReference:(HCFRColorReference*)newReference
{
  if (colorReference != nil)
    [colorReference release];
  
  colorReference = [newReference retain];
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
-(BOOL) isGeneratorAvailable
{
  return YES;
}

/*
 Dans le cas du générateur manuel, on se fiche du type de mesure a faire.
 On prend juste le paramètre pour se conformer à l'interface des générateur.
 
 De même, nextFrame ne fait que retourner TRUE : le nombre de mesures est infini.
*/
-(void) startMeasures:(HCFRMeasuresType)measuresToPerforme
{
  NSAssert (colorReference!=nil, @"HCFRManualFullscreenGenerator->startMeasures : no color reference available.\nUse setColorReference before starting measures");

  if (screenUsed != nil)
  {
    [fullScreenWindow setScreen:screenUsed];
    [fullScreenWindow setContentView:unicolorView];
    [fullScreenWindow makeKeyAndOrderFront:self];
  }
  else {
    NSWindow *window = [[NSWindow alloc] init];
    [window setContentSize:NSMakeSize(200, 200)];
    [window setReleasedWhenClosed:true];
    [window setContentView:unicolorView];
    [window makeKeyAndOrderFront:self];
    [window setLevel:NSFloatingWindowLevel];
  }  
}
-(BOOL) nextFrame
{
  return YES;
}
-(void) close
{
  [fullScreenWindow close];
}

#pragma mark Les fonctions de gestion des options
-(void) setScreenUsed:(NSScreen*)newScreenUsed
{
  screenUsed = newScreenUsed;
}
-(NSScreen*) screenUsed
{
  return screenUsed;
}

#pragma mark Gestion du type de données
-(HCFRDataType) currentDataType
{
  return kContinuousDataType;
}

-(double) currentReferenceValue
{
  return 0.0;
}

#pragma mark Surcharges divers
-(NSString*) description
{
  return [[self class] generatorName];
}

#pragma mark IBActions
-(IBAction) colorTypeChanged:(id)sender
{
  switch (selectedColorType) {
    case kRGBColorType:
      [colorControlBox setContentView:rgbColorSetupView];
      break;
    case kGrayscaleColorType:
      [colorControlBox setContentView:grayscaleColorSetupView];
      break;
    case kSaturationColorType:
      [colorControlBox setContentView:saturationColorSetupView];
      break;
  }
  [self colorParameterChanged:sender];
}
-(IBAction) colorParameterChanged:(id)sender
{
  NSColor   *colorToDisplay = nil;

  switch (selectedColorType) {
    case kRGBColorType:
    {
      colorToDisplay = [NSColor colorWithDeviceRed:redValue
                                             green:greenValue
                                              blue:blueValue
                                             alpha:1.0];
      break;
    }
    case kGrayscaleColorType:
    {      
      colorToDisplay = [HCFRColor greyNSColorWithIRE:grayscaleLuminanceValue];
       
      break;
    }
    case kSaturationColorType:
    {
      switch (selectedSaturationColor) {
        case 1:
          colorToDisplay = [HCFRColor redNSColorWithSaturation:saturationValue
                                             forColorReference:colorReference];
          break;
        case 2:
          colorToDisplay = [HCFRColor greenNSColorWithSaturation:saturationValue
                                             forColorReference:colorReference];
          break;
        case 3:
          colorToDisplay = [HCFRColor blueNSColorWithSaturation:saturationValue
                                             forColorReference:colorReference];
          break;
        default:
          NSAssert (false, @"HCFRManualFullscreenGenerator : unknown selected color");
          break;
      }
      
      break;
    }
  }
  if (colorToDisplay != nil)
    [unicolorView setColor:colorToDisplay];
}

@end
