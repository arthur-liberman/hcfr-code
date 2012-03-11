//
//  ArgyllSensor.h
//  ColorHCFR
//
//  Created by Nick on 25/02/12.
//  Copyright 2012 Homecinema-fr.com. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <HCFRPlugins/HCFRPlugins.h>
#import "ArgyllMeterWrapper.h"


@interface ArgyllSensor : HCFRSensor {
	IBOutlet NSView*					setupView;
	
	int                                 m_DisplayType;
	ArgyllMeterWrapper::eReadingType	m_ReadingType;
	uint32_t							m_PortNumber;
	bool								m_HiRes;
	
	ArgyllMeterWrapper*					m_meter;
}

+(NSString*) sensorName;
+(NSString*) sensorId;

-(BOOL) isAvailable;
-(BOOL) needsCalibration;

-(BOOL) hasASetupView;
-(NSView*) setupView;

-(HCFRColor*) currentMeasure;

@end
