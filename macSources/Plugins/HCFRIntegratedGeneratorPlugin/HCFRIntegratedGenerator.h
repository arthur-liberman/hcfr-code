//  ColorHCFR
//  HCFRIntegratedGenerator.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
//
//  $Rev: 80 $
//  $Date: 2008-07-24 21:42:47 +0100 (Thu, 24 Jul 2008) $
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


#import <Cocoa/Cocoa.h>
#import <HCFRPlugins/HCFRGenerator.h>
#import <HCFRPlugins/HCFRFullScreenWindow.h>
#import <HCFRPlugins/HCFRUnicolorView.h>
#import <HCFRPlugins/HCFRColor.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import "HCFRANSIContrastView.h"

/*!
    @class
    @abstract    Générateur de frame informatique. Idéal pour calibrer un couple projecteur/ordinateur
    @discussion  (comprehensive description)
*/
@interface HCFRIntegratedGenerator : HCFRGenerator {
  IBOutlet NSView         *configurationView;
  IBOutlet NSPopUpButton  *screensPopupButton;
  
  HCFRFullScreenWindow    *fullScreenWindow;
  HCFRUnicolorView        *unicolorView;
  HCFRANSIContrastView    *ansiContrastView;
  
  BOOL                    abortRequested;
  
  // variables de l'automate à états
  int   currentStep, stepFrameIndex; // les état de l'automate
  bool  stepDidEnd; // pour savoir si l'étape actuelle est finie

  int                 measuresToPerformeBitmask;
  HCFRColorReference  *colorReference; // la reference de couleur à utiliser pour la série de mesure

  // Les options
  unsigned short   stepForScaleMeasures; // le pas pour les mesures d'echelle (luminosité, saturation)
                              // Il faut que ce soit un diviseur de 100 !
  unsigned short   stepForDetailedScaleMeasures; // le pas pour les mesures d'echelle détaillées (luminosité near black ou near white)
}

-(IBAction) selectedScreenChanged:(NSPopUpButton*)sender;

-(double) currentReferenceValue;

#pragma mark KVC
-(int) screenCoverage;
-(void) setScreenCoverage:(int)newCoverage;

#pragma mark Les étapes du générateur
-(bool) grayscaleStep;
-(bool) nearBlackStep;
-(bool) nearWhiteStep;
-(bool) redSaturationStep;
-(bool) greenSaturationStep;
-(bool) blueSaturationStep;
-(bool) fullscreenContrastStep;
-(bool) ansiContrastStep;
-(bool) primaryComponentsStep;
-(bool) secondaryComponentsStep;
@end
