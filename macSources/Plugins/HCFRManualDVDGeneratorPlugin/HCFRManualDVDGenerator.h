//  ColorHCFR
//  HCFRManualDVDGenerator.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 22/11/07.
//
//  $Rev: 67 $
//  $Date: 2008-07-20 20:02:36 +0100 (Sun, 20 Jul 2008) $
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
#import "HCFRPlugins/HCFRGenerator.h"
#import <HCFRPlugins/HCFRUtilities.h>

/*!
    @class
    @abstract    Utilisation d'un lecteur DVD pour générer des images.
    @discussion  (comprehensive description)
*/
@interface HCFRManualDVDGenerator : HCFRGenerator {
  // variables de l'automate à états
  int   currentStep, stepFrameIndex; // les état de l'automate
  bool  stepDidEnd; // pour savoir si l'étape actuelle est finie
  int   measuresToPerformeBitmask;  

  bool  abortRequested;
}

#pragma mark Identification du générateur et Gestion du panneau de configuration
+(NSString*) generatorName;
+(NSString*) generatorId;
-(BOOL) hasASetupView;

#pragma mark Control du générateur
-(BOOL) isGeneratorAvailable;
-(void) startMeasures:(HCFRMeasuresType)measuresToPerforme;
-(BOOL) nextFrame;
-(void) close;

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

#pragma mark Gestion du type de données
-(HCFRDataType) currentDataType;
-(double) currentReferenceValue;
@end
