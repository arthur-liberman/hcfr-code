//  ColorHCFR
//  HCFRContinuousAcquitisionEngine.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 28/12/10.
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
#import <HCFRPlugins/HCFRSensor.h>
#import <HCFRPlugins/HCFRColorReference.h>
#import "HCFRPluginsController.h"
#import "HCFRAbstractEngine.h"
#import "HCFRManualFullscreenGenerator.h"
#import "HCFRDataSerie.h"
#import "HCFRContinuousAcquisitionControlView.h"

/*!
 @class
 @abstract    La moteur d'acquisition continue
 @discussion  Cette classe est chargée de gérer l'acquisition continue de données.
 Contrairement au moteur de calibration, elle utilise un générateur spécifique, et ne stock pas
 ses données dans le data store.
 */
@interface HCFRContinuousAcquisitionEngine : HCFRAbstractEngine {
  IBOutlet HCFRContinuousAcquisitionControlView *controlView;
  
  HCFRSensor    *sensor;
  HCFRDataSerie *dataSerie;
  
  NSString      *engineStatus;
  BOOL          abortRequested;
  BOOL          pauseRequested;

  NSLock        *measureLock; // Pour éviter des mesures concurentes.
  
  // configuration des mesures
  HCFRManualFullscreenGenerator *generator; // le générateur, si on en utilise un. Peut être Nil si on n'en utilise pas.
  float                         pauseBetweenMeasures; // durée en seconde de la pause entre deux mesures
}

@property(retain) HCFRDataSerie* dataSerie;

- (HCFRContinuousAcquisitionEngine*) initWithPluginController:(HCFRPluginsController*)pluginsController;

-(HCFRGenerator*) generator;
-(HCFRSensor*) sensor;
-(void) setSensor:(HCFRSensor*)newSensor;
/*!
 @function 
 @abstract   Lancement des mesures
 @discussion Cette procédure lance les mesures en continue.
 @result     YES si les mesures ont effectivement été lancées, NO si le lancement à été annulé.
 */
-(bool) startMeasuresWithColorReference:(HCFRColorReference*)colorReference;

/*!
 @function 
 @abstract   Interromp les mesures
 @discussion Cette procédure enregistre une demande d'interruption des mesures.
 L'interruption n'est pas immédiate, et ne sera pas effective lors du retour de cette fonction.
 */
-(IBAction) abortMeasures:(id)sender;

/*!
 @function 
 @abstract   Met les mesures en pause
 @discussion Cette procédure met en pause la boucle de mesure ou interromp la pause selon l'état du
 boutton appelant.
 */
-(IBAction) toggleMeasuresPause:(NSButton*)sender;

/*!
 @function 
 @abstract   Indicates wether the engine is running
 @discussion This function returns true if the engine is currently running, false if it is not.
 */
-(bool) isRunning;

@end
