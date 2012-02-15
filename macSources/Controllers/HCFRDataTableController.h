//  ColorHCFR
//  HCFRDataTableController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 10/09/07.
//
//  $Rev: 96 $
//  $Date: 2008-08-19 12:35:23 +0100 (Tue, 19 Aug 2008) $
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

#import <HCFRPlugins/HCFRDataSerie.h>
#import <HCFRPlugins/HCFRDataSerieListener.h>
#import <HCFRPlugins/HCFRUtilities.h>

/*!
    @enum 
    @abstract   Modes d'affichage
    @discussion Cette énumération liste les modes d'affichage disponibles. ATTENTION, les valeurs doivent être
 les mêmes que les tags associées aux éléments du popup permettant de choisir le mode.
    @constant   (name) (description)
*/
typedef enum {
  kSensorDisplayMode = 0,
  kRGBDisplayMode = 1,
  kXYZDisplayMode = 2,
  kxyzDisplayMode = 3,
  kxyYDisplayMode = 4
} HCFRDisplayMode;

/*!
    @class
    @abstract    Controlleur pour les tables d'affichage des valeurs
    @discussion  Ce controller est chargé d'afficher les valeurs enregistrées dans les tables prévues à cet
 effet.
*/
@interface HCFRDataTableController : NSObject <HCFRDataSerieListener> {
  IBOutlet NSPopUpButton  *dataTypesPopup;
  IBOutlet NSTableView    *grayscaleTable;
  IBOutlet NSTextField    *maxLuminosityTextField;
  IBOutlet NSTextField    *minLuminosityTextField;
  IBOutlet NSTextField    *onOffContrastTextField;
  IBOutlet NSTextField    *ansiContrastTextField;
    
  HCFRDataSerie         *currentSerie;
  int                   currentMode;
  HCFRDataType          currentDataType;
  
  HCFRColorReference    *colorReference;
}
-(HCFRDataTableController*) init;

-(void) setDataSerie:(HCFRDataSerie*)newSerie;

#pragma mark Actions IB
- (IBAction)changeDataToDisplay:(NSPopUpButton*)sender;
- (IBAction)changeDisplayMode:(NSPopUpButton*)sender;

-(void) reloadDataFromCurrentSerie;
-(void) reloadFullscreenContrastData;
-(void) reloadANSIContrastData;

#pragma mark TableView data source
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;

-(void) setColorReference:(HCFRColorReference*)newReference;

#pragma mark fonctions utilitaires
/*!
    @function 
    @abstract   indique si des données peuvent être copiées
    @discussion Des données peuvent être copiées si une ou plusieurs lignes existent dans la table.
    @result     YES si des données peuvente être copiées, NO sinon
*/
-(BOOL)canCopy;
/*!
    @function 
    @abstract   Copie les données dans le presse papier général
    @discussion Si des lignes sont sélectionnées, elles sont copiées dans le presse papier sour forme de text et de texte tabulé.
 Si il n'y a pas de lignes sélectionnées, toutes les lignes sont copieées.
*/
-(void) copyToPasteboard;
@end
