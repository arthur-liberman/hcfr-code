//
//  ArgyllSensorPlugin.m
//  ColorHCFR
//
//  Created by Nick on 25/02/12.
//  Copyright 2012 Homecinema-fr.com. All rights reserved.
//

#import "ArgyllSensorPlugin.h"
#import "ArgyllSensor.h"

@implementation ArgyllSensorPlugin
-(Class) sensorClass
{
	return [ArgyllSensor class];
}

@end
