//  ColorHCFR
//  HCFRSeriesTableController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 15/05/08.
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
#import "HCFRDataStore.h"

/*!
    @class
    @abstract    Controller pour la table des séries
*/
@interface HCFRSeriesTableController : NSObject {
  IBOutlet  NSTableView   *seriesTableView;
  
  HCFRDataStore           *dataStore;
}
-(void) setDataStore:(HCFRDataStore*)newDataStore;

#pragma mark Fonctions pour les bindings
-(BOOL) currentSerieIsEditable;
-(void) setCurrentNotes:(NSAttributedString*) newNotes;
-(NSAttributedString*) currentNotes;

#pragma mark Actions IB
-(IBAction) addNewSerie:(id)sender;
-(IBAction) deleteSerie:(id)sender;
-(IBAction) toogleReferenceForCurrent:(id)sender;

#pragma mark Fonctions utilitaires
/*!
    @function 
    @abstract   Crée une nouvelle série avec un nom spécifié
    @discussion Contrairement à l'action IB addNewSerie:, cette fonction ne sélectionne pas la nouvelle série,
 et n'entre pas en mode d'édition.
    @param      name Le nom de la nouvelle série.
    @result     La nouvelle série est retournée.
*/
-(HCFRDataSerie*) addNewSerieWithName:(NSString*)name;

#pragma mark Tableview data source
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;

#pragma mark Tableview delegate
- (void)tableViewSelectionDidChange:(NSNotification *)aNotification;
@end
