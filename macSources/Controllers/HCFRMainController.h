//  ColorHCFR
//  HCFRSensor.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 31/08/07.
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

#import <Cocoa/Cocoa.h>
#import <list>

#import "SM2DGraphView/SM2DGraphView.h"

#import "HCFRConfiguration.h"

#import "HCFRCalibrationEngine.h"
#import "HCFREngineDelegate.h"
#import "HCFRDataStore.h"

#import "HCFRDataTableController.h"
#import "HCFRPluginsController.h"
#import "HCFRSeriesTableController.h"

#import "HCFRControlView.h"

/*!
@class
@abstract    Controlleur principal de l'application
@discussion  Cette classe est le contrôlleur principal de l'application.
Il est chargé, entre autre, de charger les générateurs et les capteurs, de répondres
aux evenements des menus ...
C'est également le délégué de l'application.
*/
@interface HCFRMainController : NSObject <HCFREngineDelegate>
{
  IBOutlet NSWindow                  *mainWindow;
  IBOutlet NSWindow                  *graphInspectorWindow;
  NSWindow                           *splashWindow;
  
  // le menu mesures qui sera complété lors du lancement
  IBOutlet NSMenuItem                *toolsMenuItem;
  IBOutlet NSMenuItem                *measuresMenuItem;
  
  // la partie garphiques
  IBOutlet NSPanel                   *graphOptionsPanel;
  IBOutlet NSView                    *noGraphOptionsView;
  IBOutlet NSTabView                 *graphsTabView;
  IBOutlet HCFRDataTableController   *dataTableController;
  IBOutlet HCFRSeriesTableController *seriesTableController;
  
  // le status
  IBOutlet NSTextField               *statusTextField;
  IBOutlet NSProgressIndicator       *statusProgressIndicator;
  
  // la fenetre de setup
  IBOutlet NSWindow                  *setupWindow;
  IBOutlet NSTabView                 *setupTabView;
  IBOutlet NSPopUpButton             *colorStandardPopup;
  IBOutlet NSPopUpButton             *generatorsPopupButton;
  IBOutlet NSBox                     *generatorSetupBox;
  IBOutlet NSPopUpButton             *sensorsPopupButton;
  IBOutlet NSPopUpButton             *calibrationFilesPopup;
  IBOutlet NSMatrix                  *calibrationTypeMatrix;
  IBOutlet NSTabView                 *sensorOptionsTabView;
  IBOutlet NSView                    *calibrationView;
  IBOutlet NSTextField               *gammaTextField;
  IBOutlet NSTextField               *delayTextField;
  
  // les varibales pour gérer la toolbar
  NSToolbar                          *mainToolbar;
  
  HCFRDataStore                      *dataStore;
  HCFRPluginsController              *pluginsController;
  HCFRAbstractEngine                 *calibrationEngine;
  HCFRConfiguration                  *configuration;
}

#pragma mark fonctions utilitaires d'interface
/*!
    @function 
    @abstract   Charge les différents profiles de calibration dans le popup associé.
*/
-(void) loadCalibrationFilesInPopup;
/*!
    @function 
    @abstract   Selectionne le capteur founi dans le popup
 @discussion Le capteur fourni est sélectionné dans le popup, et les vues d'option et de calibrations sont chargées.
 Cette fonction ne modifie pas le capteur utilisé par le calibrationEngine. Son but est de préparer la fenêtre de setup
 avant affichage
    @param      sensor Le capteur à sélectionner (celui du calibration engine normalement)
    @result     YES si ça a marché, NO sinon (ce qui indique que le capteur n'a pas été trouvé dans le popup.
*/
-(bool) selectSensorInPopup:(HCFRSensor*)sensor;
/*!
    @function 
    @abstract   Met en place les vues de préférence et de calibration pour le capteur fourni
    @param      selectedSensor Le capteur dont les vues doivent être installées
*/
-(void) setSetupViewForSensor:(HCFRSensor*)selectedSensor;
/*!
    @function 
    @abstract   Selectionne le generateur founi dans le popup
    @discussion Le generateur fourni est sélectionné dans le popup, et dz vue d'option est chargée.
 Cette fonction ne modifie pas le générateur utilisé par le calibrationEngine. Son but est de préparer la fenêtre de setup
 avant affichage
    @param      generator Le générateur à sélectionner
    @result     YES si ça a marché, NO sinon (ce qui indique que le capteur n'a pas été trouvé dans le popup.
*/
-(bool) selectGeneratorInPopup:(HCFRGenerator*)generator;
/*!
    @function 
    @abstract   Met en place la vue de préférence du générateur fourni
    @param      selectedGenerator Le générateur dont les vues doivent être mises en place
*/
-(void) setSetupViewForGenerator:(HCFRGenerator*)selectedGenerator;
/*!
    @function 
    @abstract   Selectionne un profile de calibration dans le popup associé.
    @discussion Si advertise est à YES, alors une action "calibrationFileChanged est lancée.
 C'est naze, il faudrait retravailler ça. TODO.
*/
-(bool) selectCalibrationFileWithFilename:(NSString*)filename advertise:(BOOL)advertise;

#pragma mark IBActions
-(IBAction) startSensorCalibration:(id)Sender;
-(IBAction) startMeasures:(id)Sender;
-(IBAction) startMesureFromMenu:(NSMenuItem*)menuItem;
-(IBAction) startMesureFromTooblarItem:(NSToolbarItem*)toolbarItem;
-(IBAction) startContinuousMesure:(id)Sender;
-(IBAction) cancelCalibration:(id)sender;
-(IBAction) showSetupSheet:(id)sender;
-(IBAction) endSetupSheet:(id)sender;
-(IBAction) sensorChanged:(id)sender;
-(IBAction) generatorChanged:(id)sender;
-(IBAction) colorReferenceChanged:(NSPopUpButton*)sender;
-(IBAction) gammaChanged:(NSTextField*)sender;
-(IBAction) calibrationModeChanged:(NSMatrix*)sender;
-(IBAction) calibrationFileChanged:(NSPopUpButton*)sender;
-(IBAction) save:(id)sender;
-(IBAction) load:(id)sender;
-(IBAction) updateProfiles:(id)sender;
-(IBAction) showHideGraphInspector:(id)sender;
-(IBAction) showAboutPanel:(id)sender;
-(IBAction) startTool:(id)sender;
-(IBAction) createNewDataStore:(id)sender;

#pragma mark actions divers
-(void) copy:(id)sender;
-(void) startMeasuresForMask:(HCFRMeasuresType)measures;
-(void) loadCalibrationFile:(NSString*)filename;
/*!
    @method     
    @abstract   Crée une série si nécessaire
    @discussion Vérifie si une nouvelle série doit être crée, et la crée si nécessaire.
 La nouvelle série est automatiquement sélectionnée.
 Si l'utilisateur à demandé à avoir le choix dans les préférences, une alerte lui est
 présentée pour savoir quoi faire.
*/
-(void) createNewSerieIfNeeded;

#pragma mark chargement/sauvegarde du dataStore
-(BOOL) loadFileAtPath:(NSString*)path;
-(BOOL) saveToPath:(NSString*)path;
-(void) setDataStore:(HCFRDataStore*)newDataStore;

#pragma mark tabView delegate
-(void) tabView:(NSTabView *)tabView didSelectTabViewItem: (NSTabViewItem *)tabViewItem;

#pragma mark KVO listener
-(void) observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context;

#pragma mark detelagué du calibration engine
-(void) engineStartMeasures:(HCFRAbstractEngine*)startingEngine;
-(void) engineMeasuresEnded:(HCFRAbstractEngine*)endingEngine;

#pragma mark Gestion du splash
-(void) showSplash;
-(void) closeSplash;

#pragma mark fonctions callback pour les open et save panels
-(void) importFileSheetDidEnd:(NSOpenPanel *)panel returnCode:(int)returnCode  contextInfo:(void*)contextInfo;
@end
