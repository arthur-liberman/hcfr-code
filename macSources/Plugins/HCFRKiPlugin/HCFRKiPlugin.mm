//  ColorHCFR
//  HCFRKiPlugin.m
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

#import "HCFRKiPlugin.h"
#import "HCFRKiSensor.h"
#import "HCFRKiGenerator.h"
#import "HCFRIRProfilesManager.h"
#import "HCFRSensorupdater.h"

@implementation HCFRKiPlugin
-(Class) sensorClass
{
  return [HCFRKiSensor class];
}
-(Class) generatorClass
{
  return [HCFRKiGenerator class];
}
-(NSArray*) toolsClassArray
{
  return [NSArray arrayWithObjects:[HCFRIRProfilesManager class],
                                   [HCFRSensorUpdater class],
                                   nil];
}
@end
