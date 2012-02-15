//  ColorHCFR
//  HCFRGeneratorDelegate.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 30/08/07.
//
//  $Rev: 2 $
//  $Date: 2008-01-20 21:40:53 +0000 (Sun, 20 Jan 2008) $
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
    @protocol
    @abstract    Délégué d'un générateur
    @discussion  Le délégué du générateur est une classe qui sera tenu au courant des changement
d'états du générateur par des appels de methode. Par exemple, un passage à l'étape suivante fera l'objet d'un
appel de methode au niveau du délégué.
*/
@protocol HCFRGeneratorDelegate
/*!
    @function 
    @abstract   Le générateur à changé d'étape
    @discussion Cette fonction sera appelée lors d'un changement d'étape du générateur.
    @param      generator Un pointeur sur le générateur qui à généré l'appel.
Le parametre est de type id pour éviter une référence circulaire entre
HCFRGeneratorDelegate et HCFGenerator. C'est pas top, mais je n'ai pas trouvé d'autres solutions.
*/
-(void) generatorDidLoadFrame:(byref in id)sender;
@end
