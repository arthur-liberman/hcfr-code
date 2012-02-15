//  ColorHCFR
//  HCFRPluginsController.mm
// -----------------------------------------------------------------------------
//  Created by Jérôme Duquennoy on 18/09/07.
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

#import "HCFRPluginsController.h"
#import "HCFRRgbGraphController.h"
#import "HCFRCIEChartController.h"
#import "HCFRSatLumGraphController.h"
#import "HCFRLuminanceGraphController.h"
#import "HCFRGammaGraphController.h"
#import "HCFRTemperatureGraphController.h"
#import "HCFRSatShiftGraphController.h"

#import "HCFRContinuousLuminanceGraphController.h"

#import "HCFRIntegratedGenerator.h"
#import "HCFRManualDVDGenerator.h"
#import "HCFRKiGenerator.h"
#import <HCFRPlugins/HCFRTool.h>

@implementation HCFRPluginsController
-(HCFRPluginsController*) init
{
  [super init];
  
  pluginsArray = [[NSMutableArray alloc] initWithCapacity:5];
    
  graphicPlugins = [[NSMutableArray alloc] initWithCapacity:5];
  sensorPlugins = [[NSMutableArray alloc] initWithCapacity:2];
  generatorPlugins = [[NSMutableArray alloc] initWithCapacity:3];
  toolsPlugins =  [[NSMutableArray alloc] initWithCapacity:3];
  
  sensorsInstances = [[NSMutableDictionary alloc] init];
  generatorsInstances = [[NSMutableDictionary alloc] init];
  toolsInstances = [[NSMutableDictionary alloc] init];

  [self loadAllPlugins];
  
  return self;
}

-(void) dealloc
{
  [pluginsArray release];
  
  [graphicPlugins release];
  [sensorPlugins release];
  [generatorPlugins release];
  [toolsPlugins release];
  
  [toolsInstances release];
  [sensorsInstances release];
  [generatorsInstances release];
  
  [super dealloc];
}

-(void) loadAllPlugins
{
  NSString* folderPath = [[NSBundle mainBundle] builtInPlugInsPath];
  if (folderPath) {
    NSEnumerator* enumerator = [[NSBundle pathsForResourcesOfType:@"hcfrPlugin"
                                                      inDirectory:folderPath] objectEnumerator];
    NSString* pluginPath;
    while ((pluginPath = [enumerator nextObject])) {
      NSBundle*     pluginBundle = [NSBundle bundleWithPath:pluginPath];
      NSDictionary* pluginDict = [pluginBundle infoDictionary];
      NSString*     pluginClassName = [pluginDict objectForKey:@"NSPrincipalClass"];
      
//      NSLog (@"HCFRPluginManager : loading plugin %@", pluginClassName);

      if (!pluginClassName) {
        NSLog (@"HCFRPluginManager : Unable to load plugin %@ : no principal class found", [pluginPath lastPathComponent], pluginClassName);
        continue;
      }
      
      Class pluginClass = NSClassFromString(pluginClassName);
      // on vérifie que la classe n'existe pas déjà.
      if (pluginClass)
      {
        NSLog (@"HCFRPluginManager : Unable to load plugin %@ : Class %@ already exists", [pluginPath lastPathComponent], pluginClassName);
        continue;
      }
      
      // si la classe n'existe pas, on la charge.
      pluginClass = [pluginBundle principalClass];
      if (!pluginClass)
      {
        NSLog (@"HCFRPluginManager : Unable to load plugin %@ : principal class %@ not found", [pluginPath lastPathComponent], pluginClassName, pluginClass);
        continue;
      }
      
      // on instancie la classe
      HCFRPlugin *currentPlugin = [(HCFRPlugin*)[pluginClass alloc] init];
      if (! currentPlugin)
      {
        NSLog (@"HCFRPluginManager : Unable to load plugin %@ : could not init principal class %@", [pluginPath lastPathComponent], pluginClass);
        continue;
      }
      
      // on vérifie qu'elle dérive bien de la classe de base HCFRPlugin
      if (![currentPlugin  isKindOfClass:[HCFRPlugin class]])
      {
        NSLog (@"HCFRPluginManager : Unable to load plugin %@ : principal class do not inherit from HCFRPlugin", [pluginPath lastPathComponent]);
        continue;
      }

      [pluginsArray addObject:currentPlugin];
      
      [currentPlugin release];
    }
  }  
  
  //-----------------------------------
  // On charge les class des capteurs et des générateurs
  // Ils seront instanciés lors de leur première utilisation.
  //-----------------------------------
  NSEnumerator  *pluginsEnumerator = [pluginsArray objectEnumerator];
  HCFRPlugin    *currentPlugin;
  while (currentPlugin = [pluginsEnumerator nextObject])
  {
    //-----------------------------------
    // Le capteur du plugin (si il y en a un)
    //-----------------------------------
    Class sensorClass = [currentPlugin sensorClass];
    if (sensorClass != nil)
    {
      //      NSLog (@"HCFRPluginManager : loading sensor for plugin %@", [currentPlugin class]);
      if ([sensorClass isKindOfClass:[HCFRSensor class]])
      {
        NSLog (@"HCFRPluginManager : Sensor do not inherit from HCFRSensor for plugin %@", [currentPlugin class]);
        continue;
      }
      [sensorPlugins addObject:sensorClass];
    }

    //-----------------------------------
    // Le générateur du plugin (si il y en a un)
    //-----------------------------------
    Class generatorClass = [currentPlugin generatorClass];
    if (generatorClass != nil)
    {
//      NSLog (@"HCFRPluginManager : loading generator for plugin %@", [currentPlugin class]);
      if ([generatorClass isKindOfClass:[HCFRGenerator class]])
      {
        NSLog (@"HCFRPluginManager : Generator do not inherit from HCFRGenerator for plugin %@", [currentPlugin class]);
        continue;
      }
      [generatorPlugins addObject:generatorClass];
    }

    //-----------------------------------
    // Les outils du plugin (si il y en a)
    //-----------------------------------
    NSArray *toolsArray = [currentPlugin toolsClassArray];
    if (toolsArray != nil)
    {
//      NSLog (@"HCFRPluginManager : loading tools for plugin %@", [currentPlugin class]);
      NSEnumerator  *toolsEnumerator = [toolsArray objectEnumerator];
      Class currentToolClass;
      while (currentToolClass = [toolsEnumerator nextObject])
      {
        if ([currentToolClass isKindOfClass:[HCFRTool class]])
        {
          NSLog (@"HCFRPluginManager : Sensor do not inherit from HCFRSensor for plugin %@", [currentPlugin class]);
          continue;
        }
        [toolsPlugins addObject:currentToolClass];
      }      
    }
  }
  
  //-----------------------------------
  // Les controlleurs de graph (qui ne sont pas des plugins pour le moment)
  //-----------------------------------
  HCFRLuminanceGraphController   *lumGraph = [[HCFRLuminanceGraphController alloc] init];
  [graphicPlugins addObject:lumGraph];
  [lumGraph release];
  
  HCFRGammaGraphController       *gammaGraph = [[HCFRGammaGraphController alloc] init];
  [graphicPlugins addObject:gammaGraph];
  [gammaGraph release];
  
  HCFRRgbGraphController         *rgbGraph = [[HCFRRgbGraphController alloc] init];
  [graphicPlugins addObject:rgbGraph];
  [rgbGraph release];
  
  HCFRTemperatureGraphController *temperatureGraph = [[HCFRTemperatureGraphController alloc] init];
  [graphicPlugins addObject:temperatureGraph];
  [temperatureGraph release];
  
  HCFRCIEChartController         *cieGraph = [[HCFRCIEChartController alloc] init];
  [graphicPlugins addObject:cieGraph];
  [cieGraph release];
  
  HCFRSatLumGraphController      *satLumGraph = [[HCFRSatLumGraphController alloc] init];
  [graphicPlugins addObject:satLumGraph];
  [satLumGraph release];
  
  HCFRSatShiftGraphController    *satShiftGraph = [[HCFRSatShiftGraphController alloc] init];
  [graphicPlugins addObject:satShiftGraph];
  [satShiftGraph release];
  
  HCFRContinuousLuminanceGraphController    *continuousLuminanceGraph = [[HCFRContinuousLuminanceGraphController alloc] init];
  [graphicPlugins addObject:continuousLuminanceGraph];
  [continuousLuminanceGraph release];
}

-(NSArray*) plugins
{
  return pluginsArray;
}

-(NSArray*) graphicPlugins
{
  return graphicPlugins;
}
-(NSArray*) sensorPlugins
{
  return sensorPlugins;
}
-(NSArray*) generatorPlugins
{
  return generatorPlugins;
}
-(NSArray*) toolsPlugins
{
  return toolsPlugins;
}

-(HCFRSensor*) sensorWithId:(NSString*)sensorId
{
  HCFRSensor  *instance = nil;

  // on regarde dans la liste des instances
  instance = [sensorsInstances objectForKey:sensorId];
  
  // si on n'a rien trouvé, on instancie
  if (instance == nil)
  {
    NSEnumerator  *pluginsEnumerator = [sensorPlugins objectEnumerator];
    Class         currentSensorClass;
    while (currentSensorClass = [pluginsEnumerator nextObject])
    {
      if ([sensorId compare:[currentSensorClass sensorId] options:nil] == NSOrderedSame)
      {
        //      NSLog (@"HCFRPluginManager : Instanciating sensor %@", [currentSensorPlugin sensorName]);
        instance = [[currentSensorClass alloc] init];
        if (instance == nil)
        {
          [[NSException exceptionWithName:kHCFRPluginInstanciationFailureException
                                   reason:HCFRLocalizedString(@"The sensor failed to initialize.",@"The sensor failed to initialize.")
                                 userInfo:nil] raise];
        } 

        [sensorsInstances setObject:instance forKey:sensorId];
        [instance release];
        
        break;
      }
    }
  }
  return instance;
}
-(HCFRGenerator*) generatorWithId:(NSString*)generatorId
{
  HCFRGenerator  *instance = nil;

  // on regarde dans la liste des instances
  instance = [generatorsInstances objectForKey:generatorId];
  
  // si on n'a rien trouvé, on instancie
  if (instance == nil)
  {
    NSEnumerator  *pluginsEnumerator = [generatorPlugins objectEnumerator];
    Class         currentGeneratorClass;
    while (currentGeneratorClass = [pluginsEnumerator nextObject])
    {
      if ([generatorId compare:[currentGeneratorClass generatorId] options:nil] == NSOrderedSame)
      {
        //      NSLog (@"HCFRPluginManager : Instanciating generator %@", [currentGeneratorClass generatorName]);
        instance = [[currentGeneratorClass alloc] init];
        if (instance == nil)
        {
          [[NSException exceptionWithName:kHCFRPluginInstanciationFailureException
                                   reason:HCFRLocalizedString(@"The generator failed to initialize.",@"The generator failed to initialize.")
                                 userInfo:nil] raise];
        } 

        [generatorsInstances setObject:instance forKey:generatorId];
        [instance release];
          
        break;
      }
    }
  }
  return instance;
}

-(void) startTool:(Class)toolClass
{
  HCFRTool  *instance = [toolsInstances objectForKey:toolClass];
  
  if (instance == nil)
  {
    // il n'y a pas encore d'instance crée, on en crée une à la volée
    instance = [[toolClass alloc] init];
    if (instance == nil)
    {
      [[NSException exceptionWithName:kHCFRPluginInstanciationFailureException
                               reason:HCFRLocalizedString(@"The tool failed to initialize.",@"The tool failed to initialize.")
                             userInfo:nil] raise];
    } 
    [toolsInstances setObject:instance forKey:toolClass];
  }
  [instance startTool];
}
@end
