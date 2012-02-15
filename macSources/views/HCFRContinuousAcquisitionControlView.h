//  ColorHCFR
//  HCFRContinuousAcquisitionControlView.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 21/01/11.
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

#import <BGHUDAppKit/BGHUDAppKit.h>

#import "HCFRGraphController.h"
#import "HCFRDataSerie.h"


/*!
 @class
 @abstract    Fenêtre de retour pendant les mesures
 @discussion  Cette classe gère une fenêtre affichant un état pour fournir un feedback à l'utilisateur
 tout en assombrissant l'écran.
 Elle est utilisée par le continuousAcquisitionEngine.
 */
@interface HCFRContinuousAcquisitionControlView : NSView {
  IBOutlet  NSBox       *generatorControlViewContainer;
  IBOutlet  NSTabView   *graphicsTabView;
  
  IBOutlet  NSTextField *currentStatusTextField;
  
  // les graphs
  NSMutableArray  *graphicsArray;
}

-(void) setGeneratorControlView:(NSView*)newControlView;
-(void) setDataSerie:(HCFRDataSerie*)newDataSerie;
-(void) addGraph:(HCFRGraphController*)newGraph;
@end
