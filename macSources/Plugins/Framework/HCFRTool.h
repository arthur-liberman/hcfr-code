//  ColorHCFR
//  HCFRTool.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 01/05/08.
//
//  $Rev: 33 $
//  $Date: 2008-05-05 21:30:42 +0100 (Mon, 05 May 2008) $
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

/*!
    @class
    @abstract    Classe de base pour les outils
    @discussion  Cette classe est la classe de base des outils. Tous les outils fournis par les
 plugins doivent dériver de cette classe
 L'outil sera instancié lors de sa première utilisation.
 A chaque utilisation, la fonction startTool sera appelée.
*/
@interface HCFRTool : NSObject {

}
+(NSString*) toolName;

/*!
    @function 
    @abstract   Lancement de l'outil
    @discussion Cette fonction sera appelée lorsque l'outil sera selectionné dans le menu.
 Il doit alors executer l'action pour laquelle il à été prévue.
*/
-(void) startTool;
@end
