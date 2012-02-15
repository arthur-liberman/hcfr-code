//  ColorHCFR
//  HCFRMainController+toolbarDelegate.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 19/01/08.
//
//  $Rev: 3 $
//  $Date: 2008-01-27 01:11:32 +0000 (Sun, 27 Jan 2008) $
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
#import "HCFRMainController.h"

/*!
    @class
    @abstract    Rassemble les fonctions relatives aux menu et à la toolbar
    @discussion  Cette catégorie de HCFRMainController ne contient que les fonctions relatives
 aux menus et à la toolbar. Le seul interet et d'amméliorer la lisibilité globale du code, en groupant les
 fonctions et les déclarations, et en limitant la taille des fichiers.
*/
@interface HCFRMainController (ToolbarAndMenuCategory)
- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
     itemForItemIdentifier:(NSString *)itemIdentifier
 willBeInsertedIntoToolbar:(BOOL)flag;
- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar;
- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar;

// generation des menus
//-----------------------------------------
// On genere ces menus au lieu de les créer dans interfaceBuilder,
// car les tags des éléments doivent correspondre aux différentes valeurs du type
// HCFRMeasuresType (cf HCFRGenerator.h")
// La maintenabilité du logiciel sera probablement meilleur ainsi.
-(NSMenu*) generateMeasuresMenu;
-(NSMenu*) generateGrayscaleMenu;
-(NSMenu*) generateSaturationsMenu;
-(NSMenu*) generateContrastsMenu;

// auto-activation des menus
//-----------------------------------------
-(NSMenu*) generateContrastsMenu;
@end
