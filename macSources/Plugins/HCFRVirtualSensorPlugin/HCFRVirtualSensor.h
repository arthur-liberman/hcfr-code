//  ColorHCFR
//  HCFRVirtualSensor.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 27/11/07.
//
//  $Rev: 122 $
//  $Date: 2008-11-18 15:05:33 +0000 (Tue, 18 Nov 2008) $
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
#import <HCFRPlugins/HCFRPlugins.h>

/*!
    @class
    @abstract    Capteur virtuel, fournissant des données bidon
    @discussion  Pratique pour faire des tests sans avoir à brancher la sonde
*/
@interface HCFRVirtualSensor : HCFRSensor {
  IBOutlet NSView       *setupView;
  IBOutlet NSTextField  *dataStoreStatus;
  IBOutlet NSButton     *alertBeforeValueButton;
  
  HCFRDataSerie   *simulatedDataSerie;
  HCFRColor       *nextMeasure;
}

#pragma mark fonction divers
+(NSString*) sensorName;
+(NSString*) sensorId;

#pragma mark Gestion du capteur
-(BOOL) isAvailable;
-(BOOL) needsCalibration;

#pragma mark Gestion du panneau de configuration
-(BOOL) hasASetupView;
-(NSView*) setupView;

#pragma mark Implementation du protocol DataSource
-(HCFRColor*) currentMeasure;

#pragma mark Actions
-(IBAction)loadDataFromFile:(id)sender;

@end
