//  ColorHCFR
//  HCFRPluginsController.h
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 18/09/07.
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
#import <HCFRPlugins/HCFRPlugin.h>
#import <HCFRPlugins/HCFRSensor.h>
#import <HCFRPlugins/HCFRGenerator.h>
#import "HCFRDataStore.h"

#define kHCFRPluginInstanciationFailureException @"HCFRPluginInstanciationFailureException"

/*!
    @class
    @abstract    Gestion des plugins
    @discussion  Cette classe charge les différents plugins et les classe selon leur type :
 - les plugins graphiques
 - les plugins capteurs
 - les plugins générateurs
*/
@interface HCFRPluginsController : NSObject {
  NSMutableArray      *pluginsArray;
  
  NSMutableArray      *graphicPlugins;
  NSMutableArray      *sensorPlugins;
  NSMutableArray      *generatorPlugins;
  NSMutableArray      *toolsPlugins;
  
  NSMutableDictionary *sensorsInstances;
  NSMutableDictionary *generatorsInstances;
  NSMutableDictionary *toolsInstances;
}
-(HCFRPluginsController*) init;
-(void) loadAllPlugins;

-(NSArray*) plugins;
-(NSArray*) graphicPlugins;
/*!
   @function 
   @abstract   Retourne un tableau contenant les classes des capteurs fournis par les plugins.
   @discussion Les classes ne sont pas instanciées. Pour instancier un capteurs, utilisez la methode sensorForId:
   du pluginManager.
   @result     Un tableau contenant les classes des différents capteurs
*/
-(NSArray*) sensorPlugins;
/*!
   @function 
   @abstract   Retourne un tableau contenant les classes des générateurs fournis par les plugins.
   @discussion Les classes ne sont pas instanciées. Pour instancier un generateur, utilisez la methode generatorForId:
   du pluginManager.
   @result     Un tableau contenant les classes des différents générateurs
*/
-(NSArray*) generatorPlugins;
/*!
    @function 
    @abstract   Retourne un tableau contenant les classes des outils fournis par les plugins.
    @discussion Les classes ne sont pas instanciées. Pour instancier un outil, utilisez la methode startTool:
 du pluginManager. L'outil sera automatiquement instancié si necessaire, puis executé.
    @result     Un tableau contenant les classes des différents outils
*/
-(NSArray*) toolsPlugins;

/*!
   @function 
   @abstract   Fourni une instance du capteur correspondant à l'ID fourni.
   @discussion Cette fonction recherche et instancie le capteur possédant l'ID fourni.
   L'instance retournée est autoreleasée, une nouvelle instance est renvoyée à chaque appel.
   Une exception kHCFRPluginInstanciationFailureException est levée en cas d'echec de l'instanciation.
   @param      sensorId L'ID du capteur souhaité.
   @result     L'instance crée, ou nil si aucun capteur n'a été trouvé pour l'ID donné.
*/
-(HCFRSensor*) sensorWithId:(NSString*)sensorId;
/*!
    @function 
    @abstract   Fourni une instance du generateur correspondant à l'ID fourni.
    @discussion Cette fonction recherche et instancie le générateur possédant l'ID fourni.
 L'instance retournée est autoreleasée, une nouvelle instance est renvoyée à chaque appel.
 Une exception kHCFRPluginInstanciationFailureException est levée en cas d'echec de l'instanciation.
    @param      generatorId L'ID du générateur souhaité
    @result     L'instance crée, ou nil si aucun generateur n'a été trouvé pour l'ID donné.
*/
-(HCFRGenerator*) generatorWithId:(NSString*)generatorId;
/*!
    @function 
    @abstract   Execute un outil spécifié par sa classe.
    @discussion Si l'outil n'a pas été instancié, il le sera. Un message startTool sera ensuite envoyé.
 Les outils instanciés seront concervés en cache.
    @param      toolClass La classe de l'outil à démarrer.
*/
-(void) startTool:(Class)toolClass;
@end
