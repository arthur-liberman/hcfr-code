//  ColorHCFR
//  HCFRManualFullscreenGenerator.h
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

#import <Cocoa/Cocoa.h>

#import <HCFRPlugins/HCFRFullScreenWindow.h>
#import <HCFRPlugins/HCFRUnicolorView.h>

#import "HCFRGenerator.h"

/*!
    @class
    @abstract    Générateur de couleurs plein écran manuel
    @discussion  Ce générateur permet d'afficher une couleur unie en plein écran sur n'importe quel écran du système.
 L'écran utilisé pour l'affichage est également protégé contre l'affichage d'éléments perturbateurs (alertes d'autres
 applications par exemple).
*/
@interface HCFRManualFullscreenGenerator : HCFRGenerator {
  IBOutlet HCFRUnicolorView   *controlView;
  IBOutlet NSBox              *colorControlBox;
  
  IBOutlet NSView             *grayscaleColorSetupView;
  IBOutlet NSView             *saturationColorSetupView;
  IBOutlet NSView             *rgbColorSetupView;

@private
  int   selectedColorType;
  float redValue;
  float greenValue;
  float blueValue;
  float grayscaleLuminanceValue;
  int   selectedSaturationColor;
  float saturationValue;
  
  HCFRFullScreenWindow        *fullScreenWindow;
  HCFRUnicolorView            *unicolorView;
  
  NSScreen                    *screenUsed;
  
  HCFRColorReference  *colorReference; // la reference de couleur à utiliser pour la série de mesure
}

@property int   selectedColorType;
@property float redValue;
@property float greenValue;
@property float blueValue;
@property float grayscaleLuminanceValue;
@property int   selectedSaturationColor;
@property float saturationValue;

-(void) setScreenUsed:(NSScreen*)newScreenUsed;
-(NSScreen*) screenUsed;

#pragma mark IBActions
-(IBAction) colorTypeChanged:(id)sender;
-(IBAction) colorParameterChanged:(id)sender;

@end
