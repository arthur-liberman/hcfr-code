//  ColorHCFR
//  HCFRManualDVDGenerator.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 22/11/07.
//
//  $Rev: 64 $
//  $Date: 2008-07-17 10:47:55 +0100 (Thu, 17 Jul 2008) $
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
#import "HCFRKiDevice.h"
#import "HCFRIrProfilesRepositoryListener.h"
#import "HCFRKiGeneratorIRProfile.h"

typedef enum
{
  DVDMenuSettings,
  DVDMenuAdvancedSettings,
  DVDMenuUnknown
} DVDMenus;

/*!
    @class
    @abstract    Utilisation d'un lecteur DVD pour générer des images.
    @discussion  (comprehensive description)
*/
@interface HCFRKiGenerator : HCFRGenerator <HCFRIrProfilesRepositoryListener> {
  IBOutlet  NSView          *setupView;
  IBOutlet  NSPopUpButton   *irProfilesPopup;
  IBOutlet  NSTextField     *authorTextField;
  IBOutlet  NSTextView      *descriptionTextView;
  
  // variables de l'automate à états
  int                       currentStep, stepFrameIndex; // les état de l'automate
  bool                      stepDidEnd; // pour savoir si l'étape actuelle est finie
  int                       measuresToPerformeBitmask;
  HCFRKiGeneratorIRProfile  *profileToUse;   // le profile sera chargé au lancement du générateur,
                                             //et sera libéré à la fin de la séquence de mesures

  HCFRKiDevice  *kiDevice;

  bool  abortRequested;
  
  // on doit naviguer dans les menus du DVD.
  // Sauf que on peut sauter des étapes
  // Donc, en stock la position (nb de code "flêche gauche" envoyé => xPosition,
  // nb de code "flêche bas" envoyé" => yPosition, et on a une fonction pour aller à la position nécessaire.
  unsigned short xPosition, yPosition, currentMenu;
}

#pragma mark Identification du générateur et Gestion du panneau de configuration
+(NSString*) generatorName;
+(NSString*) generatorId;
-(BOOL) hasASetupView;

#pragma mark Les actions IB
-(IBAction) irProfileChanged:(id)sender;

#pragma mark Control du générateur
-(BOOL) isGeneratorAvailable;
-(void) startMeasures:(HCFRMeasuresType)measuresToPerforme;
-(BOOL) nextFrame;
-(void) close;

#pragma mark Les étapes du générateur
-(bool) sensorCalibrationStep;
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
-(void) finishCurrentStep;

#pragma mark Gestion du type de données
-(HCFRDataType) currentDataType;
-(double) currentReferenceValue;

#pragma mark implémentation du protocol HCFRIrProfilesRepositoryListener
-(void) irProfilesListChanged;

#pragma mark gestion de la position dans les menus du DVD
-(BOOL) gotoPositionX:(int)x Y:(int)y inMenu:(DVDMenus)menu;
@end
