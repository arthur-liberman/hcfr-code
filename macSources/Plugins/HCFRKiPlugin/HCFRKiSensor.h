//  ColorHCFR
//  HCFRKiSensor.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 15/09/07.
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


#import <Cocoa/Cocoa.h>
#import <HCFRPlugins/HCFRSensor.h>
#import <HCFRPlugins/HCFRUtilities.h>
#import "HCFRKiDevice.h"

// les constantes de commande du capteur.

/*!
    @class
    @abstract    La classe qui gère la sonde HCFR
    @discussion  Cette classe permet d'obtenir des données depuis une sonde HCFR branchée en USB.
*/
@interface HCFRKiSensor : HCFRSensor <HCFRKiDeviceStatusListener> {
  IBOutlet NSButton       *fastMeasuresButton;
  IBOutlet NSView         *setupView;
  IBOutlet NSTextField    *softwareVersionTextField;
  IBOutlet NSTextField    *hardwareVersionTextField;
  IBOutlet NSPopUpButton  *sensorToUsePopup;
  IBOutlet NSPopUpButton  *interleavedMeasuresPopup;
  
  HCFRKiDevice  *kiDevice;
  
  // Les options
/*  short   sensorToUse;
  BOOL    fastMeasureMode;
  BOOL    performeRGBMeasure;
  BOOL    performeWhiteMeasure;*/
  
}
-(void) refresh;
-(void) updateSensorToUsePopupForHardwareVersion:(NSString*)version;
#pragma mark Actions de l'interface graphique
-(IBAction) optionChanged:(id)sender;

/*!
    @function 
    @abstract   Décode une chaine de caractères retournée par la sonde HCFR.
    @discussion Cette fonction décode la chaine de caractères retournée par la sonde HCFR,
 et stoque les valeurs RGB dans le tableau d'entiers reçus en argument.
    @param      answer La chaine de caractères retournée par la sonde
    @param      toBuffer Un tableau de 3 entiers (int) dans lesquels seront stockés les trois composantes.
    @result     (description)
*/
-(void) decodeKiAnswer:(char*)answer toBuffer:(unsigned int[])RGB;

#pragma kiDeviceListener
-(void) kiSensorAvailabilityChanged;
@end
