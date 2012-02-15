//  ColorHCFR
//  HCFRPlugin.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 09/02/08.
//
//  $Rev: 103 $
//  $Date: 2008-08-28 18:39:32 +0100 (Thu, 28 Aug 2008) $
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
#import <HCFRPlugins/HCFRUtilities.h>

/*!
    @class
    @abstract    Classe de base pour les plugins
    @discussion  Cette classe doit être dérivée par les plugins. Chaque plugin pourra fournir
 un générateur, un capteur et un outil, aucun n'étant obligatoire.
 Pour fournir un generator, il sufft de surcharger la fonction generatorClass pour retourner une instance du générateur
 Pour fournir un capteur, il suffit de surcharger la fonction sensorClass pour retourner une instance du capteur
 Pour fournin un outil, il suffit de surcharer la fonction toolClass pour retourner une insatnce de l'outil
*/
@interface HCFRPlugin : NSObject {
}
/*!
   @function 
   @abstract   Initialisation du plugin
   @result     le plugin initialisé
*/
-(HCFRPlugin*) init;
  /*!
    @function 
    @abstract   Fournis le nom du plugin sous forme de NSString autoreleasé.
    @discussion Pour le moment, cette fonction n'est pas utilisée.
    @result     Le nom du plugin
*/
-(NSString*) pluginName;
/*!
    @function 
    @abstract   Accesseur pour le capteur fourni par le plugin
    @discussion Cette fonction est à surcharger pour une class descendant de HCFRSensor.
    @result     La class du capteur si il y en a un, nil sinon.
*/
-(Class) sensorClass;
/*!
   @function 
   @abstract   Accesseur pour le générateur fourni par le plugin
   @discussion Cette fonction est à surcharger pour fournir la class du générateur fourni par le plugin
   @result     La class du générateur si il y en a un, nil sinon.
*/
-(Class) generatorClass;
/*!
   @function 
   @abstract   Accesseur fournissant un NSArray contenant les classes d'outils fournis par le plugin
   @discussion Cette fonction est à surcharger pour donner un accès aux outils fournis par le plugin
   @result     Un enumerateur permettant de lister les classes des outils fournis par le plugin.
*/
-(NSArray*) toolsClassArray;
@end
